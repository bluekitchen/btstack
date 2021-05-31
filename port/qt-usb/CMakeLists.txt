cmake_minimum_required(VERSION 3.5)

project(qt-usb LANGUAGES C CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

message("${Qt5_DIR}")

find_package(Qt5Core)
include(FindPythonInterp)

# BTstack Root
set(BTSTACK_ROOT "../..")
message("BTSTACK_ROOT: ${BTSTACK_ROOT}")

# BTstack include
include_directories(${BTSTACK_ROOT}/3rd-party/micro-ecc)
include_directories(${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/include)
include_directories(${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/include)
include_directories(${BTSTACK_ROOT}/3rd-party/md5)
include_directories(${BTSTACK_ROOT}/3rd-party/hxcmod-player)
include_directories(${BTSTACK_ROOT}/3rd-party/hxcmod-player/mod)
include_directories(${BTSTACK_ROOT}/3rd-party/lwip/core/src/include)
include_directories(${BTSTACK_ROOT}/3rd-party/lwip/dhcp-server)
include_directories(${BTSTACK_ROOT}/3rd-party/rijndael)
include_directories(${BTSTACK_ROOT}/3rd-party/yxml)
include_directories(${BTSTACK_ROOT}/3rd-party/tinydir)
include_directories(${BTSTACK_ROOT}/src)
include_directories(${BTSTACK_ROOT}/platform/posix)
include_directories(${BTSTACK_ROOT}/platform/embedded)
include_directories(${BTSTACK_ROOT}/platform/lwip)
include_directories(${BTSTACK_ROOT}/platform/lwip/port)
include_directories(${BTSTACK_ROOT}/platform/qt)
include_directories(.)

# BTstack sources
file(GLOB SOURCES_SRC       "${BTSTACK_ROOT}/src/*.c" "${BTSTACK_ROOT}/example/sco_demo_util.c")
file(GLOB SOURCES_BLE       "${BTSTACK_ROOT}/src/ble/*.c")
file(GLOB SOURCES_GATT      "${BTSTACK_ROOT}/src/ble/gatt-service/*.c")
file(GLOB SOURCES_CLASSIC   "${BTSTACK_ROOT}/src/classic/*.c")
file(GLOB SOURCES_MESH      "${BTSTACK_ROOT}/src/mesh/*.c")
file(GLOB SOURCES_BLUEDROID "${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/srce/*.c" "${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/srce/*.c")
file(GLOB SOURCES_MD5       "${BTSTACK_ROOT}/3rd-party/md5/md5.c")
file(GLOB SOURCES_UECC      "${BTSTACK_ROOT}/3rd-party/micro-ecc/uECC.c")
file(GLOB SOURCES_YXML      "${BTSTACK_ROOT}/3rd-party/yxml/yxml.c")
file(GLOB SOURCES_HXCMOD    "${BTSTACK_ROOT}/3rd-party/hxcmod-player/*.c"  "${BTSTACK_ROOT}/3rd-party/hxcmod-player/mods/*.c")
file(GLOB SOURCES_RIJNDAEL  "${BTSTACK_ROOT}/3rd-party/rijndael/rijndael.c")

set(SOURCES_POSIX
    ${BTSTACK_ROOT}/platform/posix/btstack_audio_portaudio.c
    ${BTSTACK_ROOT}/platform/posix/btstack_tlv_posix.c
    ${BTSTACK_ROOT}/platform/posix/hci_dump_posix_fs.c
    ${BTSTACK_ROOT}/platform/posix/wav_util.c
)


set(LWIP_CORE_SRC
        ${BTSTACK_ROOT}/3rd-party/lwip/core/src/core/def.c
        ${BTSTACK_ROOT}/3rd-party/lwip/core/src/core/inet_chksum.c
        ${BTSTACK_ROOT}/3rd-party/lwip/core/src/core/init.c
        ${BTSTACK_ROOT}/3rd-party/lwip/core/src/core/ip.c
        ${BTSTACK_ROOT}/3rd-party/lwip/core/src/core/mem.c
        ${BTSTACK_ROOT}/3rd-party/lwip/core/src/core/memp.c
        ${BTSTACK_ROOT}/3rd-party/lwip/core/src/core/netif.c
        ${BTSTACK_ROOT}/3rd-party/lwip/core/src/core/pbuf.c
        ${BTSTACK_ROOT}/3rd-party/lwip/core/src/core/tcp.c
        ${BTSTACK_ROOT}/3rd-party/lwip/core/src/core/tcp_in.c
        ${BTSTACK_ROOT}/3rd-party/lwip/core/src/core/tcp_out.c
        ${BTSTACK_ROOT}/3rd-party/lwip/core/src/core/timeouts.c
        ${BTSTACK_ROOT}/3rd-party/lwip/core/src/core/udp.c
)
set (LWIP_IPV4_SRC
        ${BTSTACK_ROOT}/3rd-party/lwip/core/src/core/ipv4/acd.c
        ${BTSTACK_ROOT}/3rd-party/lwip/core/src/core/ipv4/dhcp.c
        ${BTSTACK_ROOT}/3rd-party/lwip/core/src/core/ipv4/etharp.c
        ${BTSTACK_ROOT}/3rd-party/lwip/core/src/core/ipv4/icmp.c
        ${BTSTACK_ROOT}/3rd-party/lwip/core/src/core/ipv4/ip4.c
        ${BTSTACK_ROOT}/3rd-party/lwip/core/src/core/ipv4/ip4_addr.c
        ${BTSTACK_ROOT}/3rd-party/lwip/core/src/core/ipv4/ip4_frag.c
)
set (LWIP_NETIF_SRC
        ${BTSTACK_ROOT}/3rd-party/lwip/core/src/netif/ethernet.c
)
set (LWIP_HTTPD
        ${BTSTACK_ROOT}/3rd-party/lwip/core/src/apps/http/altcp_proxyconnect.c
        ${BTSTACK_ROOT}/3rd-party/lwip/core/src/apps/http/fs.c
        ${BTSTACK_ROOT}/3rd-party/lwip/core/src/apps/http/httpd.c
)
set (LWIP_DHCPD
        ${BTSTACK_ROOT}/3rd-party/lwip/dhcp-server/dhserver.c
)
set (LWIP_PORT
        ${BTSTACK_ROOT}/platform/lwip/port/sys_arch.c
        ${BTSTACK_ROOT}/platform/lwip/bnep_lwip.c
)

set (SOURCES_LWIP ${LWIP_CORE_SRC} ${LWIP_IPV4_SRC} ${LWIP_NETIF_SRC} ${LWIP_HTTPD} ${LWIP_DHCPD} ${LWIP_PORT})

file(GLOB SOURCES_BLE_OFF   "${BTSTACK_ROOT}/src/ble/le_device_db_memory.c")
list(REMOVE_ITEM SOURCES_BLE ${SOURCES_BLE_OFF})


# select POSIX vs Windows versions
IF (WIN32)
    message("Building for Windows using WinUSB")
    set(SOURCES_HCI_USB ${BTSTACK_ROOT}/platform/windows/hci_transport_h2_winusb.c)
    set(SOURCES_STDIN   ${BTSTACK_ROOT}/platform/windows/btstack_stdin_windows.c)
    link_libraries( winusb setupapi)
ELSE()
    message("Building for POSIX using libusb")

    # fink pkgconfig
    find_package(PkgConfig REQUIRED)

    # libusb
    pkg_check_modules(LIBUSB REQUIRED libusb-1.0)
    include_directories(${LIBUSB_INCLUDE_DIRS})
    link_directories(${LIBUSB_LIBRARY_DIRS})
    link_libraries(${LIBUSB_LIBRARIES})

    set(SOURCES_HCI_USB ${BTSTACK_ROOT}/platform/libusb/hci_transport_h2_libusb.c)
    set(SOURCES_STDIN   ${BTSTACK_ROOT}/platform/posix/btstack_stdin_posix.c)

    list(APPEND SOURCES_POSIX ${BTSTACK_ROOT}/platform/posix/btstack_network_posix.c)
ENDIF()

set(SOURCES
        ${SOURCES_MD5}
        ${SOURCES_YXML}
        ${SOURCES_BLUEDROID}
        ${SOURCES_POSIX}
        ${SOURCES_RIJNDAEL}
        ${SOURCES_HCI_USB}
        ${SOURCES_STDIN}
        ${SOURCES_SRC}
        ${SOURCES_BLE}
        ${SOURCES_GATT}
        ${SOURCES_MESH}
        ${SOURCES_CLASSIC}
        ${SOURCES_UECC}
        ${SOURCES_HXCMOD}
)
list(SORT SOURCES)

# create static lib
add_library(btstack-lib STATIC ${SOURCES})

# create targets for all examples
file(GLOB EXAMPLES_C    "${BTSTACK_ROOT}/example/*.c")
list(SORT EXAMPLES_C)
file(GLOB EXAMPLES_GATT "${BTSTACK_ROOT}/example/*.gatt")

# remove some
file(GLOB EXAMPLES_OFF  "${BTSTACK_ROOT}/example/panu_demo.c" "${BTSTACK_ROOT}/example/sco_demo_util.c" "${BTSTACK_ROOT}/example/ant_test.c" "${BTSTACK_ROOT}/example/avrcp_browsing_client.c")
list(REMOVE_ITEM EXAMPLES_C ${EXAMPLES_OFF})

# on Mac 10.14, adding lwip to libstack results in a yet not understood link error
# workaround: add lwip sources only to lwip_examples
set (LWIP_EXAMPLES pan_lwip_http_server)

# create targets
foreach(EXAMPLE_FILE ${EXAMPLES_C})
        get_filename_component(EXAMPLE ${EXAMPLE_FILE} NAME_WE)
        set (SOURCE_FILES ${EXAMPLE_FILE})

        # add qt main.cpp and run loop
        list(APPEND SOURCE_FILES ${BTSTACK_ROOT}/platform/qt/btstack_run_loop_qt.cpp main.cpp)

        # add lwip sources for lwip examples
        if ( "${LWIP_EXAMPLES}" MATCHES ${EXAMPLE} )
                list(APPEND SOURCE_FILES ${SOURCES_LWIP})
        endif()

        # add GATT DB creation
        if ( "${EXAMPLES_GATT}" MATCHES ${EXAMPLE} )
                message("example ${EXAMPLE} -- with GATT DB")
                add_custom_command(
                    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${EXAMPLE}.h
                        COMMAND  ${PYTHON_EXECUTABLE}
                        ARGS ${BTSTACK_ROOT}/tool/compile_gatt.py ${BTSTACK_ROOT}/example/${EXAMPLE}.gatt ${CMAKE_CURRENT_BINARY_DIR}/${EXAMPLE}.h
                )
                list(APPEND SOURCE_FILES ${CMAKE_CURRENT_BINARY_DIR}/${EXAMPLE}.h)

                # generated file does not need AUTOMOC and AUOUIC processing
                set_property(SOURCE {CMAKE_CURRENT_BINARY_DIR}/${EXAMPLE}.h PROPERTY SKIP_AUTOGEN ON)
        else()
                message("example ${EXAMPLE}")
        endif()
        add_executable(${EXAMPLE} ${SOURCE_FILES} )
        target_link_libraries(${EXAMPLE} btstack-lib Qt5::Core)
endforeach(EXAMPLE_FILE)
