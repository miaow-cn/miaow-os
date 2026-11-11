/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Cache maintenance
 *
 * Copyright (C) 2001 Deep Blue Solutions Ltd.
 * Copyright (C) 2012 ARM Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

.text
.balign 4
.global flush_icache_range
.type flush_icache_range, %function
flush_icache_range:
	cmp x0, x1
	b.hs 2f
	mrs x3, ctr_el0
	ubfx x3, x3, #16, #4
	mov x2, #4
	lsl x2, x2, x3
	sub x3, x2, #1
	bic x4, x0, x3
1:
	dc cvau, x4
	add x4, x4, x2
	cmp x4, x1
	b.lo 1b
	dsb sy
	ic iallu
	dsb sy
	isb
2:
	ret
.size flush_icache_range, . - flush_icache_range
.section .note.GNU-stack, "", %progbits