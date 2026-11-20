/*
 * SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "app.h"

static const unsigned char data[8193] = {
	[0] = 0x12,
	[4095] = 0x34,
	[4096] = 0x56,
	[8192] = 0x78,
};

int app_main(void)
{
	volatile unsigned index = 0;

	if (data[index] != 0x12 || data[index + 4095] != 0x34 || data[index + 4096] != 0x56 ||
	    data[index + 8192] != 0x78) {
		return 1;
	}
	log_text("multi-page image OK\n");
	return 0;
}
