/*
 * Raspberry Pi emulation (c) 2012 Gregory Estrade
 * Upstreaming code cleanup [including bcm2835_*] (c) 2013 Jan Petrous
 *
 * Rasperry Pi 2 emulation and refactoring Copyright (c) 2015, Microsoft
 * Written by Andrew Baumann
 *
 * This code is licensed under the GNU GPLv2 and later.
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu-common.h"
#include "cpu.h"
#include "hw/arm/bcm2837.h"
#include "hw/arm/raspi_platform.h"
#include "hw/sysbus.h"
#include "exec/address-spaces.h"

static void bcm2837_class_init(ObjectClass *oc, void *data)
{
    BCM283XClass *bc = BCM283X_CLASS(oc);

    bc->cpu_type = "cortex-a53-" TYPE_ARM_CPU;
}

static const TypeInfo bcm2837_type_info = {
    .name = TYPE_BCM2837,
    .parent = TYPE_BCM283X,
    .class_init = bcm2837_class_init,
};

static void bcm2837_register_types(void)
{
    type_register_static(&bcm2837_type_info);
}

type_init(bcm2837_register_types)
