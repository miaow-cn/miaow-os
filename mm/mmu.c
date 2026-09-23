/*
 * SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "kernel.h"
#include "abi.h"

#define TABLE_ENTRIES 512
#define L2_SHIFT      21
#define RAM_TABLES    (RAM_SIZE >> L2_SHIFT)
#define DESC_TABLE    3UL
#define DESC_PAGE     3UL
#define PTE_USER      (1UL << 6)
#define PTE_SHARED    (3UL << 8)
#define PTE_AF        (1UL << 10)
#define PTE_PXN       (1UL << 53)
#define PTE_UXN       (1UL << 54)
#define PTE_NORMAL    (1UL << 2)

static alignas(PAGE_SIZE) struct {
	uint64_t root[TABLE_ENTRIES];
	uint64_t devices[TABLE_ENTRIES];
	uint64_t memory[TABLE_ENTRIES];
	uint64_t gic[TABLE_ENTRIES];
	uint64_t uart[TABLE_ENTRIES];
	uint64_t ram[RAM_TABLES][TABLE_ENTRIES];
} boot_tables;

static void map_device(uint64_t *table, uintptr_t start, uintptr_t end)
{
	for (uintptr_t address = start; address < end; address += PAGE_SIZE) {
		table[(address >> PAGE_SHIFT) % TABLE_ENTRIES] =
			address | DESC_PAGE | PTE_AF | PTE_PXN | PTE_UXN;
	}
}

void mmu_init(void)
{
	boot_tables.root[0] = (uintptr_t)boot_tables.devices | DESC_TABLE;
	boot_tables.root[RAM_BASE >> 30] = (uintptr_t)boot_tables.memory | DESC_TABLE;
	boot_tables.devices[0x08000000 >> L2_SHIFT] = (uintptr_t)boot_tables.gic | DESC_TABLE;
	boot_tables.devices[0x09000000 >> L2_SHIFT] = (uintptr_t)boot_tables.uart | DESC_TABLE;
	map_device(boot_tables.gic, 0x08000000, 0x08010000);
	map_device(boot_tables.gic, 0x080a0000, 0x080c0000);
	map_device(boot_tables.uart, 0x09000000, 0x09001000);

	for (unsigned index = 0; index < RAM_TABLES; ++index) {
		boot_tables.memory[index] = (uintptr_t)boot_tables.ram[index] | DESC_TABLE;
		for (unsigned page = 0; page < TABLE_ENTRIES; ++page) {
			uintptr_t address = RAM_BASE + ((uintptr_t)index << L2_SHIFT) +
					    ((uintptr_t)page << PAGE_SHIFT);
			uint64_t flags = DESC_PAGE | PTE_AF | PTE_SHARED | PTE_NORMAL;

			if (address >= APP_FIRST &&
			    address < APP_FIRST + APP_COUNT * APP_SLOT_SIZE) {
				flags |= PTE_USER;
			}
			boot_tables.ram[index][page] = address | flags;
		}
	}

	__asm__ volatile("dsb sy" : : : "memory");
	WRITE_SYSREG(mair_el1, 0x4400);
	WRITE_SYSREG(tcr_el1, 25UL | (3UL << 12) | (25UL << 16) | (1UL << 23) | (2UL << 30));
	WRITE_SYSREG(ttbr0_el1, (uintptr_t)boot_tables.root);
	WRITE_SYSREG(ttbr1_el1, 0);
	__asm__ volatile("isb\n\ttlbi vmalle1\n\tdsb sy\n\tisb" : : : "memory");
	WRITE_SYSREG(sctlr_el1,
		     (READ_SYSREG(sctlr_el1) &
		      ~((1UL << 1) | (1UL << 2) | (1UL << 12) | (1UL << 19))) | 1UL);
	__asm__ volatile("isb" : : : "memory");
}

