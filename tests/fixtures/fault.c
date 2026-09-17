/* SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "app.h"

int app_main(void)
{
	log_text("fault probe\n");
	uint64_t control;
	__asm__ volatile("mrs %0, sctlr_el1" : "=r"(control));
	log_text("ERROR privileged access returned\n");
	return (int)control;
}