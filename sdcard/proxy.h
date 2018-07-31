/*
 * proxy.h
 * SD card emulation proxy to FPGA hardware
 *
 * Copyright (C) 2018 Alexander Graf <agraf@suse.de>
 * SPDX-License-Identifier: GPL-2.0
 */

#ifndef SDCARD_PROXY_H
#define SDCARD_PROXY_H

#include "qemu/osdep.h"
#include "qemu-common.h"
#include "hw/sd/sd.h"

int proxy_init(void);



/************************** Register interface *****************************/


/*
 * Register layout
 *
 * REG0 [w]        Control Register
 *    bit0         CMD read enabled
 *    bit1         CMD sending (flips to 0 when send is finished)
 *    bit3         0=48bit cmd len 1=136bit cmd len
 *    bit23..16    to be sent cmd
 *    bit24        autocrc7 - automatically calculates crc7 if set
 *    bit31..25    to be sent crc7
 *
 * REG1 [r,wc]     Status Register
 *    bit0         New CMD arrived      [WC]
 *    bit1         CMD sent             [WC]
 *    bit2         bus powered
 *    bit3         cmd wire status
 *    bit4         new data packet arrived
 *    bit5         data send complete
 *    bit6         data recv CRC16 error
 *    bit15..9     self calculated crc7 (only valid with STS_NEw=1)
 *    bit23..16    received cmd
 *    bit24        end bit (should be 1)
 *    bit31..25    received crc7
 *
 * REG2 [w]        Pointer to data region
 *    bit31..16    Length of buffer
 *    bit15..0     Offset of buffer (to S01 AXI device)
 *
 * REG3 [w]        DAT Control Register
 *    bit0         DAT read enabled
 *    bit1         DAT in send mode (flips to 0 when send is finished)
 *    bit2         0=1bit sd mode 1=4bit sd mode
 *    bit4         automatically append/check crc16 on send (reduces reg2.len by crc16)
 *    bit5         1=pull down DAT0 (indicate busy)
 *    bit8..10     CRC status indicator (get sent when busy turns on, if != 0)
 *
 * REG4 [rw]       recevied / sending CMD arg contents (without crc7, cmd id)
 * REG5..7         Additional CMD send bits (used with CTL.R2 set)
 *
 */

#define SDCARD_CMD_HOST2CARD		0x40

#define SDCARD_REG_CTRL			0x00
#define SDCARD_CTRL_EN			(1 << 0)
#define SDCARD_CTRL_SEND		(1 << 1)
#define SDCARD_CTRL_136BIT		(1 << 3)
#define SDCARD_CTRL_IRQEN_NEWCMD	(1 << 4)
#define SDCARD_CTRL_IRQEN_REST		(1 << 5)
#define SDCARD_CTRL_CMD_SHIFT           16
#define SDCARD_CTRL_CMD_MASK            (0xff << SDCARD_CTRL_CMD_SHIFT)
#define SDCARD_CTRL_AUTOCRC7		(1 << 24)
#define SDCARD_CTRL_CRC_SHIFT           25
#define SDCARD_CTRL_CRC_MASK            (0x7f << SDCARD_CTRL_CMD_SHIFT)

#define SDCARD_REG_STATUS		0x04
#define SDCARD_STATUS_NEW		(1 << 0)
#define SDCARD_STATUS_COMP		(1 << 1)	/* CMD send finished, WC */
#define SDCARD_STATUS_POW		(1 << 2)	/* bus powered */
#define SDCARD_STATUS_CMDWIRE		(1 << 3)	/* CMD wire status */
#define SDCARD_STATUS_NEWDATA		(1 << 4)
#define SDCARD_STATUS_DATACOMP		(1 << 5)
#define SDCARD_STATUS_DATACRCERROR	(1 << 6)
#define SDCARD_STATUS_BUSYSENT		(1 << 7)
#define SDCARD_STATUS_TRANSIT		(1 << 8)
#define SDCARD_STATUS_CMD_SHIFT		16
#define SDCARD_STATUS_CMD_MASK		(0xff << SDCARD_STATUS_CMD_SHIFT)
#define SDCARD_STATUS_CMD_DELAY_SHIFT	16
#define SDCARD_STATUS_CMD_DELAY_MASK	(0xff << SDCARD_STATUS_CMD_SHIFT)
#define SDCARD_STATUS_ENDBIT		24
#define SDCARD_STATUS_CRC7_SHIFT	25
#define SDCARD_STATUS_CRC7_MASK		(0x7f << SDCARD_STATUS_CMD_SHIFT)

#define SDCARD_REG_PTR		0x08
#define SDCARD_PTR_PTR_SHIFT	0
#define SDCARD_PTR_PTR_MASK		0x0000ffff
#define SDCARD_PTR_LEN_SHIFT	16
#define SDCARD_PTR_LEN_MASK		0xffff0000

#define SDCARD_REG_DATCTRL		0x0c
#define SDCARD_DATCTRL_EN		(1 << 0)
#define SDCARD_DATCTRL_SEND		(1 << 1)
#define SDCARD_DATCTRL_4BIT		(1 << 2)
#define SDCARD_DATCTRL_AUTOCRC16	(1 << 4)
#define SDCARD_DATCTRL_BUSY		(1 << 5)
#define SDCARD_DATCTRL_AUTOBUSY		(1 << 6)
#define SDCARD_DATCTRL_STATUS_SHIFT	8
#define SDCARD_DATCTRL_STATUS_MASK	(0xf << SDCARD_DATCTRL_STATUS_SHIFT)

#define SDCARD_REG_ARG		0x10
#define SDCARD_REG_ARG2		0x14
#define SDCARD_REG_ARG3		0x18
#define SDCARD_REG_ARG4		0x1c

#define CRC_STATUS_NONE		(0x0 << SDCARD_DATCTRL_STATUS_SHIFT)
#define CRC_STATUS_ACCEPTED	(0x5 << SDCARD_DATCTRL_STATUS_SHIFT)
#define CRC_STATUS_CRCERROR	(0xb << SDCARD_DATCTRL_STATUS_SHIFT)
#define CRC_STATUS_WRITEERROR	(0xd << SDCARD_DATCTRL_STATUS_SHIFT)

extern SDBus sdbus;
extern SDState sddev;
extern void *sdcard_map;

#endif
