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

#ifdef TEST_MM
#define CHECK(condition)                                                                           \
	do {                                                                                       \
		if (!(condition)) {                                                                \
			printk("MM SELFTEST FAIL line=%d\n", __LINE__);                            \
			return;                                                                    \
		}                                                                                  \
	} while (0)

void mem_selftest(void)
{
	static const unsigned orders[] = {0, 1, 4, 0};
	static const size_t sizes[] = {1, 16, 17, 2048, 4096, 9000};
	unsigned long baseline = page_alloc_free_count();
	struct page *blocks[4];
	void *objects[6];

	for (unsigned index = 0; index < 4; ++index) {
		uintptr_t address;

		blocks[index] = alloc_pages(orders[index]);
		CHECK(blocks[index]);
		address = page_to_phys(blocks[index]);
		CHECK(!(address & ((PAGE_SIZE << orders[index]) - 1)));
		for (unsigned other = 0; other < index; ++other) {
			uintptr_t taken = page_to_phys(blocks[other]);

			CHECK(address + (PAGE_SIZE << orders[index]) <= taken ||
			      taken + (PAGE_SIZE << orders[other]) <= address);
		}
		*(volatile uint64_t *)address = 0x5aa5000000000000UL + index;
	}
	for (unsigned index = 0; index < 4; ++index) {
		CHECK(*(volatile uint64_t *)page_to_phys(blocks[index]) ==
		      0x5aa5000000000000UL + index);
		free_pages(blocks[index], orders[index]);
	}
	CHECK(page_alloc_free_count() == baseline);

	for (unsigned index = 0; index < 6; ++index) {
		objects[index] = kmalloc(sizes[index]);
		CHECK(objects[index]);
		CHECK(!((uintptr_t)objects[index] % KMALLOC_MIN_SIZE));
		memset(objects[index], (int)index, sizes[index]);
	}
	for (unsigned index = 0; index < 6; ++index) {
		unsigned char *bytes = objects[index];

		CHECK(bytes[0] == index && bytes[sizes[index] - 1] == index);
		kfree(objects[index]);
	}
	kfree(nullptr);

	objects[0] = kzalloc(64);
	CHECK(objects[0]);
	for (unsigned offset = 0; offset < 64; ++offset) {
		CHECK(((const unsigned char *)objects[0])[offset] == 0);
	}
	kfree(objects[0]);
	CHECK(page_alloc_free_count() == baseline);
	printk("MM SELFTEST OK\n");
}
#endif
