/*
 * SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "kernel.h"

#include <miaow/mm.h>
#include <miaow/pgtable.h>

/**
 * @brief Allocate one zeroed page from the buddy allocator.
 *
 * @return Linear-map address of the page, or null when no page is free.
 */
static void *get_zeroed_page(void)
{
	struct page *page = alloc_pages(0);

	if (!page) {
		return nullptr;
	}
	return memset(page_address(page), 0, PAGE_SIZE);
}

/**
 * @brief Return one page to the buddy allocator.
 *
 * @param address Linear-map address of the page.
 */
static void free_page(void *address)
{
	__free_pages(virt_to_page(address), 0);
}

/**
 * @brief Follow a table entry, allocating the next-level table if it is empty.
 *
 * @param entry L1 or L2 entry to follow or populate.
 * @return Linear-map address of the next-level table, or null on allocation failure.
 */
static uint64_t *table_alloc(uint64_t *entry)
{
	if (!*entry) {
		void *table = get_zeroed_page();

		if (!table) {
			return nullptr;
		}
		/* The walker must not see the table before its zeroes. */
		__asm__ volatile("dsb ishst" : : : "memory");
		*entry = __pa(table) | PMD_TYPE_TABLE;
	}
	return __va(*entry & PTE_ADDR_MASK);
}

/**
 * @brief Allocate an empty L1 root table.
 *
 * @param mm Address space that will own the root; not used yet.
 * @return Linear-map address of the root, or null on allocation failure.
 */
pgd_t *pgd_alloc([[maybe_unused]] struct mm_struct *mm)
{
	return get_zeroed_page();
}

/**
 * @brief Free a root and all L2/L3 tables below it.
 *
 * @details Mapped pages are not freed. The root must not be live in TTBR0.
 *
 * @param mm Address space that owns the root; not used yet.
 * @param pgd Root returned by pgd_alloc().
 */
void pgd_free([[maybe_unused]] struct mm_struct *mm, pgd_t *pgd)
{
	for (unsigned i = 0; i < PTRS_PER_PGD; i++) {
		if (!pgd[i]) {
			continue;
		}
		pmd_t *pmd = __va(pgd[i] & PTE_ADDR_MASK);
		for (unsigned j = 0; j < PTRS_PER_PMD; j++) {
			if (pmd[j]) {
				free_page(__va(pmd[j] & PTE_ADDR_MASK));
			}
		}
		free_page(pmd);
	}
	free_page(pgd);
}

/**
 * @brief Walk to the leaf entry for @p addr, allocating missing L2/L3 tables.
 *
 * @param mm Address space to walk.
 * @param addr Virtual address to translate.
 * @return Pointer to the L3 entry, or null on allocation failure.
 */
pte_t *pte_alloc(struct mm_struct *mm, uintptr_t addr)
{
	pmd_t *pmd = table_alloc(mm->pgd + pgd_index(addr));

	if (!pmd) {
		return nullptr;
	}
	pte_t *pte = table_alloc(pmd + pmd_index(addr));
	return pte ? pte + pte_index(addr) : nullptr;
}

/**
 * @brief Invalidate a leaf entry and flush its TLB entry.
 *
 * @param mm Address space that owns @p ptep; not used yet.
 * @param addr Virtual address mapped by @p ptep.
 * @param ptep Leaf entry to clear.
 */
void pte_clear([[maybe_unused]] struct mm_struct *mm, uintptr_t addr, pte_t *ptep)
{
	*ptep = 0;
	flush_tlb_page(addr);
}

/**
 * @brief Install a leaf entry and make it visible to the table walker.
 *
 * @details Break-before-make: a valid entry is cleared and flushed before it changes.
 *
 * @param mm Address space that owns @p ptep.
 * @param addr Virtual address mapped by @p ptep.
 * @param ptep Leaf entry to write.
 * @param pte New descriptor.
 */
void set_pte_at(struct mm_struct *mm, uintptr_t addr, pte_t *ptep, pte_t pte)
{
	if ((*ptep & PTE_VALID) && *ptep != pte) {
		pte_clear(mm, addr, ptep);
	}
	*ptep = pte;
	__asm__ volatile("dsb ishst\n\t"
			 "isb"
			 :
			 :
			 : "memory");
}

void switch_mm(struct mm_struct *mm)
{
	write_sysreg(__pa(mm->pgd), ttbr0_el1);
	__asm__ volatile("isb" : : : "memory");
	flush_tlb_all();
}
