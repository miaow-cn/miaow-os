/* SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "kernel.h"
#include "abi.h"

#define RET 0xd65f03c0u

/* Written by probe_vectors in fault_probe.S. */
struct {
	uint64_t esr;
	uint64_t far;
} probe_fault;

extern const char probe_vectors[];
extern const char vectors[];
void probe_read(uintptr_t address);
void probe_write(uintptr_t address);
void probe_exec(uintptr_t address);

/* Branch targets return if their page is wrongly executable. */
static uint32_t data_ret = RET;
static const uint32_t rodata_ret = RET;

static void probe(const char *name, void (*access)(uintptr_t), uintptr_t address)
{
	probe_fault.esr = 0;
	probe_fault.far = 0;
	access(address);
	printk("PROBE %s address=%016lx ESR=%016lx FAR=%016lx\n", name, address, probe_fault.esr, probe_fault.far);
}

[[noreturn]] void __wrap_start_kernel(void)
{
	volatile uint32_t stack_ret = RET;

	write_sysreg(probe_vectors, vbar_el1);
	__asm__ volatile("isb" : : : "memory");
	probe("write-text", probe_write, (uintptr_t)_text);
	probe("write-rodata", probe_write, (uintptr_t)&rodata_ret);
	probe("exec-rodata", probe_exec, (uintptr_t)&rodata_ret);
	probe("exec-data", probe_exec, (uintptr_t)&data_ret);
	probe("exec-stack", probe_exec, (uintptr_t)&stack_ret);
	probe("exec-user", probe_exec, APP_FIRST);
	probe("read-unmapped", probe_read, APP_FIRST + APP_IMAGE_SIZE);
	write_sysreg(vectors, vbar_el1);
	__asm__ volatile("isb" : : : "memory");

	/* The real vectors must report this and halt. */
	*(volatile uint32_t *)_text = 0;
	printk("FAULT SELFTEST FAIL\n");
	kernel_panic();
}