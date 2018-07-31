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
__sram_data static void *sdctl_map;
__sram_data static bool sdcard_in_newcmd;
__sram_data SDBus sdbus;
__sram_data SDState sddev;
__sram_data static char proxy_stack[10240];

__sram_data uint64_t last_time = 0;
__sram_data double time_per_s = 0;

static void sdcard_map_sram(void)
{
    int fd = open ("/sys/devices/soc0/amba/fffc0000.ocm/fffc0000.sram", O_RDWR);
    int max_size = 0x40000 - 0x1000;
    void *sram;
    uintptr_t sram_size = (uintptr_t)__sram_stop - (uintptr_t)__sram_start;

    g_assert(sram_size <= max_size);

    /* Map real SRAM */
    sram = mmap(NULL, sram_size, PROT_READ | PROT_WRITE | PROT_EXEC,
                MAP_PRIVATE | MAP_LOCKED | MAP_POPULATE, fd, 0);
    g_assert(sram != MAP_FAILED);

    /* Copy SRAM section into SRAM */
    memcpy(sram, __sram_start, sram_size);

    /* Overlay old SRAM section with SRAM backed memory */
    g_assert(!munmap(__sram_start, sram_size));
    g_assert(!munmap(sram, sram_size));
    sram = mmap(__sram_start, sram_size, PROT_READ | PROT_WRITE | PROT_EXEC,
                MAP_PRIVATE | MAP_LOCKED | MAP_POPULATE | MAP_FIXED, fd, 0);
    g_assert(sram == __sram_start);
}

static int sdcard_set_affinity(void)
{
    cpu_set_t cpuset;
    qemu_thread_get_self(&thread);

    CPU_ZERO(&cpuset);
    /* CPU 1 (2nd core) is our destination CPU */
    CPU_SET(1, &cpuset);

    return pthread_setaffinity_np(thread.thread, sizeof(cpu_set_t), &cpuset);
}

__sram static uint32_t sdctl_readl(int reg)
{
    return readl((uint32_t*)(sdctl_map + reg));
}

__attribute__((unused))
static uint32_t sdctl_writel(uint32_t val, int reg)
{
    return writel(val, (uint32_t*)(sdctl_map + reg));
}

__sram static void sdcard_newcmd(SDRequest *request)
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

/* XXX remove */
static void init_perfcounters (int32_t do_reset, int32_t enable_divider)
{
    struct timeval tv_before;
    struct timeval tv_after;
    uint64_t usec_before;
    uint64_t usec_after;

#ifdef __arm__
    // in general enable all counters (including cycle counter)
    int32_t value = 1;

    // peform reset:
    if (do_reset) {
            value |= 2;         // reset all counters to zero.
            value |= 4;         // reset cycle counter to zero.
    }

    if (enable_divider) {
        value |= 8;         // enable "by 64" divider for CCNT.
    }

    value |= 16;

    // program the performance-counter control-register:
    asm volatile ("MCR p15, 0, %0, c9, c12, 0\t\n" :: "r"(value));

    // enable all counters:
    asm volatile ("MCR p15, 0, %0, c9, c12, 1\t\n" :: "r"(0x8000000f));

    // clear overflows:
    asm volatile ("MCR p15, 0, %0, c9, c12, 3\t\n" :: "r"(0x8000000f));

#endif

    gettimeofday(&tv_before, NULL);
    last_time = cpu_get_host_ticks();
    usec_before = tv_before.tv_sec * 1000000ULL + tv_before.tv_usec;
    usec_after = usec_before + 100000;
    while (1) {
        gettimeofday(&tv_after, NULL);
        usec_after = tv_after.tv_sec * 1000000ULL + tv_after.tv_usec;
        if (usec_after >= (usec_before + 100000)) {
            break;
        }
    }
    time_per_s = (cpu_get_host_ticks() - last_time) * 10;
}


__sram static void *sdcard_proxy(void *opaque)
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

    /* Initialize PMC */
    init_perfcounters(1, 0);


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
    pthread_attr_t tattr;
    pthread_t tid;

    sdcard_map_sram();

#if 1
    pthread_attr_init(&tattr);
    pthread_attr_setstack(&tattr, proxy_stack, sizeof(proxy_stack));
    pthread_create(&tid, &tattr, sdcard_proxy, NULL);
#else
    qemu_thread_create(&thread, "sdcard proxy CMD handler", sdcard_proxy,
                              NULL, QEMU_THREAD_JOINABLE);
#endif

    qemu_thread_create(&thread, "sdcard proxy DAT handler", sdcard_loop_data,
                              NULL, QEMU_THREAD_JOINABLE);

    return 0;
}
