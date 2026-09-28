/*
 * SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef _KERNEL_H
#define _KERNEL_H

#include <stddef.h>
#include <stdint.h>

#include <miaow/memory.h>
#include <miaow/printk.h>
#include <miaow/string.h>

#if __STDC_VERSION__ < 202311L
#error "The kernel requires C23"
#endif

#define read_sysreg(r)                                                                             \
	({                                                                                         \
		uint64_t __val;                                                                    \
		__asm__ volatile("mrs %0, " #r : "=r"(__val));                                     \
		__val;                                                                             \
	})
#define write_sysreg(v, r)                                                                         \
	__asm__ volatile("msr " #r ", %0" : : "r"((uint64_t)(v)) : "memory")

extern const char _text[];
extern const char _end[];
extern uintptr_t __fdt_pointer;

void uart_putc(char character);
void uart_puts(const char *text);
[[noreturn]] void kernel_panic(void);
[[noreturn]] void kernel_main(void);
struct pt_regs;
[[noreturn]] void enter_app(struct pt_regs *regs);
[[noreturn]] void start_apps(void);
void timer_init(void);
void timer_rearm(void);
void timer_stop(void);
bool timer_interrupt(void);
void mmu_init(void);
void cpu_uninstall_idmap(void);
void mem_init(void);

#endif /* _KERNEL_H */
