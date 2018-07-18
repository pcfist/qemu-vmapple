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

volatile struct fast_queue_elem fast_queue[512];
volatile struct fast_queue_elem *fast_head, *fast_tail;

int fast_done(volatile struct fast_queue_elem *el)
{
    assert(fast_tail == el);
    if (fast_tail == &fast_queue[ARRAY_SIZE(fast_queue) - 1]) {
        fast_tail = fast_head;
    } else {
        fast_tail++;
    }

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
    fast_head = fast_queue;
    fast_tail = fast_queue;

    return 0;
}
