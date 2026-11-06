cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME stm32f4-discovery-cyw55310)

# Include toolchain file
include("cmake/gcc-arm-none-eabi.cmake")

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Set STM32F4Cube path
set(STM32F4CUBE_PATH "${CMAKE_SOURCE_DIR}/../../3rd-party/STM32CubeF4/")

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Select board
set(BSP STM32F4-Discovery)
set(BSP_COMPONENTS cs43l22 lis302dl lis3dsh)

# Include BSP
include(cmake/bsp.cmake)

# Build information
message("Build: ${CMAKE_BUILD_TYPE}\n")

# Include BTstack.
set(BTSTACK_ROOT ${CMAKE_SOURCE_DIR}/../..)
include(${BTSTACK_ROOT}/tool/cmake/BTstack.cmake)

btstack_declare_port(stm32-f4discovery-cyw55310)
btstack_collect_public_include_directories(BTSTACK_PUBLIC_INCLUDE_DIRECTORIES
    ROOT "${BTSTACK_ROOT}")
btstack_declare_port_provider(stm32-f4discovery-cyw55310
    CAPABILITY audio
    GROUPS platform-embedded-audio
    PUBLIC_INCLUDE_DIRECTORIES
        "${CMAKE_CURRENT_SOURCE_DIR}/port"
        "${BTSTACK_ROOT}/platform/embedded")
btstack_declare_port_provider(stm32-f4discovery-cyw55310
    CAPABILITY led
    PUBLIC_INCLUDE_DIRECTORIES
        "${CMAKE_CURRENT_SOURCE_DIR}/port"
        "${BTSTACK_ROOT}/platform/embedded")

# PatchRAM file for CYW55310
include(${BTSTACK_ROOT}/chipset/bcm/btstack_bcm_patchram.cmake)
include(${BTSTACK_ROOT}/tool/cmake/BTstackFirmware.cmake)
set(PATCHRAM ${BTSTACK_CYW55310_PATCHRAM_NAME})
set(PATCHRAM_FILE ${BTSTACK_CYW55310_PATCHRAM_FILE})
set(PATCHRAM_PATH ${CMAKE_CURRENT_BINARY_DIR}/${PATCHRAM_FILE})
btstack_add_firmware_download(
    TARGET cyw55310_patchram_download
    OUTPUT ${PATCHRAM_PATH}
    URL ${BTSTACK_CYW55310_PATCHRAM_URL}
    SHA256 ${BTSTACK_CYW55310_PATCHRAM_SHA256}
)

# Convert PatchRAM file into C
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${PATCHRAM}.c
    DEPENDS ${PATCHRAM_PATH} ${BTSTACK_ROOT}/chipset/bcm/convert_hcd.py
    COMMAND ${Python3_EXECUTABLE}
    ARGS ${BTSTACK_ROOT}/chipset/bcm/convert_hcd.py ${PATCHRAM_PATH} ${CMAKE_CURRENT_BINARY_DIR}
)

set(SOURCES_PORT port/hal_audio_f4discovery.c port/hal_flash_bank_stm32.c port/port.c ${CMAKE_CURRENT_BINARY_DIR}/${PATCHRAM}.c)

# Sources that implement this board's platform, including the generated
# PatchRAM array, are identical for every example. Build them once instead of
# adding them to each executable below.
add_custom_target(cyw55310_patchram_source DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${PATCHRAM}.c)
add_dependencies(cyw55310_patchram_source cyw55310_patchram_download)

btstack_collect_port_configuration_macros(BTSTACK_CONFIG_MACROS
    CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/port/btstack_config.h")
btstack_set_port_configuration_macros(stm32-f4discovery-cyw55310 ${BTSTACK_CONFIG_MACROS})

include(${BTSTACK_ROOT}/example/requirements.cmake)
set(EXAMPLES ${btstack_declared_examples})
btstack_filter_examples_for_port(EXAMPLES
    PORT stm32-f4discovery-cyw55310
    CANDIDATES ${EXAMPLES})

set(BTSTACK_STM32_F4_BASE_GROUPS
    core ble le-device-db-tlv link-key-db-tlv transport-h4 chipset-bcm
    third-party-micro-ecc third-party-rijndael third-party-hxcmod
    platform-embedded-audio platform-embedded-run-loop platform-embedded-stdin
    platform-embedded-tlv-flash-bank platform-embedded-uart-block
    platform-embedded-hci-dump-stdout)

foreach(EXAMPLE IN LISTS EXAMPLES)
    btstack_add_scoped_example_library(
        TARGET btstack_${EXAMPLE}_library
        ROOT "${BTSTACK_ROOT}"
        PORT stm32-f4discovery-cyw55310
        EXAMPLE ${EXAMPLE}
        GROUP_OBJECT_TARGET_PREFIX btstack_stm32_f4discovery_cyw55310
        BASE_GROUPS ${BTSTACK_STM32_F4_BASE_GROUPS}
        PORT_SOURCES ${SOURCES_PORT}
        PUBLIC_INCLUDE_DIRECTORIES
            ${BTSTACK_PUBLIC_INCLUDE_DIRECTORIES}
            "${CMAKE_CURRENT_SOURCE_DIR}/port"
        PRIVATE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_BINARY_DIR}"
        LINK_LIBRARIES stm32cubemx bsp)
    add_dependencies(btstack_${EXAMPLE}_library cyw55310_patchram_source)

    set(SOURCES_EXAMPLE)
    foreach(EXAMPLE_SOURCE IN LISTS btstack_example_${EXAMPLE}_sources)
        list(APPEND SOURCES_EXAMPLE ${BTSTACK_ROOT}/example/${EXAMPLE_SOURCE})
    endforeach()
    add_executable(${EXAMPLE} ${SOURCES_EXAMPLE})
    if(btstack_example_${EXAMPLE}_gatt_db)
        btstack_target_gatt(${EXAMPLE} "${BTSTACK_ROOT}/example/${btstack_example_${EXAMPLE}_gatt_db}")
    endif()
    btstack_target_warnings(${EXAMPLE} STRICT)
    target_link_libraries(${EXAMPLE} btstack_${EXAMPLE}_library)
endforeach()

btstack_add_info_targets(PORT stm32-f4discovery-cyw55310)
