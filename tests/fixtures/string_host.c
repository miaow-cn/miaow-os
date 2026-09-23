/* SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <stdio.h>
#include <stdlib.h>

void string_selftest(unsigned char *end);

[[noreturn]] void string_failure(int line)
{
	fprintf(stderr, "STRING SELFTEST FAIL line=%d\n", line);
	exit(1);
}

int main(void)
{
	static unsigned char boundary[8192];

	string_selftest(boundary + sizeof(boundary));
	return 0;
}