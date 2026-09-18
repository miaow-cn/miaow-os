# SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

foreach(INDEX RANGE 0 2)
    list(GET APP_BINARIES ${INDEX} APP_BIN)
    set_property(SOURCE payloads.S APPEND PROPERTY
        COMPILE_DEFINITIONS "APP${INDEX}_BIN=\"${APP_BIN}\"")
endforeach()
set_source_files_properties(payloads.S PROPERTIES OBJECT_DEPENDS "${APP_BINARIES}")

add_executable(kernel boot.S vectors.S main.c uart.c task.c timer.c payloads.S)
set_target_properties(kernel PROPERTIES
    SUFFIX ".elf"
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
    LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/linker.ld"
)
target_link_libraries(kernel PRIVATE build_options kernel_lib)
target_link_options(kernel PRIVATE "LINKER:-T,${CMAKE_CURRENT_SOURCE_DIR}/linker.ld")
add_dependencies(kernel app_images)
add_custom_command(
    OUTPUT "${PROJECT_BINARY_DIR}/kernel.bin"
    COMMAND "${CMAKE_OBJCOPY}" -O binary "$<TARGET_FILE:kernel>"
            "${PROJECT_BINARY_DIR}/kernel.bin"
    DEPENDS kernel
    VERBATIM
)
add_custom_target(kernel_image ALL DEPENDS "${PROJECT_BINARY_DIR}/kernel.bin")