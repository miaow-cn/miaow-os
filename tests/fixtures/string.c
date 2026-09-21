/*
 * SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <assert.h>
#include <stdint.h>

#include <miaow/string.h>

#define SIZE 96

static unsigned char actual[SIZE];
static unsigned char expected[SIZE];
static unsigned char source[SIZE];

static void fill(unsigned char *buffer, size_t size, unsigned seed)
{
	for (size_t index = 0; index < size; ++index) {
		buffer[index] = (unsigned char)(seed + index * 7);
	}
}

static int same(const unsigned char *left, const unsigned char *right, size_t size)
{
	for (size_t index = 0; index < size; ++index) {
		if (left[index] != right[index]) {
			return 0;
		}
	}
	return 1;
}

int main(void)
{
	fill(source, SIZE, 3);

	for (size_t offset = 0; offset < 17; ++offset) {
		for (size_t length = 0; offset + length <= 48; ++length) {
			fill(actual, SIZE, 1);
			fill(expected, SIZE, 1);
			for (size_t index = 0; index < length; ++index) {
				expected[offset + index] = 0xa5;
			}
			assert(memset(actual + offset, 0xa5, length) == actual + offset);
			assert(same(actual, expected, SIZE));

			for (size_t from = 0; from < 17; ++from) {
				fill(actual, SIZE, 1);
				fill(expected, SIZE, 1);
				for (size_t index = 0; index < length; ++index) {
					expected[offset + index] = source[from + index];
				}
				assert(memcpy(actual + offset, source + from, length) ==
				       actual + offset);
				assert(same(actual, expected, SIZE));
			}
		}
	}

	/* memmove must work when the ranges overlap in either direction. */
	for (size_t from = 0; from < 17; ++from) {
		for (size_t to = 0; to < 17; ++to) {
			for (size_t length = 0; length <= 48; ++length) {
				unsigned char staged[SIZE];

				fill(actual, SIZE, 1);
				fill(expected, SIZE, 1);
				for (size_t index = 0; index < length; ++index) {
					staged[index] = expected[from + index];
				}
				for (size_t index = 0; index < length; ++index) {
					expected[to + index] = staged[index];
				}
				assert(memmove(actual + to, actual + from, length) == actual + to);
				assert(same(actual, expected, SIZE));
			}
		}
	}

	fill(actual, SIZE, 1);
	fill(expected, SIZE, 1);
	assert(memcmp(actual, expected, SIZE) == 0);
	assert(memcmp(actual, expected, 0) == 0);
	actual[40] = 1;
	expected[40] = 2;
	assert(memcmp(actual, expected, 40) == 0);
	assert(memcmp(actual, expected, 41) < 0);
	assert(memcmp(expected, actual, 41) > 0);
	return 0;
}
