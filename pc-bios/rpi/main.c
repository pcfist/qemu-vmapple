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

char stack[PAGE_SIZE * 8] __attribute__((__aligned__(PAGE_SIZE)));

void __attribute__ ((noreturn)) panic(const char *string)
{
    while (1) { }
}

int main(void)
{
    panic("Failed to load OS from SD Card\n");
}
