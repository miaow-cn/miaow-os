/*
 * SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "kernel.h"
#include "task.h"

#include <miaow/mm.h>
#include <miaow/pgtable.h>
#include <miaow/slab.h>

extern const unsigned char app0_start[], app0_end[], app1_start[], app1_end[], app2_start[], app2_end[];

static struct task_struct tasks[APP_COUNT];
static unsigned current;

static void task_prefix(void)
{
	char prefix[] = "[app 0] ";

	prefix[5] = (char)('0' + current);
	uart_puts(prefix);
}

static struct pt_regs *schedule(void)
{
	int next = next_runnable(tasks, current);
	if (next >= 0) {
		current = (unsigned)next;
		switch_mm(tasks[current].mm);
		timer_rearm();
		return &tasks[next].regs;
	}
	timer_stop();
	if (read_sysreg(cntp_ctl_el0) & 1) {
		kernel_panic();
	}
	printk("ALL APPS DONE\n");
	for (;;) {
		__asm__ volatile("wfi");
	}
}

static bool contains(uintptr_t start, size_t size, uintptr_t pointer, size_t length)
{
	return pointer >= start && pointer - start <= size && length <= size - (pointer - start);
}

static long sys_log(struct task_struct *task, uintptr_t pointer, size_t length)
{
	if (length > LOG_LIMIT) {
		return -EINVAL;
	}
	if (!length) {
		return 0;
	}
	if (!contains(task->image, task->size, pointer, length) &&
	    !contains(task->stack_bottom, APP_STACK_SIZE, pointer, length)) {
		return -EFAULT;
	}
	task_prefix();
	for (size_t i = 0; i < length; i++) {
		uart_putc(((const char *)pointer)[i]);
	}
	return (long)length;
}

struct pt_regs *trap(struct pt_regs *regs, uint64_t irq)
{
	struct task_struct *task = &tasks[current];
	if ((regs->pstate & 0x1f) || !(read_sysreg(daif) & 0x80) || regs->sp < task->stack_bottom ||
	    regs->sp > task->stack_top || (regs->sp & 15)) {
		kernel_panic();
	}
	task->regs = *regs;
	if (!task->observed) {
		printk("[app %u] TRAP origin=%016lx handler=%016lx\n", current, regs->pstate & 0x1f,
		       read_sysreg(CurrentEL) >> 2);
		task->observed = true;
	}
	if (irq) {
		if (!timer_interrupt()) {
			return &task->regs;
		}
		return schedule();
	}
	uint64_t syndrome = read_sysreg(esr_el1);
	if ((syndrome >> 26) != 0x15 || (syndrome & 0xffff)) {
		printk("[app %u] FAULT ESR=%016lx FAR=%016lx ELR=%016lx\n", current, syndrome, read_sysreg(far_el1),
		       regs->pc);
		task->runnable = false;
		return schedule();
	}
	switch (regs->regs[8]) {
	case __NR_log:
		task->regs.regs[0] = sys_log(task, regs->regs[0], regs->regs[1]);
		break;
	case __NR_exit:
		task->exit_code = (long)regs->regs[0];
		printk("[app %u] EXIT status=%ld\n", current, regs->regs[0]);
		task->runnable = false;
		return schedule();
	default:
		task->regs.regs[0] = (uint64_t)-ENOSYS;
		break;
	}
	return &task->regs;
}

static void *map_app_page(struct mm_struct *mm, uintptr_t address, pgprot_t prot)
{
	struct page *page = alloc_pages(0);
	if (!page) {
		printk("PANIC: cannot allocate app page at %016lx\n", address);
		kernel_panic();
	}
	pte_t *pte = pte_alloc(mm, address);
	if (!pte) {
		__free_pages(page, 0);
		printk("PANIC: cannot allocate app page table at %016lx\n", address);
		kernel_panic();
	}
	if (*pte) {
		printk("PANIC: duplicate app mapping at %016lx\n", address);
		kernel_panic();
	}
	void *destination = memset(page_address(page), 0, PAGE_SIZE);
	set_pte_at(mm, address, pte, page_to_phys(page) | prot);
	return destination;
}

[[noreturn]] void start_apps(void)
{
	const unsigned char *starts[] = {app0_start, app1_start, app2_start};
	const unsigned char *ends[] = {app0_end, app1_end, app2_end};
	for (unsigned i = 0; i < APP_COUNT; i++) {
		struct task_struct *task = &tasks[i];
		task->image = APP_LOAD_ADDR;
		task->size = (uintptr_t)ends[i] - (uintptr_t)starts[i];
		task->stack_top = STACK_TOP;
		task->stack_bottom = task->stack_top - APP_STACK_SIZE;
		if (!task->size || task->size > APP_IMAGE_SIZE || task->image + task->size > task->stack_bottom) {
			printk("PANIC: invalid app %u image size\n", i);
			kernel_panic();
		}
		task->mm = kzalloc(sizeof(*task->mm));
		if (!task->mm) {
			printk("PANIC: cannot allocate app %u address space\n", i);
			kernel_panic();
		}
		task->mm->pgd = pgd_alloc(task->mm);
		if (!task->mm->pgd) {
			printk("PANIC: cannot allocate app %u root table\n", i);
			kernel_panic();
		}
		for (size_t offset = 0; offset < task->size; offset += PAGE_SIZE) {
			size_t bytes = task->size - offset;
			if (bytes > PAGE_SIZE) {
				bytes = PAGE_SIZE;
			}
			void *destination = map_app_page(task->mm, task->image + offset, PAGE_READONLY_EXEC);
			memcpy(destination, starts[i] + offset, bytes);
			if (memcmp(destination, starts[i] + offset, bytes)) {
				printk("PANIC: app %u image copy failed\n", i);
				kernel_panic();
			}
			flush_icache_range((uintptr_t)destination, (uintptr_t)destination + bytes);
		}
		for (uintptr_t address = task->stack_bottom; address < task->stack_top; address += PAGE_SIZE) {
			map_app_page(task->mm, address, PAGE_SHARED);
		}
		task->regs.pc = task->image;
		task->regs.sp = task->stack_top;
		task->regs.pstate = 0x340;
		task->runnable = true;
		printk("LOADED app=%u image=%016lx stack=%016lx copy=OK\n", i, task->image, task->stack_top);
	}
	timer_init();
	switch_mm(tasks[0].mm);
	enter_app(&tasks[0].regs);
}
