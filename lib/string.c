/*
 * SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdint.h>

#include <miaow/string.h>

typedef uint64_t __attribute__((aligned(1), may_alias)) word_t;

#define WORD_SIZE sizeof(word_t)

static_assert(alignof(word_t) == 1);

void *memset(void *destination, int value, size_t size)
{
	unsigned char *output = destination;
	unsigned char byte = (unsigned char)value;

	word_t pattern = (word_t)byte * 0x0101010101010101ULL;
	while (size >= WORD_SIZE) {
		*(word_t *)output = pattern;
		output += WORD_SIZE;
		size -= WORD_SIZE;
	}
	while (size--) {
		*output++ = byte;
	}
	return destination;
}

void *memcpy(void *destination, const void *source, size_t size)
{
	unsigned char *output = destination;
	const unsigned char *input = source;

	while (size >= WORD_SIZE) {
		*(word_t *)output = *(const word_t *)input;
		output += WORD_SIZE;
		input += WORD_SIZE;
		size -= WORD_SIZE;
	}
	while (size--) {
		*output++ = *input++;
	}
	return destination;
}

void *memmove(void *destination, const void *source, size_t size)
{
	unsigned char *output = destination;
	const unsigned char *input = source;

	if (output == input || !size) {
		return destination;
	}
	if (output < input || output >= input + size) {
		return memcpy(destination, source, size);
	}
	output += size;
	input += size;
	while (size >= WORD_SIZE) {
		output -= WORD_SIZE;
		input -= WORD_SIZE;
		*(word_t *)output = *(const word_t *)input;
		size -= WORD_SIZE;
	}
	while (size--) {
		*--output = *--input;
	}
	return destination;
}

int memcmp(const void *left, const void *right, size_t size)
{
	const unsigned char *first = left;
	const unsigned char *second = right;

	for (size_t index = 0; index < size; ++index) {
		if (first[index] != second[index]) {
			return (int)first[index] - (int)second[index];
		}
	}
	return 0;
}
