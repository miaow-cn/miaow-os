/*
 * SPDX-FileCopyrightText: 2001 Peter Bergner, IBM Corp.
 * SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Ported from Linux v7.3-rc3-520-g518e5b794c06 include/linux/memblock.h
 * see docs/third-party.md.
 */

#ifndef _MEMBLOCK_H
#define _MEMBLOCK_H

#include <stddef.h>
#include <stdint.h>

#define MEMBLOCK_MAX_REGIONS 16

struct memblock_region {
	uintptr_t base;
	size_t size;
};

struct memblock_type {
	unsigned count;
	size_t total_size;
	struct memblock_region regions[MEMBLOCK_MAX_REGIONS];
};

struct memblock {
	struct memblock_type memory;
	struct memblock_type reserved;
};

/* Position of a walk over the free ranges; zero-initialize to start over. */
struct memblock_cursor {
	unsigned memory;
	unsigned reserved;
};

extern struct memblock memblock;

int memblock_add(uintptr_t base, size_t size);
int memblock_reserve(uintptr_t base, size_t size);
int memblock_free(uintptr_t base, size_t size);
/* Returns uninitialized memory, or zero when no free range fits. */
uintptr_t memblock_alloc(size_t size, size_t align);
bool memblock_next_free(struct memblock_cursor *cursor, uintptr_t *start, uintptr_t *end);

#endif /* _MEMBLOCK_H */
