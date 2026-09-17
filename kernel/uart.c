/* SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "kernel.h"

void uart_putc(char character)
{
	volatile uint32_t *uart = (volatile uint32_t *)0x09000000;
	while (uart[0x18 / 4] & (1u << 5)) {
	}
	uart[0] = (unsigned char)character;
}

void uart_puts(const char *text)
{
	while (*text) {
		uart_putc(*text++);
	}
}

void uart_hex(uint64_t value)
{
	const char *digits = "0123456789abcdef";
	for (int shift = 60; shift >= 0; shift -= 4) {
		uart_putc(digits[(value >> shift) & 15]);
	}
}