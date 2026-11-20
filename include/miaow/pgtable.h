/*
 * SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef _PGTABLE_H
#define _PGTABLE_H

#include <stdint.h>

#include <miaow/memory.h>

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
#define PTE_VALID        1UL
#define PTE_TYPE_PAGE    3UL
#define PTE_ATTRINDX(t)  ((uint64_t)(t) << 2)
#define PTE_USER         (1UL << 6) /* AP[1] */
#define PTE_RDONLY       (1UL << 7) /* AP[2] */
#define PTE_SHARED       (3UL << 8)
#define PTE_AF           (1UL << 10)
#define PTE_PXN          (1UL << 53)
#define PTE_UXN          (1UL << 54)
#define MT_DEVICE_nGnRnE 0
#define MT_NORMAL        1

/* Linux's arm64 protections without nG, DBM and software bits. */
#define PROT_DEFAULT       (PTE_TYPE_PAGE | PTE_AF | PTE_SHARED)
#define PROT_DEVICE_nGnRnE (PROT_DEFAULT | PTE_PXN | PTE_UXN | PTE_ATTRINDX(MT_DEVICE_nGnRnE))
#define PROT_NORMAL        (PROT_DEFAULT | PTE_PXN | PTE_UXN | PTE_ATTRINDX(MT_NORMAL))
#define _PAGE_DEFAULT      (PROT_DEFAULT | PTE_ATTRINDX(MT_NORMAL))

#define PAGE_KERNEL        PROT_NORMAL
#define PAGE_KERNEL_RO     (PAGE_KERNEL | PTE_RDONLY)
#define PAGE_KERNEL_ROX    ((PAGE_KERNEL & ~PTE_PXN) | PTE_RDONLY)
#define PAGE_SHARED        (_PAGE_DEFAULT | PTE_USER | PTE_PXN | PTE_UXN)
#define PAGE_READONLY_EXEC (_PAGE_DEFAULT | PTE_USER | PTE_RDONLY | PTE_PXN)

/* Grows into Linux's mm_struct with later milestones. */
struct mm_struct {
	pgd_t *pgd;
};

pgd_t *pgd_alloc(struct mm_struct *mm);
void pgd_free(struct mm_struct *mm, pgd_t *pgd);
pte_t *pte_alloc(struct mm_struct *mm, uintptr_t addr);
void set_pte_at(struct mm_struct *mm, uintptr_t addr, pte_t *ptep, pte_t pte);
void pte_clear(struct mm_struct *mm, uintptr_t addr, pte_t *ptep);
void switch_mm(struct mm_struct *mm);

#endif /* _PGTABLE_H */
