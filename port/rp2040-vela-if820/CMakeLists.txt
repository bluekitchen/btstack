
cmake_minimum_required(VERSION 3.13)

set(PROJECT rp2040-vela-cyw20820)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(${PROJECT} C CXX)

pico_sdk_init()

# Source files for HCI transport and UART driver
pico_add_library(pico_btstack_hci_transport_h4 NOFLAG)
target_sources(pico_btstack_hci_transport_h4 INTERFACE
    ${PICO_BTSTACK_PATH}/src/hci_transport_h4.c
    ${PICO_BTSTACK_PATH}/platform/embedded/btstack_uart_block_embedded.c
)

# port specific source files
pico_add_library(vela_port NOFLAG)
target_sources(vela_port INTERFACE
    ${PICO_BTSTACK_PATH}/platform/embedded/btstack_audio_embedded.c
    btstack_chunk_buffer.c
    hal_audio_tinyusb.c
    hal_uart_dma_pico.c
    main.c
    uac2.c
    usb_descriptors.c
)

# create examples
# get list of examples, skipping mesh_node_demo
include(${PICO_BTSTACK_PATH}/example/CMakeLists.txt)
set (EXAMPLES ${EXAMPLES_GENERAL} ${EXAMPLES_CLASSIC_ONLY}  ${EXAMPLES_LE_ONLY} ${EXAMPLES_DUAL_MODE})
list(REMOVE_DUPLICATES EXAMPLES)
list(REMOVE_ITEM EXAMPLES "mesh_node_demo" "hsp_ag_demo" "hsp_hs_demo" "hfp_ag_demo" "hfp_hf_demo" "led_demo")
set (EXAMPLES_MODPLAYER "a2dp_source_demo" "mod_player")
set (EXAMPLES_SBC "a2dp_source_demo" "a2dp_sink_demo")

foreach(EXAMPLE ${EXAMPLES})

    add_executable(${EXAMPLE})

    target_include_directories(${EXAMPLE} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
    )

    target_sources(${EXAMPLE} PUBLIC
        ${PICO_BTSTACK_PATH}/example/${EXAMPLE}.c
    )

    target_compile_definitions(${EXAMPLE} PRIVATE
        # Uncomment for HCI traces
        # WANT_HCI_DUMP=1

        # Re-use pico-sdk implementation for USB CDC and Reset via vendor interface
        PICO_STDIO_USB=1
        PICO_STDIO_USB_ENABLE_IRQ_BACKGROUND_TASK=1
        PICO_STDIO_USB_ENABLE_TINYUSB_INIT=1
        PICO_STDIO_USB_ENABLE_RESET_VIA_VENDOR_INTERFACE=1
        PICO_STDIO_USB_RESET_INTERFACE_SUPPORT_RESET_TO_BOOTSEL=1
        PICO_STDIO_USB_RESET_INTERFACE_SUPPORT_MS_OS_20_DESCRIPTOR=1
        PICO_STDIO_USB_RESET_BOOTSEL_INTERFACE_DISABLE_MASK=0
    )

    # create .h file from .gatt if needed
    if ( "${EXAMPLES_GATT_FILES}" MATCHES ${EXAMPLE} )
        message("example ${EXAMPLE} -- with GATT DB")
        pico_btstack_make_gatt_header(${EXAMPLE} PRIVATE "${PICO_BTSTACK_PATH}/example/${EXAMPLE}.gatt")
    else()
        message("example ${EXAMPLE}")
    endif()

    # add mod player sources for mod player examples
    if ( "${EXAMPLES_MODPLAYER}" MATCHES ${EXAMPLE} )
        target_include_directories(${EXAMPLE} PUBLIC
            ${PICO_BTSTACK_PATH}/3rd-party/hxcmod-player/
        )
        file(GLOB SOURCES_HXCMOD
                "${PICO_BTSTACK_PATH}/3rd-party/hxcmod-player/*.c"
                "${PICO_BTSTACK_PATH}/3rd-party/hxcmod-player/mods/*.c")
        target_sources(${EXAMPLE} PUBLIC ${SOURCES_HXCMOD})
    endif()

    # add SBC codec for a2dp examples
    if ( "${EXAMPLES_SBC}" MATCHES ${EXAMPLE} )
        target_link_libraries(${EXAMPLE} PUBLIC
            pico_btstack_sbc_common
            pico_btstack_sbc_encoder
            pico_btstack_sbc_decoder
        )
    endif()

    target_link_libraries(${EXAMPLE} PUBLIC
        vela_port
        hardware_uart
        hardware_dma
        pico_async_context_poll
        pico_sync
        pico_btstack_ble
        pico_btstack_classic
        pico_btstack_flash_bank
        pico_btstack_hci_transport_h4
        pico_btstack_run_loop_async_context
        pico_btstack_run_loop_async_context_headers
        pico_stdlib
        pico_stdio_usb
        tinyusb_device
        tinyusb_board
    )

    pico_add_extra_outputs(${EXAMPLE})

    pico_enable_stdio_usb(${EXAMPLE} 1)
    pico_enable_stdio_uart(${EXAMPLE} 0)

endforeach(EXAMPLE)