/*
 * SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "abi.h"

.text
.global app_main
app_main:
	sub sp, sp, #16
	mov x0, sp
	str x0, [sp]
.irp register,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30
	mov x\register, #(100 + \register)
.endr
	cmp xzr, xzr
	svc #0
	b.ne failure
	b.cc failure
	b.mi failure
	b.vs failure
	cmn x0, #ERR_NOSYS
	b.ne failure
.irp register,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30
	cmp x\register, #(100 + \register)
	b.ne failure
.endr
	ldr x0, [sp]
	mov x1, sp
	cmp x0, x1
	b.ne failure
	mov x0, #0
	b exit
failure:
	mov x0, #1
exit:
	mov x8, #SYS_EXIT
	svc #0
	udf #0
.section .note.GNU-stack, "", %progbits