/*
 * SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdarg.h>

#include <miaow/printk.h>
#include <miaow/sprintf.h>

#include "kernel.h"

int printk(const char *fmt, ...)
{
	char buffer[256];
	va_list args;
	int length;

	va_start(args, fmt);
	length = vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);
	uart_puts(buffer);
	return length;
}
