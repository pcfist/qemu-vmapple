/*
 * Raspberry Pi GPIO emulation (c) 2017 Alexander Graf
 * This code is licensed under the GNU GPLv2 and later.
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/gpio/bcm2835_gpio.h"
#include "qemu/log.h"

#define GPFSEL0                         0x0
#define GPFSEL1                         0x4
#define GPFSEL2                         0x8
#define GPFSEL3                         0xc
#define GPFSEL4                         0x10
#define GPFSEL5                         0x14
#define GPSET0                          0x1c
#define GPSET1                          0x20
#define GPCLR0                          0x28
#define GPCLR1                          0x2c
#define GPLEV0                          0x34
#define GPLEV1                          0x38
#define GPEDS0                          0x40
#define GPEDS1                          0x44
#define GPREN0                          0x4c
#define GPREN1                          0x50
#define GPFEN0                          0x58
#define GPFEN1                          0x5c
#define GPHEN0                          0x64
#define GPHEN1                          0x68
#define GPLEN0                          0x70
#define GPLEN1                          0x74
#define GPAREN0                         0x7c
#define GPAREN1                         0x80
#define GPAFEN0                         0x88
#define GPAFEN1                         0x8c
#define GPPUD                           0x94
#define GPPUDCLK0                       0x98
#define GPPUDCLK1                       0x9c

static void bcm2835_gpio_update(BCM2835GPIOState *s)
{
    /* XXX Update IRQ pins, GPIO input */
}

static void bcm2835_gpio_set_irq(void *opaque, int irq, int level)
{
    BCM2835GPIOState *s = opaque;

    s->gpio_in[irq] = level;
    bcm2835_gpio_update(s);
}

static uint64_t bcm2835_gpio_read(void *opaque, hwaddr offset, unsigned size)
{
    BCM2835GPIOState *s = opaque;
    uint32_t res = 0;
    uint32_t *reg;

    reg = &s->reg[offset / 4];

    switch (offset) {
    case GPFSEL0:
    case GPFSEL1:
    case GPFSEL2:
    case GPFSEL3:
    case GPFSEL4:
    case GPFSEL5:
    case GPSET0:
    case GPSET1:
    case GPCLR0:
    case GPCLR1:
    case GPLEV0:
    case GPLEV1:
    case GPEDS0:
    case GPEDS1:
    case GPREN0:
    case GPREN1:
    case GPFEN0:
    case GPFEN1:
    case GPHEN0:
    case GPHEN1:
    case GPLEN0:
    case GPLEN1:
    case GPAREN0:
    case GPAREN1:
    case GPAFEN0:
    case GPAFEN1:
    case GPPUD:
    case GPPUDCLK0:
    case GPPUDCLK1:
        res = *reg;
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad offset %"HWADDR_PRIx"\n",
                      __func__, offset);
        return 0;
    }

    return res;
}

static void bcm2835_gpio_write(void *opaque, hwaddr offset,
                               uint64_t value, unsigned size)
{
    BCM2835GPIOState *s = opaque;
    uint32_t *reg __attribute__ ((unused));

    reg = &s->reg[offset / 4];

    switch (offset) {
    case GPFSEL0:
    case GPFSEL1:
    case GPFSEL2:
    case GPFSEL3:
    case GPFSEL4:
    case GPFSEL5:
    case GPSET0:
    case GPSET1:
    case GPCLR0:
    case GPCLR1:
    case GPLEV0:
    case GPLEV1:
    case GPEDS0:
    case GPEDS1:
    case GPREN0:
    case GPREN1:
    case GPFEN0:
    case GPFEN1:
    case GPHEN0:
    case GPHEN1:
    case GPLEN0:
    case GPLEN1:
    case GPAREN0:
    case GPAREN1:
    case GPAFEN0:
    case GPAFEN1:
    case GPPUD:
    case GPPUDCLK0:
    case GPPUDCLK1:
        /* XXX Implement register functionality */
        qemu_log_mask(LOG_GUEST_ERROR, "%s: Unimplemented write at %"HWADDR_PRIx"\n",
                      __func__, offset);
        break;

    default:
        qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad offset %"HWADDR_PRIx"\n",
                      __func__, offset);
        return;
    }

    bcm2835_gpio_update(s);
}

static const MemoryRegionOps bcm2835_gpio_ops = {
    .read = bcm2835_gpio_read,
    .write = bcm2835_gpio_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
};

