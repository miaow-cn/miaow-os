/*
 * SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "kernel.h"
#include "abi.h"

typedef uint64_t pgd_t;
typedef uint64_t pmd_t;
typedef uint64_t pte_t;
typedef uint64_t pgprot_t;

#define PTRS_PER_PGD     512
#define PTRS_PER_PMD     512
#define PTRS_PER_PTE     512
#define PGDIR_SHIFT      30
#define PMD_SHIFT        21
#define pgd_index(addr)  (((uintptr_t)(addr) >> PGDIR_SHIFT) & (PTRS_PER_PGD - 1))
#define pmd_index(addr)  (((uintptr_t)(addr) >> PMD_SHIFT) & (PTRS_PER_PMD - 1))
#define pte_index(addr)  (((uintptr_t)(addr) >> PAGE_SHIFT) & (PTRS_PER_PTE - 1))
#define PTE_ADDR_MASK    0x0000fffffffff000UL
#define PMD_TYPE_TABLE   3UL
#define PTE_TYPE_PAGE    3UL
#define PTE_ATTRINDX(t)  ((uint64_t)(t) << 2)
#define PTE_USER         (1UL << 6) /* AP[1] */
#define PTE_RDONLY       (1UL << 7) /* AP[2] */
#define PTE_SHARED       (3UL << 8)
#define PTE_AF           (1UL << 10)
#define PTE_PXN          (1UL << 53)
#define PTE_UXN          (1UL << 54)
#define MT_DEVICE_nGnRnE 0
#define MT_NORMAL_NC     1
#define MAIR(attr, mt)   ((uint64_t)(attr) << ((mt) * 8))

/* Linux's arm64 protections without nG, DBM and software bits; RAM stays non-cacheable for now. */
#define PROT_DEFAULT       (PTE_TYPE_PAGE | PTE_AF | PTE_SHARED)
#define PROT_DEVICE_nGnRnE (PROT_DEFAULT | PTE_PXN | PTE_UXN | PTE_ATTRINDX(MT_DEVICE_nGnRnE))
#define PROT_NORMAL_NC     (PROT_DEFAULT | PTE_PXN | PTE_UXN | PTE_ATTRINDX(MT_NORMAL_NC))
#define _PAGE_DEFAULT      (PROT_DEFAULT | PTE_ATTRINDX(MT_NORMAL_NC))

#define PAGE_KERNEL        PROT_NORMAL_NC
#define PAGE_KERNEL_RO     (PAGE_KERNEL | PTE_RDONLY)
#define PAGE_KERNEL_ROX    ((PAGE_KERNEL & ~PTE_PXN) | PTE_RDONLY)
#define PAGE_SHARED        (_PAGE_DEFAULT | PTE_USER | PTE_PXN | PTE_UXN)
#define PAGE_READONLY_EXEC (_PAGE_DEFAULT | PTE_USER | PTE_RDONLY | PTE_PXN)

