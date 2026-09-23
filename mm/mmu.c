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

/*
 * clang-format off

 * VA translation: 39-bit addresses (TCR.T0SZ=25), 4 KiB granule, 3 levels.
 *
 *  63    39 38     30 29     21 20     12 11      0       63                   0
 * +--------+---------+---------+---------+---------+     +----------------------+
 * | 0 0..0 |   L1    |    L2   |    L3   | offset  |     |         PA           |
 * +--------+---------+---------+---------+---------+     +----------------------+
 * |        |         |         |        0| 0..4095 |     |0x08000000..0x08000FFF|
 * |        |         |         |        1| 0..4095 |     |0x08001000..0x08001FFF|
 * |        |         |         |       10| 0..4095 |     |0x08002000..0x08002FFF|
 * |        |         |         |       11| 0..4095 |     |0x08003000..0x08003FFF|
 * |        |         |         |      ...| 0..4095 |     |          ...         |
 * |        |         |001000000|     1111| 0..4095 |     |0x0800F000..0x0800FFFF|
 * |        |000000000|  gic[]  +---------+---------+     +----------------------+
 * |        | device[]|         |010100000| 0..4095 |     |0x080A0000..0x080a0FFF|
 * |        |         |         |010100001| 0..4095 |     |0x080A1000..0x080A1FFF|
 * |        |         |         |010100010| 0..4095 |     |0x080A2000..0x080A2FFF|
 * |        |         |         |010100011| 0..4095 |     |0x080A3000..0x080A3FFF|
 * |        |         |         |      ...| 0..4095 |     |          ...         |
 * |        |         |         |010111111| 0..4095 |     |0x080BF000..0x080BFFFF|
 * |        |         +---------+---------+---------+     |----------------------+
 * |        |         |001001000|000000000| 0..4095 |     |0x09000000..0x09000FFF|
 * |0000..00|         | uart[]  |         |         |     |                      |
 * |        +---------+---------+---------+---------+     +----------------------+
 * |        |         |         |        0| 0..4095 |     |0x40000000..0x40000FFF|
 * |        |         |         |        1| 0..4095 |     |0x40001000..0x40001FFF|
 * |        |         |    0    |       10| 0..4095 |     |0x40002000..0x40002FFF|
 * |        |         |         |       11| 0..4095 |     |0x40003000..0x40003FFF|
 * |        |         |         |      ...| 0..4095 |     |          ...         |
 * |        |         |         |111111111| 0..4095 |     |0x400FF000..0x400FFFFF|
 * |        |000000001+---------+---------+---------+     +----------------------+
 * |        | memory[]|   ...   |   ...   |   ...   |     |          ...         |
 * |        |         +---------+---------+---------+     +----------------------+
 * |        |         |         |        0| 0..4095 |     |0x47E00000..0x47E00FFF|
 * |        |         |         |        1| 0..4095 |     |0x47E01000..0x47E01FFF|
 * |        |         |000111111|       10| 0..4095 |     |0x47E02000..0x47E02FFF|
 * |        |         |         |       11| 0..4095 |     |0x47E03000..0x47E03FFF|
 * |        |         |         |      ...| 0..4095 |     |          ...         |
 * |        |         |         |111111111| 0..4095 |     |0x47FFF000..0x47FFFFFF|
 * +--------+---------+---------+---------+---------+     +----------------------+
 *
 * Table descriptor (L1/L2 entries, DESC_TABLE = 0b11):
 *  63                 48 47                       12 11   2 1 0
 * +---------------------+---------------------------+------+---+
 * | reserved / ignored  | next-level table PA[47:12]| res  |1 1|
 * +---------------------+---------------------------+------+---+
 *
 * Page descriptor (L3 entries, DESC_PAGE = 0b11):
 *  63   55 54  53  52     48 47                  12 11 10 9 8 7 6  5 4      2 1 0
 * +-------+---+---+---------+----------------------+--+--+---+---+--+--------+---+
 * | res   |UXN|PXN| res     | page PA[47:12]       |nG|AF|SH |AP |NS|AttrIndx|1 1|
 * +-------+---+---+---------+----------------------+--+--+---+---+--+--------+---+
 *                                                           
 *   AP[2:1]: 00 = RW at EL1 only (kernel RAM, devices)
 *            01 = RW at EL1 and EL0 (app slots; PTE_USER sets AP[1])
 *   SH = 11: requests inner-shareable; Normal-NC and Device memory are
 *        effectively outer-shareable regardless
 *   AttrIndx 0 = Device-nGnRnE (MAIR[0]=0x00), no share bits set
 *             1 = Normal non-cacheable (MAIR[1]=0x44), PTE_SHARED set
 *   UXN=PXN=1 on device pages: no execution at any EL; RAM stays
 *        executable everywhere in this stage (W^X comes later)
 *   AF = 1: access flag pre-set, hardware never needs to update it
 *   nG = 0: global entries, no ASID matching (single address space)
 *
 * clang-format on
 */
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

	/* slot 0: Device-nGnRnE, slot 1: Normal Outer+Inner Non-Cacheable */
	WRITE_SYSREG(mair_el1, 0x4400);

	/*
	 * 64 - 25 = 39 bits for TTBR0 and TTBR1 addressing,
	 * TTBR0 page tables: Normal memory, Inner+Outer Non-cacheable，Inner shareable, 4KB page,
	 * current ASID is from TTBR0, no TTBR1 (TTBR1 is not used and TTBR1 configurations are
	 * ignored),
	 * TTBR1 page tables: Normal memory, Inner+Outer Non-cacheable，Non-shareable, 4KB page,
	 * 32 bits for IPA addressing (IPA not used actually), 8-bit ASID
	 */
	WRITE_SYSREG(tcr_el1, 25UL | (3UL << 12) | (25UL << 16) | (1UL << 23) | (2UL << 30));

	/* 4 KB-aligned base: bits 11:0 are zero, so CnP (bit 0) is clear
	 * and ASID (bits 63:48) stays 0. */
	WRITE_SYSREG(ttbr0_el1, (uintptr_t)boot_tables.root);

	/* ttbr1 not used, ignored */
	WRITE_SYSREG(ttbr1_el1, 0);

	/* invalidate TLB */
	__asm__ volatile("isb\n\t"
			 "tlbi vmalle1\n\t"
			 "dsb sy\n\t"
			 "isb"
			 :
			 :
			 : "memory");

	/* enable MMU, disable align check, disable data & instruction cache, allow excute on
	 * writable memory */
	WRITE_SYSREG(sctlr_el1, (READ_SYSREG(sctlr_el1) &
				 ~((1UL << 1) | (1UL << 2) | (1UL << 12) | (1UL << 19))) |
					1UL);
	__asm__ volatile("isb" : : : "memory");
}
