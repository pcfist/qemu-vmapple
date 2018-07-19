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

#include "fast.h"
#include "uio.h"
#include "proxy.h"

#include <pthread.h>

static QemuThread thread;
static void *sdctl_map;
SDBus sdbus;

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

static void sdcard_newcmd(SDRequest *request)
{
    int rsplen;
    uint8_t response[16];
    uint32_t cmd = request->cmd & ~SDCARD_CMD_HOST2CARD;

    rsplen = sdbus_do_command(&sdbus, request, response);
    switch (rsplen) {
    case 16:
        sdctl_writel(ldl_be_p(response), SDCARD_REG_ARG);
        sdctl_writel(ldl_be_p(response + 4), SDCARD_REG_ARG2);
        sdctl_writel(ldl_be_p(response + 8), SDCARD_REG_ARG3);
        sdctl_writel(ldl_be_p(response + 12), SDCARD_REG_ARG4);
        sdctl_writel(SDCARD_CTRL_EN | SDCARD_CTRL_SEND | SDCARD_CTRL_AUTOCRC7 |
                     SDCARD_CTRL_136BIT |
                     (cmd << SDCARD_CTRL_CMD_SHIFT), SDCARD_REG_CTRL);
        break;
    case 4:
        sdctl_writel(ldl_be_p(response), SDCARD_REG_ARG);
        sdctl_writel(SDCARD_CTRL_EN | SDCARD_CTRL_SEND | SDCARD_CTRL_AUTOCRC7 |
                     (cmd << SDCARD_CTRL_CMD_SHIFT), SDCARD_REG_CTRL);
        break;
    default:
        printf("CMD%d failed (%d)\n", cmd, rsplen);
        break;
    }
}

static void sdcard_send_data(void)
{
    uint8_t data[4];
    int data_cnt = 0;
    uint32_t *sddat = sdcard_map;
    int len = 0;

    while (sdbus_data_ready(&sdbus)) {
        data[data_cnt++] = sdbus_read_data(&sdbus);
        if (data_cnt == 4) {
            writel(ldl_be_p(data), sddat++);
            data_cnt = 0;
            len += 4;
        }

        /* The biggest sector size we support is 512 bytes */
        if (len == 512) {
            break;
        }
    }

    /* Send data at addr 0 */
    sdctl_writel(len << SDCARD_PTR_LEN_SHIFT, SDCARD_REG_PTR);
    sdctl_writel(SDCARD_DATCTRL_SEND | SDCARD_DATCTRL_AUTOCRC16,
                 SDCARD_REG_DATCTRL);
}

static void *sdcard_proxy(void *opaque)
{
    /* Make sure we're running on the realtime CPU */
    sdcard_set_affinity();

    sdctl_map = uio_map(UIO_RANGE_CTL);
    if (!sdctl_map) {
        printf("ERROR opening SD control register block\n");
        return NULL;
    }

    /* Enable command reads */
    sdctl_writel(SDCARD_CTRL_EN, SDCARD_REG_CTRL);

    while (1) {
        uint32_t sts;

        sts = sdctl_readl(SDCARD_REG_STATUS);

        if (sts & SDCARD_STATUS_NEW) {
            SDRequest req;

            printf("New Command: %08x\n", sts);

            req.cmd = (sts & SDCARD_STATUS_CMD_MASK)
                          >> SDCARD_STATUS_CMD_SHIFT;
            req.arg = sdctl_readl(SDCARD_REG_ARG);
            req.crc = (sts & SDCARD_STATUS_CRC7_MASK
                          >> SDCARD_STATUS_CRC7_SHIFT);

            sdcard_newcmd(&req);

            /* ACK the status register */
            sdctl_writel(SDCARD_STATUS_NEW, SDCARD_REG_STATUS);
        }

        if (sts & SDCARD_STATUS_TRANSIT) {
            //printf("Command in flight: %08x\n", sts);
        }

        if (sts & SDCARD_STATUS_COMP) {
            printf("Command complete: %08x\n", sts);
            sdctl_writel(SDCARD_STATUS_COMP, SDCARD_REG_STATUS);
        }

        sdcard_send_data();
    }

    return NULL;
}

int proxy_init(void)
{
    qemu_thread_create(&thread, "sdcard proxy", sdcard_proxy,
                              NULL, QEMU_THREAD_JOINABLE);

    return 0;
}
