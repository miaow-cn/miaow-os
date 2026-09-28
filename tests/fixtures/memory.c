/* SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "kernel.h"
#include "abi.h"
#include <miaow/memblock.h>
#include <miaow/mm.h>
#include <miaow/slab.h>

void string_selftest(unsigned char *end);
[[noreturn]] void __real_kernel_main(void);
void __real_mem_init(void);

#define CHECK(condition) do { if (!(condition)) string_failure(__LINE__); } while (0)

[[noreturn]] void string_failure(int line)
{
	printk("SELFTEST FAIL line=%d\n", line);
	kernel_panic();
}

static void mem_selftest(void)
{
	static const unsigned orders[] = {0, 1, 4, 0};
	static const size_t sizes[] = {1, 16, 17, 2048, 4096, 9000};
	unsigned long baseline = nr_free_pages();
	struct page *blocks[4];
	void *objects[6];

	for (unsigned index = 0; index < 4; ++index) {
		blocks[index] = alloc_pages(orders[index]);
		CHECK(blocks[index]);
		uintptr_t address = page_to_phys(blocks[index]);
		CHECK(!(address & ((PAGE_SIZE << orders[index]) - 1)));
		for (unsigned other = 0; other < index; ++other) {
			uintptr_t taken = page_to_phys(blocks[other]);
			CHECK(address + (PAGE_SIZE << orders[index]) <= taken ||
			      taken + (PAGE_SIZE << orders[other]) <= address);
		}
		*(volatile uint64_t *)page_address(blocks[index]) = 0x5aa5000000000000UL + index;
	}
	for (unsigned index = 0; index < 4; ++index) {
		CHECK(*(volatile uint64_t *)page_address(blocks[index]) ==
		      0x5aa5000000000000UL + index);
		__free_pages(blocks[index], orders[index]);
	}
	CHECK(nr_free_pages() == baseline);
	for (unsigned index = 0; index < 6; ++index) {
		objects[index] = kmalloc(sizes[index]);
		CHECK(objects[index]);
		CHECK(!((uintptr_t)objects[index] % KMALLOC_MIN_SIZE));
		CHECK((uintptr_t)objects[index] - PAGE_OFFSET < RAM_SIZE);
		CHECK(page_address(virt_to_page(objects[index])) ==
		      (void *)((uintptr_t)objects[index] & ~(uintptr_t)(PAGE_SIZE - 1)));
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
	CHECK(nr_free_pages() == baseline);
	printk("MM SELFTEST OK\n");
}

static uint64_t translation(uintptr_t address, unsigned access)
{
	switch (access) {
	case 0:
		__asm__ volatile("at s1e1r, %0" : : "r"(address) : "memory");
		break;
	case 1:
		__asm__ volatile("at s1e1w, %0" : : "r"(address) : "memory");
		break;
	case 2:
		__asm__ volatile("at s1e0r, %0" : : "r"(address) : "memory");
		break;
	default:
		__asm__ volatile("at s1e0w, %0" : : "r"(address) : "memory");
		break;
	}
	__asm__ volatile("isb" : : : "memory");
	return read_sysreg(par_el1);
}

enum kind { UNMAPPED, RAM, DEVICE, USER, KINDS };

static const struct {
	uintptr_t start;
	uintptr_t size;
} mapped[] = {
	{PAGE_OFFSET, RAM_SIZE},
	{IO_ADDRESS(0x08000000), 0x10000},
	{IO_ADDRESS(0x080a0000), 0x20000},
	{IO_ADDRESS(0x09000000), 0x1000},
	{APP_FIRST, APP_COUNT * APP_SLOT_SIZE},
};

static bool overlaps(uintptr_t start, uintptr_t size)
{
	for (unsigned index = 0; index < sizeof(mapped) / sizeof(mapped[0]); ++index) {
		if (start <= mapped[index].start + mapped[index].size - 1 &&
		    mapped[index].start <= start + size - 1) {
			return true;
		}
	}
	return false;
}

static uint64_t expected(uintptr_t address, enum kind *kind)
{
	if (address - PAGE_OFFSET < RAM_SIZE) {
		*kind = RAM;
		return __pa(address) | 0x707UL;
	}
	if (address - APP_FIRST < APP_COUNT * APP_SLOT_SIZE) {
		*kind = USER;
		return address | 0x747UL;
	}
	if (overlaps(address, PAGE_SIZE)) {
		*kind = DEVICE;
		return (address - IO_OFFSET) | 0x403UL | (3UL << 53);
	}
	*kind = UNMAPPED;
	return 0;
}

static void check_translation(uintptr_t address, uintptr_t physical, enum kind kind)
{
	for (unsigned access = 0; access < 4; ++access) {
		uint64_t result = translation(address, access);
		bool fault = kind == UNMAPPED || (access >= 2 && kind != USER);
		CHECK((result & 1) == fault);
		if (!fault) {
			CHECK((result & 0x0000fffffffff000UL) == physical);
			CHECK((result >> 56) == (kind == DEVICE ? 0 : 0x44));
			CHECK(((result >> 7) & 3) == 2);
		}
	}
}

#define TABLES 72

static uintptr_t tables[TABLES];
static unsigned table_count;
static unsigned pages[KINDS];

static uint64_t *table_pointer(uint64_t descriptor)
{
	uintptr_t address = descriptor & ~0xfffUL;
	CHECK((descriptor & 0xfff) == 3);
	CHECK(address >= __pa(_text) && address + PAGE_SIZE <= __pa(_end));
	CHECK(table_count < TABLES);
	for (unsigned index = 0; index < table_count; ++index) {
		CHECK(tables[index] != address);
	}
	bool reserved = false;
	for (unsigned index = 0; index < memblock.reserved.cnt; ++index) {
		struct memblock_region *region = &memblock.reserved.regions[index];
		reserved |= address >= region->base &&
			    address + PAGE_SIZE <= region->base + region->size;
	}
	CHECK(reserved);
	tables[table_count++] = address;
	return __va(address);
}

/* Only branches that overlap a mapped region may hold a table. */
static void walk(uintptr_t root_address, uintptr_t base)
{
	CHECK(!(root_address & 0xfff));
	uint64_t *root = table_pointer(root_address | 3);
	for (unsigned upper = 0; upper < 512; ++upper) {
		uintptr_t block = base | ((uintptr_t)upper << 30);
		if (!overlaps(block, 1UL << 30)) {
			CHECK(root[upper] == 0);
			continue;
		}
		uint64_t *middle = table_pointer(root[upper]);
		for (unsigned index = 0; index < 512; ++index) {
			uintptr_t section = block | ((uintptr_t)index << 21);
			if (!overlaps(section, 1UL << 21)) {
				CHECK(middle[index] == 0);
				continue;
			}
			uint64_t *leaf = table_pointer(middle[index]);
			for (unsigned page = 0; page < 512; ++page) {
				uintptr_t address = section | ((uintptr_t)page << 12);
				enum kind kind;
				uint64_t descriptor = expected(address, &kind);
				CHECK(leaf[page] == descriptor);
				check_translation(address + PAGE_SIZE - 1,
						  descriptor & 0x0000fffffffff000UL, kind);
				pages[kind]++;
			}
		}
	}
}

