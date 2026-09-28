/* SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later */

/* Until the MMU is on, code runs at physical addresses: take addresses PC-relatively. */
.macro adr_l, register, symbol
	adrp \register, \symbol
	add \register, \register, :lo12:\symbol
.endm

.section .text.boot, "ax"
.global _start
_start:
	mov x19, x0
	msr daifset, #15
	mrs x0, CurrentEL
	cmp x0, #4
	b.ne unsupported_el
	msr spsel, #1
	adr_l x0, __stack_top
	mov sp, x0
	mrs x0, sctlr_el1
	ldr x1, =0x1005
	bic x0, x0, x1
	msr sctlr_el1, x0
	isb
	adr_l x0, __bss_start
	adr_l x1, __bss_stop
clear_bss:
	cmp x0, x1
	b.hs bss_ready
	str xzr, [x0], #8
	b clear_bss
bss_ready:
	adr_l x0, __fdt_pointer
	str x19, [x0]
	bl mmu_init
	/* Literal pool entries hold link-time, high virtual addresses. */
	ldr x0, =__mmap_switched
	br x0
.global __mmap_switched
__mmap_switched:
	ldr x0, =__stack_top
	mov sp, x0
	ldr x0, =vectors
	msr vbar_el1, x0
	isb
	bl cpu_uninstall_idmap
	bl start_kernel
halt:
	wfi
	b halt
unsupported_el:
	adr_l x0, unsupported_message
	ldr x1, =0x09000000
unsupported_next:
	ldrb w2, [x0], #1
	cbz w2, halt
unsupported_wait:
	ldr w3, [x1, #0x18]
	tbnz w3, #5, unsupported_wait
	str w2, [x1]
	b unsupported_next
.section .rodata
unsupported_message:
	.asciz "PANIC unsupported entry EL\n"
.section .note.GNU-stack, "", %progbits