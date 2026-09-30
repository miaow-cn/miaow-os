/*
 * SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <assert.h>
#include "task.h"

int main(void)
{
	for (unsigned mask = 0; mask < 8; mask++) {
		struct task_struct tasks[APP_COUNT] = {};
		unsigned ordered[APP_COUNT];
		unsigned count = 0;
		for (unsigned i = 0; i < APP_COUNT; i++) {
			tasks[i].runnable = (mask & (1u << i)) != 0;
			if (tasks[i].runnable) {
				ordered[count++] = i;
			}
		}
		for (unsigned current = 0; current < APP_COUNT; current++) {
			int expected = count ? (int)ordered[0] : -1;
			for (unsigned i = 0; i < count; i++) {
				if (ordered[i] > current) {
					expected = (int)ordered[i];
					break;
				}
			}
			assert(next_runnable(tasks, current) == expected);
		}
	}
	return 0;
}
