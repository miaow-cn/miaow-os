/*
 * SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "abi.h"

.section .text.start, "ax"
.global _start
_start:
	bl app_main
	mov x8, #SYS_EXIT
	svc #0
	udf #0
.section .note.GNU-stack, "", %progbits