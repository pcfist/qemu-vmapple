/*
 * gpio.c
 * GPIO driver
 *
 * Copyright (C) 2017 Alexander Graf <agraf@suse.de>
 * SPDX-License-Identifier: GPL-2.0
 */
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include "uio.h"
#include "gpio.h"

static void *gpio_map;

void gpio_set_reset(int high)
{
    uint32_t *gpio_channel0 = gpio_map;

    writel(high ? 1 : 0, gpio_channel0);
}

void gpio_set_kick(int high)
{
    uint32_t *gpio_channel1 = gpio_map + 8;

    writel(high ? 1 : 0, gpio_channel1);
}

int gpio_init(void)
{
    if (!(gpio_map = uio_map(2))) {
        return 1;
    }

    return 0;
}
