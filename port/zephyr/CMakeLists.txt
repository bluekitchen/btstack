cmake_minimum_required(VERSION 3.13)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
list(APPEND BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

get_filename_component(BTSTACK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
if(DEFINED ENV{BTSTACK_ROOT})
    set(BTSTACK_ROOT "$ENV{BTSTACK_ROOT}")
endif()

include("${BTSTACK_ROOT}/chipset/bcm/btstack_bcm_patchram.cmake")

set(BTSTACK_ZEPHYR_PATCHRAM_CHIPSETS)
if(BOARD MATCHES "^if310($|/)")
    list(APPEND BTSTACK_ZEPHYR_PATCHRAM_CHIPSETS CYW55310)
elseif(BOARD MATCHES "^nucleo_(f439zi|h563zi|h753zi)($|/)")
    # Provide both firmware choices documented in the Nucleo configurations.
    list(APPEND BTSTACK_ZEPHYR_PATCHRAM_CHIPSETS CYW55513 CYW55573)
endif()

foreach(chipset IN LISTS BTSTACK_ZEPHYR_PATCHRAM_CHIPSETS)
    set(BTSTACK_ZEPHYR_PATCHRAM_FILE "${BTSTACK_${chipset}_PATCHRAM_FILE}")
    set(BTSTACK_ZEPHYR_PATCHRAM_URL "${BTSTACK_${chipset}_PATCHRAM_URL}")
    set(BTSTACK_ZEPHYR_PATCHRAM_SHA256 "${BTSTACK_${chipset}_PATCHRAM_SHA256}")
    set(BTSTACK_ZEPHYR_PATCHRAM_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${BTSTACK_ZEPHYR_PATCHRAM_FILE}")
    if(NOT EXISTS "${BTSTACK_ZEPHYR_PATCHRAM_PATH}")
        message(STATUS "Downloading ${BTSTACK_ZEPHYR_PATCHRAM_FILE}")
        file(DOWNLOAD
            "${BTSTACK_ZEPHYR_PATCHRAM_URL}"
            "${BTSTACK_ZEPHYR_PATCHRAM_PATH}"
            EXPECTED_HASH SHA256=${BTSTACK_ZEPHYR_PATCHRAM_SHA256}
            STATUS BTSTACK_ZEPHYR_PATCHRAM_DOWNLOAD_STATUS)
        list(GET BTSTACK_ZEPHYR_PATCHRAM_DOWNLOAD_STATUS 0 BTSTACK_ZEPHYR_PATCHRAM_DOWNLOAD_RESULT)
        if(NOT BTSTACK_ZEPHYR_PATCHRAM_DOWNLOAD_RESULT EQUAL 0)
            list(GET BTSTACK_ZEPHYR_PATCHRAM_DOWNLOAD_STATUS 1 BTSTACK_ZEPHYR_PATCHRAM_DOWNLOAD_ERROR)
            file(REMOVE "${BTSTACK_ZEPHYR_PATCHRAM_PATH}")
            message(FATAL_ERROR "Failed to download ${BTSTACK_ZEPHYR_PATCHRAM_FILE}: ${BTSTACK_ZEPHYR_PATCHRAM_DOWNLOAD_ERROR}")
        endif()
    endif()
endforeach()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(BTstack-Zephyr)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
include("${BTSTACK_ROOT}/tool/cmake/BTstack.cmake")
include("${BTSTACK_ROOT}/example/requirements.cmake")

btstack_declare_port(zephyr)
btstack_collect_public_include_directories(BTSTACK_ZEPHYR_PUBLIC_INCLUDE_DIRECTORIES)

# Zephyr normally force-includes autoconf.h for compilation. Include it
# explicitly while extracting btstack_config.h's ENABLE_*/HAVE_* macros so the
# resolver sees the same Kconfig values as the compiler.
btstack_configure_port(OUTPUT BTSTACK_ZEPHYR_CONFIGURATION_MACROS
    CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/btstack_config.h"
    INCLUDE_DIRECTORIES "${CMAKE_CURRENT_BINARY_DIR}/zephyr/include/generated")
# Zephyr normally reconfigures after Kconfig changes; retain the generated
# header explicitly because it directly controls resolver selection.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_BINARY_DIR}/zephyr/include/generated/zephyr/autoconf.h")

string(JOIN ", " BTSTACK_ZEPHYR_CONFIGURATION_MACROS_DISPLAY ${BTSTACK_ZEPHYR_CONFIGURATION_MACROS})
message(STATUS "BTstack config ENABLE_*/HAVE_* macros: ${BTSTACK_ZEPHYR_CONFIGURATION_MACROS_DISPLAY}")

if(DEFINED BTSTACK_ZEPHYR_EXAMPLE AND NOT "${BTSTACK_ZEPHYR_EXAMPLE}" STREQUAL "")
    set(BTSTACK_ZEPHYR_EXAMPLE "${BTSTACK_ZEPHYR_EXAMPLE}")
elseif(DEFINED ENV{EXAMPLE} AND NOT "$ENV{EXAMPLE}" STREQUAL "")
    set(BTSTACK_ZEPHYR_EXAMPLE "$ENV{EXAMPLE}")
else()
    set(BTSTACK_ZEPHYR_EXAMPLE gatt_counter)
endif()

btstack_filter_examples_for_port(BTSTACK_ZEPHYR_EXAMPLES
    CANDIDATES ${btstack_declared_applications})

list(FIND btstack_declared_applications "${BTSTACK_ZEPHYR_EXAMPLE}" BTSTACK_ZEPHYR_EXAMPLE_INDEX)
if(BTSTACK_ZEPHYR_EXAMPLE_INDEX EQUAL -1)
    message(FATAL_ERROR "Unknown Zephyr example '${BTSTACK_ZEPHYR_EXAMPLE}'")
endif()
list(FIND BTSTACK_ZEPHYR_EXAMPLES "${BTSTACK_ZEPHYR_EXAMPLE}" BTSTACK_ZEPHYR_ENABLED_EXAMPLE_INDEX)
if(BTSTACK_ZEPHYR_ENABLED_EXAMPLE_INDEX EQUAL -1)
    message(FATAL_ERROR
        "Zephyr example '${BTSTACK_ZEPHYR_EXAMPLE}' is not enabled by btstack_config.h. "
        "Run 'cmake --build <build-dir> --target port-info' for the available configuration.")
endif()

# src/main.c always installs the Zephyr HCI transport, flash-backed TLV
# implementation, and the optional LE/Classic databases. Feature-gated groups
# are pruned by btstack_filter_groups_for_port.
set(BTSTACK_ZEPHYR_BASE_GROUPS
    core
    chipset-zephyr
    le-device-db-tlv
    link-key-db-tlv
    tlv-none
    platform-embedded-tlv-flash-bank
    third-party-micro-ecc
    third-party-rijndael)

set(BTSTACK_ZEPHYR_PORT_SOURCES
    "${BTSTACK_ROOT}/platform/zephyr/btstack_run_loop_zephyr.c"
    "${BTSTACK_ROOT}/platform/zephyr/hal_flash_bank_zephyr.c"
    "${BTSTACK_ROOT}/platform/zephyr/hci_transport_zephyr.c")

# Match the toolchain settings that Zephyr applies to its own targets.
zephyr_get_include_directories_for_lang(C includes)
zephyr_get_system_include_directories_for_lang(C system_includes)
zephyr_get_compile_definitions_for_lang(C definitions)
zephyr_get_compile_options_for_lang(C options)
set(external_project_cflags
    ${includes}$<SEMICOLON>${definitions}$<SEMICOLON>${options}$<SEMICOLON>${system_includes})
string(REGEX REPLACE "\\$<TARGET_PROPERTY:compiler,imacros>\\$<SEMICOLON>([^\\$]+)"
    "\$<1:SHELL:-imacros \\1>"
    BTSTACK_ZEPHYR_COMPILE_OPTIONS "${external_project_cflags}")

btstack_add_application_library(
    TARGET btstack_zephyr
    APPLICATION ${BTSTACK_ZEPHYR_EXAMPLE}
    BASE_GROUPS ${BTSTACK_ZEPHYR_BASE_GROUPS}
    PORT_SOURCES ${BTSTACK_ZEPHYR_PORT_SOURCES}
    PUBLIC_INCLUDE_DIRECTORIES
        ${BTSTACK_ZEPHYR_PUBLIC_INCLUDE_DIRECTORIES}
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}"
        "${BTSTACK_ROOT}/chipset/zephyr"
        "${BTSTACK_ROOT}/platform/embedded"
        "${BTSTACK_ROOT}/platform/zephyr"
    PRIVATE_COMPILE_OPTIONS ${BTSTACK_ZEPHYR_COMPILE_OPTIONS})
# The BTstack library is external to Zephyr's normal app target dependency
# chain. Wait for both the standard interface and generated headers (including
# heap_constants.h, which upstream Zephyr creates separately).
add_dependencies(btstack_zephyr zephyr_interface zephyr_generated_headers)

set(BTSTACK_ZEPHYR_APPLICATION_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/main.c")
btstack_get_application_property(application_sources "${BTSTACK_ZEPHYR_EXAMPLE}" SOURCES)
btstack_get_application_property(application_gatt "${BTSTACK_ZEPHYR_EXAMPLE}" GATT_DB)
foreach(source IN LISTS application_sources)
    list(APPEND BTSTACK_ZEPHYR_APPLICATION_SOURCES "${source}")
endforeach()
target_sources(app PRIVATE ${BTSTACK_ZEPHYR_APPLICATION_SOURCES})
target_include_directories(app PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

if(application_gatt)
    message(STATUS "Zephyr example ${BTSTACK_ZEPHYR_EXAMPLE} -- with GATT DB")
    btstack_target_gatt(app
        "${application_gatt}")
else()
    message(STATUS "Zephyr example ${BTSTACK_ZEPHYR_EXAMPLE}")
endif()
target_link_libraries(app PRIVATE btstack_zephyr)

# Register a complete source and include closure for every supported example.
# Zephyr builds one application at a time, so only the selected one needs a
# library target during this configure pass.
foreach(example IN LISTS BTSTACK_ZEPHYR_EXAMPLES)
    btstack_resolve_example_for_port(zephyr "${example}" example_groups example_configuration)
    btstack_filter_groups_for_port(example_base_groups
        GROUPS ${BTSTACK_ZEPHYR_BASE_GROUPS})
    btstack_resolve_groups(example_all_groups
        GROUPS ${example_groups} ${example_base_groups})
    btstack_collect_sources(example_sources
        GROUPS ${example_all_groups})
    list(APPEND example_sources
        ${BTSTACK_ZEPHYR_PORT_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/src/main.c")
    btstack_get_application_property(application_sources "${example}" SOURCES)
    foreach(source IN LISTS application_sources)
        list(APPEND example_sources "${source}")
    endforeach()
    list(REMOVE_DUPLICATES example_sources)
    list(SORT example_sources)
    string(REPLACE "-" "_" example_suffix "${example}")
    set_property(GLOBAL PROPERTY "btstack_info_zephyr_${example_suffix}_sources" "${example_sources}")

    btstack_collect_include_directories(example_group_include_directories
        GROUPS ${example_all_groups})
    set(example_include_directories
        ${BTSTACK_ZEPHYR_PUBLIC_INCLUDE_DIRECTORIES}
        ${example_group_include_directories}
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}"
        "${BTSTACK_ROOT}/chipset/zephyr"
        "${BTSTACK_ROOT}/platform/embedded"
        "${BTSTACK_ROOT}/platform/zephyr")
    list(REMOVE_DUPLICATES example_include_directories)
    list(SORT example_include_directories)
    set_property(GLOBAL PROPERTY
        "btstack_info_zephyr_${example_suffix}_include_directories"
        "${example_include_directories}")
endforeach()

btstack_add_info_targets()
