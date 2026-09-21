/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (C) 1991, 1992, 1993, 1994  Linus Torvalds
 *
 * Imported from Linux v2.6.12 mm/page_alloc.c (GPL-2.0-only); see
 * docs/third-party.md for the third-party inventory. That revision predates
 * migrate types and is much closer to the plain buddy algorithm than current
 * code. Ported to this freestanding tree: one implicit zone instead of
 * pgdat/zonelists, no GFP flags, watermarks, per-CPU page lists, reclaim,
 * compound pages, page reference counts or bootmem interoperation; the free
 * block state lives in explicit struct page fields instead of PG_private and
 * page->private, and buddies are computed from the absolute page frame number
 * rather than the MAX_ORDER-masked index. The algorithm is unchanged.
 */

#ifndef _MM_H
#define _MM_H

#include <stddef.h>
#include <stdint.h>

#include "abi.h"
#include <miaow/list.h>

#define MAX_ORDER 11

#define PAGE_BUDDY 0x1u
#define PAGE_SLAB  0x2u

struct kmem_cache;

struct page {
	struct list_head list;
	unsigned flags;
	unsigned order;
	/* Used only while the page backs a slab. */
	void *freelist;
	struct kmem_cache *cache;
	unsigned inuse;
};

struct free_area {
	struct list_head list;
	unsigned long count;
};

void page_alloc_init(uintptr_t base, unsigned long pages, struct page *map);
void page_alloc_free_range(uintptr_t start, uintptr_t end);
unsigned long page_alloc_free_count(void);
struct page *alloc_pages(unsigned order);
void free_pages(struct page *page, unsigned order);
uintptr_t page_to_phys(const struct page *page);
struct page *phys_to_page(uintptr_t address);

#endif /* _MM_H */
