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
	uint64_t ticks = READ_SYSREG(cntpct_el0);
	uint64_t frequency = READ_SYSREG(cntfrq_el0);
	unsigned long seconds = (unsigned long)(ticks / frequency);
	unsigned long micros = (unsigned long)(ticks % frequency * 1000000 / frequency);

	length = snprintf(buffer, sizeof(buffer), "[%5lu.%06lu] ", seconds, micros);
	va_start(args, fmt);
	length += vsnprintf(buffer + length, sizeof(buffer) - (size_t)length, fmt, args);
	va_end(args);
	uart_puts(buffer);
	return length;
}
