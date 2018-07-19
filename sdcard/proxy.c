/*
 * proxy.c
 * SD card emulation proxy to FPGA hardware
 *
 * Copyright (C) 2018 Alexander Graf <agraf@suse.de>
 * SPDX-License-Identifier: GPL-2.0
 *
 * This file contains all pieces that need to run in the real time critical
 * domain. Any SD message needs to get a respective response within at most
 * 64 SD clock cycles, so we need to make sure that our SD command processing
 * loop never blocks.
 */
#include "qemu/osdep.h"
#include "qemu-common.h"
#include "qemu/thread.h"
#include <pthread.h>
#include "fast.h"
#include "uio.h"
#include "proxy.h"

static QemuThread thread;
static void *sdctl_map;

static int sdcard_set_affinity(void)
{
    cpu_set_t cpuset;
    qemu_thread_get_self(&thread);

    /* CPU 1 (2nd core) is our destination CPU */
    CPU_SET(1, &cpuset);

    return pthread_setaffinity_np(thread.thread, sizeof(cpu_set_t), &cpuset);
}

static uint32_t sdctl_readl(int reg)
{
    return readl((uint32_t*)(sdctl_map + reg));
}

__attribute__((unused))
static uint32_t sdctl_writel(uint32_t val, int reg)
{
    return writel(val, (uint32_t*)(sdctl_map + reg));
}

static void *sdcard_proxy(void *opaque)
{
    /* Make sure we're running on the realtime CPU */
    sdcard_set_affinity();

    sdctl_map = uio_map(1);
    if (!sdctl_map) {
        printf("ERROR opening SD control register block\n");
        return NULL;
    }

    while (1) {
        uint32_t sts;

        sts = sdctl_readl(SDCARD_REG_STATUS);

        if (sts & SDCARD_STATUS_NEW) {
            printf("New Command: %08x\n", sts);
            sdctl_writel(SDCARD_STATUS_NEW, SDCARD_REG_STATUS);
        }

        if (sts & SDCARD_STATUS_TRANSIT) {
            printf("Command in flight: %08x\n", sts);
        }

        if (sts & SDCARD_STATUS_COMP) {
            printf("Command complete: %08x\n", sts);
            sdctl_writel(SDCARD_STATUS_COMP, SDCARD_REG_STATUS);
        }
    }

    return NULL;
}

int proxy_init(void)
{
    qemu_thread_create(&thread, "sdcard proxy", sdcard_proxy,
                              NULL, QEMU_THREAD_JOINABLE);

    return 0;
}
