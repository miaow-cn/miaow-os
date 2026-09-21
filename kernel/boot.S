/* SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later */

.section .text.boot, "ax"
.global _start
_start:
	mov x19, x0
	msr daifset, #15
	mrs x0, CurrentEL
	cmp x0, #4
	b.ne unsupported_el
	msr spsel, #1
	ldr x0, =__stack_top
	mov sp, x0
	mrs x0, sctlr_el1
	ldr x1, =0x1005
	bic x0, x0, x1
	msr sctlr_el1, x0
	isb
	ldr x0, =__bss_start
	ldr x1, =__bss_end
#ifdef TEST_DIRTY_BSS
	mov x2, x0
	mov x3, #-1
dirty_bss:
	cmp x2, x1
	b.hs clear_bss
	str x3, [x2], #8
	b dirty_bss
#endif
clear_bss:
	cmp x0, x1
	b.hs bss_ready
	str xzr, [x0], #8
	b clear_bss
bss_ready:
	ldr x0, =dtb_pointer
	str x19, [x0]
	ldr x0, =vectors
	msr vbar_el1, x0
	isb
	bl kernel_main
halt:
	wfi
	b halt
unsupported_el:
	ldr x0, =unsupported_message
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