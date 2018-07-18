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
};

/********* Consumer API *********/
/* Peek the next element in the ring */
volatile struct fast_queue_elem *fast_next(void);
/* Call this when an element was processed by consumer */
int fast_done(volatile struct fast_queue_elem *el);

/********* Producer API **********/
int fast_init(void);

#endif
