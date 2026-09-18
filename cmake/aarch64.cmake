# SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CROSS_COMPILE "aarch64-linux-gnu-" CACHE STRING "Cross-toolchain prefix")
find_program(CMAKE_C_COMPILER NAMES "${CROSS_COMPILE}gcc" REQUIRED)
set(CMAKE_ASM_COMPILER "${CMAKE_C_COMPILER}")
find_program(CMAKE_OBJCOPY NAMES "${CROSS_COMPILE}objcopy" REQUIRED)