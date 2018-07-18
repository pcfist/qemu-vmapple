/*
 *  SD Card fake CPU header
 *
 *  Copyright (c) 2018 Alexander Graf <agraf@suse.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SDCARD_CPU_H
#define SDCARD_CPU_H

#include "qemu-common.h"

#define TARGET_LONG_BITS 64

#define CPUArchState struct CPUSDCardState

#include "exec/cpu-defs.h"

#define NB_MMU_MODES 1

typedef struct CPUSDCardState {
    CPU_COMMON
} CPUSDCardState;

#define ENV_GET_CPU(e) NULL
#define ENV_OFFSET 0

#define TARGET_PAGE_BITS 12
#define TARGET_PHYS_ADDR_SPACE_BITS 32
#define TARGET_VIRT_ADDR_SPACE_BITS 32
#define MMU_USER_IDX    0  /* Current memory operation is in user mode */

#endif
