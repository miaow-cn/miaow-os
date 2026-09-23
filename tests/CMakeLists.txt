# SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

foreach(NAME memory fault timer)
    add_executable(${NAME}_test $<TARGET_OBJECTS:kernel_core>)
    set_target_properties(${NAME}_test PROPERTIES
        SUFFIX ".elf"
        RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
        LINK_DEPENDS "${PROJECT_SOURCE_DIR}/kernel/linker.ld"
    )
    target_link_libraries(${NAME}_test PRIVATE build_options kernel_lib kernel_mm)
    target_link_options(${NAME}_test PRIVATE
        "LINKER:-T,${PROJECT_SOURCE_DIR}/kernel/linker.ld"
        "LINKER:--wrap=kernel_main"
    )
    add_custom_command(
        OUTPUT "${PROJECT_BINARY_DIR}/${NAME}_test.bin"
        COMMAND "${CMAKE_OBJCOPY}" -O binary "$<TARGET_FILE:${NAME}_test>"
                "${PROJECT_BINARY_DIR}/${NAME}_test.bin"
        DEPENDS ${NAME}_test
        VERBATIM
    )
    add_custom_target(${NAME}_test_image DEPENDS "${PROJECT_BINARY_DIR}/${NAME}_test.bin")
endforeach()
target_sources(memory_test PRIVATE fixtures/memory.c fixtures/string.c)
target_sources(fault_test PRIVATE fixtures/kernel_fault.c)
target_sources(timer_test PRIVATE fixtures/timer.c)
target_link_options(memory_test PRIVATE "LINKER:--wrap=mem_init")