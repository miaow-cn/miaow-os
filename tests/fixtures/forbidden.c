/* SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com> */
/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "app.h"

#if CASE == 1
volatile int storage = 1;
#elif CASE == 2
volatile int storage;
#elif CASE == 3
thread_local volatile int storage;
#elif CASE == 4
__attribute__((constructor)) void initialize(void) { __asm__ volatile(""); }
#elif CASE == 5
const char storage[APP_IMAGE_SIZE + 1] = {1};
#elif CASE == 6
volatile int storage __attribute__((common));
#endif

int app_main(void)
{
	return 0;
}