/* SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "kernel.h"

static volatile uint64_t bss_probe;

[[noreturn]] void kernel_panic(void)
{
	__asm__ volatile("msr daifset, #15");
	uart_puts("PANIC EL1 ESR=");
	uart_hex(READ_SYSREG(esr_el1));
	uart_puts(" ELR=");
	uart_hex(READ_SYSREG(elr_el1));
	uart_putc('\n');
	for (;;) {
		__asm__ volatile("wfi");
	}
}

[[noreturn]] void kernel_main(void)
{
	uintptr_t stack;
	__asm__ volatile("mov %0, sp" : "=r"(stack));
	uart_puts("BOOT EL=");
	uart_hex(READ_SYSREG(CurrentEL) >> 2);
	uart_puts(" SCTLR=");
	uart_hex(READ_SYSREG(sctlr_el1));
	uart_puts(" VBAR=");
	uart_hex(READ_SYSREG(vbar_el1));
	uart_puts(" BSS=");
	uart_hex(bss_probe);
	uart_puts(" SP=");
	uart_hex(stack);
	uart_puts("\nBOOT OK\n");
#ifdef TEST_KERNEL_FAULT
	__asm__ volatile("udf #0");
#endif
	start_apps();
}