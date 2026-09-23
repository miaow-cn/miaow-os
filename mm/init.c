/*
 * SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "kernel.h"

#include <miaow/memblock.h>
#include <miaow/mm.h>
#include <miaow/slab.h>

#define FDT_MAGIC 0xd00dfeedu

static uint32_t fdt_read(uintptr_t address)
{
	return __builtin_bswap32(*(const volatile uint32_t *)address);
}

/* Keeps the blob QEMU left in RAM out of the allocators; nothing parses it yet. */
static void reserve_device_tree(void)
{
	if (dtb_pointer < RAM_BASE || dtb_pointer >= RAM_BASE + RAM_SIZE ||
	    dtb_pointer % alignof(uint64_t)) {
		return;
	}
	if (fdt_read(dtb_pointer) != FDT_MAGIC) {
		return;
	}
	if (memblock_reserve(dtb_pointer, fdt_read(dtb_pointer + 4))) {
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

	if (memblock_add(RAM_BASE, RAM_SIZE) ||
	    memblock_reserve(RAM_BASE, (uintptr_t)_end - RAM_BASE) ||
	    memblock_reserve(APP_FIRST, (size_t)APP_COUNT * APP_SLOT_SIZE)) {
		kernel_panic();
	}
	reserve_device_tree();

	map = memblock_alloc(pages * sizeof(struct page), alignof(struct page));
	if (!map) {
		kernel_panic();
	}
	page_alloc_init(RAM_BASE, pages, (struct page *)map);
	while (memblock_next_free(&cursor, &start, &end)) {
		page_alloc_free_range(start, end);
	}
	slab_init();

	printk("MEM ram=%016lx-%016lx map=%016lx dtb=%016lx\n", (uintptr_t)RAM_BASE,
	       (uintptr_t)RAM_BASE + RAM_SIZE, map, dtb_pointer);
	for (unsigned index = 0; index < memblock.reserved.count; ++index) {
		struct memblock_region *region = &memblock.reserved.regions[index];

		printk("MEM reserved %016lx-%016lx\n", region->base, region->base + region->size);
	}
	printk("MEM free=%lu KiB\n", page_alloc_free_count() << (PAGE_SHIFT - 10));
}

