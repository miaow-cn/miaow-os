/*
 * SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef _ABI_H
#define _ABI_H

#define __NR_log       0
#define __NR_exit      1
#define EFAULT         14
#define EINVAL         22
#define ENOSYS         38
#define LOG_LIMIT      256
#define APP_LOAD_ADDR  0x400000
#define APP_IMAGE_SIZE 0x10000
#define APP_STACK_SIZE 0x4000
#define TASK_SIZE      0x8000000000
#define STACK_TOP      TASK_SIZE
#define APP_COUNT      3

#endif /* _ABI_H */
