/*
 * gpio.c
 * GPIO driver
 *
 * Copyright (C) 2017 Alexander Graf <agraf@suse.de>
 * SPDX-License-Identifier: GPL-2.0
 */

void gpio_set_reset(int high);
int gpio_init(void);
void gpio_set_kick(int high);
