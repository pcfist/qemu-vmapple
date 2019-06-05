/*
 *  Apple SMC controller
 *
 *  Copyright (c) 2007 Alexander Graf
 *
 *  Authors: Alexander Graf <agraf@suse.de>
 *           Susanne Graf <suse@csgraf.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * *****************************************************************
 *
 * In all Intel-based Apple hardware there is an SMC chip to control the
 * backlight, fans and several other generic device parameters. It also
 * contains the magic keys used to dongle Mac OS X to the device.
 *
 * This driver was mostly created by looking at the Linux AppleSMC driver
 * implementation and does not support IRQ.
 *
 */

#include "qemu/osdep.h"
#include "hw/hw.h"
#include "hw/isa/isa.h"
#include "ui/console.h"
#include "qemu/timer.h"

/* #define DEBUG_SMC */

#define APPLESMC_DEFAULT_IOBASE        0x300

enum {
    APPLESMC_DATA_PORT               = 0x00,
    APPLESMC_CMD_PORT                = 0x04,
    APPLESMC_ERR_PORT                = 0x1e,
    APPLESMC_NUM_PORTS               = 0x20,
};

enum {
    APPLESMC_READ_CMD                = 0x10,
    APPLESMC_WRITE_CMD               = 0x11,
    APPLESMC_GET_KEY_BY_INDEX_CMD    = 0x12,
    APPLESMC_GET_KEY_TYPE_CMD        = 0x13,
};

enum {
    APPLESMC_ST_CMD_DONE             = 0x00,
    APPLESMC_ST_DATA_READY           = 0x01,
    APPLESMC_ST_BUSY                 = 0x02,
    APPLESMC_ST_ACK                  = 0x04,
    APPLESMC_ST_NEW_CMD              = 0x08,
};

enum {
    APPLESMC_ST_1E_CMD_INTRUPTED     = 0x80,
    APPLESMC_ST_1E_STILL_BAD_CMD     = 0x81,
    APPLESMC_ST_1E_BAD_CMD           = 0x82,
    APPLESMC_ST_1E_NOEXIST           = 0x84,
    APPLESMC_ST_1E_WRITEONLY         = 0x85,
    APPLESMC_ST_1E_READONLY          = 0x86,
    APPLESMC_ST_1E_BAD_INDEX         = 0xb8,
};

#ifdef DEBUG_SMC
#define smc_debug_enabled 1
#else
#define smc_debug_enabled 0
#endif

#define smc_debug(...) if (smc_debug_enabled) fprintf(stderr, "AppleSMC: " __VA_ARGS__)

static char default_osk[64] = "This is a dummy key. Enter the real key "
                              "using the -osk parameter";

struct AppleSMCData {
    /* This structure reflects the 6 byte CMD_TYPE reply */
    struct {
        uint8_t len;
        char type[4];
        uint8_t unknown;
    } __attribute__((packed));
    const char *key;
    const char *data;
    QLIST_ENTRY(AppleSMCData) node;
};

#define APPLE_SMC(obj) OBJECT_CHECK(AppleSMCState, (obj), TYPE_APPLE_SMC)

typedef struct AppleSMCState AppleSMCState;
struct AppleSMCState {
    ISADevice parent_obj;

    MemoryRegion io_data;
    MemoryRegion io_cmd;
    MemoryRegion io_err;
    qemu_irq irq;
    uint32_t iobase;
    uint8_t isairq;
    uint8_t cmd;
    uint8_t status;
    uint8_t status_1e;
    uint8_t last_ret;
    char key[4];
    uint8_t read_pos;
    uint8_t data_len;
    uint8_t data_pos;
    uint8_t data[255];
    char *osk;
    QLIST_HEAD(, AppleSMCData) data_def;
};

static const char *applesmc_cmdstr(AppleSMCState *s)
{
    const char *opstr = "???  ";

    switch (s->cmd) {
    case APPLESMC_READ_CMD:
        opstr = "READ ";
        break;
    case APPLESMC_WRITE_CMD:
        opstr = "WRITE";
        break;
    case APPLESMC_GET_KEY_BY_INDEX_CMD:
        opstr = "KEY  ";
        break;
    case APPLESMC_GET_KEY_TYPE_CMD:
        opstr = "TYPE ";
        break;
    }

    return opstr;
}

