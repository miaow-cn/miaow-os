/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (C) 2007 SGI, Christoph Lameter
 *
 * Derived from the initial SLUB implementation in Linux commit
 * 81819f0fc8285a2a5a921c019e3e3d7b6169d225 ("SLUB core"), mm/slub.c
 * (GPL-2.0-only); see docs/third-party.md for the third-party inventory.
 * Ported to this freestanding tree: no per-CPU slab,
 * locking, NUMA nodes, debug or poisoning options, sysfs, cache merging or
 * cache creation by name; one page per slab; the size classes are the only
 * caches. The free pointer is stored at offset zero inside a free object and
 * slabs move between the partial list and the page allocator exactly as the
 * original does.
 */

#include <miaow/mm.h>
#include <miaow/slab.h>
#include <miaow/string.h>

static struct kmem_cache kmalloc_caches[KMALLOC_CACHES];

void slab_init(void)
{
	for (unsigned index = 0; index < KMALLOC_CACHES; ++index) {
		struct kmem_cache *cache = &kmalloc_caches[index];

		cache->size = KMALLOC_MIN_SIZE << index;
		cache->objects = PAGE_SIZE / cache->size;
		INIT_LIST_HEAD(&cache->partial);
	}
}

static void **free_pointer(void *object)
{
	return object;
}

/* Carves a fresh page into objects chained through their own free pointers. */
static struct page *new_slab(struct kmem_cache *cache)
{
	struct page *page = alloc_pages(0);
	char *start;
	char *last;

	if (!page) {
		return nullptr;
	}
	page->cache = cache;
	page->inuse = 0;
	page->flags |= PAGE_SLAB;

	start = (char *)page_to_phys(page);
	last = start;
	for (char *object = start + cache->size; object < start + cache->objects * cache->size;
	     object += cache->size) {
		*free_pointer(last) = object;
		last = object;
	}
	*free_pointer(last) = nullptr;
	page->freelist = start;
	list_add(&page->list, &cache->partial);
	return page;
}

static void *slab_alloc(struct kmem_cache *cache)
{
	struct page *page;
	void *object;

	if (list_empty(&cache->partial)) {
		page = new_slab(cache);
		if (!page) {
			return nullptr;
		}
	} else {
		page = list_first_entry(&cache->partial, struct page, list);
	}

	object = page->freelist;
	page->freelist = *free_pointer(object);
	page->inuse++;
	/* A full slab is not tracked; kfree puts it back on the partial list. */
	if (!page->freelist) {
		list_del(&page->list);
	}
	return object;
}

static void slab_free(struct page *page, void *object)
{
	struct kmem_cache *cache = page->cache;
	void *prior = page->freelist;

	*free_pointer(object) = prior;
	page->freelist = object;
	page->inuse--;

	if (!page->inuse) {
		if (prior) {
			list_del(&page->list);
		}
		page->flags &= ~PAGE_SLAB;
		free_pages(page, 0);
	} else if (!prior) {
		list_add(&page->list, &cache->partial);
	}
}

static unsigned kmalloc_index(size_t size)
{
	unsigned index = 0;

	while ((KMALLOC_MIN_SIZE << index) < size) {
		index++;
	}
	return index;
}

void *kmalloc(size_t size)
{
	if (!size) {
		return nullptr;
	}
	if (size > KMALLOC_MAX_SIZE) {
		struct page *page;
		unsigned order = 0;

		while (((size_t)PAGE_SIZE << order) < size) {
			if (++order >= MAX_ORDER) {
				return nullptr;
			}
		}
		page = alloc_pages(order);
		if (!page) {
			return nullptr;
		}
		/* Recorded so that kfree needs nothing but the pointer. */
		page->order = order;
		return (void *)page_to_phys(page);
	}
	return slab_alloc(&kmalloc_caches[kmalloc_index(size)]);
}

void *kzalloc(size_t size)
{
	void *object = kmalloc(size);

	if (object) {
		memset(object, 0, size);
	}
	return object;
}

void kfree(void *object)
{
	struct page *page;

	if (!object) {
		return;
	}
	page = phys_to_page((uintptr_t)object);
	if (page->flags & PAGE_SLAB) {
		slab_free(page, object);
	} else {
		free_pages(page, page->order);
	}
}
