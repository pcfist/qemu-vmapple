/*
 * Q35 chipset based pc system emulator
 *
 * Copyright (c) 2003-2004 Fabrice Bellard
 * Copyright (c) 2009, 2010
 *               Isaku Yamahata <yamahata at valinux co jp>
 *               VA Linux Systems Japan K.K.
 * Copyright (C) 2012 Jason Baron <jbaron@redhat.com>
 *
 * This is based on pc.c, but heavily modified.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "hw/hw.h"
#include "hw/loader.h"
#include "sysemu/arch_init.h"
#include "hw/i2c/smbus.h"
#include "hw/boards.h"
#include "hw/timer/mc146818rtc.h"
#include "hw/xen/xen.h"
#include "sysemu/kvm.h"
#include "hw/kvm/clock.h"
#include "hw/pci-host/q35.h"
#include "exec/address-spaces.h"
#include "hw/i386/ich9.h"
#include "hw/i386/smbios.h"
#include "hw/ide/pci.h"
#include "hw/ide/ahci.h"
#include "hw/usb.h"
#include "hw/cpu/icc_bus.h"
#include "qemu/error-report.h"
#include "hw/timer/hpet.h"

/* ICH9 AHCI has 6 ports */
#define MAX_SATA_PORTS     6

static bool has_acpi_build = true;
static bool smbios_defaults = true;
static bool smbios_legacy_mode;
static bool smbios_uuid_encoded = true;
/* Make sure that guest addresses aligned at 1Gbyte boundaries get mapped to
 * host addresses aligned at 1Gbyte boundaries.  This way we can use 1GByte
 * pages in the host.
 */
static bool gigabyte_align = true;
static bool has_reserved_memory = true;