/* vmstate of the entire device */
static const VMStateDescription vmstate_bcm2835_gpio = {
    .name = TYPE_BCM2835_GPIO,
    .version_id = 1,
    .minimum_version_id = 1,
    .minimum_version_id_old = 1,
    .fields      = (VMStateField[]) {
        VMSTATE_UINT32_ARRAY(reg, BCM2835GPIOState, BCM2835_GPIO_NR_REGS),
        VMSTATE_BOOL_ARRAY(gpio_in, BCM2835GPIOState, BCM2835_GPIO_NR_GPIOS),
        VMSTATE_END_OF_LIST()
    }
};

static void bcm2835_gpio_init(Object *obj)
{
    BCM2835GPIOState *s = BCM2835_GPIO(obj);

    memory_region_init_io(&s->iomem, obj, &bcm2835_gpio_ops, s,
                          TYPE_BCM2835_GPIO, BCM2835_GPIO_NR_REGS * 4);
    sysbus_init_mmio(SYS_BUS_DEVICE(s), &s->iomem);
    sysbus_init_irq(SYS_BUS_DEVICE(s), &s->arm_irq);
    qdev_init_gpio_in(DEVICE(s), bcm2835_gpio_set_irq, BCM2835_GPIO_NR_GPIOS);
}

static void bcm2835_gpio_gpfsel(BCM2835GPIOState *s, int irq, int mode)
{
    uint32_t *gpfsel;
    int shift = (irq % 10) * 3;

    gpfsel = &s->reg[irq / 10];
    *gpfsel &= ~(BCM2835_GPIO_FSEL_MASK << shift);
    *gpfsel |= mode << shift;
}

static void bcm2835_gpio_reset(DeviceState *dev)
{
    BCM2835GPIOState *s = BCM2835_GPIO(dev);
    int n;

    for (n = 0; n < BCM2835_GPIO_NR_REGS; n++) {
        s->reg[n] = 0;
    }

    /* Copy gpio mode configuration over */
    for (n = 0; n < BCM2835_GPIO_NR_GPIOS; n++) {
        bcm2835_gpio_gpfsel(s, n, s->gpio_mode[n]);
    }

    for (n = 0; n < BCM2835_GPIO_NR_GPIOS; n++) {
        s->gpio_in[n] = false;
    }
}

static void bcm2835_gpio_realize(DeviceState *dev, Error **errp)
{
    bcm2835_gpio_reset(dev);
}

