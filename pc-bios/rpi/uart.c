/*
 * Raspberry Pi QEMU firmware
 *
 * Copyright (c) 2017 Alexander Graf <agraf@suse.de>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or (at
 * your option) any later version. See the COPYING file in the top-level
 * directory.
 */

#include "rpi.h"

void print(const char *s)
{
    panic("No UART implemented yet\n");
}
