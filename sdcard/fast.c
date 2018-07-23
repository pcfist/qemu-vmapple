/*
 * fast.h
 * Simple IPC protocol
 *
 * Copyright (C) 2017 Alexander Graf <agraf@suse.de>
 * SPDX-License-Identifier: GPL-2.0
 */

#include "fast.h"
#include <assert.h>
#include <stdint.h>
#include "uio.h"

volatile struct fast_queue_elem fast_queue[512];
volatile struct fast_queue_elem *fast_head = fast_queue;
volatile struct fast_queue_elem *fast_tail = fast_queue;

int fast_done(volatile struct fast_queue_elem *el)
{
    assert(fast_tail == el);
    dmb();
    if (el == &fast_queue[511]) {
        fast_tail = fast_head;
    } else {
        fast_tail = el + 1;
    }
    dmb();

    return 0;
}

volatile struct fast_queue_elem *fast_next(void)
{
    if (fast_head != fast_tail) {
        return fast_tail;
    }

    return NULL;
}

int fast_init(void)
{
    return 0;
}
