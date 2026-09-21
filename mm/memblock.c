/*
 * SPDX-FileCopyrightText: 2001 Peter Bergner, IBM Corp.
 * SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Ported from Linux v7.3-rc3-520-g518e5b794c06 mm/memblock.c, see
 * docs/third-party.md. Reduced to what this kernel needs: NUMA node ids,
 * region flags, the resizable region arrays, the top-down search, kmemleak
 * and debugfs are gone, and the array is a fixed-size static. The
 * region bookkeeping algorithm is unchanged.
 */

#include <miaow/memblock.h>
#include <miaow/string.h>

struct memblock memblock;

/**
 * @brief Limit a range size so that base + size cannot wrap around.
 *
 * @param base Base address of the range.
 * @param size Requested size of the range.
 * @return @p size, or the largest size that still fits in the address space.
 */
static size_t cap_size(uintptr_t base, size_t size)
{
	return size < UINTPTR_MAX - base ? size : UINTPTR_MAX - base;
}

/**
 * @brief Insert a region into @p type at @p index.
 *
 * @param type Region list to grow.
 * @param index Slot to insert at; later regions are shifted up.
 * @param base Base address of the new region.
 * @param size Size of the new region.
 */
static void memblock_insert_region(struct memblock_type *type, unsigned index, uintptr_t base,
				   size_t size)
{
	struct memblock_region *region = &type->regions[index];

	memmove(region + 1, region, (type->count - index) * sizeof(*region));
	region->base = base;
	region->size = size;
	type->count++;
	type->total_size += size;
}

/**
 * @brief Remove the region at @p index from @p type.
 *
 * @param type Region list to shrink.
 * @param index Slot to remove; later regions are shifted down.
 */
static void memblock_remove_region(struct memblock_type *type, unsigned index)
{
	struct memblock_region *region = &type->regions[index];

	type->total_size -= region->size;
	memmove(region, region + 1, (type->count - index - 1) * sizeof(*region));
	type->count--;
}

/**
 * @brief Merge adjacent regions between @p start_rgn and @p end_rgn.
 *
 * @details Coalesces region pairs where one region ends exactly at the base of the
 * next. The region just before @p start_rgn may merge with the range itself.
 *
 * @param type Region list to compact.
 * @param start_rgn First index of the range that may have changed.
 * @param end_rgn One past the last index of the range.
 */
static void memblock_merge_regions(struct memblock_type *type, unsigned start_rgn, unsigned end_rgn)
{
	unsigned index = start_rgn ? start_rgn - 1 : 0;

	if (end_rgn > type->count - 1) {
		end_rgn = type->count - 1;
	}
	while (index < end_rgn) {
		struct memblock_region *current = &type->regions[index];
		struct memblock_region *next = &type->regions[index + 1];

		if (current->base + current->size != next->base) {
			index++;
			continue;
		}
		current->size += next->size;
		memmove(next, next + 1, (type->count - index - 2) * sizeof(*next));
		type->count--;
		end_rgn--;
	}
}

/**
 * @brief Add [base, base + size) to @p type, merging with existing regions.
 *
 * @note Runs twice: the first pass counts the regions the insertion needs, the
 * second pass performs it once the fixed array is known to have room.
 *
 * @param type Region list to grow.
 * @param base Base address of the range.
 * @param size Size of the range.
 * @retval 0   Success.
 * @retval -1  The fixed region array overflow.
 */
static int memblock_add_range(struct memblock_type *type, uintptr_t base, size_t size)
{
	bool insert = false;
	uintptr_t original_base = base;
	uintptr_t end = base + cap_size(base, size);
	unsigned start_rgn = 0;
	unsigned end_rgn = 0;
	unsigned index;
	unsigned added;

	if (!size) {
		return 0;
	}
	if (!type->count) {
		type->regions[0].base = base;
		type->regions[0].size = size;
		type->total_size = size;
		type->count = 1;
		return 0;
	}

repeat:
	base = original_base;
	added = 0;

	for (index = 0; index < type->count; ++index) {
		struct memblock_region *region = &type->regions[index];
		uintptr_t region_base = region->base;
		uintptr_t region_end = region_base + region->size;

		if (region_base >= end) {
			break;
		}
		if (region_end <= base) {
			continue;
		}
		/* The region splits off the lower part of the new range. */
		if (region_base > base) {
			added++;
			if (insert) {
				if (!end_rgn) {
					start_rgn = index;
				}
				end_rgn = index + 1;
				memblock_insert_region(type, index++, base, region_base - base);
			}
		}
		base = region_end < end ? region_end : end;
	}

	if (base < end) {
		added++;
		if (insert) {
			if (!end_rgn) {
				start_rgn = index;
			}
			end_rgn = index + 1;
			memblock_insert_region(type, index, base, end - base);
		}
	}

	if (!added) {
		return 0;
	}
	if (!insert) {
		if (type->count + added > MEMBLOCK_MAX_REGIONS) {
			return -1;
		}
		insert = true;
		goto repeat;
	}
	memblock_merge_regions(type, start_rgn, end_rgn);
	return 0;
}

/**
 * @brief Cut regions so that [base, base + size) is covered by whole regions.
 *
 * @param type Region list to split.
 * @param base Base address of the range to isolate.
 * @param size Size of the range to isolate.
 * @param[out] start_rgn First index of the isolated regions.
 * @param[out] end_rgn One past the last index of the isolated regions.
 * @retval 0   Success.
 * @retval -1  Region array overflow.
 */
