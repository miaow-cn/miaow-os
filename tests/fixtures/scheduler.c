/* SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <assert.h>
#include "task.h"

int main(void)
{
	for (unsigned mask = 0; mask < 8; ++mask) {
		struct task tasks[APP_COUNT] = {};
		unsigned ordered[APP_COUNT];
		unsigned count = 0;
		for (unsigned index = 0; index < APP_COUNT; ++index) {
			tasks[index].runnable = (mask & (1u << index)) != 0;
			if (tasks[index].runnable) {
				ordered[count++] = index;
			}
		}
		for (unsigned current = 0; current < APP_COUNT; ++current) {
			int expected = count ? (int)ordered[0] : -1;
			for (unsigned index = 0; index < count; ++index) {
				if (ordered[index] > current) {
					expected = (int)ordered[index];
					break;
				}
			}
			assert(next_runnable(tasks, current) == expected);
		}
	}
	return 0;
}