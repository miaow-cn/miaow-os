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

/* clang-format off */

/*
 * translation: 4 KiB granule, 3 levels.
 * VA: 39-bit addresses (TCR.T0SZ=25).
 * PA: 32-bit addresses (TCR_EL1.IPS=0).
 *
 *  63    39 38     30 29     21 20     12 11      0     39    32 31                   0
 * +--------+---------+---------+---------+---------+   +--------+----------------------+  +-------------+---------+---------+----+-------+--------+-------------+
 * | 0 0..0 |   L1    |    L2   |    L3   | offset  |   | 0 0..0 |         PA           |  |     VA      | EL0 RWX | EL1 RWX | nG | Share | Device | Normal      |
 * +--------+---------+---------+---------+---------+   +--------+----------------------+  +-------------+---------+---------+----+-------+--------+-------------+
 * |        |         |         |        0| 0..4095 |   |        |0x08000000..0x08000FFF|  |             |         |         |    |       |        |             |
 * |        |         |         |        1| 0..4095 |   |        |0x08001000..0x08001FFF|  |             |         |         |    |       |        |             |
 * |        |         |         |       10| 0..4095 |   |        |0x08002000..0x08002FFF|  |             |         |         |    |       |        |             |
 * |        |         |         |       11| 0..4095 |   |        |0x08003000..0x08003FFF|  |             |         |         |    |       |        |             |
 * |        |         |         |      ...| 0..4095 |   |        |          ...         |  |             |         |         |    |       |        |             |
 * |        |         |001000000|     1111| 0..4095 |   |        |0x0800F000..0x0800FFFF|  |             |         |         |    |       |        |             |
 * |        |000000000|  gic[]  +---------+---------+   |        +----------------------+  | devices     |   ---   |   rw-   | 0  |  non  | nGnRnE |      -      |
 * |        | device[]|         |010100000| 0..4095 |   |        |0x080A0000..0x080a0FFF|  |             |         |         |    |       |        |             |
 * |        |         |         |010100001| 0..4095 |   |        |0x080A1000..0x080A1FFF|  |             |         |         |    |       |        |             |
 * |        |         |         |010100010| 0..4095 |   |        |0x080A2000..0x080A2FFF|  |             |         |         |    |       |        |             |
 * |        |         |         |010100011| 0..4095 |   |        |0x080A3000..0x080A3FFF|  |             |         |         |    |       |        |             |
 * |        |         |         |      ...| 0..4095 |   |        |          ...         |  |             |         |         |    |       |        |             |
 * |        |         |         |010111111| 0..4095 |   |        |0x080BF000..0x080BFFFF|  |             |         |         |    |       |        |             |
 * |        |         +---------+---------+---------+   |        |----------------------+  |             |         |         |    |       |        |             |
 * |        |         |001001000|000000000| 0..4095 |   |        |0x09000000..0x09000FFF|  |             |         |         |    |       |        |             |
 * |0000..00|         | uart[]  |         |         |   |   0    |                      |  |             |         |         |    |       |        |             |
 * |        +---------+---------+---------+---------+   |        +----------------------+  +-------------+---------+---------+----+-------+--------+-------------+
 * |        |         |         |        0| 0..4095 |   |        |0x40000000..0x40000FFF|  | 0x00000000  |         |         |    |       |        |             |
 * |        |         |         |        1| 0..4095 |   |        |0x40001000..0x40001FFF|  |             |         |         |    |       |        |             |
 * |        |         |    0    |       10| 0..4095 |   |        |0x40002000..0x40002FFF|  |             |         |         |    |       |        |             |
 * |        |         |         |       11| 0..4095 |   |        |0x40003000..0x40003FFF|  | Kernel      |   ---   |   rwx   | 0  |  in   |   -    |  out+in nc  |
 * |        |         |         |      ...| 0..4095 |   |        |          ...         |  |             |         |         |    |       |        |             |
 * |        |         |         |111111111| 0..4095 |   |        |0x400FF000..0x400FFFFF|  |             |         |         |    |       |        |             |
 * |        |000000001+---------+---------+---------+   |        +----------------------+  | 0x40FFFFFF  |         |         |    |       |        |             |
 * |        | memory[]|   ...   |   ...   |   ...   |   |        |          ...         |  +-------------+---------+---------+----+-------+--------+-------------+
 * |        |         |         |         |         |   |        |                      |  | 0x41000000  |         |         |    |       |        |             |
 * |        |         |         |         |         |   |        |                      |  |             |         |         |    |       |        |             |
 * |        |         |         |         |         |   |        |                      |  | Apps        |   rwx   |   rwx   | 0  |  in   |   -    |  out+in nc  |
 * |        |         |         |         |         |   |        |                      |  |             |         |         |    |       |        |             |
 * |        |         |         |         |         |   |        |                      |  | 0x4105FFFF  |         |         |    |       |        |             |
 * |        |         |         |         |         |   |        |                      |  +-------------+---------+---------+----+-------+--------+-------------+
 * |        |         |         |         |         |   |        |                      |  | 0x41060000  |         |         |    |       |        |             |
 * |        |         +---------+---------+---------+   |        +----------------------+  |             |         |         |    |       |        |             |
 * |        |         |         |        0| 0..4095 |   |        |0x47E00000..0x47E00FFF|  |             |         |         |    |       |        |             |
 * |        |         |         |        1| 0..4095 |   |        |0x47E01000..0x47E01FFF|  | Kernel Heap |   ---   |   rwx   | 0  |  in   |   -    |  out+in nc  |
 * |        |         |000111111|       10| 0..4095 |   |        |0x47E02000..0x47E02FFF|  |             |         |         |    |       |        |             |
 * |        |         |         |       11| 0..4095 |   |        |0x47E03000..0x47E03FFF|  |             |         |         |    |       |        |             |
 * |        |         |         |      ...| 0..4095 |   |        |          ...         |  |             |         |         |    |       |        |             |
 * |        |         |         |111111111| 0..4095 |   |        |0x47FFF000..0x47FFFFFF|  | 0x47FFFFFF  |         |         |    |       |        |             |
 * +--------+---------+---------+---------+---------+   +--------+----------------------+  +-------------+---------+---------+----+-------+--------+-------------+
 *
 * devices: gic[] and uart[]. RAM, apps: 0x41000000..0x4105FFFF, the APP_COUNT slots.
 * RWX: r/w/x granted, '-' denied. App pages are writable at EL0, which makes them
 *     execute-never at EL1 whatever PXN says.
 * Share: non/in/out = Non-shareable / Inner Shareable / Outer Shareable. Device memory is
 *     always Outer Shareable, so SH is ignored on the device pages.
 * Device: nGnRnE = non-Gathering, non-Reordering, no Early write acknowledgement.
 * Normal: <out|in|out+in> <nc|wt|wb> [nt] [ra] [wa]; nc/wt/wb = Non-cacheable /
 *     Write-Through / Write-Back, nt = Non-transient, ra = Read-Allocate,
 *     wa = Write-Allocate.
 */

