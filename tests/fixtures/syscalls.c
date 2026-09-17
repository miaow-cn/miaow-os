/* SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "app.h"

int app_main(void)
{
	char buffer[LOG_LIMIT];
	for (unsigned index = 0; index < sizeof(buffer); ++index) {
		buffer[index] = 'Z';
	}
	if (syscall(SYS_LOG, 0, 0) != 0 || syscall(SYS_LOG, UINTPTR_MAX, 0) != 0 ||
	    syscall(SYS_LOG, (uintptr_t)buffer, 257) != -ERR_INVALID ||
	    syscall(SYS_LOG, 0, 1) != -ERR_FAULT ||
	    syscall(SYS_LOG, UINTPTR_MAX - 1, 4) != -ERR_FAULT ||
	    syscall(SYS_LOG, APP_FIRST + APP_SLOT_SIZE - 1, 2) != -ERR_FAULT ||
	    syscall(SYS_LOG, APP_FIRST + APP_IMAGE_SIZE - 1, 2) != -ERR_FAULT ||
	    syscall(SYS_LOG, APP_FIRST + APP_SLOT_SIZE, 1) != -ERR_FAULT ||
	    syscall(SYS_LOG, APP_FIRST + APP_SLOT_SIZE * 2 - 16, 1) != -ERR_FAULT ||
	    syscall(SYS_LOG, 0x40080000, 1) != -ERR_FAULT ||
	    syscall(SYS_LOG, 0x09000000, 1) != -ERR_FAULT ||
	    syscall(99, 0, 0) != -ERR_NOSYS) {
		return 1;
	}
	if (syscall(SYS_LOG, (uintptr_t)buffer, sizeof(buffer)) != sizeof(buffer) ||
	    log_text("\n") != 1 || log_text("syscalls OK\n") != 12) {
		return 2;
	}
	return 0;
}