/* L2 and L3 tables: IO (1 + 2), linear map (1 + one per 2 MiB of RAM), app slots (1 + 1). */
#define EARLY_PGTABLES (6 + (RAM_SIZE >> PMD_SHIFT))

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
 * TTBR1 (swapper_pg_dir), kernel only (EL0 ---):
 * +----------------------------------------+------------------------+------------+---------+---------------+
 * | VA                                     | PA                     | Use        | EL1 RWX | Attribute     |
 * +----------------------------------------+------------------------+------------+---------+---------------+
 * | 0xffffff8008000000..0xffffff800800ffff | 0x08000000..0x0800ffff | GICD       |   rw-   | nGnRnE        |
 * | 0xffffff80080a0000..0xffffff80080bffff | 0x080a0000..0x080bffff | GICR + SGI |   rw-   | nGnRnE        |
 * | 0xffffff8009000000..0xffffff8009000fff | 0x09000000..0x09000fff | PL011      |   rw-   | nGnRnE        |
 * | 0xffffffc000000000..0xffffffc007ffffff | 0x40000000..0x47ffffff | linear map |   rw-   | in, out+in nc |
 * |   _text.._etext                        |   0x40080000..         |   text     |   r-x   |               |
 * |   __start_rodata..__end_rodata         |                        |   rodata   |   r--   |               |
 * +----------------------------------------+------------------------+------------+---------+---------------+
 *
 * TTBR0 (user_pg_dir), VA = PA, one mapping shared by all apps, n = 0..2:
 * +----------------------------------------+------------+---------+---------+
 * | VA                                     | Use        | EL0 RWX | EL1 RWX |
 * +----------------------------------------+------------+---------+---------+
 * | 0x41000000 + n * 0x20000, 64 KiB       | app image  |   r-x   |   r--   |
 * | 0x4101c000 + n * 0x20000, 16 KiB       | app stack  |   rw-   |   rw-   |
 * +----------------------------------------+------------+---------+---------+
 * The 48 KiB between image and stack stays unmapped as a guard.
 *
 * While the MMU is being enabled, TTBR0 holds idmap_pg_dir instead. Its only entry is the
 * linear map's L1 entry, placed at the index of PHYS_OFFSET: PAGE_OFFSET and PHYS_OFFSET
 * are 1 GiB aligned, so the same L2 and L3 tables translate all of RAM to itself.
 * cpu_uninstall_idmap() then installs user_pg_dir.
 *
 * The linear map is the only mapping of kernel text and read-only data, so restricting
 * those segments there leaves no writable alias.
 * RWX: r/w/x granted, '-' denied.
 * Share: non/in/out = Non-shareable / Inner Shareable / Outer Shareable. SH is ignored for
 *     Device and for Normal Inner+Outer Non-cacheable memory: both are always Outer
 *     Shareable, so "in" only records what the RAM descriptors request.
 * Device: nGnRnE = non-Gathering, non-Reordering, no Early write acknowledgement.
 * Normal: <out|in|out+in> <nc|wt|wb> [nt] [ra] [wa]; nc/wt/wb = Non-cacheable /
 *     Write-Through / Write-Back, nt = Non-transient, ra = Read-Allocate,
 *     wa = Write-Allocate.
 */

/* clang-format on */

static alignas(PAGE_SIZE) pgd_t swapper_pg_dir[PTRS_PER_PGD];
static alignas(PAGE_SIZE) pgd_t idmap_pg_dir[PTRS_PER_PGD];
static alignas(PAGE_SIZE) pgd_t user_pg_dir[PTRS_PER_PGD];
static alignas(PAGE_SIZE) uint64_t early_pgtables[EARLY_PGTABLES][PTRS_PER_PTE];
static unsigned early_pgtables_used;

/*
 * All but cpu_uninstall_idmap() run with the MMU off: PC-relative symbol addresses are
 * physical, and a table's physical address is also a pointer to it.
 */
static uintptr_t early_pgtable_alloc(void)
{
	return (uintptr_t)early_pgtables[early_pgtables_used++];
}

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
 *
 * Table descriptors leave all hierarchical controls at 0: permissions come from pages only.
 */
/* clang-format on */
static pte_t *walk_to_pte(pgd_t *pgdir, uintptr_t addr)
{
	pgd_t *pgdp = pgdir + pgd_index(addr);
	if (!*pgdp) {
		*pgdp = early_pgtable_alloc() | PMD_TYPE_TABLE;
	}
	pmd_t *pmdp = (pmd_t *)(*pgdp & PTE_ADDR_MASK) + pmd_index(addr);
	if (!*pmdp) {
		*pmdp = early_pgtable_alloc() | PMD_TYPE_TABLE;
	}
	return (pte_t *)(*pmdp & PTE_ADDR_MASK) + pte_index(addr);
}

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
 *     an access through a descriptor whose AF is 0 takes an Access flag fault, so
 *     PROT_DEFAULT presets it.
 * SH: 00 - non-shareable; 01 - reserved; 10 - outer-shareable; 11 - inner-shareable
 * AP: 00 - PrivRead, PrivWrite; 01 - PrivRead, PrivWrite, UnprivRead, UnprivWrite;
 *     10 - PrivRead; 11 - PrivRead, UnprivRead
 *     AP[2] is PTE_RDONLY and AP[1] is PTE_USER.
 * AttrIndx: holds the value n used to select the 8-bit field MAIR_ELx.Attr<n>
 *     that specifies the memory region attributes.
 * Type: 0b11 for page descriptor
 */
/* clang-format on */
static void create_pgd_mapping(pgd_t *pgdir, uintptr_t phys, uintptr_t virt, size_t size, pgprot_t prot)
{
	for (size_t offset = 0; offset < size; offset += PAGE_SIZE) {
		*walk_to_pte(pgdir, virt + offset) = (phys + offset) | prot;
	}
}

