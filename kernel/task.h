/*
 * SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef _TASK_H
#define _TASK_H

#include "context.h"
#include "abi.h"

struct task {
	struct context context;
	uintptr_t image;
	size_t size;
	uintptr_t stack_bottom;
	uintptr_t stack_top;
	bool runnable;
	bool observed;
	long exit_status;
};

static inline int next_runnable(const struct task tasks[APP_COUNT], unsigned current)
{
	for (unsigned offset = 1; offset <= APP_COUNT; ++offset) {
		unsigned next = (current + offset) % APP_COUNT;
		if (tasks[next].runnable) {
			return (int)next;
		}
	}
	return -1;
}

#endif /* _TASK_H */
