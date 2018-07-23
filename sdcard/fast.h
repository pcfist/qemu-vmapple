/*
 * fast.h
 * Simple IPC protocol
 *
 * Copyright (C) 2017 Alexander Graf <agraf@suse.de>
 * SPDX-License-Identifier: GPL-2.0
 */

#ifndef SDCARD_FAST_H
#define SDCARD_FAST_H

#include "qemu/osdep.h"
#include "qemu-common.h"
#include "qemu/timer.h"

enum sdcard_msg_cmd {
    /* FAST commands */

    SDCARD_MSG_QUERY_SD_INIT         = 'I',
    SDCARD_MSG_SET_SIZE              = 'S',
    SDCARD_MSG_PREAD_4BIT            = 'R',
    SDCARD_MSG_PWRITE_4BIT           = 'W',
    SDCARD_MSG_DONE                  = '\0',
    SDCARD_MSG_DBG_INT               = 'Y',
    SDCARD_MSG_DBG                   = ' ',
    SDCARD_MSG_GET_SIZE,
    SDCARD_MSG_READ_SECTOR,
    SDCARD_MSG_WRITE_SECTOR,
    SDCARD_MSG_READ_SD_STATUS,
    SDCARD_MSG_READ_NUM_WR_BLOCKS,
    SDCARD_MSG_READ_SCR,
    SDCARD_MSG_READ_SWITCH,
};

struct sdcard_msg_read_sector {
    uint64_t sector;
    uint32_t ptr;
};

struct fast_queue_elem {
    uint8_t cmd;
    uint8_t __pad[3];
    void *ptr;
    uint64_t extra;
    uint64_t time;
};

extern volatile struct fast_queue_elem fast_queue[512];
extern volatile struct fast_queue_elem *fast_head, *fast_tail;

/********* Consumer API *********/
/* Peek the next element in the ring */
volatile struct fast_queue_elem *fast_next(void);
/* Call this when an element was processed by consumer */
int fast_done(volatile struct fast_queue_elem *el);

/********* Producer API **********/
int fast_init(void);

static inline void fast_send(enum sdcard_msg_cmd cmd, const void *ptr,
                             uint64_t extra)
{
    struct fast_queue_elem *el = (struct fast_queue_elem *)fast_head;

    el->cmd = cmd;
    el->ptr = (void *)ptr;
    el->extra = extra;
    el->time = cpu_get_host_ticks();
    asm volatile("" : : : "memory");

    if (el == &fast_queue[511]) {
        fast_head = fast_queue;
    } else {
        fast_head = el + 1;
    }
}

static inline void fast_dbg(const char *msg)
{
    fast_send(SDCARD_MSG_DBG, msg, 0);
}

static inline void fast_dbg_int(const char *msg, uint64_t num)
{
    fast_send(SDCARD_MSG_DBG_INT, msg, num);
}

#endif
