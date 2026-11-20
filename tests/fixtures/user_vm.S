/* SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "abi.h"

.text
.global app_main
app_main:
	mov x19, x0
	mov x20, x1
	mov x1, #1
	mov x8, #__NR_log
	svc #0
	cmp x0, x20
	b.ne failure
	ldrb w2, [x19]
	mov x0, #0
	ret
failure:
	mov x0, #1
	ret
.section .note.GNU-stack, "", %progbits
