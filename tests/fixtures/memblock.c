/*
 * SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <assert.h>

#include <miaow/memblock.h>

static void reset(void)
{
	memblock = (struct memblock){};
}

int main(void)
{
	struct memblock_cursor cursor;
	uintptr_t start;
	uintptr_t end;

	/* Regions are kept sorted, and filling a gap merges the neighbours. */
	reset();
	assert(memblock_add(0x2000, 0x1000) == 0);
	assert(memblock_add(0x0000, 0x1000) == 0);
	assert(memblock.memory.count == 2);
	assert(memblock.memory.regions[0].base == 0x0000);
	assert(memblock.memory.regions[1].base == 0x2000);
	assert(memblock_add(0x1000, 0x1000) == 0);
	assert(memblock.memory.count == 1);
	assert(memblock.memory.regions[0].base == 0x0000);
	assert(memblock.memory.regions[0].size == 0x3000);
	assert(memblock.memory.total_size == 0x3000);

	/* An overlapping addition contributes only the part that is new. */
	reset();
	assert(memblock_add(0x1000, 0x2000) == 0);
	assert(memblock_add(0x2000, 0x2000) == 0);
	assert(memblock.memory.count == 1);
	assert(memblock.memory.regions[0].base == 0x1000);
	assert(memblock.memory.regions[0].size == 0x3000);
	assert(memblock.memory.total_size == 0x3000);
	assert(memblock_add(0x1800, 0x400) == 0);
	assert(memblock.memory.count == 1);
	assert(memblock.memory.total_size == 0x3000);

	/* Iteration reports memory minus the reserved ranges. */
	reset();
	assert(memblock_add(0x0000, 0x10000) == 0);
	assert(memblock_reserve(0x2000, 0x1000) == 0);
	assert(memblock_reserve(0x5000, 0x1000) == 0);
	cursor = (struct memblock_cursor){};
	assert(memblock_next_free(&cursor, &start, &end) && start == 0x0000 && end == 0x2000);
	assert(memblock_next_free(&cursor, &start, &end) && start == 0x3000 && end == 0x5000);
	assert(memblock_next_free(&cursor, &start, &end) && start == 0x6000 && end == 0x10000);
	assert(!memblock_next_free(&cursor, &start, &end));

	/* Releasing part of a reserved region splits it. */
	assert(memblock_reserve(0x2000, 0x4000) == 0);
	assert(memblock.reserved.count == 1);
	assert(memblock.reserved.regions[0].base == 0x2000);
	assert(memblock.reserved.regions[0].size == 0x4000);
	assert(memblock_free(0x3000, 0x1000) == 0);
	assert(memblock.reserved.count == 2);
	assert(memblock.reserved.regions[0].base == 0x2000);
	assert(memblock.reserved.regions[0].size == 0x1000);
	assert(memblock.reserved.regions[1].base == 0x4000);
	assert(memblock.reserved.regions[1].size == 0x2000);
	assert(memblock.reserved.total_size == 0x3000);

	/* Allocation takes the lowest range that satisfies size and alignment. */
	reset();
	assert(memblock_add(0x0000, 0x10000) == 0);
	assert(memblock_reserve(0x0000, 0x1001) == 0);
	assert(memblock_alloc(0x100, 0x100) == 0x1100);
	assert(memblock_alloc(0x100, 0x1000) == 0x2000);
	assert(memblock_alloc(0x100000, 1) == 0);
	assert(memblock_alloc(0, 1) == 0);

	/* The fixed region array refuses to overflow. */
	reset();
	for (unsigned index = 0; index < MEMBLOCK_MAX_REGIONS; ++index) {
		assert(memblock_add(index * 0x2000, 0x1000) == 0);
	}
	assert(memblock.memory.count == MEMBLOCK_MAX_REGIONS);
	assert(memblock_add(MEMBLOCK_MAX_REGIONS * 0x2000, 0x1000) == -1);
	assert(memblock.memory.count == MEMBLOCK_MAX_REGIONS);
	return 0;
}
