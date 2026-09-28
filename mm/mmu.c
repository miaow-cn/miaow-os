/*
 * SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "kernel.h"
#include "abi.h"

#define PTRS_PER_PGD    512
#define PTRS_PER_PMD    512
#define PTRS_PER_PTE    512
#define PGDIR_SHIFT     30
#define PMD_SHIFT       21
#define RAM_TABLES      (RAM_SIZE >> PMD_SHIFT)
#define pgd_index(addr) (((uintptr_t)(addr) >> PGDIR_SHIFT) & (PTRS_PER_PGD - 1))
#define pmd_index(addr) (((uintptr_t)(addr) >> PMD_SHIFT) & (PTRS_PER_PMD - 1))
#define pte_index(addr) (((uintptr_t)(addr) >> PAGE_SHIFT) & (PTRS_PER_PTE - 1))
#define PMD_TYPE_TABLE  3UL
#define PTE_TYPE_PAGE   3UL
#define PTE_ATTRINDX(t) ((uint64_t)(t) << 2)
#define PTE_USER        (1UL << 6)
#define PTE_SHARED      (3UL << 8)
#define PTE_AF          (1UL << 10)
#define PTE_PXN         (1UL << 53)
#define PTE_UXN         (1UL << 54)
#define MT_DEVICE_nGnRnE 0
#define MT_NORMAL_NC     1
#define MAIR(attr, mt)   ((uint64_t)(attr) << ((mt) * 8))

/* clang-format off */

/*
 * translation: 4 KiB granule, 3 levels, 39-bit VA in both halves (T0SZ = T1SZ = 25).
 * PA: 32-bit addresses (TCR_EL1.IPS=0).
 *
 *  63    39 38     30 29     21 20     12 11      0
 * +--------+---------+---------+---------+---------+
 * | 0...0  |   L1    |    L2   |    L3   | offset  |   TTBR0: bits 63:39 all zeros
 * | 1...1  |  (pgd)  |  (pmd)  |  (pte)  |         |   TTBR1: bits 63:39 all ones
 * +--------+---------+---------+---------+---------+
 *
 * TBI0 = TBI1 = 0, so the top byte takes part in translation. Bit 55 selects TTBR0 or
 * TTBR1; any other value in bits 63:39 lies outside both ranges and faults with a
 * level 0 Translation fault without walking any table.
 *
 * TTBR1, kernel only (EL0 ---):
 * +----------------------------------------+------------------------+------------+---------+---------------+
 * | VA                                     | PA                     | Use        | EL1 RWX | Attribute     |
 * +----------------------------------------+------------------------+------------+---------+---------------+
 * | 0xffffff8008000000..0xffffff800800ffff | 0x08000000..0x0800ffff | GICD       |   rw-   | nGnRnE        |
 * | 0xffffff80080a0000..0xffffff80080bffff | 0x080a0000..0x080bffff | GICR + SGI |   rw-   | nGnRnE        |
 * | 0xffffff8009000000..0xffffff8009000fff | 0x09000000..0x09000fff | PL011      |   rw-   | nGnRnE        |
 * | 0xffffffc000000000..0xffffffc007ffffff | 0x40000000..0x47ffffff | linear map |   rwx   | in, out+in nc |
 * |   0xffffffc000080000..                 |   0x40080000..         |   kernel   |         |               |
 * +----------------------------------------+------------------------+------------+---------+---------------+
 *
 * TTBR0, VA = PA:
 * +----------------------------------------+------------------------+------------+---------+---------+
 * | VA                                     | PA                     | Use        | EL0 RWX | EL1 RWX |
 * +----------------------------------------+------------------------+------------+---------+---------+
 * | 0x41000000..0x4105ffff                 | 0x41000000..0x4105ffff | apps       |   rwx   |   rw-   |
 * | 2 MiB slots holding the kernel image   | same                   | boot only  |   ---   |   rwx   |
 * +----------------------------------------+------------------------+------------+---------+---------+
 *
 * Tables, [index]:
 *   TTBR1 swapper_pg_dir[0]   -> devices[64] -> gic[0..15], gic[160..191]
 *                                devices[72] -> uart[0]
 *         swapper_pg_dir[256] -> linear[0..63] -> ram[0..63][0..511]
 *   TTBR0 user_root[1]        -> user[8] -> apps[0..95]
 *                                user[0..] -> ram[0..]: identity map, borrowed from the
 *                                linear map and cleared by cpu_uninstall_idmap() before
 *                                kernel_main
 *
 * PAGE_OFFSET, IO_OFFSET and PHYS_OFFSET are 1 GiB aligned, so an address has the same L2
 *     and L3 index physically and in its high mapping.
 * RWX: r/w/x granted, '-' denied. App pages are writable at EL0, which makes them
 *     execute-never at EL1 whatever PXN says.
 * Share: non/in/out = Non-shareable / Inner Shareable / Outer Shareable. SH is ignored for
 *     Device and for Normal Inner+Outer Non-cacheable memory: both are always Outer
 *     Shareable, so "in" only records what the RAM descriptors request.
 * Device: nGnRnE = non-Gathering, non-Reordering, no Early write acknowledgement.
 * Normal: <out|in|out+in> <nc|wt|wb> [nt] [ra] [wa]; nc/wt/wb = Non-cacheable /
 *     Write-Through / Write-Back, nt = Non-transient, ra = Read-Allocate,
 *     wa = Write-Allocate.
 */

