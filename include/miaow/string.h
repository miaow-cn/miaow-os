/*
 * SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef _STRING_H
#define _STRING_H

#include <stddef.h>

void *memset(void *destination, int value, size_t size);
void *memcpy(void *destination, const void *source, size_t size);
void *memmove(void *destination, const void *source, size_t size);
int memcmp(const void *left, const void *right, size_t size);

#endif /* _STRING_H */
