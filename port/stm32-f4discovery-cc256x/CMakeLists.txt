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

# Include BTstack and its scoped STM32F4 helper.
set(BTSTACK_ROOT ${CMAKE_SOURCE_DIR}/../..)
include(${BTSTACK_ROOT}/tool/cmake/BTstack.cmake)
include(${BTSTACK_ROOT}/tool/cmake/BTstackStm32F4.cmake)

# Add CC256x Support and specify init script
set(CC256X_INIT_SCRIPT bluetooth_init_cc2564C_1.5.c)
include(${BTSTACK_ROOT}/chipset/cc256x/cc256x.cmake)
set(SOURCES_PORT
    port/hal_audio_f4discovery.c port/hal_flash_bank_stm32.c port/port.c
    ${CC256X_INIT_SCRIPT_SOURCES}
)

# .gatt files in src
file(GLOB GATT_FILES "../../src/*/gatt-service/*.gatt")

# on Mac 10.14, adding lwip to libstack results in a yet not understood link error
# workaround: add lwip sources only to lwip_examples
set (LWIP_EXAMPLES pan_lwip_http_server)

btstack_add_stm32f4_examples(
    PORT stm32-f4discovery-cc256x ROOT "${BTSTACK_ROOT}"
    BASE_GROUPS core ble le-device-db-tlv link-key-db-tlv transport-h4 chipset-cc256x third-party-micro-ecc third-party-rijndael third-party-hxcmod third-party-segger-rtt platform-embedded-audio platform-embedded-run-loop platform-embedded-stdin platform-embedded-tlv-flash-bank platform-embedded-uart-block platform-embedded-hci-dump-segger-rtt
    PORT_SOURCES ${SOURCES_PORT}
    PRIVATE_INCLUDE_DIRECTORIES "${BTSTACK_ROOT}/chipset/cc256x"
    LINK_LIBRARIES stm32cubemx bsp
    FIRMWARE_TARGETS ${CC256X_INIT_SCRIPT_TARGETS}
    EXCLUDE_EXAMPLES avrcp_browsing_client mesh_node_demo le_audio_broadcast_sink_lite le_audio_broadcast_source_lite le_audio_unicast_gateway_lite le_audio_unicast_headset_lite
)

if(FALSE)
foreach(EXAMPLE ${EXAMPLES})
    set(SOURCES_EXAMPLE)
    foreach(EXAMPLE_SOURCE IN LISTS btstack_example_${EXAMPLE}_sources)
        list(APPEND SOURCES_EXAMPLE ${BTSTACK_ROOT}/example/${EXAMPLE_SOURCE})
    endforeach()

    # add GATT DB creation
    set(EXAMPLE_GATT_DB ${btstack_example_${EXAMPLE}_gatt_db})
    if (EXAMPLE_GATT_DB)
        message("example ${EXAMPLE} -- with GATT DB")
        add_custom_command(
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${EXAMPLE}.h
            DEPENDS ${BTSTACK_ROOT}/example/${EXAMPLE_GATT_DB} ${GATT_FILES}
            COMMAND ${Python3_EXECUTABLE}
            ARGS ${BTSTACK_ROOT}/tool/compile_gatt.py ${BTSTACK_ROOT}/example/${EXAMPLE_GATT_DB} ${CMAKE_CURRENT_BINARY_DIR}/${EXAMPLE}.h
        )
        list(APPEND SOURCES_EXAMPLE ${CMAKE_CURRENT_BINARY_DIR}/${EXAMPLE}.h)
    else()
        message("example ${EXAMPLE}")
    endif()

    # Create Ozone Project
    set(OZONE_PROJECT_FILE ${CMAKE_CURRENT_BINARY_DIR}/${EXAMPLE}.jdebug)
    configure_file(${CMAKE_SOURCE_DIR}/ozone.jdebug ${OZONE_PROJECT_FILE})
    list(APPEND SOURCES_EXAMPLE ${OZONE_PROJECT_FILE})

    # Create an executable target for the example
    add_executable(${EXAMPLE} ${SOURCES_EXAMPLE} )

    # Add project symbols (macros)
    target_compile_definitions(${EXAMPLE} PRIVATE
            # Add user-defined symbols
    )

    # Add include paths
    target_include_directories(${EXAMPLE} PRIVATE
        # Add user defined include paths
        port
    )

    # Link directories and libraries
    target_link_directories(${EXAMPLE} PRIVATE
        # Add user-defined library search paths
    )
    target_link_libraries(${EXAMPLE}
        stm32f4_discovery_cc256x_port
        stm32cubemx
        bsp
        btstack
    )

endforeach()
endif()
