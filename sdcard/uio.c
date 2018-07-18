/*
 * uio.c
 * UIO helper functions.
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
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include "uio.h"

static int uio_fd;

int uio_init(void)
{
    uio_fd = open("/dev/uio0", O_RDWR);
    if (uio_fd < 0) {
        return uio_fd;
    }

    return 0;
}

static long uio_get_mem_val(int idx, const char *type)
{
    int ret;
    long val;
    char *filename;
    FILE* file;
    int r;

    r = asprintf(&filename, "/sys/class/uio/uio0/maps/map%d/%s", idx, type);
    if (r < 0) {
        return -1;
    }
    file = fopen(filename,"r");
    if (!file) {
        return -1;
    }

    ret = fscanf(file,"0x%lx", &val);
    fclose(file);

    if (ret < 0) {
        return -2;
    }

    return val;
}

void *uio_map(int idx)
{
    long size = uio_get_mem_val(idx, "size");
    void* map_addr;

    map_addr = mmap(NULL, size, PROT_WRITE, MAP_SHARED, uio_fd,
                    idx * getpagesize());

    return map_addr;
}
