/* SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "kernel.h"
#include "abi.h"
#include <miaow/memblock.h>
#include <miaow/mm.h>
#include <miaow/slab.h>

void string_selftest(unsigned char *end);
[[noreturn]] void __real_start_kernel(void);
void __real_mem_init(void);

#define CHECK(condition)                                                                                               \
	do {                                                                                                           \
		if (!(condition))                                                                                      \
			string_failure(__LINE__);                                                                      \
	} while (0)

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

	for (unsigned i = 0; i < 4; i++) {
		blocks[i] = alloc_pages(orders[i]);
		CHECK(blocks[i]);
		uintptr_t address = page_to_phys(blocks[i]);
		CHECK(!(address & ((PAGE_SIZE << orders[i]) - 1)));
		for (unsigned other = 0; other < i; other++) {
			uintptr_t taken = page_to_phys(blocks[other]);
			CHECK(address + (PAGE_SIZE << orders[i]) <= taken ||
			      taken + (PAGE_SIZE << orders[other]) <= address);
		}
		*(volatile uint64_t *)page_address(blocks[i]) = 0x5aa5000000000000UL + i;
	}
	for (unsigned i = 0; i < 4; i++) {
		CHECK(*(volatile uint64_t *)page_address(blocks[i]) == 0x5aa5000000000000UL + i);
		__free_pages(blocks[i], orders[i]);
	}
	CHECK(nr_free_pages() == baseline);
	for (unsigned i = 0; i < 6; i++) {
		objects[i] = kmalloc(sizes[i]);
		CHECK(objects[i]);
		CHECK(!((uintptr_t)objects[i] % KMALLOC_MIN_SIZE));
		CHECK((uintptr_t)objects[i] - PAGE_OFFSET < RAM_SIZE);
		CHECK(page_address(virt_to_page(objects[i])) ==
		      (void *)((uintptr_t)objects[i] & ~(uintptr_t)(PAGE_SIZE - 1)));
		memset(objects[i], (int)i, sizes[i]);
	}
	for (unsigned i = 0; i < 6; i++) {
		unsigned char *bytes = objects[i];
		CHECK(bytes[0] == i && bytes[sizes[i] - 1] == i);
		kfree(objects[i]);
	}
	kfree(nullptr);
	objects[0] = kzalloc(64);
	CHECK(objects[0]);
	for (unsigned offset = 0; offset < 64; offset++) {
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

/* Descriptor bits, spelled out independently of mm/mmu.c. */
#define NORMAL  0x707UL /* page, AttrIndx 1, inner shareable, AF */
#define DEVICE  0x703UL /* page, AttrIndx 0, inner shareable, AF */
#define USER    (1UL << 6)
#define RDONLY  (1UL << 7)
#define PXN     (1UL << 53)
#define UXN     (1UL << 54)
#define ADDRESS 0x0000fffffffff000UL

static const struct {
	uintptr_t start;
	uintptr_t size;
} mapped[] = {
	{PAGE_OFFSET, RAM_SIZE},          {IO_ADDRESS(0x08000000), 0x10000},      {IO_ADDRESS(0x080a0000), 0x20000},
	{IO_ADDRESS(0x09000000), 0x1000}, {APP_FIRST, APP_COUNT * APP_SLOT_SIZE},
};

static bool overlaps(uintptr_t start, uintptr_t size)
{
	for (unsigned i = 0; i < sizeof(mapped) / sizeof(mapped[0]); i++) {
		if (start <= mapped[i].start + mapped[i].size - 1 && mapped[i].start <= start + size - 1) {
			return true;
		}
	}
	return false;
}

static bool inside(uintptr_t address, const void *start, const void *end)
{
	return address - (uintptr_t)start < (uintptr_t)end - (uintptr_t)start;
}

static uint64_t expected(uintptr_t address)
{
	uintptr_t offset = (address - APP_FIRST) % APP_SLOT_SIZE;

	if (address - PAGE_OFFSET < RAM_SIZE) {
		if (inside(address, _text, _etext)) {
			return __pa(address) | NORMAL | RDONLY | UXN;
		}
		if (inside(address, __start_rodata, __end_rodata)) {
			return __pa(address) | NORMAL | RDONLY | PXN | UXN;
		}
		return __pa(address) | NORMAL | PXN | UXN;
	}
	if (address - APP_FIRST < APP_COUNT * APP_SLOT_SIZE) {
		if (offset < APP_IMAGE_SIZE) {
			return address | NORMAL | USER | RDONLY | PXN;
		}
		if (offset >= APP_SLOT_SIZE - APP_STACK_SIZE) {
			return address | NORMAL | USER | PXN | UXN;
		}
		return 0;
	}
	if (overlaps(address, PAGE_SIZE)) {
		return (address - IO_OFFSET) | DEVICE | PXN | UXN;
	}
	return 0;
}

/* AT reports only data access faults; execute permission is proven by the fault fixtures. */
static void check_translation(uintptr_t address, uint64_t descriptor)
{
	for (unsigned access = 0; access < 4; access++) {
		bool el0 = access >= 2;
		bool write = access & 1;
		bool allowed = descriptor && (!el0 || (descriptor & USER)) && (!write || !(descriptor & RDONLY));
		uint64_t result = translation(address, access);
		CHECK((result & 1) == !allowed);
		if (allowed) {
			CHECK((result & ADDRESS) == (descriptor & ADDRESS));
			CHECK((result >> 56) == ((descriptor & 0x1c) ? 0x44 : 0));
			CHECK(((result >> 7) & 3) == 2);
		} else if (descriptor) {
			CHECK(((result >> 1) & 0x3f) == 0x0f); /* level 3 permission fault */
		}
	}
}

