/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (C) 1991, 1992, 1993, 1994  Linus Torvalds
 * Buddy coalescing commentary by William Lee Irwin III.
 *
 * Imported from Linux v2.6.12 mm/page_alloc.c (GPL-2.0-only); see
 * docs/third-party.md and the porting notes in <miaow/mm.h>.
 */

#include <miaow/mm.h>
#include <miaow/string.h>

static struct page *mem_map;
static unsigned long mem_map_base_pfn;
static unsigned long mem_map_pages;
static struct free_area free_area[MAX_ORDER];
static unsigned long free_count;

/**
 * @brief Get the page frame number of @p page.
 *
 * @param page Page to query.
 * @return PFN, offset by the base PFN of @p mem_map.
 */
static unsigned long page_to_pfn(const struct page *page)
{
	return mem_map_base_pfn + (unsigned long)(page - mem_map);
}

/**
 * @brief Get the page descriptor for @p pfn.
 *
 * @param pfn Page frame number to resolve.
 * @return Entry in @p mem_map; undefined if @p pfn is invalid.
 */
static struct page *pfn_to_page(unsigned long pfn)
{
	return &mem_map[pfn - mem_map_base_pfn];
}

/**
 * @brief Test whether @p pfn belongs to the managed memory range.
 *
 * @param pfn Page frame number to test.
 * @return True if @p pfn maps to an entry in @p mem_map.
 */
static bool pfn_valid(unsigned long pfn)
{
	return pfn >= mem_map_base_pfn && pfn < mem_map_base_pfn + mem_map_pages;
}

/**
 * @brief Get the physical address of @p page.
 *
 * @param page Page to query.
 * @return Physical address of the first byte of the page.
 */
uintptr_t page_to_phys(const struct page *page)
{
	return (uintptr_t)page_to_pfn(page) << PAGE_SHIFT;
}

/**
 * @brief Get the page descriptor for a physical address.
 *
 * @param address Any address within the page.
 * @return Entry in @p mem_map; undefined if @p address is unmanaged.
 */
struct page *phys_to_page(uintptr_t address)
{
	return pfn_to_page(address >> PAGE_SHIFT);
}

/**
 * @brief Get the number of free pages.
 *
 * @return Total pages currently held in all free-area lists.
 */
unsigned long page_alloc_free_count(void)
{
	return free_count;
}

/**
 * @brief Initialize the allocator over a physical memory range.
 *
 * @param base Base address of the managed range, page aligned.
 * @param pages Number of pages in the range.
 * @param map Caller-provided page descriptor array of @p pages entries.
 */
void page_alloc_init(uintptr_t base, unsigned long pages, struct page *map)
{
	mem_map = map;
	mem_map_base_pfn = base >> PAGE_SHIFT;
	mem_map_pages = pages;
	free_count = 0;
	memset(map, 0, pages * sizeof(*map));
	for (unsigned order = 0; order < MAX_ORDER; ++order) {
		INIT_LIST_HEAD(&free_area[order].list);
		free_area[order].count = 0;
	}
}

/**
 * @brief Free one block, coalescing it with its buddies upwards.
 *
 * @details Marks the block free and merges it while its buddy, the block that
 * differs only in bit @p order of the page frame number, is free at the same
 * order. Clearing that bit gives the index of the combined block.
 *
 * @param page First page of the block.
 * @param order Order of the block; the block spans 2^@p order pages.
 */
static void __free_one_page(struct page *page, unsigned order)
{
	unsigned long pfn = page_to_pfn(page);

	while (order < MAX_ORDER - 1) {
		unsigned long buddy_pfn = pfn ^ (1UL << order);
		struct page *buddy;

		if (!pfn_valid(buddy_pfn)) {
			break;
		}
		buddy = pfn_to_page(buddy_pfn);
		if (!(buddy->flags & PAGE_BUDDY) || buddy->order != order) {
			break;
		}
		list_del(&buddy->list);
		buddy->flags &= ~PAGE_BUDDY;
		free_area[order].count--;
		pfn &= ~(1UL << order);
		page = pfn_to_page(pfn);
		order++;
	}
	page->order = order;
	page->flags |= PAGE_BUDDY;
	list_add(&page->list, &free_area[order].list);
	free_area[order].count++;
}

/**
 * @brief Return the unused upper halves of an oversized block to lower orders.
 *
 * @param page Block of 2^@p high pages just removed from the free list.
 * @param low Order the caller requested.
 * @param high Order of @p page.
 * @return @p page, now representing the 2^@p low pages at its start.
 */
static struct page *expand(struct page *page, unsigned low, unsigned high)
{
	unsigned long size = 1UL << high;

	while (high > low) {
		struct page *half;

		high--;
		size >>= 1;
		half = &page[size];
		half->order = high;
		half->flags |= PAGE_BUDDY;
		list_add(&half->list, &free_area[high].list);
		free_area[high].count++;
	}
	return page;
}

/**
 * @brief Allocate a block of contiguous pages.
 *
 * @details Takes the first free block at the smallest available order no lower
 * than @p order, splitting it down with expand() when it is oversized.
 *
 * @param order Order to allocate; must be less than MAX_ORDER.
 * @return First page of the block, or NULL if no block can satisfy @p order.
 */
struct page *alloc_pages(unsigned order)
{
	if (order >= MAX_ORDER) {
		return nullptr;
	}
	for (unsigned current = order; current < MAX_ORDER; ++current) {
		struct page *page;

		if (list_empty(&free_area[current].list)) {
			continue;
		}
		page = list_first_entry(&free_area[current].list, struct page, list);
		list_del(&page->list);
		page->flags &= ~PAGE_BUDDY;
		free_area[current].count--;
		free_count -= 1UL << order;
		return expand(page, order, current);
	}
	return nullptr;
}

/**
 * @brief Free a block allocated by alloc_pages().
 *
 * @param page First page of the block.
 * @param order Order used at allocation time.
 */
void free_pages(struct page *page, unsigned order)
{
	__free_one_page(page, order);
	free_count += 1UL << order;
}

/**
 * @brief Hand a physical range to the allocator as its largest aligned blocks.
 *
 * @details Clips the range to the managed area, aligns it to page boundaries,
 * then frees it in descending power-of-two pieces so that merging has nothing
 * left to do. Used to seed the allocator after init.
 *
 * @param start Start address of the range, rounded up to a page boundary.
 * @param end End address of the range, rounded down to a page boundary.
 */
void page_alloc_free_range(uintptr_t start, uintptr_t end)
{
	unsigned long pfn = (start + PAGE_SIZE - 1) >> PAGE_SHIFT;
	unsigned long limit = end >> PAGE_SHIFT;

	if (pfn < mem_map_base_pfn) {
		pfn = mem_map_base_pfn;
	}
	if (limit > mem_map_base_pfn + mem_map_pages) {
		limit = mem_map_base_pfn + mem_map_pages;
	}
	while (pfn < limit) {
		unsigned order = MAX_ORDER - 1;

		while (order && ((pfn & ((1UL << order) - 1)) || pfn + (1UL << order) > limit)) {
			order--;
		}
		free_pages(pfn_to_page(pfn), order);
		pfn += 1UL << order;
	}
}
