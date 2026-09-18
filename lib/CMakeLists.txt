# SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

add_library(kernel_lib OBJECT printk.c vsprintf.c)
target_link_libraries(kernel_lib PRIVATE build_options)