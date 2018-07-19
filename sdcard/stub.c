/*
 * stub.c
 * QEMU function stubs for unused functionality.
 *
 * Copyright (C) 2017 Alexander Graf <agraf@suse.de>
 * SPDX-License-Identifier: GPL-2.0
 */

#include "qemu/osdep.h"
#include "qemu-common.h"
#include "migration/vmstate.h"
#include "sysemu/blockdev.h"
#include "net/net.h"

int vmstate_load_state(QEMUFile *f, const VMStateDescription *vmsd,
                       void *opaque, int version_id)
{
    return -1;
}

int vmstate_save_state(QEMUFile *f, const VMStateDescription *vmsd,
                       void *opaque, QJSON *vmdesc)
{
    return -1;
}

void blockdev_auto_del(BlockBackend *blk)
{
}

int qemu_find_net_clients_except(const char *id, NetClientState **ncs,
                                 NetClientDriver type, int max)
{
    return -1;
}

const VMStateInfo vmstate_info_timer = { };
