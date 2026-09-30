/*
 * SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "kernel.h"
#include "abi.h"

#include <miaow/memblock.h>
#include <miaow/mm.h>
#include <miaow/slab.h>

#define FDT_MAGIC 0xd00dfeedu

static uint32_t fdt_read(uintptr_t address)
{
	return __builtin_bswap32(*(const volatile uint32_t *)__va(address));
}

/* Keeps the blob QEMU left in RAM out of the allocators; nothing parses it yet. */
static void early_init_fdt_reserve_self(void)
{
	if (__fdt_pointer < PHYS_OFFSET || __fdt_pointer >= PHYS_OFFSET + RAM_SIZE ||
	    __fdt_pointer % alignof(uint64_t)) {
		return;
	}
	if (fdt_read(__fdt_pointer) != FDT_MAGIC) {
		return;
	}
	if (memblock_reserve(__fdt_pointer, fdt_read(__fdt_pointer + 4))) {
		kernel_panic();
	}
}

void mem_init(void)
{
	unsigned long pages = RAM_SIZE >> PAGE_SHIFT;
	struct memblock_cursor cursor = {};
	uintptr_t start;
	uintptr_t end;
	uintptr_t map;

	if (memblock_add(PHYS_OFFSET, RAM_SIZE) || memblock_reserve(PHYS_OFFSET, __pa(_end) - PHYS_OFFSET) ||
	    memblock_reserve(APP_FIRST, (size_t)APP_COUNT * APP_SLOT_SIZE)) {
		kernel_panic();
	}
	early_init_fdt_reserve_self();

	map = memblock_phys_alloc(pages * sizeof(struct page), alignof(struct page));
	if (!map) {
		kernel_panic();
	}
	free_area_init(PHYS_OFFSET, pages, __va(map));
	while (memblock_next_free(&cursor, &start, &end)) {
		__free_memory_core(start, end);
	}
	pages = nr_free_pages();
	kmem_cache_init();

	printk("phy: %016lx-%016lx map: %016lx dtb: %016lx\n", (uintptr_t)PHYS_OFFSET,
	       (uintptr_t)PHYS_OFFSET + RAM_SIZE, map, __fdt_pointer);
	for (unsigned i = 0; i < memblock.reserved.cnt; i++) {
		struct memblock_region *region = &memblock.reserved.regions[i];

		printk("resv: %016lx-%016lx\n", region->base, region->base + region->size);
	}
	printk("tot: %lu pg = %lu KiB, free: %lu pg = %lu KiB\n", pages, pages << (PAGE_SHIFT - 10), nr_free_pages(),
	       nr_free_pages() << (PAGE_SHIFT - 10));
}
