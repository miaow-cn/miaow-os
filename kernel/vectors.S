/* SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "context.h"

.text
.balign 2048
.global vectors
vectors:
.rept 8
	b unexpected_exception
	.balign 128
.endr
	b lower_sync
	.balign 128
	b lower_irq
	.balign 128
.rept 6
	b unexpected_exception
	.balign 128
.endr
unexpected_exception:
	bl kernel_panic

.macro save_context
	sub sp, sp, #FRAME_SIZE
	stp x0, x1, [sp, #0]
	stp x2, x3, [sp, #16]
	stp x4, x5, [sp, #32]
	stp x6, x7, [sp, #48]
	stp x8, x9, [sp, #64]
	stp x10, x11, [sp, #80]
	stp x12, x13, [sp, #96]
	stp x14, x15, [sp, #112]
	stp x16, x17, [sp, #128]
	stp x18, x19, [sp, #144]
	stp x20, x21, [sp, #160]
	stp x22, x23, [sp, #176]
	stp x24, x25, [sp, #192]
	stp x26, x27, [sp, #208]
	stp x28, x29, [sp, #224]
	str x30, [sp, #240]
	mrs x9, sp_el0
	str x9, [sp, #FRAME_SP]
	mrs x9, elr_el1
	str x9, [sp, #FRAME_PC]
	mrs x9, spsr_el1
	str x9, [sp, #FRAME_PSTATE]
.endm

lower_sync:
	save_context
	mov x1, #0
	b dispatch
lower_irq:
	save_context
	mov x1, #1
dispatch:
	mov x0, sp
	bl trap
	add sp, sp, #FRAME_SIZE
.global enter_app
enter_app:
	mov x30, x0
	ldr x9, [x30, #FRAME_SP]
	msr sp_el0, x9
	ldr x9, [x30, #FRAME_PC]
	msr elr_el1, x9
	ldr x9, [x30, #FRAME_PSTATE]
	msr spsr_el1, x9
	ldp x0, x1, [x30, #0]
	ldp x2, x3, [x30, #16]
	ldp x4, x5, [x30, #32]
	ldp x6, x7, [x30, #48]
	ldp x8, x9, [x30, #64]
	ldp x10, x11, [x30, #80]
	ldp x12, x13, [x30, #96]
	ldp x14, x15, [x30, #112]
	ldp x16, x17, [x30, #128]
	ldp x18, x19, [x30, #144]
	ldp x20, x21, [x30, #160]
	ldp x22, x23, [x30, #176]
	ldp x24, x25, [x30, #192]
	ldp x26, x27, [x30, #208]
	ldp x28, x29, [x30, #224]
	ldr x30, [x30, #240]
	eret
.section .note.GNU-stack, "", %progbits