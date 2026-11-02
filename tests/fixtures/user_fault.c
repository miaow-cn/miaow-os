/*
 * SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "app.h"
#include <miaow/memory.h>

/* Built into every slot; the slot index selects which forbidden access to make. */
int app_main(void)
{
	volatile uint32_t stack_ret = 0xd65f03c0; /* ret */

	switch (((uintptr_t)app_main - APP_FIRST) / APP_SLOT_SIZE) {
	case 0:
		*(volatile uint32_t *)(uintptr_t)app_main = 0;
		break;
	case 1:
		((void (*)(void))(uintptr_t)&stack_ret)();
		break;
	default:
		(void)*(volatile uint32_t *)(PAGE_OFFSET + TEXT_OFFSET);
		break;
	}
	log_text("ERROR forbidden access returned\n");
	return 1;
}