static int memblock_isolate_range(struct memblock_type *type, uintptr_t base, size_t size,
				  unsigned *start_rgn, unsigned *end_rgn)
{
	uintptr_t end = base + cap_size(base, size);

	*start_rgn = 0;
	*end_rgn = 0;

	if (!size) {
		return 0;
	}
	if (type->count + 2 > MEMBLOCK_MAX_REGIONS) {
		return -1;
	}

	for (unsigned index = 0; index < type->count; ++index) {
		struct memblock_region *region = &type->regions[index];
		uintptr_t region_base = region->base;
		uintptr_t region_end = region_base + region->size;

		if (region_base >= end) {
			break;
		}
		if (region_end <= base) {
			continue;
		}
		if (region_base < base) {
			region->base = base;
			region->size -= base - region_base;
			type->total_size -= base - region_base;
			memblock_insert_region(type, index, region_base, base - region_base);
		} else if (region_end > end) {
			region->base = end;
			region->size -= end - region_base;
			type->total_size -= end - region_base;
			memblock_insert_region(type, index--, region_base, end - region_base);
		} else {
			if (!*end_rgn) {
				*start_rgn = index;
			}
			*end_rgn = index + 1;
		}
	}
	return 0;
}

/**
 * @brief Remove [base, base + size) from @p type.
 *
 * @param type Region list to shrink.
 * @param base Base address of the range to remove.
 * @param size Size of the range to remove.
 * @retval 0   Success.
 * @retval -1  The fixed region array overflow at isolating the range.
 */
static int memblock_remove_range(struct memblock_type *type, uintptr_t base, size_t size)
{
	unsigned start_rgn;
	unsigned end_rgn;

	if (memblock_isolate_range(type, base, size, &start_rgn, &end_rgn)) {
		return -1;
	}
	for (unsigned index = end_rgn; index-- > start_rgn;) {
		memblock_remove_region(type, index);
	}
	return 0;
}

/**
 * @brief Add a range of usable physical memory.
 *
 * @param base Base address of the range.
 * @param size Size of the range.
 * @retval 0   Success.
 * @retval -1  Region array overflow.
 */
int memblock_add(uintptr_t base, size_t size)
{
	return memblock_add_range(&memblock.memory, base, size);
}

/**
 * @brief Mark a range of physical memory as reserved.
 *
 * @param base Base address of the range.
 * @param size Size of the range.
 * @retval 0   Success.
 * @retval -1  Region array overflow.
 */
int memblock_reserve(uintptr_t base, size_t size)
{
	return memblock_add_range(&memblock.reserved, base, size);
}

/**
 * @brief Release a reserved range of physical memory.
 *
 * @param base Base address of the range.
 * @param size Size of the range.
 * @retval 0   Success.
 * @retval -1  The fixed region array overflow at isolating the range.
 */
int memblock_free(uintptr_t base, size_t size)
{
	return memblock_remove_range(&memblock.reserved, base, size);
}

/**
 * @brief Report the next range of memory that is not reserved.
 *
 * @details Walks the parts of the memory regions that no reserved region covers,
 * bottom up from the lowest address.
 *
 * @param cursor Walk position; zero-initialize before the first call.
 * @param[out] start Base address of the free range.
 * @param[out] end End address (exclusive) of the free range.
 * @return true    A free range found.
 * @return false   No free range found.
 */
bool memblock_next_free(struct memblock_cursor *cursor, uintptr_t *start, uintptr_t *end)
{
	for (; cursor->memory < memblock.memory.count; ++cursor->memory) {
		struct memblock_region *area = &memblock.memory.regions[cursor->memory];
		uintptr_t area_start = area->base;
		uintptr_t area_end = area->base + area->size;

		/* Index count addresses the gap that follows the last reserved region. */
		for (; cursor->reserved < memblock.reserved.count + 1; ++cursor->reserved) {
			struct memblock_region *taken =
				&memblock.reserved.regions[cursor->reserved];
			uintptr_t gap_start =
				cursor->reserved ? taken[-1].base + taken[-1].size : 0;
			uintptr_t gap_end = cursor->reserved < memblock.reserved.count
						    ? taken->base
						    : UINTPTR_MAX;

			if (gap_start >= area_end) {
				break;
			}
			if (area_start < gap_end) {
				*start = area_start > gap_start ? area_start : gap_start;
				*end = area_end < gap_end ? area_end : gap_end;
				/* Advance whichever range ends first. */
				if (area_end <= gap_end) {
					++cursor->memory;
				} else {
					++cursor->reserved;
				}
				return true;
			}
		}
	}
	return false;
}

/**
 * @brief Allocate and reserve a range of free physical memory.
 *
 * @details The memory is uninitialized. The bottom-up search returns the lowest
 * aligned range that is large enough.
 *
 * @param size Number of bytes to allocate.
 * @param align Alignment of the returned address; 0 means byte alignment.
 * @retval 0   No free range fits.
 * @retval !=0 Base address of the reserved range.
 */
uintptr_t memblock_alloc(size_t size, size_t align)
{
	struct memblock_cursor cursor = {};
	uintptr_t start;
	uintptr_t end;

	if (!size) {
		return 0;
	}
	if (!align) {
		align = 1;
	}
	while (memblock_next_free(&cursor, &start, &end)) {
		uintptr_t candidate = (start + align - 1) & ~(uintptr_t)(align - 1);

		if (candidate < end && end - candidate >= size) {
			return memblock_reserve(candidate, size) ? 0 : candidate;
		}
	}
	return 0;
}
