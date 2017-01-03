/*
 * Raspberry Pi QEMU firmware
 *
 * Copyright (c) 2017 Alexander Graf <agraf@suse.de>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or (at
 * your option) any later version. See the COPYING file in the top-level
 * directory.
 */

#include "rpi.h"

struct pl01x_regs {
    u32     dr;             /* 0x00 Data register */
    u32     ecr;            /* 0x04 Error clear register (Write) */
    u32     pl010_lcrh;     /* 0x08 Line control register, high byte */
    u32     pl010_lcrm;     /* 0x0C Line control register, middle byte */
    u32     pl010_lcrl;     /* 0x10 Line control register, low byte */
    u32     pl010_cr;       /* 0x14 Control register */
    u32     fr;             /* 0x18 Flag register (Read only) */
    u32     pl011_rlcr;     /* 0x1c Receive line control register */
    u32     ilpr;           /* 0x20 IrDA low-power counter register */
    u32     pl011_ibrd;     /* 0x24 Integer baud rate register */
    u32     pl011_fbrd;     /* 0x28 Fractional baud rate register */
    u32     pl011_lcrh;     /* 0x2C Line control register */
    u32     pl011_cr;       /* 0x30 Control register */
};

#define UART_PL01x_FR_TXFE              0x80
#define UART_PL01x_FR_RXFF              0x40
#define UART_PL01x_FR_TXFF              0x20
#define UART_PL01x_FR_RXFE              0x10
#define UART_PL01x_FR_BUSY              0x08
#define UART_PL01x_FR_TMSK              (UART_PL01x_FR_TXFF + UART_PL01x_FR_BUSY)

volatile struct pl01x_regs *regs = (void*)0x3f201000;

void putc(const char c)
{
    while (regs->fr & UART_PL01x_FR_TXFF) {
        /* busy, need to wait */
    }

    regs->dr = c;
}

void print(const char *s)
{
    for (; *s; s++) {
        putc(*s);
    }
}
