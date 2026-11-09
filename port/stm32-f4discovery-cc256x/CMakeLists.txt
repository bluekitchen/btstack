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
set(CMAKE_PROJECT_NAME stm32f4-discovery-cc256x)

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

btstack_declare_port(stm32-f4discovery-cc256x)
btstack_collect_public_include_directories(BTSTACK_PUBLIC_INCLUDE_DIRECTORIES
    ROOT "${BTSTACK_ROOT}")
btstack_declare_port_capability(stm32-f4discovery-cc256x
    CAPABILITY audio
    GROUPS platform-embedded-audio
    PUBLIC_INCLUDE_DIRECTORIES
        "${CMAKE_CURRENT_SOURCE_DIR}/port"
        "${BTSTACK_ROOT}/platform/embedded")
btstack_declare_port_capability(stm32-f4discovery-cc256x
    CAPABILITY led
    PUBLIC_INCLUDE_DIRECTORIES
        "${CMAKE_CURRENT_SOURCE_DIR}/port"
        "${BTSTACK_ROOT}/platform/embedded")

# Add CC256x Support and specify init script
set(CC256X_INIT_SCRIPT bluetooth_init_cc2564C_1.5.c)
include(${BTSTACK_ROOT}/chipset/cc256x/cc256x.cmake)
set(SOURCES_PORT
    port/hal_audio_f4discovery.c port/hal_flash_bank_stm32.c port/port.c
    ${CC256X_INIT_SCRIPT_SOURCES}
)

btstack_collect_port_configuration_macros(BTSTACK_CONFIG_MACROS
    CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/port/btstack_config.h")
btstack_set_port_configuration_macros(stm32-f4discovery-cc256x ${BTSTACK_CONFIG_MACROS})

include(${BTSTACK_ROOT}/example/requirements.cmake)
set(EXAMPLES ${btstack_declared_examples})
btstack_filter_examples_for_port(EXAMPLES
    PORT stm32-f4discovery-cc256x
    CANDIDATES ${EXAMPLES})
list(REMOVE_ITEM EXAMPLES
    avrcp_browsing_client
    mesh_node_demo)

set(BTSTACK_STM32_F4_BASE_GROUPS
    core ble le-device-db-tlv link-key-db-tlv transport-h4 chipset-cc256x
    third-party-micro-ecc third-party-rijndael third-party-hxcmod
    third-party-segger-rtt platform-embedded-audio platform-embedded-run-loop
    platform-embedded-stdin platform-embedded-tlv-flash-bank
    platform-embedded-uart-block platform-embedded-hci-dump-segger-rtt)

foreach(EXAMPLE IN LISTS EXAMPLES)
    btstack_add_scoped_example_library(
        TARGET btstack_${EXAMPLE}_library
        ROOT "${BTSTACK_ROOT}"
        PORT stm32-f4discovery-cc256x
        EXAMPLE ${EXAMPLE}
        GROUP_OBJECT_TARGET_PREFIX btstack_stm32_f4discovery_cc256x
        BASE_GROUPS ${BTSTACK_STM32_F4_BASE_GROUPS}
        PORT_SOURCES ${SOURCES_PORT}
        PUBLIC_INCLUDE_DIRECTORIES
            ${BTSTACK_PUBLIC_INCLUDE_DIRECTORIES}
            "${CMAKE_CURRENT_SOURCE_DIR}/port"
        PRIVATE_INCLUDE_DIRECTORIES "${BTSTACK_ROOT}/chipset/cc256x"
        LINK_LIBRARIES stm32cubemx bsp)
    add_dependencies(btstack_${EXAMPLE}_library ${CC256X_INIT_SCRIPT_TARGETS})

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

btstack_add_info_targets(PORT stm32-f4discovery-cc256x)
