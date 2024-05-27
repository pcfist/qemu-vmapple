/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * QEMU bdrv based helpers for file loading
 */

#include "qemu/osdep.h"
#include "qemu/datadir.h"
#include "qemu/error-report.h"
#include "trace.h"
#include "hw/hw.h"
#include "sysemu/sysemu.h"
#include "hw/boards.h"
#include "qemu/cutils.h"
#include "qapi/qmp/qdict.h"
#include "block/block.h"
#include "qemu/option.h"

static void unref_initrd(gpointer key, gpointer opaque)
{
    BlockDriverState *bs = key;

    bdrv_unref(bs);
}

void call_with_kernel(MachineState *ms,
                      void (*fn)(MachineState *ms, BlockDriverState *kernel_bs,
                                  GList *initrds, BlockDriverState *dtb_bs,
                                  Error **errp),
                      Error **errp)
{
    BlockDriverState *kernel_bs;
    QDict *kernel_options = qdict_new();
    GList *initrds = NULL;
    int bdrv_flags = 0;
    BlockDriverState *dtb_bs = NULL;
    QDict *dtb_options = qdict_new();

    /* Get block state for the kernel */
    qdict_put_str(kernel_options, "driver", "raw");
    kernel_bs = bdrv_open(ms->kernel_filename, NULL, kernel_options, bdrv_flags,
                          errp);
    if (*errp) {
        return;
    }

    if (ms->dtb) {
        qdict_put_str(dtb_options, "driver", "raw");
        dtb_bs = bdrv_open(ms->dtb, NULL, dtb_options, bdrv_flags, errp);
        if (*errp) {
            goto out;
        }
    }

    /* Extract initrd files from comma separated argument list */
    if (ms->initrd_filename) {
        const char *r = ms->initrd_filename;

        while (*r) {
            BlockDriverState *bs;
            QDict *initrd_options;
            char *fname;

            r = get_opt_value(r, &fname);
            if (*r) {
                /* skip comma */
                r++;
            }

            /* Allocate new block backend for initrd file */
            initrd_options = qdict_new();
            qdict_put_str(initrd_options, "driver", "raw");
            bs = bdrv_open(fname, NULL, initrd_options, bdrv_flags, errp);
            if (*errp) {
                goto out;
            }

            /* And append it to our list for later use */
            initrds = g_list_append(initrds, bs);
        }
    }

    /* Call callback with all block devices open */
    fn(ms, kernel_bs, initrds, dtb_bs, errp);

out:
    /* Free all block device references again */
    bdrv_unref(kernel_bs);
    g_list_foreach(initrds, unref_initrd, NULL);
    if (dtb_bs) {
        bdrv_unref(dtb_bs);
    }
}
