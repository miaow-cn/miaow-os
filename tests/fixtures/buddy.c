/*
 * SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <assert.h>
#include <stdlib.h>

#include <miaow/mm.h>

#define PAGES 64

static struct page *held[PAGES];

int main(void)
{
	void *memory = aligned_alloc(PAGE_SIZE, PAGES * PAGE_SIZE);
	struct page *map = calloc(PAGES, sizeof(struct page));
	uintptr_t base;
	unsigned largest = 0;
	unsigned count = 0;

	assert(memory && map);
	base = (uintptr_t)memory;
	page_alloc_init(base, PAGES, map);
	assert(page_alloc_free_count() == 0);
	page_alloc_free_range(base, base + PAGES * PAGE_SIZE);
	assert(page_alloc_free_count() == PAGES);

	/* A block is aligned to its own size and lies inside the managed range. */
	for (unsigned order = 0; order < MAX_ORDER; ++order) {
		struct page *block = alloc_pages(order);

		if (!block) {
			break;
		}
		uintptr_t address = page_to_phys(block);

		assert(!(address & ((uintptr_t)(PAGE_SIZE << order) - 1)));
		assert(address >= base && address + (PAGE_SIZE << order) <= base + PAGES * PAGE_SIZE);
		assert(phys_to_page(address) == block);
		assert(page_alloc_free_count() == PAGES - (1UL << order));
		free_pages(block, order);
		assert(page_alloc_free_count() == PAGES);
		largest = order;
	}
	assert(largest > 0);
	assert(!alloc_pages(MAX_ORDER));

	/* Mixed orders held at the same time never overlap. */
	static const unsigned orders[] = {0, 3, 1, 2, 0, 4};
	for (unsigned index = 0; index < 6; ++index) {
		held[index] = alloc_pages(orders[index]);
		assert(held[index]);
		for (unsigned other = 0; other < index; ++other) {
			uintptr_t mine = page_to_phys(held[index]);
			uintptr_t theirs = page_to_phys(held[other]);

			assert(mine + (PAGE_SIZE << orders[index]) <= theirs ||
			       theirs + (PAGE_SIZE << orders[other]) <= mine);
		}
	}
	for (unsigned index = 0; index < 6; ++index) {
		free_pages(held[index], orders[index]);
	}
	assert(page_alloc_free_count() == PAGES);

	/* Fragment the arena completely, then release it and check it coalesced. */
	while (count < PAGES && (held[count] = alloc_pages(0)) != nullptr) {
		count++;
	}
	assert(count == PAGES);
	assert(page_alloc_free_count() == 0);
	assert(!alloc_pages(0));
	for (unsigned index = 0; index < count; ++index) {
		free_pages(held[index], 0);
	}
	assert(page_alloc_free_count() == PAGES);

	struct page *restored = alloc_pages(largest);

	assert(restored);
	free_pages(restored, largest);
	assert(page_alloc_free_count() == PAGES);

	free(map);
	free(memory);
	return 0;
}
