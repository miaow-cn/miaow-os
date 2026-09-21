/*
 * SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef _ABI_H
#define _ABI_H

#define SYS_LOG        0
#define SYS_EXIT       1
#define ERR_FAULT      14
#define ERR_INVALID    22
#define ERR_NOSYS      38
#define LOG_LIMIT      256
#define RAM_BASE       0x40000000
#define RAM_SIZE       0x08000000
#define PAGE_SHIFT     12
#define PAGE_SIZE      (1 << PAGE_SHIFT)
#define APP_FIRST      0x41000000
#define APP_SLOT_SIZE  0x20000
#define APP_IMAGE_SIZE 0x10000
#define APP_STACK_SIZE 0x4000
#define APP_COUNT      3

#endif /* _ABI_H */
