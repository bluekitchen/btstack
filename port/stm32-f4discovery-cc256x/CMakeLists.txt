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

# Include BTStack
include(cmake/btstack.cmake)

# Add CC256x Support and specify init script
set(CC256X_INIT_SCRIPT bluetooth_init_cc2564C_1.5.c)
include(${BTSTACK_ROOT}/chipset/cc256x/cc256x.cmake)

# create targets for all examples
file(GLOB EXAMPLES_C    "../../example/*.c")
list(SORT EXAMPLES_C)
file(GLOB EXAMPLES_GATT "../../example/*.gatt")

# remove some
file(GLOB EXAMPLES_OFF
        "../../example/*demo_util*.c"
        "../../example/ant_test.c"
        "../../example/avrcp_browsing_client.c"
        "../../example/mesh_node_demo.c"
        "../../example/le_audio*"
        "../../example/panu_demo.c"
)
list(REMOVE_ITEM EXAMPLES_C ${EXAMPLES_OFF})

# .gatt files in src
file(GLOB GATT_FILES "../../src/*/gatt-service/*.gatt")

# on Mac 10.14, adding lwip to libstack results in a yet not understood link error
# workaround: add lwip sources only to lwip_examples
set (LWIP_EXAMPLES pan_lwip_http_server)

# Create targets
foreach(EXAMPLE_FILE ${EXAMPLES_C})
    # Extract the base name of the file (e.g., a2dp_sink_demo.c -> a2dp_sink_demo)
    get_filename_component(EXAMPLE ${EXAMPLE_FILE} NAME_WE)

    # get c file
    set (SOURCES_EXAMPLE ${BTSTACK_ROOT}/example/${EXAMPLE}.c)

    # add SCO Demo util
    if ("hfp_ag_demo hfp_hf_demo hsp_hs_demo hsp_ag_demo" MATCHES ${EXAMPLE})
        list(APPEND SOURCES_EXAMPLE ${BTSTACK_ROOT}/example/sco_demo_util.c)
    endif()

    # add lwip sources for lwip examples
    if ( "${LWIP_EXAMPLES}" MATCHES ${EXAMPLE} )
        list(APPEND SOURCES_EXAMPLE ${SOURCES_LWIP})
    endif()

    # add GATT DB creation
    if ( "${EXAMPLES_GATT}" MATCHES ${EXAMPLE} )
        message("example ${EXAMPLE} -- with GATT DB")
        add_custom_command(
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${EXAMPLE}.h
            DEPENDS ${BTSTACK_ROOT}/example/${EXAMPLE}.gatt ${GATT_FILES}
            COMMAND ${Python_EXECUTABLE}
            ARGS ${BTSTACK_ROOT}/tool/compile_gatt.py ${BTSTACK_ROOT}/example/${EXAMPLE}.gatt ${CMAKE_CURRENT_BINARY_DIR}/${EXAMPLE}.h
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
        stm32cubemx
        bsp
        btstack
    )

    # Add sources to the example
    target_sources(${EXAMPLE} PRIVATE
        # Add user sources here
        port/hal_audio_f4discovery.c
        port/hal_flash_bank_stm32.c
        port/port.c
        ${SOURCES_EXAMPLE}
    )

endforeach()