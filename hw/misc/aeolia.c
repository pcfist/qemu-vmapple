/*
 *  Aeolia bucket interface
 *
 *  Copyright (c) 2015 Alexander Graf
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "hw/hw.h"
#include "hw/i386/pc.h"
#include "ui/console.h"
#include "qemu/timer.h"
#include "hw/sysbus.h"
#include "hw/pci/msi.h"
#include "hw/timer/hpet.h"
#include "sysemu/block-backend.h"

#define AEOLIA_DEBUG
#ifdef AEOLIA_DEBUG
#define DPRINTF printf
#else
#define DPRINTF(...)
#endif

#define TYPE_AEOLIA_ACPI "aeolia-acpi"
#define TYPE_AEOLIA_SPM "aeolia-spm"
#define TYPE_AEOLIA_GBE "aeolia-gbe"
#define TYPE_AEOLIA_DMAC "aeolia-dmac"
#define TYPE_AEOLIA_BUCKET "aeolia-bucket"

#define AEOLIA_ACPI(obj) OBJECT_CHECK(AeoliaAcpiState, (obj), TYPE_AEOLIA_ACPI)
#define AEOLIA_SPM(obj) OBJECT_CHECK(AeoliaSPMState, (obj), TYPE_AEOLIA_SPM)
#define AEOLIA_DMAC(obj) OBJECT_CHECK(AeoliaDMACState, (obj), TYPE_AEOLIA_DMAC)
#define AEOLIA_GBE(obj) OBJECT_CHECK(AeoliaGBEState, (obj), TYPE_AEOLIA_GBE)
#define AEOLIA_BUCKET(obj) OBJECT_CHECK(AeoliaBucketState, (obj), TYPE_AEOLIA_BUCKET)

typedef struct AeoliaGBEState {
    /*< private >*/
    PCIDevice parent_obj;
    /*< public >*/

    MemoryRegion iomem[3];
} AeoliaGBEState;

typedef struct AeoliaDMACState {
    /*< private >*/
    PCIDevice parent_obj;
    /*< public >*/

    MemoryRegion iomem[3];
} AeoliaDMACState;

typedef struct AeoliaSPMState {
    /*< private >*/
    PCIDevice parent_obj;
    uint8_t data[0x40000];
    /*< public >*/

    MemoryRegion iomem[3];
} AeoliaSPMState;

typedef struct AeoliaAcpiState {
    /*< private >*/
    PCIDevice parent_obj;
    /*< public >*/

    MemoryRegion iomem[3];
} AeoliaAcpiState;

typedef struct AeoliaBucketState {
    /*< private >*/
    PCIDevice parent_obj;
    /*< public >*/

    MemoryRegion iomem[6];
    SysBusDevice *hpet;
    uint64_t bars[8 * 6][2];
    MemoryRegion bar[8 * 6];
    BlockDriverState *flash;
    uint32_t cmd;
    uint32_t dma_wip;
    uint32_t dma_active; // transfer enable == 1?
    uint32_t dma_addr;
    uint32_t dma_len;
    uint32_t sflash_offset;
    uint32_t sflash_data;
    uint32_t sflash_status;

    uint32_t msi_data_fn4;
    uint32_t msi_data_fn5;
    uint32_t msi_data_fn4dev3;
    uint32_t msi_data_fn4dev5;
    uint32_t msi_data_fn4dev11;
    uint32_t msi_data_fn5dev1;
    uint32_t msi_addr_fn4;
    uint32_t msi_addr_fn5;

    uint32_t doorbell_status;
    uint32_t icc_status;
} AeoliaBucketState;

void hpet_set_msi(SysBusDevice *hpet, uint32_t addr, uint32_t val);
static AeoliaSPMState *spm; // XXX

static uint64_t aeolia_spm_read(void *opaque, hwaddr addr,
                                unsigned size)
{
    AeoliaSPMState *s = opaque;
    uint64_t r;

    r = ldl_le_p(&s->data[addr]);
    DPRINTF("qemu: %s[%d] at %" PRIx64 " -> %" PRIx64 "\n", __func__, size, addr, r);

    return r;
}

