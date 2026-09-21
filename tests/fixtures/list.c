/*
 * SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <assert.h>

#include <miaow/list.h>

struct node {
	int value;
	struct list_head list;
};

static int order(struct list_head *head, int *values)
{
	struct node *node;
	int count = 0;

	list_for_each_entry(node, head, list) {
		values[count++] = node->value;
	}
	return count;
}

int main(void)
{
	LIST_HEAD(head);
	struct node nodes[4] = {{0, {}}, {1, {}}, {2, {}}, {3, {}}};
	int values[4];

	assert(list_empty(&head));

	list_add(&nodes[0].list, &head);
	list_add(&nodes[1].list, &head);
	list_add_tail(&nodes[2].list, &head);
	list_add_tail(&nodes[3].list, &head);
	assert(!list_empty(&head));
	assert(order(&head, values) == 4);
	assert(values[0] == 1 && values[1] == 0 && values[2] == 2 && values[3] == 3);
	assert(list_first_entry(&head, struct node, list) == &nodes[1]);
	assert(container_of(&nodes[2].list, struct node, list) == &nodes[2]);

	list_del(&nodes[0].list);
	assert(order(&head, values) == 3);
	assert(values[0] == 1 && values[1] == 2 && values[2] == 3);

	/* A removed entry can go back on the list. */
	list_add_tail(&nodes[0].list, &head);
	assert(order(&head, values) == 4);
	assert(values[3] == 0);

	for (unsigned index = 0; index < 4; ++index) {
		list_del(&nodes[index].list);
	}
	assert(list_empty(&head));
	assert(order(&head, values) == 0);
	return 0;
}
