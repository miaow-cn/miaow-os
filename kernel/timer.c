/*
 * SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "kernel.h"

#define GICD      ((volatile uint32_t *)0x08000000)
#define GICR      ((volatile uint32_t *)0x080a0000)
#define GICR_SGI  ((volatile uint32_t *)0x080b0000)
#define TIMER_IRQ 30

static uint64_t quantum;

void timer_rearm(void)
{
	WRITE_SYSREG(cntp_tval_el0, quantum);
	WRITE_SYSREG(cntp_ctl_el0, 1);
	__asm__ volatile("isb" : : : "memory");
}

void timer_stop(void)
{
	WRITE_SYSREG(cntp_ctl_el0, 0);
	__asm__ volatile("isb" : : : "memory");
}

void timer_init(void)
{
	GICD[0] = (1u << 4);
	while (GICD[0] & (1u << 31)) {
	}
	GICR[0x14 / 4] &= ~(1u << 1);
	while (GICR[0x14 / 4] & (1u << 2)) {
	}
	GICR_SGI[0x80 / 4] = 1u << TIMER_IRQ;
	GICR_SGI[0xc00 / 4 + 1] &= ~(2u << ((TIMER_IRQ - 16) * 2));
	((volatile uint8_t *)GICR_SGI)[0x400 + TIMER_IRQ] = 0x80;
	GICR_SGI[0x100 / 4] = 1u << TIMER_IRQ;
	while (GICR[0] & (1u << 3)) {
	}
	GICD[0] |= 2;
	while (GICD[0] & (1u << 31)) {
	}
	__asm__ volatile("dsb sy" : : : "memory");
	WRITE_SYSREG(icc_sre_el1, READ_SYSREG(icc_sre_el1) | 1);
	__asm__ volatile("isb" : : : "memory");
	WRITE_SYSREG(icc_pmr_el1, 0xff);
	WRITE_SYSREG(icc_bpr1_el1, 0);
	WRITE_SYSREG(icc_ctlr_el1, 0);
	WRITE_SYSREG(icc_igrpen1_el1, 1);
	__asm__ volatile("isb" : : : "memory");
	quantum = READ_SYSREG(cntfrq_el0) / 100;
	if (!quantum || quantum > 0x7fffffff) {
		kernel_panic();
	}
	printk("TIMER frequency=%016lx quantum=%016lx\n", READ_SYSREG(cntfrq_el0), quantum);
	timer_rearm();
}

bool timer_interrupt(void)
{
	uint64_t interrupt = READ_SYSREG(icc_iar1_el1);
	if (interrupt >= 1020 && interrupt <= 1023) {
		return false;
	}
	if (interrupt != TIMER_IRQ) {
		kernel_panic();
	}
	timer_rearm();
	__asm__ volatile("dsb sy" : : : "memory");
	WRITE_SYSREG(icc_eoir1_el1, interrupt);
	__asm__ volatile("isb" : : : "memory");
	return true;
}
