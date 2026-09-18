# SPDX-FileCopyrightText: 2026 miaow <guoyr_2013@hotmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

set(APP0 "apps/sequence.c" CACHE FILEPATH "Application 0 source")
set(APP1 "apps/primes.c" CACHE FILEPATH "Application 1 source")
set(APP2 "apps/checksum.c" CACHE FILEPATH "Application 2 source")

set(APP_LINKER "${CMAKE_CURRENT_BINARY_DIR}/linker.ld")
add_custom_command(
    OUTPUT "${APP_LINKER}"
    COMMAND "${CMAKE_C_COMPILER}" -E -P -x c
            "-I${PROJECT_SOURCE_DIR}/include"
            "${CMAKE_CURRENT_SOURCE_DIR}/linker.ld.S" -o "${APP_LINKER}"
    DEPENDS linker.ld.S "${PROJECT_SOURCE_DIR}/include/abi.h"
    VERBATIM
)
add_custom_target(app_linker DEPENDS "${APP_LINKER}")

foreach(INDEX RANGE 0 2)
    get_filename_component(APP_SOURCE "${APP${INDEX}}" ABSOLUTE
        BASE_DIR "${PROJECT_SOURCE_DIR}")
    add_executable(app${INDEX} start.S "${APP_SOURCE}")
    set_target_properties(app${INDEX} PROPERTIES
        OUTPUT_NAME "${INDEX}" SUFFIX ".elf"
        LINKER_LANGUAGE C
        LINK_DEPENDS "${APP_LINKER}"
    )
    target_link_libraries(app${INDEX} PRIVATE build_options)
    target_include_directories(app${INDEX} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_options(app${INDEX} PRIVATE
        "LINKER:--orphan-handling=error" "LINKER:--defsym=APP_INDEX=${INDEX}"
        "LINKER:-T,${APP_LINKER}"
    )
    add_dependencies(app${INDEX} app_linker)
    set(APP_BIN "${CMAKE_CURRENT_BINARY_DIR}/${INDEX}.bin")
    add_custom_command(
        OUTPUT "${APP_BIN}"
        COMMAND "${CMAKE_OBJCOPY}" -O binary "$<TARGET_FILE:app${INDEX}>" "${APP_BIN}"
        DEPENDS app${INDEX}
        VERBATIM
    )
    list(APPEND APP_BINARIES "${APP_BIN}")
endforeach()
add_custom_target(app_images DEPENDS ${APP_BINARIES})
set(APP_BINARIES "${APP_BINARIES}" PARENT_SCOPE)