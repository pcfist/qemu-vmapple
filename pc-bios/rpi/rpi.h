/*
 * Raspberry Pi QEMU Firmware
 *
 * Copyright (c) 2017 Alexander Graf <agraf@suse.de>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or (at
 * your option) any later version. See the COPYING file in the top-level
 * directory.
 */

#ifndef RPI_H
#define RPI_H

/* #define DEBUG */

typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;
typedef unsigned long      ulong;
typedef long               size_t;
typedef int                bool;
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;
typedef unsigned char      __u8;
typedef unsigned short     __u16;
typedef unsigned int       __u32;
typedef unsigned long long __u64;

#define true 1
#define false 0
#define PAGE_SIZE 4096

#ifndef EIO
#define EIO     1
#endif
#ifndef EBUSY
#define EBUSY   2
#endif
#ifndef NULL
#define NULL    0
#endif

/* main.c */
void panic(const char *string);
extern char stack[PAGE_SIZE * 8] __attribute__((__aligned__(PAGE_SIZE)));

/* uart.c */
void print(const char *s);

/* helpers */
static inline void *memset(void *s, int c, size_t n)
{
    int i;
    unsigned char *p = s;

    for (i = 0; i < n; i++) {
        p[i] = c;
    }

    return s;
}

static inline void fill_hex(char *out, unsigned char val)
{
    const char hex[] = "0123456789abcdef";

    out[0] = hex[(val >> 4) & 0xf];
    out[1] = hex[val & 0xf];
}

static inline void fill_hex_val(char *out, void *ptr, unsigned size)
{
    unsigned char *value = ptr;
    unsigned int i;

    for (i = 0; i < size; i++) {
        fill_hex(&out[i*2], value[i]);
    }
}

static inline void print_int(const char *desc, u64 addr)
{
    char out[] = ": 0xffffffffffffffff\n";

    fill_hex_val(&out[4], &addr, sizeof(addr));

    print(desc);
    print(out);
}

static inline void debug_print_int(const char *desc, u64 addr)
{
#ifdef DEBUG
    print_int(desc, addr);
#endif
}

static inline void debug_print_addr(const char *desc, void *p)
{
#ifdef DEBUG
    debug_print_int(desc, (unsigned int)(unsigned long)p);
#endif
}

static inline void *memcpy(void *s1, const void *s2, size_t n)
{
    uint8_t *p1 = s1;
    const uint8_t *p2 = s2;

    while (n--) {
        p1[n] = p2[n];
    }
    return s1;
}

#endif /* RPI_H */
