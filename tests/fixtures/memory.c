/* SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "kernel.h"
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
	unsigned long baseline = page_alloc_free_count();
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
	return READ_SYSREG(par_el1);
}

static void check_translation(uintptr_t address, bool mapped, bool user, unsigned attribute)
{
	for (unsigned access = 0; access < 4; ++access) {
		uint64_t result = translation(address, access);
		bool fault = !mapped || (access >= 2 && !user);
		CHECK((result & 1) == fault);
		if (!fault) {
			CHECK((result & 0x0000fffffffff000UL) == (address & ~0xfffUL));
			CHECK((result >> 56) == attribute);
			CHECK(((result >> 7) & 3) == 2);
		}
	}
}

static uintptr_t tables[69];
static unsigned table_count;

static uint64_t *table_pointer(uint64_t descriptor)
{
	uintptr_t address = descriptor & ~0xfffUL;
	CHECK((descriptor & 0xfff) == 3);
	CHECK(address >= (uintptr_t)_text && address + PAGE_SIZE <= (uintptr_t)_end);
	CHECK(table_count < 69);
	for (unsigned index = 0; index < table_count; ++index) {
		CHECK(tables[index] != address);
	}
	bool reserved = false;
	for (unsigned index = 0; index < memblock.reserved.count; ++index) {
		struct memblock_region *region = &memblock.reserved.regions[index];
		reserved |= address >= region->base &&
			    address + PAGE_SIZE <= region->base + region->size;
	}
	CHECK(reserved);
	tables[table_count++] = address;
	return (uint64_t *)address;
}

static void mmu_selftest(void)
{
	static const uintptr_t holes[] = {
		0, RAM_BASE - PAGE_SIZE, RAM_BASE + RAM_SIZE,
		0x08000000 - PAGE_SIZE, 0x08010000, 0x080a0000 - PAGE_SIZE,
		0x080c0000, 0x09000000 - PAGE_SIZE, 0x09001000,
		1UL << 39, 0xffffff8000000000UL,
	};
	uintptr_t root_address = READ_SYSREG(ttbr0_el1);
	CHECK(!(root_address & 0xfff));
	uint64_t *root = table_pointer(root_address | 3);
	unsigned ram_pages = 0;
	unsigned device_pages = 0;
	for (unsigned upper = 0; upper < 512; ++upper) {
		if (upper > 1) {
			CHECK(root[upper] == 0);
			continue;
		}
		uint64_t *middle = table_pointer(root[upper]);
		for (unsigned index = 0; index < 512; ++index) {
			bool present = upper == 1 ? index < 64 : index == 64 || index == 72;
			if (!present) {
				CHECK(middle[index] == 0);
				continue;
			}
			uint64_t *leaf = table_pointer(middle[index]);
			for (unsigned page = 0; page < 512; ++page) {
				uintptr_t address = ((uintptr_t)upper << 30) |
					((uintptr_t)index << 21) | ((uintptr_t)page << 12);
				bool ram = upper == 1;
				bool device = (address >= 0x08000000 && address < 0x08010000) ||
					(address >= 0x080a0000 && address < 0x080c0000) ||
					address == 0x09000000;
				bool user = address >= APP_FIRST &&
					address < APP_FIRST + APP_COUNT * APP_SLOT_SIZE;
				uint64_t expected = ram ? address | 0x707UL | (user ? 0x40UL : 0) :
					device ? address | 0x403UL | (3UL << 53) : 0;
				CHECK(leaf[page] == expected);
				check_translation(address + PAGE_SIZE - 1, ram || device,
						  user, ram ? 0x44 : 0);
				ram_pages += ram;
				device_pages += device;
			}
		}
	}
	CHECK(table_count == 69);
	uintptr_t low = tables[0];
	uintptr_t high = tables[0];
	for (unsigned index = 0; index < table_count; ++index) {
		if (tables[index] < low) low = tables[index];
		if (tables[index] > high) high = tables[index];
	}
	for (unsigned index = 0; index < sizeof(holes) / sizeof(holes[0]); ++index) {
		check_translation(holes[index], false, false, 0);
	}
	printk("MMU TCR=%016lx MAIR=%016lx TTBR0=%016lx TTBR1=%016lx tables=%016lx-%016lx\n",
	       READ_SYSREG(tcr_el1), READ_SYSREG(mair_el1), root_address,
	       READ_SYSREG(ttbr1_el1), low, high + PAGE_SIZE);
	printk("MMU SELFTEST OK ram=%u device=%u\n", ram_pages, device_pages);
}

void __wrap_mem_init(void)
{
	__real_mem_init();
	mmu_selftest();
	mem_selftest();
}

[[noreturn]] void __wrap_kernel_main(void)
{
	string_selftest((unsigned char *)(uintptr_t)(RAM_BASE + RAM_SIZE));
	printk("STRING SELFTEST OK\n");
	__real_kernel_main();
}