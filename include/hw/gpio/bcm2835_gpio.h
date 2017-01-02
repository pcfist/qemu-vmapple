/*
 * Raspberry Pi GPIO (and pinmux) emulation
 *
 * This code is licensed under the GNU GPLv2 and later.
 */

#ifndef BCM2835_GPIO_H
#define BCM2835_GPIO_H

#include "hw/sysbus.h"
#include "exec/address-spaces.h"

#define TYPE_BCM2835_GPIO "bcm2835-gpio"
#define BCM2835_GPIO(obj) \
        OBJECT_CHECK(BCM2835GPIOState, (obj), TYPE_BCM2835_GPIO)

#define BCM2835_GPIO_FSEL_MASK          0x7
#define BCM2835_GPIO_INPUT              0x0
#define BCM2835_GPIO_OUTPUT             0x1
#define BCM2835_GPIO_ALT0               0x4
#define BCM2835_GPIO_ALT1               0x5
#define BCM2835_GPIO_ALT2               0x6
#define BCM2835_GPIO_ALT3               0x7
#define BCM2835_GPIO_ALT4               0x3
#define BCM2835_GPIO_ALT5               0x2

#define BCM2835_GPIO_NR_REGS 44
#define BCM2835_GPIO_NR_GPIOS 54

typedef struct {
    /*< private >*/
    SysBusDevice busdev;

    /*< public >*/
    MemoryRegion mr;
    AddressSpace as;
    MemoryRegion iomem;
    qemu_irq arm_irq;

    uint32_t reg[BCM2835_GPIO_NR_REGS];
    bool gpio_in[BCM2835_GPIO_NR_GPIOS];
    uint8_t gpio_mode[BCM2835_GPIO_NR_GPIOS];
} BCM2835GPIOState;

#endif
