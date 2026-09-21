/*
 * SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <assert.h>
#include <stdlib.h>

#include <miaow/mm.h>
#include <miaow/slab.h>
#include <miaow/string.h>

#define PAGES 64
#define COUNT 12

static const size_t sizes[COUNT] = {1, 8, 16, 17, 31, 32, 64, 255, 1024, 2048, 2049, 9000};
static void *objects[COUNT];

int main(void)
{
	void *memory = aligned_alloc(PAGE_SIZE, PAGES * PAGE_SIZE);
	struct page *map = calloc(PAGES, sizeof(struct page));
	uintptr_t base;
	unsigned long available;

	assert(memory && map);
	base = (uintptr_t)memory;
	page_alloc_init(base, PAGES, map);
	page_alloc_free_range(base, base + PAGES * PAGE_SIZE);
	slab_init();
	available = page_alloc_free_count();

	assert(!kmalloc(0));
	kfree(nullptr);

	for (unsigned index = 0; index < COUNT; ++index) {
		objects[index] = kmalloc(sizes[index]);
		assert(objects[index]);
		assert(!((uintptr_t)objects[index] % KMALLOC_MIN_SIZE));
		/* Requests beyond the largest size class come from whole pages. */
		if (sizes[index] > KMALLOC_MAX_SIZE) {
			assert(!((uintptr_t)objects[index] % PAGE_SIZE));
		}
		memset(objects[index], (int)index + 1, sizes[index]);
	}
	for (unsigned index = 0; index < COUNT; ++index) {
		const unsigned char *bytes = objects[index];

		assert(bytes[0] == index + 1);
		assert(bytes[sizes[index] - 1] == index + 1);
	}
	for (unsigned index = 0; index < COUNT; ++index) {
		kfree(objects[index]);
	}
	assert(page_alloc_free_count() == available);

	/* Released memory is handed out again. */
	void *first = kmalloc(64);
	assert(first);
	kfree(first);
	assert(kmalloc(64) == first);
	kfree(first);

	/* A whole slab worth of objects fits in one page and stays distinct. */
	unsigned objects_per_page = PAGE_SIZE / KMALLOC_MIN_SIZE;
	void **batch = calloc(objects_per_page, sizeof(*batch));

	assert(batch);
	for (unsigned index = 0; index < objects_per_page; ++index) {
		batch[index] = kmalloc(KMALLOC_MIN_SIZE);
		assert(batch[index]);
		for (unsigned other = 0; other < index; ++other) {
			assert(batch[index] != batch[other]);
		}
	}
	assert(page_alloc_free_count() == available - 1);
	for (unsigned index = 0; index < objects_per_page; ++index) {
		kfree(batch[index]);
	}
	assert(page_alloc_free_count() == available);
	free(batch);

	void *zeroed = kzalloc(200);

	assert(zeroed);
	for (unsigned offset = 0; offset < 200; ++offset) {
		assert(((const unsigned char *)zeroed)[offset] == 0);
	}
	kfree(zeroed);
	assert(page_alloc_free_count() == available);

	free(map);
	free(memory);
	return 0;
}
