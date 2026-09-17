/* SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "app.h"

int app_main(void)
{
	unsigned count = 0;
	log_text("primes start\n");
	for (unsigned candidate = 2; candidate <= 100000; ++candidate) {
		bool prime = true;
		for (unsigned divisor = 2; divisor * divisor <= candidate; ++divisor) {
			if (candidate % divisor == 0) {
				prime = false;
				break;
			}
		}
		count += prime;
		if (candidate % 25000 == 0) {
			log_value("primes progress=", candidate);
		}
	}
	log_value("primes result=", count);
	return count == 9592 ? 0 : 1;
}