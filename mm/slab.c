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

/**
 * @brief Initialize the kmalloc size-class caches.
 *
 * @details Sets up one cache per power-of-two size from KMALLOC_MIN_SIZE to
 * KMALLOC_MAX_SIZE; objects per slab is PAGE_SIZE / size since every slab is a
 * single page. Allocating caches lazily through slab_alloc() needs no further
 * setup.
 */
void slab_init(void)
{
	for (unsigned index = 0; index < KMALLOC_CACHES; ++index) {
		struct kmem_cache *cache = &kmalloc_caches[index];

		cache->size = KMALLOC_MIN_SIZE << index;
		cache->objects = PAGE_SIZE / cache->size;
		INIT_LIST_HEAD(&cache->partial);
	}
}

/**
 * @brief Get the free-pointer slot of a free object.
 *
 * @param object Free object to query.
 * @return Address of the word at offset zero holding the next free object.
 */
static void **free_pointer(void *object)
{
	return object;
}

/**
 * @brief Allocate a page and carve it into objects chained by free pointers.
 *
 * @details Each free object stores the address of the next one at offset zero;
 * the list ends with NULL and starts at page->freelist. The new slab is put on
 * the cache's partial list.
 *
 * @param cache Cache whose object size defines the carving.
 * @return The new slab page, or NULL if the page allocator is out of memory.
 */
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

/**
 * @brief Allocate one object from @p cache.
 *
 * @details Pops the first free object of the first partial slab, growing the
 * cache with new_slab() when no partial slab exists. A slab that becomes full
 * is unlinked; kfree() re-lists it when it gains a free object.
 *
 * @param cache Size-class cache to allocate from.
 * @return The object, or NULL if no memory is available.
 */
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

/**
 * @brief Return @p object to its slab.
 *
 * @details Pushes the object onto the slab's freelist. An empty slab is
 * handed back to the page allocator; a slab that was full re-enters the
 * partial list. The @p prior freelist head tells both cases apart: NULL means
 * the slab was full and therefore unlisted.
 *
 * @param page Slab owning the object.
 * @param object Object to free; must come from @p page.
 */
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

/**
 * @brief Map a request size to its size-class cache index.
 *
 * @param size Requested size in bytes, at most KMALLOC_MAX_SIZE.
 * @return Index into kmalloc_caches whose size is the smallest power of two
 * no smaller than @p size.
 */
static unsigned kmalloc_index(size_t size)
{
	unsigned index = 0;

	while ((KMALLOC_MIN_SIZE << index) < size) {
		index++;
	}
	return index;
}

/**
 * @brief Allocate @p size bytes of kernel memory.
 *
 * @details Sizes up to KMALLOC_MAX_SIZE come from the matching size-class
 * slab cache; larger requests are served directly by the page allocator with
 * the block order recorded in page->order so kfree() needs only the pointer.
 *
 * @param size Number of bytes to allocate.
 * @return Pointer to the memory, or NULL on failure or zero @p size.
 */
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

/**
 * @brief Allocate @p size bytes of zeroed kernel memory.
 *
 * @param size Number of bytes to allocate.
 * @return Pointer to the zeroed memory, or NULL on failure or zero @p size.
 */
void *kzalloc(size_t size)
{
	void *object = kmalloc(size);

	if (object) {
		memset(object, 0, size);
	}
	return object;
}

/**
 * @brief Free memory allocated by kmalloc() or kzalloc().
 *
 * @details Dispatches on the page's PAGE_SLAB flag: slab objects go back to
 * their slab, oversized blocks go back to the page allocator with the order
 * recorded at allocation time.
 *
 * @param object Pointer to free; NULL is a no-op.
 */
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
