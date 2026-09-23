/* SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "kernel.h"

[[noreturn]] void __wrap_kernel_main(void)
{
	__asm__ volatile("udf #0");
	printk("FAULT SELFTEST FAIL\n");
	kernel_panic();
}