static void applesmc_io_cmd_write(void *opaque, hwaddr addr, uint64_t val,
                                  unsigned size)
{
    AppleSMCState *s = opaque;
    uint8_t status = s->status & 0x0f;

    smc_debug("CMD received: 0x%02x\n", (uint8_t)val);
    switch (val) {
    case APPLESMC_READ_CMD:
    case APPLESMC_GET_KEY_TYPE_CMD:
        /* did last command run through OK? */
        if (status == APPLESMC_ST_CMD_DONE || status == APPLESMC_ST_NEW_CMD) {
            s->cmd = val;
            s->status = APPLESMC_ST_NEW_CMD | APPLESMC_ST_ACK;
        } else {
            smc_debug("ERROR: previous command interrupted!\n");
            s->status = APPLESMC_ST_NEW_CMD;
            s->status_1e = APPLESMC_ST_1E_CMD_INTRUPTED;
        }
        break;
    default:
        smc_debug("UNEXPECTED CMD 0x%02x\n", (uint8_t)val);
        s->status = APPLESMC_ST_NEW_CMD;
        s->status_1e = APPLESMC_ST_1E_BAD_CMD;
    }
    s->read_pos = 0;
    s->data_pos = 0;
}

static struct AppleSMCData *applesmc_find_key(AppleSMCState *s)
{
    struct AppleSMCData *d;

    QLIST_FOREACH(d, &s->data_def, node) {
        if (!memcmp(d->key, s->key, 4)) {
            return d;
        }
    }
    return NULL;
}

static void applesmc_io_data_write(void *opaque, hwaddr addr, uint64_t val,
                                   unsigned size)
{
    AppleSMCState *s = opaque;
    struct AppleSMCData *d;
    int lastpos;

    smc_debug("DATA received: 0x%02x\n", (uint8_t)val);
    switch (s->cmd) {
    case APPLESMC_GET_KEY_TYPE_CMD:
    case APPLESMC_READ_CMD:
        if ((s->status & 0x0f) == APPLESMC_ST_CMD_DONE) {
            break;
        }

        /* READ_CMD has the buffer length as argument after the key */
        lastpos = (s->cmd == APPLESMC_READ_CMD) ? 4 : 3;

        if (s->read_pos < 4) {
            s->key[s->read_pos] = val;
            s->status = APPLESMC_ST_ACK;
        }

        if (s->read_pos == lastpos) {
            d = applesmc_find_key(s);
            if (d != NULL) {
                switch (s->cmd) {
                case APPLESMC_READ_CMD:
                    memcpy(s->data, d->data, d->len);
                    s->data_len = d->len;
                    break;
                case APPLESMC_GET_KEY_TYPE_CMD:
                    memcpy(s->data, &d->len, 6);
                    s->data_len = 6;
                    break;
                }
                s->data_pos = 0;
                s->status = APPLESMC_ST_ACK | APPLESMC_ST_DATA_READY;
                s->status_1e = APPLESMC_ST_CMD_DONE;  /* clear on valid key */
            } else {
                smc_debug("READ_CMD: key '%c%c%c%c' not found!\n",
                          s->key[0], s->key[1], s->key[2], s->key[3]);
                s->status = APPLESMC_ST_CMD_DONE;
                s->status_1e = APPLESMC_ST_1E_NOEXIST;
            }
        }

        s->read_pos++;
        break;
    default:
        s->status = APPLESMC_ST_CMD_DONE;
        s->status_1e = APPLESMC_ST_1E_STILL_BAD_CMD;
    }
}

static void applesmc_io_err_write(void *opaque, hwaddr addr, uint64_t val,
                                  unsigned size)
{
    smc_debug("ERR_CODE received: 0x%02x, ignoring!\n", (uint8_t)val);
    /* NOTE: writing to the error port not supported! */
}

static uint64_t applesmc_io_data_read(void *opaque, hwaddr addr, unsigned size)
{
    AppleSMCState *s = opaque;
    const char *opstr = applesmc_cmdstr(s);

    switch (s->cmd) {
    case APPLESMC_READ_CMD:
    case APPLESMC_GET_KEY_TYPE_CMD:
        if (!(s->status & APPLESMC_ST_DATA_READY)) {
            break;
        }
        if (s->data_pos < s->data_len) {
            s->last_ret = s->data[s->data_pos];
            smc_debug("%s '%c%c%c%c'[%d] = %02x\n", opstr,
                      s->key[0], s->key[1], s->key[2], s->key[3],
                      s->data_pos, s->last_ret);
            s->data_pos++;
            if (s->data_pos == s->data_len) {
                s->status = APPLESMC_ST_CMD_DONE;
                smc_debug("%s '%c%c%c%c' Len=%d complete!\n", opstr,
                          s->key[0], s->key[1], s->key[2], s->key[3],
                          s->data_len);
            } else {
                s->status = APPLESMC_ST_ACK | APPLESMC_ST_DATA_READY;
            }
        }
        break;
    default:
        s->status = APPLESMC_ST_CMD_DONE;
        s->status_1e = APPLESMC_ST_1E_STILL_BAD_CMD;
    }
    smc_debug("DATA sent: 0x%02x\n", s->last_ret);

    return s->last_ret;
}