/* clang-format on */

static alignas(PAGE_SIZE) struct {
	uint64_t swapper_pg_dir[PTRS_PER_PGD];
	uint64_t devices[PTRS_PER_PMD];
	uint64_t linear[PTRS_PER_PMD];
	uint64_t gic[PTRS_PER_PTE];
	uint64_t uart[PTRS_PER_PTE];
	uint64_t ram[RAM_TABLES][PTRS_PER_PTE];
	uint64_t user_root[PTRS_PER_PGD];
	uint64_t user[PTRS_PER_PMD];
	uint64_t apps[PTRS_PER_PTE];
} boot_tables;

static void map_identity(bool present)
{
	for (uintptr_t index = pmd_index(_text); index <= pmd_index((uintptr_t)_end - 1);
	     ++index) {
		boot_tables.user[index] = present ? boot_tables.linear[index] : 0;
	}
}

static void map_device(uint64_t *table, uintptr_t start, uintptr_t end)
{
	for (uintptr_t address = start; address < end; address += PAGE_SIZE) {
		/*
		 * Device-nGnRnE, EL1 read/write only, execute-never at EL0 and EL1.
		 * AF preset because hardware management is off (TCR_EL1.HA=0).
		 */
		table[pte_index(address)] = address | PTE_TYPE_PAGE | PTE_AF |
					    PTE_ATTRINDX(MT_DEVICE_nGnRnE) | PTE_PXN | PTE_UXN;
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
	 * APTable: Hierarchical control of data access permissions, active because
	 *     TCR_EL1.HPD0 = HPD1 = 0.
	 *     00 - No effect on data access permissions; 01 - Removes UnprivRead and UnprivWrite;
	 *     10 - Removes UnprivWrite and PrivWrite; 11 - Removes UnprivRead, UnprivWrite, and
	 *     PrivWrite.
	 * UXNTable: Hierarchical control of the EL0 execute-never permission.
	 * PXNTable: Hierarchical control of the EL1 execute-never permission.
	 * next-level PA: The PA the next level table locates at. mmu_init() configures TCR_EL1.IPS
	 *     to 32-bit PA, PA[47:32] must be 0. Type: 0b11 for table descriptor
	 */
	/* clang-format on */
	/* Runs with the MMU off: PC-relative symbol addresses here are physical. */
	boot_tables.swapper_pg_dir[pgd_index(IO_OFFSET)] =
		(uintptr_t)boot_tables.devices | PMD_TYPE_TABLE;
	boot_tables.swapper_pg_dir[pgd_index(PAGE_OFFSET)] =
		(uintptr_t)boot_tables.linear | PMD_TYPE_TABLE;
	boot_tables.devices[pmd_index(0x08000000)] = (uintptr_t)boot_tables.gic | PMD_TYPE_TABLE;
	boot_tables.devices[pmd_index(0x09000000)] = (uintptr_t)boot_tables.uart | PMD_TYPE_TABLE;
	boot_tables.user_root[pgd_index(APP_FIRST)] = (uintptr_t)boot_tables.user | PMD_TYPE_TABLE;
	boot_tables.user[pmd_index(APP_FIRST)] = (uintptr_t)boot_tables.apps | PMD_TYPE_TABLE;

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
	 * FEAT_HPDS2 supported (ID_AA64MMFR1_EL1.HPDS=2), but TCR_EL1.HWU0nn = HWU1nn = 0 and
	 *     HPD0 = HPD1 = 0, so no page-based hardware attributes. FEAT_S1POE not supported
	 *     (ID_AA64MMFR3_EL1.S1POE=0), so [62:59] ignored.
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
	 * DBM: Dirty Bit Modifier; only acts when hardware dirty management is on (TCR_EL1.HD).
	 * GP: Indicates whether the memory region is guarded.
	 * nG: 0 - global entries, no ASID matching;
	 *     1 - non-global entries, ASID matching required.
	 * AF: Access Flag, set once the region has been accessed. With hardware management off,
	 *     an access through a descriptor whose AF is 0 takes an Access flag fault.
	 * SH: 00 - non-shareable; 01 - reserved; 10 - outer-shareable; 11 - inner-shareable
	 * AP: 00 - PrivRead, PrivWrite; 01 - PrivRead, PrivWrite, UnprivRead, UnprivWrite;
	 *     10 - PrivRead; 11 - PrivRead, UnprivRead
	 * AttrIndx: holds the value n used to select the 8-bit field MAIR_ELx.Attr<n>
	 *     that specifies the memory region attributes.
	 * Type: 0b11 for page descriptor
	 */
	/* clang-format on */

	map_device(boot_tables.gic, 0x08000000, 0x08010000);
	map_device(boot_tables.gic, 0x080a0000, 0x080c0000);
	map_device(boot_tables.uart, 0x09000000, 0x09001000);

	for (unsigned index = 0; index < RAM_TABLES; ++index) {
		boot_tables.linear[index] = (uintptr_t)boot_tables.ram[index] | PMD_TYPE_TABLE;
		for (unsigned page = 0; page < PTRS_PER_PTE; ++page) {
			uintptr_t address = PHYS_OFFSET + ((uintptr_t)index << PMD_SHIFT) +
					    ((uintptr_t)page << PAGE_SHIFT);

			/*
			 * AF preset because hardware management is off (TCR_EL1.HA=0),
			 * inner-shareable, Normal Outer+Inner Non-Cacheable, EL1 only
			 */
			boot_tables.ram[index][page] = address | PTE_TYPE_PAGE | PTE_AF |
						       PTE_SHARED | PTE_ATTRINDX(MT_NORMAL_NC);
		}
	}

	/* EL0 and EL1 can read and write, EL0 can execute */
	for (uintptr_t address = APP_FIRST; address < APP_FIRST + APP_COUNT * APP_SLOT_SIZE;
	     address += PAGE_SIZE) {
		boot_tables.apps[pte_index(address)] = address | PTE_TYPE_PAGE | PTE_AF | PTE_SHARED |
						       PTE_ATTRINDX(MT_NORMAL_NC) | PTE_USER;
	}
	map_identity(true);

	__asm__ volatile("dsb sy" : : : "memory");

	write_sysreg(MAIR(0x00, MT_DEVICE_nGnRnE) | MAIR(0x44, MT_NORMAL_NC), mair_el1);

	/*
	 * 64 - 25 = 39 bits of VA in each of TTBR0 and TTBR1
	 * TTBR0 and TTBR1 page tables: Normal memory, Inner+Outer Non-cacheable, Inner shareable,
	 *     4KB page; current ASID is from TTBR0
	 * EPD0 = EPD1 = 0: both halves are walked. TBI0 = TBI1 = 0: no top-byte ignore.
	 * HA = HD = 0: no hardware access flag or dirty state management.
	 * IPS: 32 bits for PA addressing
	 *     Cortex-A710 implements 40-bit PA(ID_AA64MMFR0_EL1.PARange = 0b0010) all current
	 *     addresses are below 4 GB, so 32-bit output is sufficient
	 * ASID: 8-bit
	 */
	write_sysreg(25UL | (3UL << 12) | (25UL << 16) | (3UL << 28) | (2UL << 30), tcr_el1);

	/* 4 KB-aligned base: bits 11:0 are zero, so CnP (bit 0) is clear
	 * and ASID (bits 63:48) stays 0. */
	write_sysreg((uintptr_t)boot_tables.user_root, ttbr0_el1);
	write_sysreg((uintptr_t)boot_tables.swapper_pg_dir, ttbr1_el1);

	/* invalidate TLB */
	__asm__ volatile("isb\n\t"
			 "tlbi vmalle1\n\t"
			 "dsb sy\n\t"
			 "isb"
			 :
			 :
			 : "memory");

	/* enable MMU, disable align check, disable data & instruction cache, allow execute on
	 * writable memory */
	write_sysreg((read_sysreg(sctlr_el1) &
		      ~((1UL << 1) | (1UL << 2) | (1UL << 12) | (1UL << 19))) |
			     1UL,
		     sctlr_el1);
	__asm__ volatile("isb" : : : "memory");
}

/* Called at the high address; afterwards TTBR0 holds only the application mapping. */
void cpu_uninstall_idmap(void)
{
	map_identity(false);
	__asm__ volatile("dsb sy\n\t"
			 "tlbi vmalle1\n\t"
			 "dsb sy\n\t"
			 "isb"
			 :
			 :
			 : "memory");
}
