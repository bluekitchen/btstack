
cmake_minimum_required(VERSION 3.13)

set(PROJECT rp2040-vela-cyw20820)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(${PROJECT} C CXX)

pico_sdk_init()

# Pico SDK's TinyUSB audio-device implementation intentionally uses switches
# without default clauses. Keep BTstack warnings strict while suppressing this
# third-party warning for every example that pulls in tinyusb_device.
if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    set_source_files_properties(
        "${PICO_SDK_PATH}/lib/tinyusb/src/class/audio/audio_device.c"
        PROPERTIES COMPILE_OPTIONS "-Wno-switch-default")
endif()

# Use this checkout's BTstack source catalog rather than the broad BTstack
# libraries bundled with pico-sdk. Pico SDK still supplies the RP2040-specific
# run loop, flash bank, and stdin implementations below.
get_filename_component(BTSTACK_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
include(${BTSTACK_ROOT}/tool/cmake/BTstack.cmake)

btstack_declare_port(rp2040-vela-if820)
btstack_declare_port_capability(CAPABILITY audio
    PUBLIC_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}")

# The feature macros live in btstack_config.h. Keeping ENABLE_BLE and
# ENABLE_CLASSIC in that file (rather than inheriting them from pico-sdk's
# pico_btstack_ble/classic targets) makes it the single source of truth.
btstack_configure_port(OUTPUT BTSTACK_CONFIG_MACROS
    CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/btstack_config.h")

string(JOIN ", " BTSTACK_CONFIG_MACROS_DISPLAY ${BTSTACK_CONFIG_MACROS})
message(STATUS "BTstack config ENABLE_*/HAVE_* macros: ${BTSTACK_CONFIG_MACROS_DISPLAY}")

# port specific source files
set(SOURCES_PORT
    hal_audio_tinyusb.c
    hal_uart_dma_pico.c
    main.c
    uac2.c
    usb_descriptors.c
    # pico-sdk integrations that normally arrive through pico_btstack_base.
    ${PICO_SDK_PATH}/src/rp2_common/pico_btstack/btstack_flash_bank.c
    ${PICO_SDK_PATH}/src/rp2_common/pico_btstack/btstack_run_loop_async_context.c
    ${PICO_SDK_PATH}/src/rp2_common/pico_btstack/btstack_stdin_pico.c
)

# This port has an RP2040 USB audio implementation, but no LED backend.
include(${BTSTACK_ROOT}/example/requirements.cmake)
set(EXAMPLES ${btstack_declared_applications})
btstack_filter_examples_for_port(EXAMPLES
    CANDIDATES ${EXAMPLES})
# The port does not configure the Mesh feature or its provisioning backend.
list(REMOVE_ITEM EXAMPLES mesh_node_demo)

set(BTSTACK_RP2040_VELA_BASE_GROUPS
    core
    ble
    le-device-db-tlv
    link-key-db-tlv
    transport-h4
    third-party-micro-ecc
    third-party-rijndael
    platform-embedded-audio
    platform-embedded-hci-dump-stdout
    platform-embedded-tlv-flash-bank
    platform-embedded-uart-block)

# pico-sdk and TinyUSB provide INTERFACE sources to their consumers. They are
# compiled as part of the BTstack library below, so do not impose BTstack's
# warning-as-error policy on this externally owned code.
set(BTSTACK_RP2040_VELA_PRIVATE_COMPILE_OPTIONS)
if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    list(APPEND BTSTACK_RP2040_VELA_PRIVATE_COMPILE_OPTIONS -Wno-error)
endif()

foreach(EXAMPLE IN LISTS EXAMPLES)
    btstack_add_application_library(
        TARGET btstack_${EXAMPLE}_library
        APPLICATION ${EXAMPLE}
        BASE_GROUPS ${BTSTACK_RP2040_VELA_BASE_GROUPS}
        PORT_SOURCES ${SOURCES_PORT}
        PRIVATE_COMPILE_OPTIONS ${BTSTACK_RP2040_VELA_PRIVATE_COMPILE_OPTIONS}
        PUBLIC_INCLUDE_DIRECTORIES
            "${CMAKE_CURRENT_SOURCE_DIR}"
            "${PICO_SDK_PATH}/src/rp2_common/pico_btstack/include"
        LINK_LIBRARIES
            hardware_uart
            hardware_dma
            pico_async_context_poll
            pico_async_context_base
            pico_flash
            pico_sync
            pico_stdlib
            pico_stdio_usb
            tinyusb_device
            tinyusb_board)

    btstack_add_application_executable(${EXAMPLE} APPLICATION ${EXAMPLE})

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
        # pico-sdk >= 2.3 requires the reset interface number when custom
        # TinyUSB descriptors are used. CDC occupies interfaces 0 and 1.
        PICO_USB_RESET_MS_OS_20_DESCRIPTOR_ITF=2
    )


    # Pico SDK and TinyUSB add implementation sources to executable targets,
    # so their warning policy must remain independent of BTstack's strict
    # warning policy.
    target_link_libraries(${EXAMPLE} btstack_${EXAMPLE}_library)

    pico_add_extra_outputs(${EXAMPLE})

    pico_enable_stdio_usb(${EXAMPLE} 1)
    pico_enable_stdio_uart(${EXAMPLE} 0)

endforeach()

btstack_add_info_targets()
