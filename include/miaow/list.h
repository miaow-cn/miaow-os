/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Imported from Linux v7.3-rc3-520-g518e5b794c06 include/linux/list.h and
 * include/linux/container_of.h (GPL-2.0-only); see docs/third-party.md for the
 * third-party inventory. Ported to this freestanding tree: reduced to the
 * operations this kernel uses; READ_ONCE/WRITE_ONCE dropped; list validation,
 * pointer poisoning, the RCU, hlist, safe, reverse and *_careful variants and
 * the container_of type check removed. The algorithm is unchanged.
 */

#ifndef _LIST_H
#define _LIST_H

#include <stddef.h>

#define container_of(pointer, type, member) ((type *)((char *)(pointer) - offsetof(type, member)))

struct list_head {
	struct list_head *next;
	struct list_head *prev;
};

#define LIST_HEAD_INIT(name) {&(name), &(name)}
#define LIST_HEAD(name)      struct list_head name = LIST_HEAD_INIT(name)

static inline void INIT_LIST_HEAD(struct list_head *list)
{
	list->next = list;
	list->prev = list;
}

static inline void __list_add(struct list_head *entry, struct list_head *prev,
			      struct list_head *next)
{
	next->prev = entry;
	entry->next = next;
	entry->prev = prev;
	prev->next = entry;
}

static inline void list_add(struct list_head *entry, struct list_head *head)
{
	__list_add(entry, head, head->next);
}

static inline void list_add_tail(struct list_head *entry, struct list_head *head)
{
	__list_add(entry, head->prev, head);
}

static inline void list_del(struct list_head *entry)
{
	entry->prev->next = entry->next;
	entry->next->prev = entry->prev;
	entry->next = nullptr;
	entry->prev = nullptr;
}

static inline int list_empty(const struct list_head *head)
{
	return head->next == head;
}

#define list_entry(pointer, type, member)    container_of(pointer, type, member)
#define list_first_entry(head, type, member) list_entry((head)->next, type, member)
#define list_next_entry(position, member)                                                          \
	list_entry((position)->member.next, typeof(*(position)), member)

#define list_for_each_entry(position, head, member)                                                \
	for (position = list_first_entry(head, typeof(*position), member);                         \
	     &position->member != (head); position = list_next_entry(position, member))

#endif /* _LIST_H */