static void mmu_selftest(void)
{
	static const uintptr_t holes[] = {
		0, PHYS_OFFSET, PHYS_OFFSET + TEXT_OFFSET, PHYS_OFFSET + RAM_SIZE - PAGE_SIZE,
		APP_FIRST - PAGE_SIZE, APP_FIRST + APP_COUNT * APP_SLOT_SIZE,
		0x08000000, 0x09000000, 1UL << 39, 0xffff000000000000UL,
		PAGE_OFFSET - PAGE_SIZE, PAGE_OFFSET + RAM_SIZE, IO_OFFSET,
		IO_ADDRESS(0x08000000 - PAGE_SIZE), IO_ADDRESS(0x08010000),
		IO_ADDRESS(0x080a0000 - PAGE_SIZE), IO_ADDRESS(0x080c0000),
		IO_ADDRESS(0x09000000 - PAGE_SIZE), IO_ADDRESS(0x09001000),
	};
	uintptr_t user_root = read_sysreg(ttbr0_el1);
	uintptr_t swapper_pg_dir = read_sysreg(ttbr1_el1);
	walk(user_root, 0);
	walk(swapper_pg_dir, 0xffffff8000000000UL);
	CHECK(table_count == TABLES);
	uintptr_t low = tables[0];
	uintptr_t high = tables[0];
	for (unsigned index = 0; index < table_count; ++index) {
		if (tables[index] < low) low = tables[index];
		if (tables[index] > high) high = tables[index];
	}
	for (unsigned index = 0; index < sizeof(holes) / sizeof(holes[0]); ++index) {
		check_translation(holes[index], 0, UNMAPPED);
	}
	printk("MMU TCR=%016lx MAIR=%016lx TTBR0=%016lx TTBR1=%016lx tables=%016lx-%016lx\n",
	       read_sysreg(tcr_el1), read_sysreg(mair_el1), user_root, swapper_pg_dir, low,
	       high + PAGE_SIZE);
	printk("MMU SELFTEST OK ram=%u device=%u user=%u\n", pages[RAM], pages[DEVICE],
	       pages[USER]);
}

void __wrap_mem_init(void)
{
	__real_mem_init();
	mmu_selftest();
	mem_selftest();
}

[[noreturn]] void __wrap_kernel_main(void)
{
	string_selftest(__va(PHYS_OFFSET + RAM_SIZE));
	printk("STRING SELFTEST OK\n");
	__real_kernel_main();
}