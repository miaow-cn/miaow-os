/*
 * SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "app.h"

int app_main(void)
{
	*(volatile uint32_t *)(uintptr_t)app_main = 0;
	log_text("ERROR forbidden access returned\n");
	return 1;
}
