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
static bool sdcard_in_newcmd;
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
        fast_dbg_int("CMD failed: ", cmd);
        fast_dbg_int("CMD rsplen: ", rsplen);
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
    if (len) {
        sdctl_writel(len << SDCARD_PTR_LEN_SHIFT, SDCARD_REG_PTR);
        sdctl_writel(SDCARD_DATCTRL_SEND | SDCARD_DATCTRL_AUTOCRC16,
                     SDCARD_REG_DATCTRL);

        printf("Sent %d bytes data\n", len);
    }
}

static void *sdcard_proxy(void *opaque)
{
    /* Make sure we're running on the realtime CPU */
    if (sdcard_set_affinity()) {
        printf("ERROR setting affinity\n");
        return NULL;
    }

    sdctl_map = uio_map(UIO_RANGE_CTL);
    if (!sdctl_map) {
        printf("ERROR opening SD control register block\n");
        return NULL;
    }

    /* Enable command reads */
    sdctl_writel(SDCARD_CTRL_EN, SDCARD_REG_CTRL);

    while (1) {
        uint32_t sts;
        uint32_t sts_mask = 0;

        sts = sdctl_readl(SDCARD_REG_STATUS);

        if (sts & SDCARD_STATUS_COMP) {
            int delay;

            fast_dbg_int("Command complete: ", sts);
            delay = (sts & SDCARD_STATUS_CMD_DELAY_MASK) >>
                    SDCARD_STATUS_CMD_DELAY_SHIFT;
            if (!(sts & SDCARD_STATUS_NEW) && (delay > 50)) {
                fast_dbg_int("Too big delay: ", delay);
            }

            sts_mask = SDCARD_STATUS_COMP;
        }

        if (sts & SDCARD_STATUS_NEW) {
            SDRequest req;

            req.cmd = (sts & SDCARD_STATUS_CMD_MASK)
                          >> SDCARD_STATUS_CMD_SHIFT;
            req.arg = sdctl_readl(SDCARD_REG_ARG);
            req.crc = (sts & SDCARD_STATUS_CRC7_MASK
                          >> SDCARD_STATUS_CRC7_SHIFT);

            fast_dbg_int("New Command: ", req.cmd);

            sdcard_in_newcmd = true;
            sdcard_newcmd(&req);
            sdcard_in_newcmd = false;

            fast_dbg_int("Command processed: ", req.cmd);

            /* ACK the status register */
            sts_mask |= SDCARD_STATUS_NEW;
        }

        if (sts & SDCARD_STATUS_TRANSIT) {
            //printf("Command in flight: %08x\n", sts);
        }

        if (sts_mask) {
            sdctl_writel(sts_mask, SDCARD_REG_STATUS);
        }
    }

    return NULL;
}

static void *sdcard_loop_data(void *opaque)
{
    while (1) {
        if (!sdcard_in_newcmd) {
            sdcard_send_data();
        }

        usleep(1000);
    }

    return NULL;
}

int proxy_init(void)
{
    qemu_thread_create(&thread, "sdcard proxy CMD handler", sdcard_proxy,
                              NULL, QEMU_THREAD_JOINABLE);

    qemu_thread_create(&thread, "sdcard proxy DAT handler", sdcard_loop_data,
                              NULL, QEMU_THREAD_JOINABLE);

    return 0;
}