/* No writable executable page at either level, and no writable alias of text or rodata. */
static void check_wx(uint64_t descriptor)
{
	bool writable = !(descriptor & RDONLY);
	uintptr_t physical = descriptor & ADDRESS;

	CHECK(!writable || (descriptor & PXN));
	CHECK(!writable || !(descriptor & USER) || (descriptor & UXN));
	CHECK(!writable || !inside(physical, (void *)__pa(_text), (void *)__pa(__end_rodata)));
}

#define TABLES 72

static uintptr_t tables[TABLES];
static unsigned table_count;
static unsigned ram_pages;
static unsigned device_pages;
static unsigned user_pages;

static uint64_t *table_pointer(uint64_t descriptor)
{
	uintptr_t address = descriptor & ~0xfffUL;
	CHECK((descriptor & 0xfff) == 3);
	CHECK(address >= __pa(_text) && address + PAGE_SIZE <= __pa(_end));
	CHECK(table_count < TABLES);
	for (unsigned i = 0; i < table_count; i++) {
		CHECK(tables[i] != address);
	}
	bool reserved = false;
	for (unsigned i = 0; i < memblock.reserved.cnt; i++) {
		struct memblock_region *region = &memblock.reserved.regions[i];
		reserved |= address >= region->base && address + PAGE_SIZE <= region->base + region->size;
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
	for (unsigned upper = 0; upper < 512; upper++) {
		uintptr_t block = base | ((uintptr_t)upper << 30);
		if (!overlaps(block, 1UL << 30)) {
			CHECK(root[upper] == 0);
			continue;
		}
		uint64_t *middle = table_pointer(root[upper]);
		for (unsigned i = 0; i < 512; i++) {
			uintptr_t section = block | ((uintptr_t)i << 21);
			if (!overlaps(section, 1UL << 21)) {
				CHECK(middle[i] == 0);
				continue;
			}
			uint64_t *leaf = table_pointer(middle[i]);
			for (unsigned page = 0; page < 512; page++) {
				uintptr_t address = section | ((uintptr_t)page << 12);
				uint64_t descriptor = expected(address);
				CHECK(leaf[page] == descriptor);
				check_translation(address + PAGE_SIZE - 1, descriptor);
				if (!descriptor) {
					continue;
				}
				check_wx(leaf[page]);
				if (descriptor & USER) {
					user_pages++;
				} else if (descriptor & 0x1c) {
					ram_pages++;
				} else {
					device_pages++;
				}
			}
		}
	}
}

static void mmu_selftest(void)
{
	static const uintptr_t holes[] = {
		0,
		PHYS_OFFSET,
		PHYS_OFFSET + TEXT_OFFSET,
		PHYS_OFFSET + RAM_SIZE - PAGE_SIZE,
		APP_FIRST - PAGE_SIZE,
		APP_FIRST + APP_COUNT * APP_SLOT_SIZE,
		0x08000000,
		0x09000000,
		1UL << 39,
		0xffff000000000000UL,
		PAGE_OFFSET - PAGE_SIZE,
		PAGE_OFFSET + RAM_SIZE,
		IO_OFFSET,
		IO_ADDRESS(0x08000000 - PAGE_SIZE),
		IO_ADDRESS(0x08010000),
		IO_ADDRESS(0x080a0000 - PAGE_SIZE),
		IO_ADDRESS(0x080c0000),
		IO_ADDRESS(0x09000000 - PAGE_SIZE),
		IO_ADDRESS(0x09001000),
	};
	uintptr_t user_pg_dir = read_sysreg(ttbr0_el1);
	uintptr_t swapper_pg_dir = read_sysreg(ttbr1_el1);
	CHECK(!((uintptr_t)_etext % PAGE_SIZE) && !((uintptr_t)__end_rodata % PAGE_SIZE));
	CHECK(inside((uintptr_t)start_kernel, _text, _etext));
	CHECK(inside((uintptr_t)holes, __start_rodata, __end_rodata));
	walk(user_pg_dir, 0);
	walk(swapper_pg_dir, 0xffffff8000000000UL);
	CHECK(table_count == TABLES);
	uintptr_t low = tables[0];
	uintptr_t high = tables[0];
	for (unsigned i = 0; i < table_count; i++) {
		if (tables[i] < low) {
			low = tables[i];
		}
		if (tables[i] > high) {
			high = tables[i];
		}
	}
	for (unsigned i = 0; i < sizeof(holes) / sizeof(holes[0]); i++) {
		check_translation(holes[i], 0);
	}
	printk("MMU TCR=%016lx MAIR=%016lx TTBR0=%016lx TTBR1=%016lx tables=%016lx-%016lx\n", read_sysreg(tcr_el1),
	       read_sysreg(mair_el1), user_pg_dir, swapper_pg_dir, low, high + PAGE_SIZE);
	printk("MMU SELFTEST OK ram=%u device=%u user=%u\n", ram_pages, device_pages, user_pages);
}

void __wrap_mem_init(void)
{
	__real_mem_init();
	mmu_selftest();
	mem_selftest();
}

[[noreturn]] void __wrap_start_kernel(void)
{
	string_selftest(__va(PHYS_OFFSET + RAM_SIZE));
	printk("STRING SELFTEST OK\n");
	__real_start_kernel();
}
