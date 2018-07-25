/*
 * uio.h
 * UIO helper functions.
 *
 * Copyright (C) 2017 Alexander Graf <agraf@suse.de>
 * SPDX-License-Identifier: GPL-2.0
 */

#include <stdint.h>

#ifdef __arm__
#define dmb_ishst()   asm volatile ("dmb ishst" : : : "memory")
#define dmb_ish()     asm volatile ("dmb ish" : : : "memory")
#else
#define dmb_ishst()   asm volatile ("" : : : "memory")
#define dmb_ish()     asm volatile ("" : : : "memory")
#endif

#define mb()     asm volatile ("" : : : "memory")

#define __iormb()       mb();
#define __iowmb()       mb();

#define __arch_getl(a)                  (*(volatile unsigned int *)(a))
#define __arch_putl(v,a)                (*(volatile unsigned int *)(a) = (v))
#define readl(c)        ({ uint32_t __v = __arch_getl(c); __iormb(); __v; })
#define writel(v,c)     ({ uint32_t __v = v; __iowmb(); __arch_putl(__v,c); __v; })

enum uio_range {
    UIO_RANGE_BRAM = 0,
    UIO_RANGE_CTL,
    UIO_RANGE_GPIO,
    UIO_RANGE_MAX,
};

int uio_init(void);
void *uio_map(enum uio_range idx);
