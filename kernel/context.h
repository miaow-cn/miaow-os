/* SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later */

#pragma once

#define FRAME_SP 248
#define FRAME_PC 256
#define FRAME_PSTATE 264
#define FRAME_SIZE 272

#ifndef __ASSEMBLER__
#include <stddef.h>
#include <stdint.h>

struct context {
	uint64_t registers[31];
	uint64_t sp;
	uint64_t pc;
	uint64_t pstate;
};

static_assert(offsetof(struct context, sp) == FRAME_SP);
static_assert(offsetof(struct context, pc) == FRAME_PC);
static_assert(offsetof(struct context, pstate) == FRAME_PSTATE);
static_assert(sizeof(struct context) == FRAME_SIZE);
#endif