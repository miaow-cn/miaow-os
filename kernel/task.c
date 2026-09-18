/*
 * SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "kernel.h"
#include "task.h"

extern const unsigned char app0_start[], app0_end[], app1_start[], app1_end[], app2_start[],
	app2_end[];

static struct task tasks[APP_COUNT];
static unsigned current;

static void task_prefix(void)
{
	char prefix[] = "[app 0] ";

	prefix[5] = (char)('0' + current);
	uart_puts(prefix);
}

static struct context *schedule(void)
{
	int next = next_runnable(tasks, current);
	if (next >= 0) {
#ifdef TEST_SCHED_TRACE
		printk("SWITCH %u -> %u\n", current, (unsigned)next);
#endif
		current = (unsigned)next;
		timer_rearm();
		return &tasks[next].context;
	}
	timer_stop();
	if (READ_SYSREG(cntp_ctl_el0) & 1) {
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

static long sys_log(struct task *task, uintptr_t pointer, size_t length)
{
	if (length > LOG_LIMIT) {
		return -ERR_INVALID;
	}
	if (!length) {
		return 0;
	}
	if (!contains(task->image, task->size, pointer, length) &&
	    !contains(task->stack_bottom, APP_STACK_SIZE, pointer, length)) {
		return -ERR_FAULT;
	}
	task_prefix();
	for (size_t index = 0; index < length; ++index) {
		uart_putc(((const char *)pointer)[index]);
	}
	return (long)length;
}

struct context *trap(struct context *frame, uint64_t irq)
{
	struct task *task = &tasks[current];
	if ((frame->pstate & 0x1f) || !(READ_SYSREG(daif) & 0x80) ||
	    frame->sp < task->stack_bottom || frame->sp > task->stack_top || (frame->sp & 15)) {
		kernel_panic();
	}
	task->context = *frame;
	if (!task->observed) {
		printk("[app %u] TRAP origin=%016lx handler=%016lx\n", current,
		       frame->pstate & 0x1f, READ_SYSREG(CurrentEL) >> 2);
		task->observed = true;
	}
	if (irq) {
		if (!timer_interrupt()) {
			return &task->context;
		}
#ifdef TEST_PREEMPT
		if (frame->pc < task->image || frame->pc >= task->image + task->size ||
		    (frame->pstate & 0xf0000080) != 0x60000000 ||
		    frame->sp != task->stack_top - 16) {
			kernel_panic();
		}
		for (unsigned index = 1; index <= 30; ++index) {
			if (frame->registers[index] != 100 + index) {
				kernel_panic();
			}
		}
		printk("TICK app=%u progress=%016lx context=OK\n", current, frame->registers[0]);
#endif
		return schedule();
	}
	uint64_t syndrome = READ_SYSREG(esr_el1);
	if ((syndrome >> 26) != 0x15 || (syndrome & 0xffff)) {
		printk("[app %u] FAULT ESR=%016lx ELR=%016lx\n", current, syndrome, frame->pc);
		task->runnable = false;
		return schedule();
	}
	switch (frame->registers[8]) {
	case SYS_LOG:
		task->context.registers[0] =
			sys_log(task, frame->registers[0], frame->registers[1]);
		break;
	case SYS_EXIT:
		task->exit_status = (long)frame->registers[0];
		printk("[app %u] EXIT status=%ld\n", current, frame->registers[0]);
		task->runnable = false;
		return schedule();
	default:
		task->context.registers[0] = (uint64_t)-ERR_NOSYS;
		break;
	}
	return &task->context;
}

[[noreturn]] void start_apps(void)
{
	const unsigned char *starts[] = {app0_start, app1_start, app2_start};
	const unsigned char *ends[] = {app0_end, app1_end, app2_end};
	for (unsigned index = 0; index < APP_COUNT; ++index) {
		struct task *task = &tasks[index];
		task->image = APP_FIRST + index * APP_SLOT_SIZE;
		task->size = (uintptr_t)ends[index] - (uintptr_t)starts[index];
		task->stack_top = task->image + APP_SLOT_SIZE;
		task->stack_bottom = task->stack_top - APP_STACK_SIZE;
		if (!task->size || task->size > APP_IMAGE_SIZE ||
		    task->image + task->size > task->stack_bottom) {
			kernel_panic();
		}
		volatile unsigned char *destination = (volatile unsigned char *)task->image;
		for (size_t offset = 0; offset < task->size; ++offset) {
			destination[offset] = starts[index][offset];
		}
		for (size_t offset = 0; offset < task->size; ++offset) {
			if (destination[offset] != starts[index][offset]) {
				kernel_panic();
			}
		}
		for (uintptr_t address = task->stack_bottom; address < task->stack_top; ++address) {
			*(volatile unsigned char *)address = 0;
		}
		task->context.pc = task->image;
		task->context.sp = task->stack_top;
		task->context.pstate = 0x340;
		task->runnable = true;
		printk("LOADED app=%u image=%016lx stack=%016lx copy=OK\n", index, task->image,
		       task->stack_top);
	}
	__asm__ volatile("dsb sy\n\tisb" : : : "memory");
	timer_init();
	enter_app(&tasks[0].context);
}

void *memcpy(void *destination, const void *source, size_t size)
{
	unsigned char *output = destination;
	const unsigned char *input = source;
	for (size_t index = 0; index < size; ++index) {
		output[index] = input[index];
	}
	return destination;
}