/* clang-format on */

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
		/*
		 * Forbid execution at EL0 and EL1. AF not used: TCR_EL1.HA=0
		 */
		table[(address >> PAGE_SHIFT) % TABLE_ENTRIES] =
			address | DESC_PAGE | PTE_AF | PTE_PXN | PTE_UXN;
	}
}

void mmu_init(void)
{
	/* clang-format off */
	/*
	 * Table descriptor (L1/L2 entries, 4KB granule 48-bit OA):
	 *  63    62   61  60    59   58    51 50  48 47          12 11     2 1  0
	 * +-----+-------+-----+-----+--------+------+--------------+--------+----+
	 * |NS   |AP     |UXN  |PXN  |IGNORED | RES0 |next-level    |IGNORED |Type|
	 * |Table|Table  |Table|Table|        |      |PA[47:12]     |        |    |
	 * +-----+-------+-----+-----+--------+------+--------------+--------+----+
	 *
	 * NSTable: only meaningful in Secure state; qemu secure=off, so [63] is ignored.
	 * APTable: Hierarchical control of data access permissions, active because TCR_EL1.HPD0=0.
	 *     00 - No effect on data access permissions; 01 - Removes UnprivRead and UnprivWrite;
	 *     10 - Removes UnprivWrite and PrivWrite; 11 - Removes UnprivRead, UnprivWrite, and
	 * PrivWrite UXNTable: Hierarchical control of the EL0 execute-never permission. 
	 * PXNTable: Hierarchical control of the EL1 execute-never permission.
	 * next-level PA: The PA the next level table locates at. mmu_init() configures TCR_EL1.IPS
	 *     to 32-bit PA, PA[47:32] must be 0. Type: 0b11 for table descriptor
	 */
	/* clang-format on */
	boot_tables.root[0] = (uintptr_t)boot_tables.devices | DESC_TABLE;
	boot_tables.root[RAM_BASE >> 30] = (uintptr_t)boot_tables.memory | DESC_TABLE;
	boot_tables.devices[0x08000000 >> L2_SHIFT] = (uintptr_t)boot_tables.gic | DESC_TABLE;
	boot_tables.devices[0x09000000 >> L2_SHIFT] = (uintptr_t)boot_tables.uart | DESC_TABLE;

	/* clang-format off */
	/*
	 * Page descriptor (L3 entries, 4KB granule 48-bit OA):
	 *  63  62   59 58 55 54  53   52   51  50
	 * +---+-------+-----+---+---+-----+---+--+
	 * |Sw |IGNORED|Sw   |UXN|PXN|Conti|DBM|GP|
	 * |USE|       |USE  |   |   |guous|   |  |
	 * +---+-------+-----+---+---+-----+---+--+
	 *
	 * 49  48 47       12     11 10 9 8 7 6  5   4  2 1  0
	 * +-----+----------+    +--+--+---+---+----+----+----+
	 * |RES0 |page      |    |nG|AF|SH |AP |RES0|Attr|Type|
	 * |     |PA[47:12] |    |  |  |   |   |    |Indx|    |
	 * +-----+----------+    +--+--+---+---+----+----+----+
	 *
	 * Non-secure state, so [63] is reserved for software use.
	 * FEAT_HPDS2 supported (ID_AA64MMFR1_EL1.HPDS2=2) and enabled (TCR_EL1.HPDx=0), but
	 *     TCR_EL1.HWUxxx=0. FEAT_S1POE not supported (ID_AA64MMFR3_EL1.S1POE=0),
	 *     so [62:59] ignored.
	 * qemu virtualization=off, no EL2, no HCR_EL2. FEAT_S1PIE not supported
	 *     (ID_AA64MMFR3_EL1.S1PIE=0), no indirect permissions. so [54:53] is {UXN,PXN},
	 * FEAT_THE not supported (ID_AA64PFR1_EL1.THE=0), [52] is contiguous.
	 * FEAT_HAFDBS supported (ID_AA64MMFR1_EL1.HAFDBS=2), so [51] is Dirty bit modifier (DBM).
	 * FEAT_BTI is supported (ID_AA64PFR1_EL1.BT=1), [50] is Guarded Page (GP).
	 * TCR_ELx.DS=0, [49:48] is RES0.
	 * EL1&0 translation, so [11] is Not global (nG).
	 * 4KB page and TCR_ELx.DS=0, [9:8] is Shareability (SH).
	 * EL1&0 translation and FEAT_S1PIE not supported (ID_AA64MMFR3_EL1.S1PIE=0),
	 *     no indirect permissions, so [7:6] is AP.
	 * EL1&0 and Non-secure state, so [5] is RES0.
	 *
	 *
	 * SW USE: Reserved for software use.
	 * UXN: Unprivileged (EL0) Execute Never.
	 * PXN: Privileged (EL1) Execute Never.
	 * Contiguous: Identifies a descriptor as belonging to a group of adjacent
	 *     translation table entries that point to a contiguous OA range.
	 * DBM: Whether dirty state hardware management is enabled.
	 * GP: Indicates whether the memory region is guarded.
	 * nG: 0 - global entries, no ASID matching;
	 *     1 - non-global entries, ASID matching required.
	 * AF: Indicates whether the memory region has not been accessed since the value of AF
	 *     was last set to 0.
	 * SH: 00 - non-shareable; 01 - reserved; 10 - outer-shareable; 11 - inner-shareable
	 * AP: 00 - PrivRead, PrivWrite; 01 - PrivRead, PrivWrite, UnprivRead, UnprivWrite;
	 *     10 - PrivRead; 11 - PrivRead, UnprivRead
	 * AttrIndx: holds the value n used to select the 8-bit field MAIR_ELx.Attr<n>
	 *     that specifies the memory region attributest.
	 * Type: 0b11 for page descriptor
	 */
	/* clang-format on */

	map_device(boot_tables.gic, 0x08000000, 0x08010000);
	map_device(boot_tables.gic, 0x080a0000, 0x080c0000);
	map_device(boot_tables.uart, 0x09000000, 0x09001000);

	for (unsigned index = 0; index < RAM_TABLES; ++index) {
		boot_tables.memory[index] = (uintptr_t)boot_tables.ram[index] | DESC_TABLE;
		for (unsigned page = 0; page < TABLE_ENTRIES; ++page) {
			uintptr_t address = RAM_BASE + ((uintptr_t)index << L2_SHIFT) +
					    ((uintptr_t)page << PAGE_SHIFT);

			/*
			 * AF preset because hardware management is off (TCR_EL1.HA=0),
			 * inner-shareable, Normal Outer+Inner Non-Cacheable
			 */
			uint64_t flags = DESC_PAGE | PTE_AF | PTE_SHARED | PTE_NORMAL;

			/* EL0 and EL1 can read and wrsite */
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
	 * 64 - 25 = 39 bits of VA for EL0/EL1 translation
	 * TTBR0 page tables: Normal memory, Inner+Outer Non-cacheable, Inner shareable, 4KB page
	 *     current ASID is from TTBR0, no TTBR1
	 * TTBR1 page tables (not used actually): Normal memory, Inner+Outer Non-cacheable,
	 *     Non-shareable, 4KB page
	 * IPS: 32 bits for PA addressing
	 *     Cortex-A710 implements 40-bit PA(ID_AA64MMFR0_EL1.PARange = 0b0010) all current
	 *     addresses are below 4 GB, so 32-bit output is sufficient
	 * ASID: 8-bit
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
