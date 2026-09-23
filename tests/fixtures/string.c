/*
 * SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdint.h>

#include <miaow/string.h>

[[noreturn]] void string_failure(int line);

#define assert(condition)                                                                          \
	do {                                                                                       \
		if (!(condition)) {                                                                \
			string_failure(__LINE__);                                                  \
		}                                                                                  \
	} while (0)

#define SIZE 96
#define TEST_PAGE_SIZE 4096

static alignas(TEST_PAGE_SIZE) unsigned char actual_buffer[2 * TEST_PAGE_SIZE];
static alignas(TEST_PAGE_SIZE) unsigned char expected_buffer[2 * TEST_PAGE_SIZE];
static alignas(TEST_PAGE_SIZE) unsigned char source_buffer[2 * TEST_PAGE_SIZE];

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

static void check_memory(unsigned char *actual, unsigned char *expected, unsigned char *source)
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

	for (size_t offset = 0; offset < 8; ++offset) {
		for (size_t from = 0; from < 8; ++from) {
			fill(actual, SIZE, 1);
			fill(expected, SIZE, 1);
			for (size_t index = 0; index < 48; ++index) {
				expected[from + index] = actual[offset + index];
			}
			assert(memcmp(actual + offset, expected + from, 48) == 0);
			expected[from + 47] ^= 0xff;
			assert(memcmp(actual + offset, expected + from, 47) == 0);
			assert(memcmp(actual + offset, expected + from, 48) ==
			       (int)actual[offset + 47] - (int)expected[from + 47]);
		}
	}
}

static void check_boundary(unsigned char *end)
{
	static const size_t lengths[] = {0, 1, 7, 8, 9, 15, 16, 17, 31, 32, 33,
					63, 64, 65, 4095, 4096, 4097};

	for (unsigned index = 0; index < sizeof(lengths) / sizeof(lengths[0]); ++index) {
		size_t length = lengths[index];
		unsigned char *edge = end - length;
		unsigned char *local = source_buffer + 1;

		fill(local, length, 3);
		edge[-1] = 0x5a;
		assert(memset(edge, 0xa5, length) == edge);
		for (size_t offset = 0; offset < length; ++offset) {
			assert(edge[offset] == 0xa5);
		}
		assert(edge[-1] == 0x5a);
		assert(memcpy(edge, local, length) == edge);
		assert(edge[-1] == 0x5a);
		assert(same(edge, local, length));
		assert(memcmp(edge, local, length) == 0);
		assert(memcmp(local, edge, length) == 0);
		assert(memcpy(expected_buffer + 1, edge, length) == expected_buffer + 1);
		assert(same(expected_buffer + 1, local, length));
		assert(memmove(edge, edge, length) == edge);
		assert(memmove(edge - 1, edge, length) == edge - 1);
		assert(same(edge - 1, local, length));
		assert(memmove(edge, edge - 1, length) == edge);
		assert(same(edge, local, length));
	}
}

void string_selftest(unsigned char *end)
{
	check_memory(actual_buffer, expected_buffer, source_buffer);
	check_memory(actual_buffer + TEST_PAGE_SIZE - 32,
		     expected_buffer + TEST_PAGE_SIZE - 32,
		     source_buffer + TEST_PAGE_SIZE - 32);
	check_boundary(end);
}
