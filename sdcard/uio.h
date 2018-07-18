/*
 * uio.h
 * UIO helper functions.
 *
 * Copyright (C) 2017 Alexander Graf <agraf@suse.de>
 * SPDX-License-Identifier: GPL-2.0
 */

#include <stdint.h>

int uio_init(void);
void *uio_map(int idx);

#ifdef __arm__
#define isb()     asm volatile ("isb sy" : : : "memory")
#define dsb()     asm volatile ("dsb sy" : : : "memory")
#define dmb()     asm volatile ("dmb sy" : : : "memory")
#else
#define isb()     asm volatile ("" : : : "memory")
#define dsb()     asm volatile ("" : : : "memory")
#define dmb()     asm volatile ("" : : : "memory")
#endif

#define mb()            dsb()
#define __iormb()       dmb()
#define __iowmb()       dmb()

#define __arch_getl(a)                  (*(volatile unsigned int *)(a))
#define __arch_putl(v,a)                (*(volatile unsigned int *)(a) = (v))
#define readl(c)        ({ uint32_t __v = __arch_getl(c); __iormb(); __v; })
#define writel(v,c)     ({ uint32_t __v = v; __iowmb(); __arch_putl(__v,c); __v; })

