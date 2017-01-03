/*
 * First stage boot loader for Raspberry Pi.
 *
 * Copyright (c) 2017 Alexander Graf <agraf@suse.de>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or (at
 * your option) any later version. See the COPYING file in the top-level
 * directory.
 */

.section .text.startup

        .globl _start
_start:

adr	x1, stack + 0x8000      /* Set up stack */
mov	sp, x1
b	main                    /* And call C */
