/*
 * SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef _MEMORY_H
#define _MEMORY_H

/* Plain constants: the kernel linker script and assembly include this header too. */
#define PAGE_SHIFT  12
#define PAGE_SIZE   (1 << PAGE_SHIFT)
#define PHYS_OFFSET 0x40000000
#define RAM_SIZE    0x08000000
#define PAGE_OFFSET 0xffffffc000000000
#define IO_OFFSET   0xffffff8000000000
#define TEXT_OFFSET 0x80000

#define __pa(address)         ((uintptr_t)(address) - PAGE_OFFSET + PHYS_OFFSET)
#define __va(address)         ((void *)((uintptr_t)(address) - PHYS_OFFSET + PAGE_OFFSET))
#define IO_ADDRESS(address)   ((uintptr_t)(address) + IO_OFFSET)
#define page_address(page)    __va(page_to_phys(page))
#define virt_to_page(address) phys_to_page(__pa(address))

#endif /* _MEMORY_H */