/* Overwrites the segment's linear map pages, which are its only mapping. */
static void map_kernel_segment(const char *start, const char *end, pgprot_t prot)
{
	create_pgd_mapping(swapper_pg_dir, (uintptr_t)start, (uintptr_t)__va(start), (size_t)(end - start), prot);
}

void mmu_init(void)
{
	create_pgd_mapping(swapper_pg_dir, PHYS_OFFSET, PAGE_OFFSET, RAM_SIZE, PAGE_KERNEL);
	map_kernel_segment(_text, _etext, PAGE_KERNEL_ROX);
	map_kernel_segment(__start_rodata, __end_rodata, PAGE_KERNEL_RO);
	create_pgd_mapping(swapper_pg_dir, 0x08000000, IO_ADDRESS(0x08000000), 0x10000, PROT_DEVICE_nGnRnE);
	create_pgd_mapping(swapper_pg_dir, 0x080a0000, IO_ADDRESS(0x080a0000), 0x20000, PROT_DEVICE_nGnRnE);
	create_pgd_mapping(swapper_pg_dir, 0x09000000, IO_ADDRESS(0x09000000), PAGE_SIZE, PROT_DEVICE_nGnRnE);

	for (uintptr_t slot = APP_FIRST; slot < APP_FIRST + APP_COUNT * APP_SLOT_SIZE; slot += APP_SLOT_SIZE) {
		uintptr_t stack = slot + APP_SLOT_SIZE - APP_STACK_SIZE;

		create_pgd_mapping(user_pg_dir, slot, slot, APP_IMAGE_SIZE, PAGE_READONLY_EXEC);
		create_pgd_mapping(user_pg_dir, stack, stack, APP_STACK_SIZE, PAGE_SHARED);
	}

	/* The linear map's L2 table, placed at the PHYS_OFFSET index, maps all RAM to itself. */
	idmap_pg_dir[pgd_index(PHYS_OFFSET)] = swapper_pg_dir[pgd_index(PAGE_OFFSET)];

	__asm__ volatile("dsb sy" : : : "memory");

	write_sysreg(MAIR(0x00, MT_DEVICE_nGnRnE) | MAIR(0x44, MT_NORMAL_NC), mair_el1);

	/*
	 * 64 - 25 = 39 bits of VA in each of TTBR0 and TTBR1
	 * TTBR0 and TTBR1 page tables: Normal memory, Inner+Outer Non-cacheable, Inner shareable,
	 *     4KB page; current ASID is from TTBR0
	 * EPD0 = EPD1 = 0: both halves are walked.
	 * TBI0 = TBI1 = 0: no top-byte ignore.
	 * HA = HD = 0: no hardware access flag or dirty state management.
	 * IPS: 32 bits for PA addressing
	 *     Cortex-A710 implements 40-bit PA(ID_AA64MMFR0_EL1.PARange = 0b0010) all current
	 *     addresses are below 4 GB, so 32-bit output is sufficient
	 * ASID: 8-bit
	 */
	write_sysreg(25UL | (3UL << 12) | (25UL << 16) | (3UL << 28) | (2UL << 30), tcr_el1);

	/* 4 KB-aligned base: bits 11:0 are zero, so CnP (bit 0) is clear
	 * and ASID (bits 63:48) stays 0. */
	write_sysreg((uintptr_t)idmap_pg_dir, ttbr0_el1);
	write_sysreg((uintptr_t)swapper_pg_dir, ttbr1_el1);

	/* invalidate TLB */
	__asm__ volatile("isb\n\t"
			 "tlbi vmalle1\n\t"
			 "dsb sy\n\t"
			 "isb"
			 :
			 :
			 : "memory");

	/* enable MMU, disable align check, disable data & instruction cache; WXN stays clear as
	 * in Linux, since no page is both writable and executable anyway */
	write_sysreg((read_sysreg(sctlr_el1) & ~((1UL << 1) | (1UL << 2) | (1UL << 12) | (1UL << 19))) | 1UL,
		     sctlr_el1);
	__asm__ volatile("isb" : : : "memory");
}

/* Called at the high address, where symbols are virtual. */
void cpu_uninstall_idmap(void)
{
	write_sysreg(__pa(user_pg_dir), ttbr0_el1);
	__asm__ volatile("isb\n\t"
			 "tlbi vmalle1\n\t"
			 "dsb sy\n\t"
			 "isb"
			 :
			 :
			 : "memory");
}