static uint64_t applesmc_io_cmd_read(void *opaque, hwaddr addr, unsigned size)
{
    AppleSMCState *s = opaque;

    smc_debug("CMD sent: 0x%02x\n", s->status);
    return s->status;
}

static uint64_t applesmc_io_err_read(void *opaque, hwaddr addr, unsigned size)
{
    AppleSMCState *s = opaque;

    /* NOTE: read does not clear the 1e status */
    smc_debug("ERR_CODE sent: 0x%02x\n", s->status_1e);
    return s->status_1e;
}

static void applesmc_add_key(AppleSMCState *s, const char *key,
                             int len, const char *type, const char *data)
{
    struct AppleSMCData *def;

    def = g_malloc0(sizeof(struct AppleSMCData));
    def->key = key;
    def->len = len;
    def->data = data;
    memcpy(def->type, type, sizeof(def->type));

    QLIST_INSERT_HEAD(&s->data_def, def, node);
}

static void qdev_applesmc_isa_reset(DeviceState *dev)
{
    AppleSMCState *s = APPLE_SMC(dev);
    struct AppleSMCData *d, *next;

    /* Remove existing entries */
    QLIST_FOREACH_SAFE(d, &s->data_def, node, next) {
        QLIST_REMOVE(d, node);
    }
    s->status = 0x00;
    s->status_1e = 0x00;
    s->last_ret = 0x00;

    applesmc_add_key(s, "REV ", 6, "{rev", "\x01\x13\x0f\x00\x00\x03");
    applesmc_add_key(s, "OSK0", 32, "ui8*", s->osk);
    applesmc_add_key(s, "OSK1", 32, "ui8*", s->osk + 32);
    applesmc_add_key(s, "NATJ", 1, "ui8 ", "\0");
    applesmc_add_key(s, "MSSP", 1, "ui8 ", "\0");
    applesmc_add_key(s, "MSSD", 1, "ui8 ", "\0x3");
}

static const MemoryRegionOps applesmc_data_io_ops = {
    .write = applesmc_io_data_write,
    .read = applesmc_io_data_read,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 1,
        .max_access_size = 1,
    },
};

static const MemoryRegionOps applesmc_cmd_io_ops = {
    .write = applesmc_io_cmd_write,
    .read = applesmc_io_cmd_read,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 1,
        .max_access_size = 1,
    },
};

static const MemoryRegionOps applesmc_err_io_ops = {
    .write = applesmc_io_err_write,
    .read = applesmc_io_err_read,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 1,
        .max_access_size = 1,
    },
};

static void applesmc_isa_realize(DeviceState *dev, Error **errp)
{
    AppleSMCState *s = APPLE_SMC(dev);

    memory_region_init_io(&s->io_data, OBJECT(s), &applesmc_data_io_ops, s,
                          "applesmc-data", 1);
    isa_register_ioport(&s->parent_obj, &s->io_data,
                        s->iobase + APPLESMC_DATA_PORT);

    memory_region_init_io(&s->io_cmd, OBJECT(s), &applesmc_cmd_io_ops, s,
                          "applesmc-cmd", 1);
    isa_register_ioport(&s->parent_obj, &s->io_cmd,
                        s->iobase + APPLESMC_CMD_PORT);

    memory_region_init_io(&s->io_err, OBJECT(s), &applesmc_err_io_ops, s,
                          "applesmc-err", 1);
    isa_register_ioport(&s->parent_obj, &s->io_err,
                        s->iobase + APPLESMC_ERR_PORT);

    if (!s->osk || (strlen(s->osk) != 64)) {
        warn_report("Using AppleSMC with invalid key");
        s->osk = default_osk;
    }

    isa_init_irq(&s->parent_obj, &s->irq, s->isairq);

    QLIST_INIT(&s->data_def);
    qdev_applesmc_isa_reset(dev);
}

static Property applesmc_isa_properties[] = {
    DEFINE_PROP_UINT32(APPLESMC_PROP_IO_BASE, AppleSMCState, iobase,
                       APPLESMC_DEFAULT_IOBASE),
    DEFINE_PROP_STRING("osk", AppleSMCState, osk),
    DEFINE_PROP_UINT8("irq", AppleSMCState, isairq, 6),
    DEFINE_PROP_END_OF_LIST(),
};

static void qdev_applesmc_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = applesmc_isa_realize;
    dc->reset = qdev_applesmc_isa_reset;
    dc->props = applesmc_isa_properties;
    set_bit(DEVICE_CATEGORY_MISC, dc->categories);
}

static const TypeInfo applesmc_isa_info = {
    .name          = TYPE_APPLE_SMC,
    .parent        = TYPE_ISA_DEVICE,
    .instance_size = sizeof(AppleSMCState),
    .class_init    = qdev_applesmc_class_init,
};

static void applesmc_register_types(void)
{
    type_register_static(&applesmc_isa_info);
}

type_init(applesmc_register_types)
