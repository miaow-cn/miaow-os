/*
 * SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "abi.h"
#include <miaow/memory.h>

ENTRY(_start)
PHDRS
{
	code PT_LOAD FLAGS(5);
	data PT_LOAD FLAGS(6);
}
SECTIONS
{
	/* Linked in the linear map, loaded at the physical address QEMU uses for raw images. */
	. = PAGE_OFFSET + TEXT_OFFSET;
	_text = .;
	.text : AT(PHYS_OFFSET + TEXT_OFFSET) { KEEP(*(.text.boot)) *(.text .text.*) } :code
	.rodata : { *(.rodata .rodata.*) } :code
	. = ALIGN(4096);
	.data : { *(.data .data.*) } :data
	.bss (NOLOAD) : {
		. = ALIGN(16);
		__bss_start = .;
		*(.bss .bss.* COMMON)
		. = ALIGN(16);
		__bss_stop = .;
	} :data
	.stack (NOLOAD) : {
		. = ALIGN(16);
		. += 16384;
		__stack_top = .;
	} :data
	_end = .;
	ASSERT(. - PAGE_OFFSET + PHYS_OFFSET <= APP_FIRST, "Kernel overlaps application slots")
	/DISCARD/ : { *(.comment .note* .eh_frame*) }
}