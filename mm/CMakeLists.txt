# SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

add_library(kernel_mm OBJECT init.c memblock.c page_alloc.c slab.c)
target_link_libraries(kernel_mm PRIVATE build_options)
