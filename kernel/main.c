/*
 * SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "kernel.h"

static volatile uint64_t bss_probe;

[[noreturn]] void kernel_panic(void)
{
	__asm__ volatile("msr daifset, #15");
	printk("PANIC EL1 ESR=%016lx ELR=%016lx\n", READ_SYSREG(esr_el1), READ_SYSREG(elr_el1));
	for (;;) {
		__asm__ volatile("wfi");
	}
}

[[noreturn]] void kernel_main(void)
{
	uintptr_t stack;
	__asm__ volatile("mov %0, sp" : "=r"(stack));
	printk("BOOT EL=%016lx SCTLR=%016lx VBAR=%016lx BSS=%016lx SP=%016lx\n",
	       READ_SYSREG(CurrentEL) >> 2, READ_SYSREG(sctlr_el1), READ_SYSREG(vbar_el1),
	       bss_probe, stack);
	printk("BOOT OK\n");
#ifdef TEST_KERNEL_FAULT
	__asm__ volatile("udf #0");
#endif
	start_apps();
}
