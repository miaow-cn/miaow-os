/*
 * SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "app.h"
#include <miaow/memory.h>

int app_main(void)
{
	(void)*(volatile uint32_t *)(PAGE_OFFSET + TEXT_OFFSET);
	log_text("ERROR forbidden access returned\n");
	return 1;
}
