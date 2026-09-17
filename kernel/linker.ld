/* SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later */

ENTRY(_start)
PHDRS
{
	code PT_LOAD FLAGS(5);
	data PT_LOAD FLAGS(6);
}
SECTIONS
{
	. = 0x40080000;
	.text : { KEEP(*(.text.boot)) *(.text .text.*) } :code
	.rodata : { *(.rodata .rodata.*) } :code
	. = ALIGN(4096);
	.data : { *(.data .data.*) } :data
	.bss (NOLOAD) : {
		. = ALIGN(16);
		__bss_start = .;
		*(.bss .bss.* COMMON)
		. = ALIGN(16);
		__bss_end = .;
	} :data
	.stack (NOLOAD) : {
		. = ALIGN(16);
		. += 16384;
		__stack_top = .;
	} :data
	ASSERT(. < 0x41000000, "Kernel overlaps application slots")
	/DISCARD/ : { *(.comment .note* .eh_frame*) }
}