/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (C) 2007 SGI, Christoph Lameter
 *
 * Derived from the initial SLUB implementation in Linux commit
 * 81819f0fc8285a2a5a921c019e3e3d7b6169d225 ("SLUB core"), mm/slub.c
 * (GPL-2.0-only); see docs/third-party.md.
 */

#ifndef _SLAB_H
#define _SLAB_H

#include <stddef.h>

#include <miaow/list.h>

#define KMALLOC_MIN_SHIFT 4
#define KMALLOC_MAX_SHIFT 11
#define KMALLOC_MIN_SIZE  ((size_t)1 << KMALLOC_MIN_SHIFT)
#define KMALLOC_MAX_SIZE  ((size_t)1 << KMALLOC_MAX_SHIFT)
#define KMALLOC_CACHES    (KMALLOC_MAX_SHIFT - KMALLOC_MIN_SHIFT + 1)

struct kmem_cache {
	size_t size;
	unsigned objects;
	struct list_head partial;
};

void slab_init(void);
void *kmalloc(size_t size);
void *kzalloc(size_t size);
void kfree(void *object);

#endif /* _SLAB_H */
