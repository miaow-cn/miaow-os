/*
 * SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <miaow/sprintf.h>

static int failures;

static void check(const char *expected, const char *actual, int length)
{
	if (strcmp(expected, actual) || length != (int)strlen(expected)) {
		printf("FAIL expected \"%s\" got \"%s\" (length %d)\n", expected, actual, length);
		++failures;
	}
}

static int format(char *buffer, size_t size, const char *fmt, ...)
{
	va_list args;
	int length;

	va_start(args, fmt);
	length = vsnprintf(buffer, size, fmt, args);
	va_end(args);
	return length;
}

int main(int argc, char **argv)
{
	char buffer[64];
	char *nothing = argv[argc];
	int length;

	check("0000000040080000", buffer,
	      snprintf(buffer, sizeof(buffer), "%016lx", 0x40080000UL));
	check("-42", buffer, snprintf(buffer, sizeof(buffer), "%d", -42));
	check("42", buffer, snprintf(buffer, sizeof(buffer), "%i", 42));
	check("4000000000", buffer, snprintf(buffer, sizeof(buffer), "%u", 4000000000U));
	check("dead|BEEF", buffer, snprintf(buffer, sizeof(buffer), "%x|%X", 0xdeadU, 0xbeefU));
	check("100", buffer, snprintf(buffer, sizeof(buffer), "%o", 64U));
	check("%", buffer, snprintf(buffer, sizeof(buffer), "%%"));
	check("A", buffer, snprintf(buffer, sizeof(buffer), "%c", 'A'));
	check("log", buffer, snprintf(buffer, sizeof(buffer), "%s", "log"));
	check("ab", buffer, snprintf(buffer, sizeof(buffer), "%.2s", "abcdef"));
	check("<NULL>", buffer, snprintf(buffer, sizeof(buffer), "%s", nothing));
	check("   42|42   |00042", buffer,
	      snprintf(buffer, sizeof(buffer), "%5d|%-5d|%05d", 42, 42, 42));
	check("+42| 42", buffer, snprintf(buffer, sizeof(buffer), "%+d|% d", 42, 42));
	check("0000000009000000", buffer,
	      snprintf(buffer, sizeof(buffer), "%p", (void *)0x09000000UL));
	check("beef", buffer, snprintf(buffer, sizeof(buffer), "%hx", 0x1beef));
	check("123456789abcdef0", buffer,
	      snprintf(buffer, sizeof(buffer), "%Lx", 0x123456789abcdef0LL));
	check("[app 2] ", buffer, format(buffer, sizeof(buffer), "[app %u] ", 2U));

	length = snprintf(buffer, 4, "%s", "abcdef");
	if (length != 6 || strcmp(buffer, "abc")) {
		printf("FAIL truncation got \"%s\" (length %d)\n", buffer, length);
		++failures;
	}
	if (failures) {
		return 1;
	}
	puts("FORMAT OK");
	return 0;
}
