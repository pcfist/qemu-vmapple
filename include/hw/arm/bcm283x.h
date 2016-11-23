/*
 * Raspberry Pi emulation (c) 2012 Gregory Estrade
 * Upstreaming code cleanup [including bcm2835_*] (c) 2013 Jan Petrous
 *
 * Rasperry Pi 2 emulation and refactoring Copyright (c) 2015, Microsoft
 * Written by Andrew Baumann
 *
 * This code is licensed under the GNU GPLv2 and later.
 */

#ifndef BCM283X_H
#define BCM283X_H

#include "hw/arm/arm.h"
#include "hw/arm/bcm2835_peripherals.h"
#include "hw/intc/bcm2836_control.h"

#define TYPE_BCM283X "bcm283x"
#define BCM283X(obj) OBJECT_CHECK(BCM283XState, (obj), TYPE_BCM283X)

#define BCM283X_CLASS(klass) \
        OBJECT_CLASS_CHECK(BCM283XClass, klass, TYPE_BCM283X)
#define BCM283X_GET_CLASS(obj) \
        OBJECT_GET_CLASS(BCM283XClass, obj, TYPE_BCM283X)

#define BCM283X_MAX_NCPUS 4

typedef struct BCM283XState {
    /*< private >*/
    DeviceState parent_obj;
    /*< public >*/

    uint32_t enabled_cpus;

    ARMCPU cpus[BCM283X_MAX_NCPUS];
    BCM2836ControlState control;
    BCM2835PeripheralState peripherals;
} BCM283XState;

typedef struct BCM283XClass
{
    DeviceClass parent;

    const char *cpu_type;
} BCM283XClass;

#endif /* BCM283X_H */
