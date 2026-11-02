/* SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later */

/*
 * Each probe faults on its first instruction or at its branch target, and the vector
 * resumes at x30, so the probe appears to return to its caller.
 */
.text
.global probe_read, probe_write, probe_exec, probe_vectors
probe_read:
	ldr x0, [x0]
	ret
probe_write:
	str xzr, [x0]
	ret
probe_exec:
	br x0

.balign 2048
probe_vectors:
	.skip 0x200
	/* Current EL with SP_ELx, synchronous */
	mrs x9, esr_el1
	mrs x10, far_el1
	adrp x11, probe_fault
	add x11, x11, :lo12:probe_fault
	stp x9, x10, [x11]
	msr elr_el1, x30
	eret
.section .note.GNU-stack, "", %progbits