static void aeolia_spm_write(void *opaque, hwaddr addr,
                             uint64_t value, unsigned size)
{
    AeoliaSPMState *s = opaque;

    DPRINTF("qemu: %s[%d] at %" PRIx64 " <- %#lx\n", __func__, size, addr, value);

    stl_le_p(&s->data[addr], value);
}

static const MemoryRegionOps aeolia_spm_ops = {
    .read = aeolia_spm_read,
    .write = aeolia_spm_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static uint64_t aeolia_ram_read(void *opaque, hwaddr addr,
                              unsigned size)
{
#if 0
    char *s = opaque;
    DPRINTF("qemu: aeolia_ram_readl %s at %" PRIx64 "\n", s, addr);
#endif

    return 0;
}

static void aeolia_ram_write(void *opaque, hwaddr addr,
                           uint64_t value, unsigned size)
{
#if 0
    char *s = opaque;
    DPRINTF("qemu: aeolia_ram_writel %s at %" PRIx64 " = %#lx\n", s, addr, value);
#endif
}

static const MemoryRegionOps aeolia_ram_ops = {
    .read = aeolia_ram_read,
    .write = aeolia_ram_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};


#define AEOLIA_CMD_GET_STATUS 5
#define AEOLIA_STATUS_BUSY 0x10000
#define AEOLIA_STATUS_DONE 0x20000
#define AEOLIA_DMA_STATUS_DONE 0x4
#define AEOLIA_DMA_STATUS_MASK 0x7

static uint64_t aeolia_bucket_misc_read(void *opaque, hwaddr addr,
                                        unsigned size)
{
    AeoliaBucketState *s = opaque;
    uint64_t r = 0;

    switch (addr) {
        case 0x184804: /* ICC doorbell */
            r = s->doorbell_status;
            break;
        case 0x184814: /* ICC status */
            r = s->icc_status;
            break;
        case 0x182000 ... 0x182400:
            io_mem_read(s->hpet->mmio[0].memory, addr - 0x182000, &r, size);
            return r;
        case 0x1c8000 ... 0x1c8180:
            addr &= 0x1ff;
            return s->bars[addr / 0x8][(addr & 0x4) ? 1 : 0];
        case 0xc2000: // aeolia_sflash_read()
            r = s->sflash_offset;
            break;
        case 0xc2004: // aeolia_sflash_read()
            r = s->sflash_data;
            break;
        case 0xc203c:
            r = s->sflash_status;
            break;
        case 0xc2040: // issue_command()
            switch (s->cmd) {
                case AEOLIA_CMD_GET_STATUS:
                    r = AEOLIA_STATUS_DONE;
                    break;
            }
            break;
        case 0xc3000: // read_dma_callback()
            s->dma_wip ^= AEOLIA_DMA_STATUS_DONE;
            r = s->dma_wip;
            break;
        case 0xc3004: // aeolia_sflash_read()
            r = s->dma_active;
            break;
    }

    DPRINTF("qemu: %s at %" PRIx64 " -> %" PRIx64 "\n", __func__, addr, r);
    return r;
}

#define AEOLIA_FUNC_HPET 5
static void update_bar(AeoliaBucketState *s, int id)
{
    int bar = id % 6;
    int func = id / 6;
    char *str = g_strdup_printf("Func %d BAR %d", func, bar);

    if (!s->bars[id][0] || !s->bars[id][1])
        return;

printf("XXX init %#lx bytes\n", s->bars[id][0]);
    memory_region_init_io(&s->bar[id], OBJECT(s), &aeolia_ram_ops, str,
                          str, s->bars[id][0]);
printf("XXX map at %#lx\n", s->bars[id][1]);

    memory_region_add_subregion_overlap(get_system_memory(),
        s->bars[id][1], &s->bar[id], -20);

#if 0
    if (func == AEOLIA_FUNC_HPET && bar == 0) {
        sysbus_mmio_map(SYS_BUS_DEVICE(s->hpet), 0, s->bars[AEOLIA_FUNC_HPET][0]);
    }
#endif
}

static void aeolia_do_cmd(AeoliaBucketState *s)
{
    void *p;

    switch (s->cmd & 0x7) {
    case 0x3:
        switch (s->cmd & ~0x7) {
        case 0x12100100:
            p = g_malloc(s->dma_len);
            bdrv_pread(s->flash, s->sflash_offset, p, s->dma_len);
            cpu_physical_memory_write(s->dma_addr, p, s->dma_len);
            g_free(p);
            s->sflash_status |= 1;
            DPRINTF("DMA transfer of %#x bytes from %#x to %x completed\n",
                    s->dma_len, s->sflash_offset, s->dma_addr);

            /* send MSI */
            stl_le_phys(&address_space_memory,
                        s->msi_addr_fn4, s->msi_data_fn4 | s->msi_data_fn4dev11);
            DPRINTF("Sending MSI to %#x/%#x\n",
                     s->msi_addr_fn4, s->msi_data_fn4 | s->msi_data_fn4dev11);
            break;
        }
    }
}

#define ICC_STATUS_MSG_PENDING   0x0001
#define ICC_STATUS_IRQ_PENDING   0x0002
#define ICC_FLAG_REPLY           0x4000

static void icc_send_irq(AeoliaBucketState *s)
{
    s->icc_status |= ICC_STATUS_IRQ_PENDING;

    /* Trigger MSI */
    stl_le_phys(&address_space_memory, s->msi_addr_fn4,
                s->msi_data_fn4 | s->msi_data_fn4dev3);
}

static void icc_calculate_csum(uint8_t *data)
{
    uint16_t csum;
    uint16_t *csum_ptr = (uint16_t *)&data[0x08];
    int i;

    *csum_ptr = 0;
    for (i = 0; i < 0x7f0; i++) {
        csum += data[i];
    }
    *csum_ptr = csum;
}

#define ICC_CMD_QUERY                         0x42
#define ICC_CMD_QUERY_BOARD                   0x02
#define ICC_CMD_QUERY_BOARD_FLAG_BOARD_ID       0x0005
#define ICC_CMD_QUERY_BOARD_FLAG_VERSION        0x0006
#define ICC_CMD_QUERY_NVRAM                   0x03
#define ICC_CMD_QUERY_NVRAM_FLAG_WRITE          0x0000
#define ICC_CMD_QUERY_NVRAM_FLAG_READ           0x0001
#define ICC_CMD_QUERY_BUTTONS                 0x08
#define ICC_CMD_QUERY_BUTTONS_FLAG_STATE        0x0000
#define ICC_CMD_QUERY_BUTTONS_FLAG_LIST         0x0001
#define ICC_CMD_QUERY_SNVRAM_READ             0x8d

static int icc_query_nvram_read(AeoliaBucketState *s)
{
    short buffer_len = lduw_le_p(&spm->data[0x2c008]);
    short len = lduw_le_p(&spm->data[0x2c010]);
    short offset = lduw_le_p(&spm->data[0x2c00e]);

    DPRINTF("qemu: ICC: NVRAM read %d bytes from %#x!\n", len, offset);
    /* XXX write read contents at 0x12 */

    return buffer_len;
}

static int icc_query_board_id(AeoliaBucketState *s)
{
    uint16_t reply[] = { 0x0000, 0x0203, 0x0101, 0x0102,
                         0x0106, 0x0000 };
    int i;

    for (i = 0; i < ARRAY_SIZE(reply); i++) {
        stw_le_p(&spm->data[0x2c00c + (i * 2)], reply[i]);
    }

    DPRINTF("qemu: ICC: read board id\n");

    return sizeof(reply);
}

static int icc_query_board_version(AeoliaBucketState *s)
{
    uint16_t reply[] = { 0x0000, 0x0000, 0x0000, 0x0025,
                         0x0000, 0x0000, 0x0000, 0x0a93,
                         0x0000, 0x0020, 0x0000, 0x0045,
                         0x0000, 0x0000, 0x0000, 0x0000,
                         0x0000, 0x0001, 0x0000, 0x0000,
                         0x0710, 0x4520, 0x0054, 0x0000 };
    int i;

    for (i = 0; i < ARRAY_SIZE(reply); i++) {
        stw_le_p(&spm->data[0x2c00c + (i * 2)], reply[i]);
    }

    DPRINTF("qemu: ICC: read board version\n");

    return sizeof(reply);
}

static void icc_query(AeoliaBucketState *s)
{
    char id = spm->data[0x2c001];
    short flags = lduw_le_p(&spm->data[0x2c002]);
    short token = lduw_le_p(&spm->data[0x2c006]);
    char *reply = (char *)&spm->data[0x2c800];
    uint16_t len = lduw_le_p(&spm->data[0x2c008]);

    DPRINTF("qemu: ICC: Device enumeration!\n");

    flags |= ICC_FLAG_REPLY;

    memset(reply, 0, 0x7f0);
    reply[0] = 0x42;
    reply[1] = id;
    stw_le_p(&reply[2], flags);
    stw_le_p(&reply[6], token);

    switch (id) {
    case ICC_CMD_QUERY_BOARD:
        switch (flags) {
        case ICC_CMD_QUERY_BOARD_FLAG_BOARD_ID:
            len = icc_query_board_id(s);
            break;
        case ICC_CMD_QUERY_BOARD_FLAG_VERSION:
            len = icc_query_board_version(s);
            break;
        default:
            DPRINTF("qemu: ICC: Unknown NVRAM query %#x!\n", flags);
        }
        break;
    case ICC_CMD_QUERY_NVRAM:
        switch (flags) {
        case ICC_CMD_QUERY_NVRAM_FLAG_READ:
            len = icc_query_nvram_read(s);
            break;
        default:
            DPRINTF("qemu: ICC: Unknown NVRAM query %#x!\n", flags);
        }
        break;
    default:
        DPRINTF("qemu: ICC: Unknown query %#x!\n", id);
        break;
    }

    stw_le_p(&reply[8], len);
    icc_calculate_csum(&spm->data[0x2c800]);
    s->icc_status |= ICC_STATUS_MSG_PENDING;
    icc_send_irq(s);
}

static void icc_doorbell(AeoliaBucketState *s)
{
    if (s->doorbell_status & 2) {
        /* IRQ active, disable it */
        s->doorbell_status &= ~2;
    }

    if (s->doorbell_status & 1) {
        char cmd = spm->data[0x2c000];
        DPRINTF("qemu: ICC: New command: %x\n", cmd);
        switch (cmd) {
        case 0x42:
            icc_query(s);
            break;
        }
        s->doorbell_status &= ~1;
    }
}

static void icc_doorbell2(AeoliaBucketState *s, int type)
{
    if (type != 3) {
        /* 3 is doorbell for 2c000 init, others unknown */
        return;
    }

    /* Indicate we're ready to receive */
    stl_le_p(&spm->data[0x2c7f4], 1);
}

static void aeolia_bucket_misc_write(void *opaque, hwaddr addr,
                                     uint64_t value, unsigned size)
{
    AeoliaBucketState *s = opaque;
    char tmp[4];
    DPRINTF("qemu: %s at %" PRIx64 " <- %#lx\n", __func__, addr, value);

    switch (addr) {
        case 0x1c849c:
            s->msi_data_fn4 = value;
            break;
        case 0x1c84a0:
            s->msi_data_fn5 = value;
            break;
        case 0x1c854c:
            s->msi_data_fn4dev3 = value;
            break;
        case 0x1c8554:
            s->msi_data_fn4dev5 = value;
            hpet_set_msi(s->hpet, s->msi_addr_fn4, s->msi_data_fn4dev5 | s->msi_data_fn4);
            break;
        case 0x1c856c:
            s->msi_data_fn4dev11 = value;
            break;
        case 0x1c85a4:
            s->msi_data_fn5dev1 = value;
            break;
        case 0x1c84bc:
            s->msi_addr_fn4 = value;
            break;
        case 0x1c84c0:
            s->msi_addr_fn5 = value;
            break;
        case 0x184804: /* ICC doorbell */
            s->doorbell_status |= value;
            icc_doorbell(s);
            break;
        case 0x184814: /* ICC status */
            s->icc_status &= ~value;
            break;
        case 0x184824: /* ICC doorbell */
            icc_doorbell2(s, value);
            break;
        case 0x182000 ... 0x182400:
            io_mem_write(s->hpet->mmio[0].memory, addr - 0x182000, value, size);
            break;
        case 0x1c8000 ... 0x1c8180:
            addr &= 0x1ff;
            s->bars[addr / 0x8][(addr & 0x4) ? 1 : 0] = value;
            update_bar(s, addr / 0x8);
            break;
        case 0xc2000: // aeolia_sflash_read()
            s->sflash_offset = value;
            if (s->flash) {
                bdrv_pread(s->flash, value, tmp, 4);
                s->sflash_data = ldl_le_p(tmp);
            }
            break;
        case 0xc2004: // aeolia_sflash_read()
            s->sflash_data = value;
            break;
        case 0xc2008: // issue_command()
            s->cmd = value & ~0x80000000;
            aeolia_do_cmd(s);
            break;
        case 0xc203c:
            s->sflash_status = value;
            break;
        case 0xc2044: // issue_command()
            s->dma_addr = value;
            break;
        case 0xc2048: // issue_command()
            s->dma_len = value & ~0x80000000;
            break;
        case 0xc3004: // aeolia_sflash_read()
            s->dma_active = value;
            break;
    }
}

static const MemoryRegionOps aeolia_bucket_misc_ops = {
    .read = aeolia_bucket_misc_read,
    .write = aeolia_bucket_misc_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static uint64_t aeolia_bucket_self_read(void *opaque, hwaddr addr,
                                        unsigned size)
{
    uint64_t r = 0;

    switch (addr) {
        case 0x1104:
            r = 0x41b30130;
            break;
        case 0x1108:
            r = 0x52024d44;
            break;
        case 0x110c:
            r = 0x00000300;
            break;
    }

    DPRINTF("qemu: %s at %" PRIx64 " -> %" PRIx64 "\n", __func__, addr, r);
    return r;
}

static void aeolia_bucket_self_write(void *opaque, hwaddr addr,
                                     uint64_t value, unsigned size)
{
    DPRINTF("qemu: %s at %" PRIx64 " <- %#lx\n", __func__, addr, value);
}

static const MemoryRegionOps aeolia_bucket_self_ops = {
    .read = aeolia_bucket_self_read,
    .write = aeolia_bucket_self_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static int aeolia_dmac_init(PCIDevice *dev)
{
    AeoliaDMACState *s = AEOLIA_DMAC(dev);
    uint8_t *pci_conf = dev->config;

    pci_set_word(pci_conf + PCI_COMMAND, PCI_COMMAND_IO | PCI_COMMAND_MEMORY |
                 PCI_COMMAND_MASTER | PCI_COMMAND_SPECIAL);

#if 0
    memory_region_init_io(&s->iomem[0], OBJECT(dev), &aeolia_ram_ops, "dmac-0",
                          "aeolia-dmac-0", 0x100);
    pci_register_bar(dev, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->iomem[0]);
    memory_region_init_io(&s->iomem[1], OBJECT(dev), &aeolia_ram_ops, "dmac-1",
                          "aeolia-dmac-1", 0x100);
    pci_register_bar(dev, 1, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->iomem[1]);
#endif
    memory_region_init_io(&s->iomem[2], OBJECT(dev), &aeolia_ram_ops, (void*)"dmac-2",
                          "aeolia-dmac-2", 0x1000);
    pci_register_bar(dev, 2, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->iomem[2]);

    if (pci_is_express(dev)) {
        pcie_endpoint_cap_init(dev, 0xa0);
    }
    msi_init(dev, 0x50, 1, true, false);

    return 0;
}

static int aeolia_gbe_init(PCIDevice *dev)
{
    AeoliaGBEState *s = AEOLIA_GBE(dev);
    uint8_t *pci_conf = dev->config;

    pci_set_word(pci_conf + PCI_COMMAND, PCI_COMMAND_IO | PCI_COMMAND_MEMORY |
                 PCI_COMMAND_MASTER | PCI_COMMAND_SPECIAL);

    memory_region_init_io(&s->iomem[0], OBJECT(dev), &aeolia_ram_ops, (void*)"gbe-0",
                          "aeolia-gbe-0", 0x100);
    pci_register_bar(dev, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->iomem[0]);
    memory_region_init_io(&s->iomem[1], OBJECT(dev), &aeolia_ram_ops, (void*)"gbe-1",
                          "aeolia-gbe-1", 0x100);
    pci_register_bar(dev, 1, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->iomem[1]);
    memory_region_init_io(&s->iomem[2], OBJECT(dev), &aeolia_ram_ops, (void*)"gbe-2",
                          "aeolia-gbe-2", 0x100);
    pci_register_bar(dev, 2, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->iomem[2]);

    if (pci_is_express(dev)) {
        pcie_endpoint_cap_init(dev, 0xa0);
    }

    return 0;
}

static int aeolia_spm_init(PCIDevice *dev)
{
    AeoliaSPMState *s = AEOLIA_SPM(dev);
    uint8_t *pci_conf = dev->config;

    pci_set_word(pci_conf + PCI_COMMAND, PCI_COMMAND_IO | PCI_COMMAND_MEMORY |
                 PCI_COMMAND_MASTER | PCI_COMMAND_SPECIAL);

    memory_region_init_io(&s->iomem[1], OBJECT(dev), &aeolia_ram_ops, (void*)"ddr3",
                          "aeolia-ddr3", 0x10000000);
    pci_register_bar(dev, 2, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->iomem[1]);

    memory_region_init_io(&s->iomem[0], OBJECT(dev), &aeolia_spm_ops, s,
                          "aeolia-spm", 0x40000);
    pci_register_bar(dev, 5, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->iomem[0]);

    if (pci_is_express(dev)) {
        pcie_endpoint_cap_init(dev, 0xa0);
    }

    spm = s; // XXX

    return 0;
}

static int aeolia_acpi_init(PCIDevice *dev)
{
    AeoliaAcpiState *s = AEOLIA_ACPI(dev);
    uint8_t *pci_conf = dev->config;

    pci_set_word(pci_conf + PCI_COMMAND, PCI_COMMAND_IO | PCI_COMMAND_MEMORY |
                 PCI_COMMAND_MASTER | PCI_COMMAND_SPECIAL);

    /* Aeolia Area */
    memory_region_init_io(&s->iomem[0], OBJECT(dev), &aeolia_ram_ops, (void*)"acpi-0",
                          "aeolia-acpi-0", 0x2000000);
    pci_register_bar(dev, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->iomem[0]);
    memory_region_init_io(&s->iomem[1], OBJECT(dev), &aeolia_ram_ops, (void*)"acpi-1",
                          "aeolia-acpi-1", 0x100);
    pci_register_bar(dev, 1, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->iomem[1]);
    memory_region_init_io(&s->iomem[2], OBJECT(dev), &aeolia_ram_ops, (void*)"acpi-2",
                          "aeolia-acpi-2", 0x100);
    pci_register_bar(dev, 2, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->iomem[2]);

    if (pci_is_express(dev)) {
        pcie_endpoint_cap_init(dev, 0xa0);
    }
    msi_init(dev, 0x50, 1, true, false);

    return 0;
}

static int aeolia_bucket_init(PCIDevice *dev)
{
    AeoliaBucketState *s = AEOLIA_BUCKET(dev);
    uint8_t *pci_conf = dev->config;
    DriveInfo *dinfo;

    pci_set_word(pci_conf + PCI_COMMAND, PCI_COMMAND_IO | PCI_COMMAND_MEMORY |
                 PCI_COMMAND_MASTER | PCI_COMMAND_SPECIAL);

    /* Aeolia Area */
    memory_region_init_io(&s->iomem[0], OBJECT(dev), &aeolia_ram_ops, (void*)"eMMC",
                          "aeolia eMMC", 0x100000);
    pci_register_bar(dev, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->iomem[0]);
    memory_region_init_io(&s->iomem[2], OBJECT(dev), &aeolia_bucket_self_ops, s,
                          "aeolia Pervasive 0", 0x8000);
    pci_register_bar(dev, 2, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->iomem[2]);
    memory_region_init_io(&s->iomem[4], OBJECT(dev), &aeolia_bucket_misc_ops, s,
                          "aeolia misc peripherals", 0x200000);
    pci_register_bar(dev, 4, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->iomem[4]);

    if (pci_is_express(dev)) {
        pcie_endpoint_cap_init(dev, 0xa0);
    }
    msi_init(dev, 0x50, 1, true, false);

    s->hpet = SYS_BUS_DEVICE(qdev_try_create(NULL, TYPE_HPET));
    qdev_prop_set_uint8(DEVICE(s->hpet), "timers", 4);
    qdev_prop_set_uint32(DEVICE(s->hpet), HPET_INTCAP, 0x10);
    qdev_init_nofail(DEVICE(s->hpet));

    dinfo = drive_get(IF_PFLASH, 0, 0);
    if (dinfo) {
        s->flash = blk_bs(blk_by_legacy_dinfo(dinfo));
    }

    return 0;
}

static void aeolia_spm_class_init(ObjectClass *klass, void *data)
{
    PCIDeviceClass *pc = PCI_DEVICE_CLASS(klass);

    pc->vendor_id = 0x104d;
    pc->device_id = 0x90a3;
    pc->revision = 1;
    pc->is_express = true;
    pc->class_id = PCI_CLASS_STORAGE_RAID;
    pc->init = aeolia_spm_init;
}

static void aeolia_dmac_class_init(ObjectClass *klass, void *data)
{
    PCIDeviceClass *pc = PCI_DEVICE_CLASS(klass);

    pc->vendor_id = 0x104d;
    pc->device_id = 0x90a2;
    pc->revision = 1;
    pc->is_express = true;
    pc->class_id = PCI_CLASS_STORAGE_RAID;
    pc->init = aeolia_dmac_init;
}

static void aeolia_bucket_class_init(ObjectClass *klass, void *data)
{
    PCIDeviceClass *pc = PCI_DEVICE_CLASS(klass);

    pc->vendor_id = 0x104d;
    pc->device_id = 0x90a1;
    pc->revision = 1;
    pc->is_express = true;
    pc->class_id = PCI_CLASS_STORAGE_RAID;
    pc->init = aeolia_bucket_init;
}

static void aeolia_acpi_class_init(ObjectClass *klass, void *data)
{
    PCIDeviceClass *pc = PCI_DEVICE_CLASS(klass);

    pc->vendor_id = 0x104d;
    pc->device_id = 0x908f;
    pc->revision = 1;
    pc->is_express = true;
    pc->class_id = PCI_CLASS_STORAGE_RAID;
    pc->init = aeolia_acpi_init;
}

static void aeolia_gbe_class_init(ObjectClass *klass, void *data)
{
    PCIDeviceClass *pc = PCI_DEVICE_CLASS(klass);

    pc->vendor_id = 0x104d;
    pc->device_id = 0x909e;
    pc->revision = 1;
    pc->is_express = true;
    pc->class_id = PCI_CLASS_STORAGE_RAID;
    pc->init = aeolia_gbe_init;
}

static const TypeInfo aeolia_acpi_info = {
    .name          = TYPE_AEOLIA_ACPI,
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(AeoliaAcpiState),
    .class_init    = aeolia_acpi_class_init,
};

static const TypeInfo aeolia_dmac_info = {
    .name          = TYPE_AEOLIA_DMAC,
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(AeoliaDMACState),
    .class_init    = aeolia_dmac_class_init,
};

static const TypeInfo aeolia_spm_info = {
    .name          = TYPE_AEOLIA_SPM,
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(AeoliaSPMState),
    .class_init    = aeolia_spm_class_init,
};

static const TypeInfo aeolia_gbe_info = {
    .name          = TYPE_AEOLIA_GBE,
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(AeoliaGBEState),
    .class_init    = aeolia_gbe_class_init,
};

static const TypeInfo aeolia_bucket_info = {
    .name          = TYPE_AEOLIA_BUCKET,
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(AeoliaBucketState),
    .class_init    = aeolia_bucket_class_init,
};

static void aeolia_register_types(void)
{
    type_register_static(&aeolia_acpi_info);
    type_register_static(&aeolia_dmac_info);
    type_register_static(&aeolia_spm_info);
    type_register_static(&aeolia_gbe_info);
    type_register_static(&aeolia_bucket_info);
}

type_init(aeolia_register_types)
