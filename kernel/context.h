/*
 * SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef _CONTEXT_H
#define _CONTEXT_H

#define S_SP         248
#define S_PC         256
#define S_PSTATE     264
#define S_FRAME_SIZE 272

#ifndef __ASSEMBLER__
#include <stddef.h>
#include <stdint.h>

struct pt_regs {
	uint64_t regs[31];
	uint64_t sp;
	uint64_t pc;
	uint64_t pstate;
};

static_assert(offsetof(struct pt_regs, sp) == S_SP);
static_assert(offsetof(struct pt_regs, pc) == S_PC);
static_assert(offsetof(struct pt_regs, pstate) == S_PSTATE);
static_assert(sizeof(struct pt_regs) == S_FRAME_SIZE);
#endif /* __ASSEMBLER__ */

#endif /* _CONTEXT_H */