/* PC hardware initialisation */
static void pc_ps4_init(MachineState *machine)
{
    PCMachineState *pc_machine = PC_MACHINE(machine);
    ram_addr_t below_4g_mem_size, above_4g_mem_size;
    Q35PCIHost *q35_host;
    DeviceState *sony_host;
    PCIHostState *phb;
    PCIBus *host_bus;
    PCIDevice *lpc;
    BusState *idebus[MAX_SATA_PORTS];
    ISADevice *rtc_state;
    ISADevice *floppy;
    MemoryRegion *pci_memory;
    MemoryRegion *rom_memory;
    MemoryRegion *ram_memory;
    GSIState *gsi_state;
    ISABus *isa_bus;
    int pci_enabled = 1;
    qemu_irq *cpu_irq;
    qemu_irq *gsi;
    qemu_irq *i8259;
    int i;
    ICH9LPCState *ich9_lpc;
    PCIDevice *ahci;
    DeviceState *icc_bridge;
    PcGuestInfo *guest_info;
    ram_addr_t lowmem;
    DriveInfo *hd[MAX_SATA_PORTS];

    /* Check whether RAM fits below 4G (leaving 1/2 GByte for IO memory
     * and 256 Mbytes for PCI Express Enhanced Configuration Access Mapping
     * also known as MMCFG).
     * If it doesn't, we need to split it in chunks below and above 4G.
     * In any case, try to make sure that guest addresses aligned at
     * 1G boundaries get mapped to host addresses aligned at 1G boundaries.
     * For old machine types, use whatever split we used historically to avoid
     * breaking migration.
     */
    if (machine->ram_size >= 0xb0000000) {
        lowmem = gigabyte_align ? 0x80000000 : 0xb0000000;
    } else {
        lowmem = 0xb0000000;
    }

    /* Handle the machine opt max-ram-below-4g.  It is basically doing
     * min(qemu limit, user limit).
     */
    if (lowmem > pc_machine->max_ram_below_4g) {
        lowmem = pc_machine->max_ram_below_4g;
        if (machine->ram_size - lowmem > lowmem &&
            lowmem & ((1ULL << 30) - 1)) {
            error_report("Warning: Large machine and max_ram_below_4g(%"PRIu64
                         ") not a multiple of 1G; possible bad performance.",
                         pc_machine->max_ram_below_4g);
        }
    }

    if (machine->ram_size >= lowmem) {
        above_4g_mem_size = machine->ram_size - lowmem;
        below_4g_mem_size = lowmem;
    } else {
        above_4g_mem_size = 0;
        below_4g_mem_size = machine->ram_size;
    }

    if (xen_enabled() && xen_hvm_init(&below_4g_mem_size, &above_4g_mem_size,
                                      &ram_memory) != 0) {
        fprintf(stderr, "xen hardware virtual machine initialisation failed\n");
        exit(1);
    }

    icc_bridge = qdev_create(NULL, TYPE_ICC_BRIDGE);
    object_property_add_child(qdev_get_machine(), "icc-bridge",
                              OBJECT(icc_bridge), NULL);

    pc_cpus_init(machine->cpu_model, icc_bridge);
    pc_acpi_init("q35-acpi-dsdt.aml");

    kvmclock_create();

    /* pci enabled */
    if (pci_enabled) {
        pci_memory = g_new(MemoryRegion, 1);
        memory_region_init(pci_memory, NULL, "pci", UINT64_MAX);
        rom_memory = pci_memory;
    } else {
        pci_memory = NULL;
        rom_memory = get_system_memory();
    }

    guest_info = pc_guest_info_init(below_4g_mem_size, above_4g_mem_size);
    guest_info->isapc_ram_fw = false;
    guest_info->has_acpi_build = has_acpi_build;
    guest_info->has_reserved_memory = has_reserved_memory;

    /* Migration was not supported in 2.0 for Q35, so do not bother
     * with this hack (see hw/i386/acpi-build.c).
     */
    guest_info->legacy_acpi_table_size = 0;

    if (smbios_defaults) {
        MachineClass *mc = MACHINE_GET_CLASS(machine);
        /* These values are guest ABI, do not change */
        smbios_set_defaults("QEMU", "Standard PC (Q35 + ICH9, 2009)",
                            mc->name, smbios_legacy_mode, smbios_uuid_encoded);
    }

    /* allocate ram and load rom/bios */
    if (!xen_enabled()) {
        pc_memory_init(machine, get_system_memory(),
                       below_4g_mem_size, above_4g_mem_size,
                       rom_memory, &ram_memory, guest_info);
    }

    /* irq lines */
    gsi_state = g_malloc0(sizeof(*gsi_state));
    if (kvm_irqchip_in_kernel()) {
        kvm_pc_setup_irq_routing(pci_enabled);
        gsi = qemu_allocate_irqs(kvm_pc_gsi_handler, gsi_state,
                                 GSI_NUM_PINS);
    } else {
        gsi = qemu_allocate_irqs(gsi_handler, gsi_state, GSI_NUM_PINS);
    }

    /* create pci host bus */
    q35_host = Q35_HOST_DEVICE(qdev_create(NULL, TYPE_Q35_HOST_DEVICE));

    object_property_add_child(qdev_get_machine(), "q35", OBJECT(q35_host), NULL);
    q35_host->mch.ram_memory = ram_memory;
    q35_host->mch.pci_address_space = pci_memory;
    q35_host->mch.system_memory = get_system_memory();
    q35_host->mch.address_space_io = get_system_io();
    q35_host->mch.below_4g_mem_size = below_4g_mem_size;
    q35_host->mch.above_4g_mem_size = above_4g_mem_size;
    q35_host->mch.guest_info = guest_info;
    /* pci */
    qdev_init_nofail(DEVICE(q35_host));
    phb = PCI_HOST_BRIDGE(q35_host);
    host_bus = phb->bus;
    /* create ISA bus */
    lpc = pci_create_simple_multifunction(host_bus, PCI_DEVFN(ICH9_LPC_DEV,
                                          ICH9_LPC_FUNC), true,
                                          TYPE_ICH9_LPC_DEVICE);

    object_property_add_link(OBJECT(machine), PC_MACHINE_ACPI_DEVICE_PROP,
                             TYPE_HOTPLUG_HANDLER,
                             (Object **)&pc_machine->acpi_dev,
                             object_property_allow_set_link,
                             OBJ_PROP_LINK_UNREF_ON_RELEASE, &error_abort);
    object_property_set_link(OBJECT(machine), OBJECT(lpc),
                             PC_MACHINE_ACPI_DEVICE_PROP, &error_abort);

    ich9_lpc = ICH9_LPC_DEVICE(lpc);
    ich9_lpc->pic = gsi;
    ich9_lpc->ioapic = gsi_state->ioapic_irq;
    pci_bus_irqs(host_bus, ich9_lpc_set_irq, ich9_lpc_map_irq, ich9_lpc,
                 ICH9_LPC_NB_PIRQS);
    pci_bus_set_route_irq_fn(host_bus, ich9_route_intx_pin_to_irq);
    isa_bus = ich9_lpc->isa_bus;

    /*end early*/
    isa_bus_irqs(isa_bus, gsi);

    if (kvm_irqchip_in_kernel()) {
        i8259 = kvm_i8259_init(isa_bus);
    } else if (xen_enabled()) {
        i8259 = xen_interrupt_controller_init();
    } else {
        cpu_irq = pc_allocate_cpu_irq();
        i8259 = i8259_init(isa_bus, cpu_irq[0]);
    }

    for (i = 0; i < ISA_NUM_IRQS; i++) {
        gsi_state->i8259_irq[i] = i8259[i];
    }
    if (pci_enabled) {
        ioapic_init_gsi(gsi_state, "q35");
    }
    qdev_init_nofail(icc_bridge);

    pc_register_ferr_irq(gsi[13]);

    /* init basic PC hardware */
    no_hpet = true;
    pc_basic_device_init(isa_bus, gsi, &rtc_state, &floppy,
                         !pc_machine->vmport, 0xff0104);

    /* connect pm stuff to lpc */
    ich9_lpc_pm_init(lpc);

#if 0
    hpet = qdev_try_create(BUS(PCI_HOST_BRIDGE(sony_host)->bus), TYPE_AEOLIA_GBE);
    qdev_prop_set_bit(hpet, "multifunction", true);
    qdev_prop_set_int32(hpet, "addr", PCI_DEVFN(0x0, 1));
    qdev_init_nofail(hpet);

    hpet = qdev_try_create(BUS(PCI_HOST_BRIDGE(sony_host)->bus), TYPE_AEOLIA_AHCI);
    qdev_prop_set_bit(hpet, "multifunction", true);
    qdev_prop_set_int32(hpet, "addr", PCI_DEVFN(0x0, 2));
    qdev_init_nofail(hpet);

    hpet = qdev_try_create(BUS(PCI_HOST_BRIDGE(sony_host)->bus), TYPE_AEOLIA_SDHCI);
    qdev_prop_set_bit(hpet, "multifunction", true);
    qdev_prop_set_int32(hpet, "addr", PCI_DEVFN(0x0, 3));
    qdev_init_nofail(hpet);
#endif

    sony_host = (qdev_create(BUS(host_bus), "aeolia-acpi"));
    qdev_prop_set_bit(sony_host, "multifunction", true);
    qdev_prop_set_int32(sony_host, "addr", PCI_DEVFN(20, 0));
    qdev_init_nofail(sony_host);

    sony_host = (qdev_create(BUS(host_bus), "aeolia-gbe"));
    qdev_prop_set_bit(sony_host, "multifunction", true);
    qdev_prop_set_int32(sony_host, "addr", PCI_DEVFN(20, 1));
    qdev_init_nofail(sony_host);

    sony_host = (qdev_create(BUS(host_bus), "aeolia-ahci"));
    qdev_prop_set_bit(sony_host, "multifunction", true);
    qdev_prop_set_int32(sony_host, "addr", PCI_DEVFN(20, 2));
    qdev_init_nofail(sony_host);

    ahci = sony_host;
    idebus[0] = qdev_get_child_bus(&ahci->qdev, "ide.0");
    idebus[1] = qdev_get_child_bus(&ahci->qdev, "ide.1");
    g_assert(MAX_SATA_PORTS == ICH_AHCI(ahci)->ahci.ports);
    ide_drive_get(hd, ICH_AHCI(ahci)->ahci.ports);
    ahci_ide_create_devs(ahci, hd);

    sony_host = (qdev_create(BUS(host_bus), "aeolia-bucket"));
    qdev_prop_set_bit(sony_host, "multifunction", true);
    qdev_prop_set_int32(sony_host, "addr", PCI_DEVFN(20, 4));
    qdev_init_nofail(sony_host);

    sony_host = (qdev_create(BUS(host_bus), "aeolia-spm"));
    qdev_prop_set_bit(sony_host, "multifunction", true);
    qdev_prop_set_int32(sony_host, "addr", PCI_DEVFN(20, 6));
    qdev_init_nofail(sony_host);

    sony_host = (qdev_create(BUS(host_bus), "aeolia-dmac"));
    qdev_prop_set_bit(sony_host, "multifunction", true);
    qdev_prop_set_int32(sony_host, "addr", PCI_DEVFN(20, 5));
    qdev_init_nofail(sony_host);

#if 0
    /* ahci and SATA device, for q35 1 ahci controller is built-in */
    ahci = pci_create_simple_multifunction(host_bus,
                                           PCI_DEVFN(ICH9_SATA1_DEV,
                                                     ICH9_SATA1_FUNC),
                                           true, "ich9-ahci");
#endif

    if (usb_enabled(false)) {
        /* Should we create 6 UHCI according to ich9 spec? */
        ehci_create_ich9_with_companions(host_bus, 0x1d);
    }

    /* TODO: Populate SPD eeprom data.  */
    smbus_eeprom_init(ich9_smb_init(host_bus,
                                    PCI_DEVFN(ICH9_SMB_DEV, ICH9_SMB_FUNC),
                                    0xb100),
                      8, NULL, 0);

    pc_cmos_init(below_4g_mem_size, above_4g_mem_size, machine->boot_order,
                 machine, floppy, idebus[0], idebus[1], rtc_state);

    /* the rest devices to which pci devfn is automatically assigned */
    pc_vga_init(isa_bus, host_bus);
    pc_nic_init(isa_bus, host_bus);
    if (pci_enabled) {
        pc_pci_device_init(host_bus);
    }
}

#define PC_PS4_MACHINE_OPTIONS \
    PC_DEFAULT_MACHINE_OPTIONS, \
    .family = "pc_ps4", \
    .desc = "Standard PC (Q35 + ICH9, 2009)", \
    .hot_add_cpu = pc_hot_add_cpu, \
    .units_per_default_bus = 1

#define PC_PS4_2_2_MACHINE_OPTIONS                      \
    PC_PS4_MACHINE_OPTIONS,                             \
    .default_machine_opts = "firmware=bios-256k.bin",   \
    .default_display = "std"

static QEMUMachine pc_ps4_machine_v2_2 = {
    PC_PS4_2_2_MACHINE_OPTIONS,
    .name = "pc-ps4",
    .alias = "ps4",
    .init = pc_ps4_init,
    .is_default = 1,
};

static void pc_ps4_machine_init(void)
{
    qemu_register_pc_machine(&pc_ps4_machine_v2_2);
}

machine_init(pc_ps4_machine_init);
