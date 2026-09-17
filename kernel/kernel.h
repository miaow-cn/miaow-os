/* SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later */

#pragma once

#include <stddef.h>
#include <stdint.h>

#if __STDC_VERSION__ < 202311L
#error "The kernel requires C23"
#endif

#define READ_SYSREG(name) ({ uint64_t value; __asm__ volatile("mrs %0, " #name : "=r"(value)); value; })
#define WRITE_SYSREG(name, value) __asm__ volatile("msr " #name ", %0" : : "r"((uint64_t)(value)) : "memory")

void uart_putc(char character);
void uart_puts(const char *text);
void uart_hex(uint64_t value);
[[noreturn]] void kernel_panic(void);
[[noreturn]] void kernel_main(void);
struct context;
[[noreturn]] void enter_app(struct context *context);
[[noreturn]] void start_apps(void);
void timer_init(void);
void timer_rearm(void);
void timer_stop(void);
bool timer_interrupt(void);