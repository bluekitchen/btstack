cmake_minimum_required(VERSION 3.13)

set(PROJECT rp2040-tinyusb)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(${PROJECT} C CXX ASM)

pico_sdk_init()

# Sources shared by every BTstack example built by this port.
if (COMMAND pico_add_library)
    pico_add_library(rp2040_tinyusb_port NOFLAG)
else()
    # pico_add_library is provided by Pico SDK versions that bundle BTstack.
    # Keep the port CMake-compatible with plain Pico SDK installations too.
    add_library(rp2040_tinyusb_port INTERFACE)
endif()
target_sources(rp2040_tinyusb_port INTERFACE
    hci_transport_usb_tinyusb.c
    main.c
)

# Create examples, except those that need a local audio implementation.
include(${PICO_BTSTACK_PATH}/example/CMakeLists.txt)
set(EXAMPLES ${EXAMPLES_GENERAL} ${EXAMPLES_CLASSIC_ONLY} ${EXAMPLES_LE_ONLY} ${EXAMPLES_DUAL_MODE})
list(REMOVE_DUPLICATES EXAMPLES)
list(REMOVE_ITEM EXAMPLES
    "a2dp_sink_demo" "a2dp_source_demo" "hfp_ag_demo" "hfp_hf_demo"
    "hsp_ag_demo" "hsp_hs_demo" "led_demo" "mesh_node_demo" "mod_player")

foreach(EXAMPLE ${EXAMPLES})
    add_executable(${EXAMPLE})

    target_include_directories(${EXAMPLE} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
    )

    target_sources(${EXAMPLE} PUBLIC
        ${PICO_BTSTACK_PATH}/example/${EXAMPLE}.c
    )

    if ("${EXAMPLES_GATT_FILES}" MATCHES ${EXAMPLE})
        if (COMMAND pico_btstack_make_gatt_header)
            pico_btstack_make_gatt_header(${EXAMPLE} PRIVATE
                "${PICO_BTSTACK_PATH}/example/${EXAMPLE}.gatt")
        else()
            set(GATT_HEADER ${CMAKE_CURRENT_BINARY_DIR}/${EXAMPLE}.h)
            add_custom_command(
                OUTPUT ${GATT_HEADER}
                COMMAND ${Python3_EXECUTABLE} ${PICO_BTSTACK_PATH}/tool/compile_gatt.py
                    ${PICO_BTSTACK_PATH}/example/${EXAMPLE}.gatt ${GATT_HEADER}
                DEPENDS ${PICO_BTSTACK_PATH}/example/${EXAMPLE}.gatt
                    ${PICO_BTSTACK_PATH}/tool/compile_gatt.py
                VERBATIM)
            add_custom_target(${EXAMPLE}_gatt_header DEPENDS ${GATT_HEADER})
            add_dependencies(${EXAMPLE} ${EXAMPLE}_gatt_header)
            target_include_directories(${EXAMPLE} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
        endif()
    endif()

    target_link_libraries(${EXAMPLE} PUBLIC
        rp2040_tinyusb_port
        pico_async_context_poll
        pico_sync
        pico_btstack_ble
        pico_btstack_classic
        pico_btstack_flash_bank
        pico_btstack_run_loop_async_context
        pico_btstack_run_loop_async_context_headers
        pico_stdlib
        tinyusb_host
        tinyusb_board
    )

    pico_add_extra_outputs(${EXAMPLE})

    # USB is used for the Bluetooth dongle in host mode. Console output uses
    # the Pico's UART (GPIO 0/1) instead.
    pico_enable_stdio_usb(${EXAMPLE} 0)
    pico_enable_stdio_uart(${EXAMPLE} 1)
endforeach()
