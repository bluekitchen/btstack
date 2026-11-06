cmake_minimum_required(VERSION 3.13)
project(BTstack-Web-H4 C)
set(CMAKE_C_STANDARD 99)

set(BTSTACK_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(SHELL_FILE_TEMPLATE ${CMAKE_CURRENT_SOURCE_DIR}/html/shell_template.html)
set(PRE_JS ${CMAKE_CURRENT_SOURCE_DIR}/js/interop.js)

include(${BTSTACK_ROOT}/tool/cmake/BTstack.cmake)
include(${BTSTACK_ROOT}/tool/cmake/BTstackFirmware.cmake)
include(${BTSTACK_ROOT}/chipset/bcm/btstack_bcm_patchram.cmake)
btstack_declare_port(web-h4)
btstack_collect_public_include_directories(BTSTACK_PUBLIC_INCLUDE_DIRECTORIES ROOT "${BTSTACK_ROOT}")

set(PATCHRAM_PATH "${CMAKE_CURRENT_BINARY_DIR}/${BTSTACK_CYW55310_PATCHRAM_FILE}")
btstack_add_firmware_download(
    TARGET web_h4_patchram_download
    OUTPUT "${PATCHRAM_PATH}"
    URL "${BTSTACK_CYW55310_PATCHRAM_URL}"
    SHA256 "${BTSTACK_CYW55310_PATCHRAM_SHA256}")

add_compile_definitions(HAVE_SDL2)
btstack_declare_port_provider(web-h4 CAPABILITY audio
    GROUPS platform-posix-audio-sdl2)
btstack_declare_port_provider(web-h4 CAPABILITY led
    PUBLIC_INCLUDE_DIRECTORIES "${BTSTACK_ROOT}/platform/embedded")

btstack_collect_port_configuration_macros(BTSTACK_CONFIG_MACROS
    CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/src/btstack_config.h"
    DEFINITIONS HAVE_SDL2)
btstack_set_port_configuration_macros(web-h4 ${BTSTACK_CONFIG_MACROS})

include(${BTSTACK_ROOT}/example/requirements.cmake)
set(EXAMPLES ${btstack_declared_examples})
btstack_filter_examples_for_port(EXAMPLES
    PORT web-h4
    CANDIDATES ${EXAMPLES})
# The browser H4 bridge does not implement the Mesh advertising bearer.
list(REMOVE_ITEM EXAMPLES mesh_node_demo)

set(BTSTACK_WEB_H4_GROUPS
    ble le-device-db-tlv link-key-db-tlv tlv-none
    chipset-bcm chipset-zephyr
    platform-embedded-hci-dump-stdout
    platform-posix-audio-sdl2
    third-party-micro-ecc third-party-rijndael third-party-qr-code-generator)
set(SOURCES_PORT
    src/btstack_run_loop_js.c
    src/btstack_stdin_js.c
    src/btstack_tlv_js.c
    src/hci_dump_js.c
    src/hci_transport_h4_js.c
    src/main.c)
set(EXPORTED_FUNCTIONS
    _main _main_set_initial_baudrate _main_start_with_patchram
    _main_set_airoc_download_mode _main_set_log_level _main_set_hci_log
    _btstack_main _btstack_stdin_js_process _btstack_run_loop_js_execute_once
    _btstack_run_loop_js_process_timer _hci_transport_h4_get_receive_buffer
    _hci_transport_h4_get_receive_buffer_size _hci_transport_h4_process_packet
    _hci_transport_h4_packet_sent)
string(JOIN "," EXPORTED_FUNCTIONS_STR ${EXPORTED_FUNCTIONS})

foreach(EXAMPLE IN LISTS EXAMPLES)
    btstack_add_scoped_example_library(
        TARGET btstack_${EXAMPLE}_library ROOT "${BTSTACK_ROOT}"
        PORT web-h4 EXAMPLE ${EXAMPLE}
        GROUP_OBJECT_TARGET_PREFIX btstack_web_h4
        BASE_GROUPS ${BTSTACK_WEB_H4_GROUPS} PORT_SOURCES ${SOURCES_PORT}
        PUBLIC_INCLUDE_DIRECTORIES ${BTSTACK_PUBLIC_INCLUDE_DIRECTORIES}
            "${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_dependencies(btstack_${EXAMPLE}_library web_h4_patchram_download)

    set(SOURCES_EXAMPLE)
    foreach(EXAMPLE_SOURCE IN LISTS btstack_example_${EXAMPLE}_sources)
        list(APPEND SOURCES_EXAMPLE ${BTSTACK_ROOT}/example/${EXAMPLE_SOURCE})
    endforeach()
    add_executable(${EXAMPLE} ${SOURCES_EXAMPLE})
    if(btstack_example_${EXAMPLE}_gatt_db)
        btstack_target_gatt(${EXAMPLE} "${BTSTACK_ROOT}/example/${btstack_example_${EXAMPLE}_gatt_db}")
    endif()
    btstack_target_warnings(${EXAMPLE} STRICT)
    target_compile_options(${EXAMPLE} PRIVATE -O3 -sUSE_SDL=2)
    target_link_options(${EXAMPLE} PRIVATE
        "-sEXPORTED_FUNCTIONS=[${EXPORTED_FUNCTIONS_STR}]"
        "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap']"
        -sASYNCIFY "-sASYNCIFY_IMPORTS=['hci_transport_h4_js_set_baudrate']"
        -sFETCH -sUSE_SDL=2 --pre-js "${PRE_JS}" -O3)
    set(SHELL_FILE ${CMAKE_CURRENT_BINARY_DIR}/${EXAMPLE}.in.html)
    configure_file(${SHELL_FILE_TEMPLATE} ${SHELL_FILE})
    set_target_properties(${EXAMPLE} PROPERTIES SUFFIX ".html"
        LINK_FLAGS "--shell-file ${SHELL_FILE}")
    set_property(TARGET ${EXAMPLE} APPEND PROPERTY LINK_DEPENDS ${SHELL_FILE_TEMPLATE} ${PRE_JS})
    add_custom_command(TARGET ${EXAMPLE} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/js/buffered_reader.js
            ${CMAKE_CURRENT_BINARY_DIR}/buffered_reader.js)
    target_link_libraries(${EXAMPLE} btstack_${EXAMPLE}_library)
endforeach()
