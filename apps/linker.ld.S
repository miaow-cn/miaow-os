/*
 * SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "abi.h"

ENTRY(_start)
SECTIONS
{
	APP_BASE = APP_FIRST + APP_INDEX * APP_SLOT_SIZE;
	. = APP_BASE;
	.text : { KEEP(*(.text.start)) *(.text .text.*) }
	.rodata : { *(.rodata .rodata.*) }
	ASSERT(. - APP_BASE <= APP_IMAGE_SIZE, "Application image exceeds slot")
	.forbidden : {
		*(.data .data.* .bss .bss.* COMMON .sdata* .sbss*)
		*(.tdata* .tbss* .init* .fini* .ctors* .dtors* .got* .dynamic .dynsym .dynstr)
		*(.iplt .igot.plt .rela.*)
	}
	ASSERT(SIZEOF(.forbidden) == 0, "Application static storage or runtime initialization is unsupported")
	.debug_info 0 : { *(.debug_info) }
	.debug_abbrev 0 : { *(.debug_abbrev) }
	.debug_line 0 : { *(.debug_line) }
	.debug_line_str 0 : { *(.debug_line_str) }
	.debug_str 0 : { *(.debug_str) }
	.debug_aranges 0 : { *(.debug_aranges) }
	.debug_rnglists 0 : { *(.debug_rnglists) }
	.debug_loclists 0 : { *(.debug_loclists) }
	.debug_frame 0 : { *(.debug_frame) }
	/DISCARD/ : { *(.comment .note* .eh_frame*) }
}