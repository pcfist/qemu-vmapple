/* SPDX-License-Identifier: GPL-2.0+ */

SECTIONS
{
	. = ALIGN(0x1000);
	__sram_start = .;

	.sram : {
		*(.text.sram*)
		*(.rodata.sram*)
		*(.data.sram*)
	}

	. = ALIGN(0x1000);
	__sram_stop = .;
}

INSERT BEFORE .data;
