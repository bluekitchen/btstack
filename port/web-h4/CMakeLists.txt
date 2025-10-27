cmake_minimum_required(VERSION 3.13)
project(BTstack-Web-H4 C)

# Require C99
set(CMAKE_C_STANDARD 99)

# Path to custom HTML shell
set(SHELL_FILE_TEMPLATE ${CMAKE_SOURCE_DIR}/html/shell_template.html)

# Path to interop js code
set(PRE_JS ${CMAKE_SOURCE_DIR}/js/interop.js)

# Allow to set path BTstack repository
if( NOT DEFINED ${BTSTACK_ROOT} )
    if( NOT DEFINED ENV{EXAMPLE} )
        set(BTSTACK_ROOT ${CMAKE_SOURCE_DIR}/../..)
    else()
        set(EXAMPLE $ENV{EXAMPLE})
    endif()
endif()
message("BTstack root: ${BTSTACK_ROOT}")

# to generate .h from .gatt files
find_package(Python REQUIRED COMPONENTS Interpreter)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include("${BTSTACK_ROOT}/tool/cmake/BTstackUtils.cmake")

# Include dirs
include_directories(src)
include_directories(${BTSTACK_ROOT}/3rd-party/micro-ecc)
include_directories(${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/include)
include_directories(${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/include)
include_directories(${BTSTACK_ROOT}/3rd-party/lc3-google/include)
include_directories(${BTSTACK_ROOT}/3rd-party/md5)
include_directories(${BTSTACK_ROOT}/3rd-party/hxcmod-player)
include_directories(${BTSTACK_ROOT}/3rd-party/hxcmod-player/mod)
include_directories(${BTSTACK_ROOT}/3rd-party/lc3-google/include)
include_directories(${BTSTACK_ROOT}/3rd-party/lwip/core/src/include)
include_directories(${BTSTACK_ROOT}/3rd-party/lwip/dhcp-server)
include_directories(${BTSTACK_ROOT}/3rd-party/rijndael)
include_directories(${BTSTACK_ROOT}/3rd-party/yxml)
include_directories(${BTSTACK_ROOT}/3rd-party/tinydir)
include_directories(${BTSTACK_ROOT}/3rd-party/qr-code-generator)
include_directories(${BTSTACK_ROOT}/3rd-party/stb)
include_directories(${BTSTACK_ROOT}/src)
include_directories(${BTSTACK_ROOT}/chipset/bcm)
include_directories(${BTSTACK_ROOT}/chipset/zephyr)
include_directories(${BTSTACK_ROOT}/platform/embedded)
include_directories(${BTSTACK_ROOT}/platform/lwip)
include_directories(${BTSTACK_ROOT}/platform/lwip/port)
include_directories(${BTSTACK_ROOT}/platform/posix)


file(GLOB SOURCES_SRC
        "${BTSTACK_ROOT}/src/*.c"
        "${BTSTACK_ROOT}/example/sco_demo_util.c"
        "${BTSTACK_ROOT}/example/audio_player_demo_util.c"
)
file(GLOB SOURCES_BLE       "${BTSTACK_ROOT}/src/ble/*.c")
file(GLOB SOURCES_BLUEDROID "${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/srce/*.c" "${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/srce/*.c")
file(GLOB SOURCES_CLASSIC   "${BTSTACK_ROOT}/src/classic/*.c")
file(GLOB SOURCES_GATT      "${BTSTACK_ROOT}/src/ble/gatt-service/*.c")
file(GLOB SOURCES_UECC      "${BTSTACK_ROOT}/3rd-party/micro-ecc/uECC.c")
file(GLOB SOURCES_HXCMOD    "${BTSTACK_ROOT}/3rd-party/hxcmod-player/*.c"  "${BTSTACK_ROOT}/3rd-party/hxcmod-player/mods/*.c")
file(GLOB SOURCES_LE_AUDIO  "${BTSTACK_ROOT}/src/le-audio/*.c" "${BTSTACK_ROOT}/src/le-audio/gatt-service/*.c")
file(GLOB SOURCES_MD5       "${BTSTACK_ROOT}/3rd-party/md5/md5.c")
file(GLOB SOURCES_RIJNDAEL  "${BTSTACK_ROOT}/3rd-party/rijndael/rijndael.c")
file(GLOB SOURCES_YXML      "${BTSTACK_ROOT}/3rd-party/yxml/yxml.c")
file(GLOB SOURCES_QRCODEGEN "${BTSTACK_ROOT}/3rd-party/qr-code-generator/qrcodegen.c")
file(GLOB SOURCES_BCM       "${BTSTACK_ROOT}/chipset/bcm/*.c")
file(GLOB SOURCES_ZEPHYR    "${BTSTACK_ROOT}/chipset/zephyr/*.c")
file(GLOB SOURCES_LC3_GOOGLE "${BTSTACK_ROOT}/3rd-party/lc3-google/src/*.c")
file(GLOB SOURCES_PORT       "${CMAKE_SOURCE_DIR}/src/*.c")

file(GLOB SOURCES_BLE_OFF "${BTSTACK_ROOT}/src/ble/le_device_db_memory.c")
list(REMOVE_ITEM SOURCES_BLE   ${SOURCES_BLE_OFF})

file(GLOB SOURCES_POSIX_OFF "${BTSTACK_ROOT}/platform/posix/le_device_db_fs.c")
list(REMOVE_ITEM SOURCES_POSIX   ${SOURCES_POSIX_OFF})

set(SOURCES
        ${SOURCES_BLE}
        ${SOURCES_BCM}
        ${SOURCES_BLUEDROID}
        ${SOURCES_CLASSIC}
        ${SOURCES_GATT}
        ${SOURCES_HXCMOD}
        ${SOURCES_LC3_GOOGLE}
        # ${SOURCES_LE_AUDIO}
        ${SOURCES_LIBUSB}
        ${SOURCES_MD5}
        ${SOURCES_PORT}
        ${SOURCES_REALTEK}
        ${SOURCES_RIJNDAEL}
        ${SOURCES_SRC}
        ${SOURCES_CSR}
        ${SOURCES_STLC2500D}
        ${SOURCES_TC2566X}
        ${SOURCES_UECC}
        ${SOURCES_YXML}
        ${SOURCES_ZEPHYR}
        ${SOURCES_QRCODEGEN}
        ${BTSTACK_ROOT}/platform/embedded/hci_dump_embedded_stdout.c
)
list(SORT SOURCES)

# create static lib from sources
add_library(btstack STATIC ${SOURCES})


# create targets for all examples
file(GLOB EXAMPLES_C    "${BTSTACK_ROOT}/example/*.c")
list(SORT EXAMPLES_C)
file(GLOB EXAMPLES_GATT "${BTSTACK_ROOT}/example/*.gatt")

# remove some
file(GLOB EXAMPLES_OFF
    "${BTSTACK_ROOT}/example/*demo_util*.c"
    "${BTSTACK_ROOT}/example/ant_test.c"
    "${BTSTACK_ROOT}/example/avrcp_browsing_client.c"
    "${BTSTACK_ROOT}/example/mesh_node_demo.c"
    "${BTSTACK_ROOT}/example/pbap_client_demo.c"
    "${BTSTACK_ROOT}/example/panu_demo.c"
    "${BTSTACK_ROOT}/example/pan_lwip_http_server.c"
)
list(REMOVE_ITEM EXAMPLES_C ${EXAMPLES_OFF})

# Export functions for JavaScript interop
set(EXPORTED_FUNCTIONS
    "_main"
    "_main_set_initial_baudrate"
    "_main_set_log_level"
    "_main_set_hci_log"
    "_btstack_main"
    # STDIN
    "_btstack_stdin_js_process"
    # Run Loop
    "_btstack_run_loop_js_execute_once"
    "_btstack_run_loop_js_process_timer"
    # HCI Transport H4
    "_hci_transport_h4_get_receive_buffer"
    "_hci_transport_h4_get_receive_buffer_size"
    "_hci_transport_h4_process_packet"
    "_hci_transport_h4_packet_sent"
)
string(JOIN "," EXPORTED_FUNCTIONS_STR ${EXPORTED_FUNCTIONS})

# create targets
foreach(EXAMPLE_FILE ${EXAMPLES_C})
    get_filename_component(EXAMPLE ${EXAMPLE_FILE} NAME_WE)

    # Your sources
    message("Building example: ${EXAMPLE}")
    add_executable(${EXAMPLE} ${EXAMPLE_FILE})
    if ( "${EXAMPLES_GATT}" MATCHES ${EXAMPLE} )
        target_gatt(${EXAMPLE} ${BTSTACK_ROOT}/example/${EXAMPLE}.gatt)
    endif()

    # --- Emscripten specific settings ---
    # Tell CMake/Emscripten to output .html instead of just .wasm/.js
    set_target_properties(${EXAMPLE} PROPERTIES SUFFIX ".html")

    target_link_options(${EXAMPLE} PRIVATE
        "-sEXPORTED_FUNCTIONS=[${EXPORTED_FUNCTIONS_STR}]"
        "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap']"
        "--pre-js" "${PRE_JS}"
    )

    # Ensure CMake re-links when shell_minimal.html or interop.js changes
    set_property(TARGET ${EXAMPLE} APPEND PROPERTY LINK_DEPENDS ${SHELL_FILE_TEMPLATE} ${PRE_JS})

    # Optional: optimize
    target_compile_options(${EXAMPLE} PRIVATE "-O3")
    target_link_options(${EXAMPLE} PRIVATE "-O3")

    # Configured Shell HTML
    set (SHELL_FILE ${CMAKE_CURRENT_BINARY_DIR}/${EXAMPLE}.in.html)
    configure_file(${SHELL_FILE_TEMPLATE} ${SHELL_FILE})

    # Provide custom HTML shell (instead of default)
    set_target_properties(${EXAMPLE} PROPERTIES LINK_FLAGS "--shell-file ${SHELL_FILE}")

    # Add our JS files
    add_custom_command(TARGET ${EXAMPLE} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_SOURCE_DIR}/js/buffered_reader.js
            ${CMAKE_CURRENT_BINARY_DIR}/buffered_reader.js
    )

    # Link example against static library
    target_link_libraries(${EXAMPLE} PRIVATE btstack)

endforeach(EXAMPLE_FILE)
