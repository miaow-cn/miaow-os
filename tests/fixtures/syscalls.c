/*
 * SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "app.h"
#include <miaow/memory.h>

int app_main(void)
{
	char buffer[LOG_LIMIT];
	for (unsigned i = 0; i < sizeof(buffer); i++) {
		buffer[i] = 'Z';
	}
	if (syscall(__NR_log, 0, 0) != 0 || syscall(__NR_log, UINTPTR_MAX, 0) != 0 ||
	    syscall(__NR_log, (uintptr_t)buffer, 257) != -EINVAL || syscall(__NR_log, 0, 1) != -EFAULT ||
	    syscall(__NR_log, UINTPTR_MAX - 1, 4) != -EFAULT ||
	    syscall(__NR_log, APP_FIRST + APP_SLOT_SIZE - 1, 2) != -EFAULT ||
	    syscall(__NR_log, APP_FIRST + APP_IMAGE_SIZE - 1, 2) != -EFAULT ||
	    syscall(__NR_log, APP_FIRST + APP_SLOT_SIZE, 1) != -EFAULT ||
	    syscall(__NR_log, APP_FIRST + APP_SLOT_SIZE * 2 - 16, 1) != -EFAULT ||
	    syscall(__NR_log, 0x40080000, 1) != -EFAULT || syscall(__NR_log, 0x09000000, 1) != -EFAULT ||
	    syscall(__NR_log, PAGE_OFFSET + TEXT_OFFSET, 1) != -EFAULT ||
	    syscall(__NR_log, IO_ADDRESS(0x09000000), 1) != -EFAULT || syscall(99, 0, 0) != -ENOSYS) {
		return 1;
	}
	if (syscall(__NR_log, (uintptr_t)buffer, sizeof(buffer)) != sizeof(buffer) || log_text("\n") != 1 ||
	    log_text("syscalls OK\n") != 12) {
		return 2;
	}
	return 0;
}
