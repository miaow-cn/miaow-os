/*
 * SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef _APP_H
#define _APP_H

#include <stddef.h>
#include <stdint.h>
#include "abi.h"

#if __STDC_VERSION__ < 202311L
#error "Applications require C23"
#endif

static inline long syscall(long number, uintptr_t first, uintptr_t second)
{
	register uintptr_t argument0 __asm__("x0") = first;
	register uintptr_t argument1 __asm__("x1") = second;
	register long call __asm__("x8") = number;
	__asm__ volatile("svc #0" : "+r"(argument0) : "r"(argument1), "r"(call) : "memory");
	return (long)argument0;
}

static inline long log_text(const char *text)
{
	size_t length = 0;
	while (text[length]) {
		++length;
	}
	return syscall(SYS_LOG, (uintptr_t)text, length);
}

static inline void log_value(const char *label, uint64_t value)
{
	char buffer[96];
	char digits[20];
	size_t length = 0;
	size_t count = 0;
	while (*label && length < 74) {
		buffer[length++] = *label++;
	}
	do {
		digits[count++] = '0' + value % 10;
		value /= 10;
	} while (value);
	while (count) {
		buffer[length++] = digits[--count];
	}
	buffer[length++] = '\n';
	syscall(SYS_LOG, (uintptr_t)buffer, length);
}

#endif /* _APP_H */
