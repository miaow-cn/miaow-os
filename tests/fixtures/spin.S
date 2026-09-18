/*
 * SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

.text
.global app_main
app_main:
	sub sp, sp, #16
	mov x0, #0
.irp register,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30
	mov x\register, #(100 + \register)
.endr
	cmp xzr, xzr
spin:
	add x0, x0, #1
	b spin
.section .note.GNU-stack, "", %progbits