static Property bcm2835_gpio_properties[] = {
    DEFINE_PROP_UINT8("gpio0-mode", BCM2835GPIOState, gpio_mode[0], 0),
    DEFINE_PROP_UINT8("gpio1-mode", BCM2835GPIOState, gpio_mode[1], 0),
    DEFINE_PROP_UINT8("gpio2-mode", BCM2835GPIOState, gpio_mode[2], 0),
    DEFINE_PROP_UINT8("gpio3-mode", BCM2835GPIOState, gpio_mode[3], 0),
    DEFINE_PROP_UINT8("gpio4-mode", BCM2835GPIOState, gpio_mode[4], 0),
    DEFINE_PROP_UINT8("gpio5-mode", BCM2835GPIOState, gpio_mode[5], 0),
    DEFINE_PROP_UINT8("gpio6-mode", BCM2835GPIOState, gpio_mode[6], 0),
    DEFINE_PROP_UINT8("gpio7-mode", BCM2835GPIOState, gpio_mode[7], 0),
    DEFINE_PROP_UINT8("gpio8-mode", BCM2835GPIOState, gpio_mode[8], 0),
    DEFINE_PROP_UINT8("gpio9-mode", BCM2835GPIOState, gpio_mode[9], 0),
    DEFINE_PROP_UINT8("gpio10-mode", BCM2835GPIOState, gpio_mode[10], 0),
    DEFINE_PROP_UINT8("gpio11-mode", BCM2835GPIOState, gpio_mode[11], 0),
    DEFINE_PROP_UINT8("gpio12-mode", BCM2835GPIOState, gpio_mode[12], 0),
    DEFINE_PROP_UINT8("gpio13-mode", BCM2835GPIOState, gpio_mode[13], 0),
    DEFINE_PROP_UINT8("gpio14-mode", BCM2835GPIOState, gpio_mode[14], 0),
    DEFINE_PROP_UINT8("gpio15-mode", BCM2835GPIOState, gpio_mode[15], 0),
    DEFINE_PROP_UINT8("gpio16-mode", BCM2835GPIOState, gpio_mode[16], 0),
    DEFINE_PROP_UINT8("gpio17-mode", BCM2835GPIOState, gpio_mode[17], 0),
    DEFINE_PROP_UINT8("gpio18-mode", BCM2835GPIOState, gpio_mode[18], 0),
    DEFINE_PROP_UINT8("gpio19-mode", BCM2835GPIOState, gpio_mode[19], 0),
    DEFINE_PROP_UINT8("gpio20-mode", BCM2835GPIOState, gpio_mode[20], 0),
    DEFINE_PROP_UINT8("gpio21-mode", BCM2835GPIOState, gpio_mode[21], 0),
    DEFINE_PROP_UINT8("gpio22-mode", BCM2835GPIOState, gpio_mode[22], 0),
    DEFINE_PROP_UINT8("gpio23-mode", BCM2835GPIOState, gpio_mode[23], 0),
    DEFINE_PROP_UINT8("gpio24-mode", BCM2835GPIOState, gpio_mode[24], 0),
    DEFINE_PROP_UINT8("gpio25-mode", BCM2835GPIOState, gpio_mode[25], 0),
    DEFINE_PROP_UINT8("gpio26-mode", BCM2835GPIOState, gpio_mode[26], 0),
    DEFINE_PROP_UINT8("gpio27-mode", BCM2835GPIOState, gpio_mode[27], 0),
    DEFINE_PROP_UINT8("gpio28-mode", BCM2835GPIOState, gpio_mode[28], 0),
    DEFINE_PROP_UINT8("gpio29-mode", BCM2835GPIOState, gpio_mode[29], 0),
    DEFINE_PROP_UINT8("gpio30-mode", BCM2835GPIOState, gpio_mode[30], 0),
    DEFINE_PROP_UINT8("gpio31-mode", BCM2835GPIOState, gpio_mode[31], 0),
    DEFINE_PROP_UINT8("gpio32-mode", BCM2835GPIOState, gpio_mode[32], 0),
    DEFINE_PROP_UINT8("gpio33-mode", BCM2835GPIOState, gpio_mode[33], 0),
    DEFINE_PROP_UINT8("gpio34-mode", BCM2835GPIOState, gpio_mode[34], 0),
    DEFINE_PROP_UINT8("gpio35-mode", BCM2835GPIOState, gpio_mode[35], 0),
    DEFINE_PROP_UINT8("gpio36-mode", BCM2835GPIOState, gpio_mode[36], 0),
    DEFINE_PROP_UINT8("gpio37-mode", BCM2835GPIOState, gpio_mode[37], 0),
    DEFINE_PROP_UINT8("gpio38-mode", BCM2835GPIOState, gpio_mode[38], 0),
    DEFINE_PROP_UINT8("gpio39-mode", BCM2835GPIOState, gpio_mode[39], 0),
    DEFINE_PROP_UINT8("gpio40-mode", BCM2835GPIOState, gpio_mode[40], 0),
    DEFINE_PROP_UINT8("gpio41-mode", BCM2835GPIOState, gpio_mode[41], 0),
    DEFINE_PROP_UINT8("gpio42-mode", BCM2835GPIOState, gpio_mode[42], 0),
    DEFINE_PROP_UINT8("gpio43-mode", BCM2835GPIOState, gpio_mode[43], 0),
    DEFINE_PROP_UINT8("gpio44-mode", BCM2835GPIOState, gpio_mode[44], 0),
    DEFINE_PROP_UINT8("gpio45-mode", BCM2835GPIOState, gpio_mode[45], 0),
    DEFINE_PROP_UINT8("gpio46-mode", BCM2835GPIOState, gpio_mode[46], 0),
    DEFINE_PROP_UINT8("gpio47-mode", BCM2835GPIOState, gpio_mode[47], 0),
    DEFINE_PROP_UINT8("gpio48-mode", BCM2835GPIOState, gpio_mode[48], 0),
    DEFINE_PROP_UINT8("gpio49-mode", BCM2835GPIOState, gpio_mode[49], 0),
    DEFINE_PROP_UINT8("gpio50-mode", BCM2835GPIOState, gpio_mode[50], 0),
    DEFINE_PROP_UINT8("gpio51-mode", BCM2835GPIOState, gpio_mode[51], 0),
    DEFINE_PROP_UINT8("gpio52-mode", BCM2835GPIOState, gpio_mode[52], 0),
    DEFINE_PROP_UINT8("gpio53-mode", BCM2835GPIOState, gpio_mode[53], 0),
    DEFINE_PROP_END_OF_LIST(),
};

static void bcm2835_gpio_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = bcm2835_gpio_realize;
    dc->reset = bcm2835_gpio_reset;
    dc->props = bcm2835_gpio_properties;
    dc->vmsd = &vmstate_bcm2835_gpio;
}

static TypeInfo bcm2835_gpio_info = {
    .name          = TYPE_BCM2835_GPIO,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(BCM2835GPIOState),
    .class_init    = bcm2835_gpio_class_init,
    .instance_init = bcm2835_gpio_init,
};

static void bcm2835_gpio_register_types(void)
{
    type_register_static(&bcm2835_gpio_info);
}

type_init(bcm2835_gpio_register_types)
