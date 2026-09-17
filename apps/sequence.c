/* SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "app.h"

int app_main(void)
{
	volatile uint64_t total = 0;
	log_text("sequence start\n");
	for (uint64_t number = 1; number <= 3000000; ++number) {
		total += number;
		if (number % 1000000 == 0) {
			log_value("sequence progress=", number);
		}
	}
	log_value("sequence result=", total);
	return total == 4500001500000ULL ? 0 : 1;
}