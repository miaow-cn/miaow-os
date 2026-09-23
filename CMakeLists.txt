# SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/aarch64.cmake")
endif()

project(miaow-os LANGUAGES C ASM)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(EXTRA_CFLAGS "" CACHE STRING "Extra C and assembly compiler flags")
separate_arguments(EXTRA_FLAGS NATIVE_COMMAND "${EXTRA_CFLAGS}")

add_library(build_options INTERFACE)
target_compile_options(build_options INTERFACE
    "$<$<COMPILE_LANGUAGE:C>:-std=c23>"
    -O2 -g -Wall -Wextra -Werror -ffreestanding -fno-builtin -fno-pie
    -fno-stack-protector -fno-asynchronous-unwind-tables -fno-unwind-tables
    -mgeneral-regs-only -mno-outline-atomics -mcpu=cortex-a710
    ${EXTRA_FLAGS}
)
target_include_directories(build_options INTERFACE include kernel)
target_link_options(build_options INTERFACE
    -nostdlib -static -no-pie "LINKER:--build-id=none"
)

add_subdirectory(apps)
add_subdirectory(lib)
add_subdirectory(mm)
add_subdirectory(kernel)
add_subdirectory(tests EXCLUDE_FROM_ALL)

add_custom_target(run
    COMMAND qemu-system-aarch64
        -machine virt-10.1,gic-version=3,virtualization=off,secure=off,its=off
        -cpu cortex-a710 -smp 1 -m 128M
        -display none -serial stdio -monitor none -no-reboot
        -kernel "${CMAKE_CURRENT_BINARY_DIR}/kernel.bin"
    DEPENDS kernel_image
    USES_TERMINAL
    VERBATIM
)