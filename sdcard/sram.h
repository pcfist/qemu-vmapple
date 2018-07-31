/*
 * sram.h
 * Helpers to push parts of our code into SRAM
 *
 * Copyright (C) 2018 Alexander Graf <agraf@suse.de>
 * SPDX-License-Identifier: GPL-2.0
 */

#ifndef SDCARD_SRAM_H
#define SDCARD_SRAM_H

#include "qemu-common.h"

#ifdef CONFIG_SDCARD

extern char __sram_start[];
extern char __sram_stop[];
#define __sram_data __attribute__ ((section (".data.sram")))
#define __sram __attribute__ ((section (".text.sram")))

#else

#define __sram_data
#define __sram

#endif /* CONFIG_SDCARD */

#endif
