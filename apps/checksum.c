/* SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "app.h"

int app_main(void)
{
	volatile uint32_t checksum = 0;
	log_text("checksum start\n");
	for (unsigned number = 0; number < 4000000; ++number) {
		checksum += number & 255;
		if ((number + 1) % 1000000 == 0) {
			log_value("checksum progress=", number + 1);
		}
	}
	log_value("checksum result=", checksum);
	return checksum == 510000000 ? 0 : 1;
}