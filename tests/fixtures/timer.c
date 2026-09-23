/* SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "kernel.h"

[[noreturn]] void __wrap_kernel_main(void)
{
	timer_init();
	timer_stop();
	__asm__ volatile("isb" : : : "memory");
	if (timer_interrupt() || timer_interrupt() || (READ_SYSREG(cntp_ctl_el0) & 1)) {
		kernel_panic();
	}
	printk("SPURIOUS OK\n");
	for (;;) {
		__asm__ volatile("wfi");
	}
}