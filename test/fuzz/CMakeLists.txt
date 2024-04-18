cmake_minimum_required (VERSION 3.5)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang)

project(BTstack-Fuzzer)

SET(BTSTACK_ROOT ${CMAKE_SOURCE_DIR}/../..)

include_directories(../../3rd-party/micro-ecc)
include_directories(../../3rd-party/bluedroid/decoder/include)
include_directories(../../3rd-party/bluedroid/encoder/include)
include_directories(../../3rd-party/md5)
include_directories(../../3rd-party/lc3-google/include)
include_directories(../../3rd-party/hxcmod-player)
include_directories(../../3rd-party/hxcmod-player/mod)
include_directories(../../3rd-party/lwip/core/src/include)
include_directories(../../3rd-party/lwip/dhcp-server)
include_directories(../../3rd-party/rijndael)
include_directories(../../3rd-party/yxml)
include_directories(../../3rd-party/tinydir)
include_directories(../../src)
include_directories(../../platform/posix)
include_directories(../../platform/embedded)
include_directories(../../platform/lwip)
include_directories(../../platform/lwip/port)
include_directories(.)

file(GLOB SOURCES_SRC       "../../src/*.c" "../../example/sco_demo_util.c")
file(GLOB SOURCES_BLE       "../../src/ble/*.c")
file(GLOB SOURCES_GATT      "../../src/ble/gatt-service/*.c")
file(GLOB SOURCES_CLASSIC   "../../src/classic/*.c")
file(GLOB SOURCES_MESH      "../../src/mesh/*.c")
file(GLOB SOURCES_BLUEDROID "../../3rd-party/bluedroid/encoder/srce/*.c" "../../3rd-party/bluedroid/decoder/srce/*.c")
file(GLOB SOURCES_MD5       "../../3rd-party/md5/md5.c")
file(GLOB SOURCES_UECC      "../../3rd-party/micro-ecc/uECC.c")
file(GLOB SOURCES_YXML      "../../3rd-party/yxml/yxml.c")
file(GLOB SOURCES_HXCMOD    "../../3rd-party/hxcmod-player/*.c"  "../../3rd-party/hxcmod-player/mods/*.c")
file(GLOB SOURCES_RIJNDAEL  "../../3rd-party/rijndael/rijndael.c")
file(GLOB SOURCES_POSIX     "../../platform/posix/*.c")
file(GLOB SOURCES_LIBUSB    "../../port/libusb/*.c" "../../platform/libusb/*.c")
file(GLOB SOURCES_LC3_GOOGLE "../../3rd-party/lc3-google/src/*.c")

set(LWIP_CORE_SRC
        ../../3rd-party/lwip/core/src/core/def.c
        ../../3rd-party/lwip/core/src/core/inet_chksum.c
        ../../3rd-party/lwip/core/src/core/init.c
        ../../3rd-party/lwip/core/src/core/ip.c
        ../../3rd-party/lwip/core/src/core/mem.c
        ../../3rd-party/lwip/core/src/core/memp.c
        ../../3rd-party/lwip/core/src/core/netif.c
        ../../3rd-party/lwip/core/src/core/pbuf.c
        ../../3rd-party/lwip/core/src/core/tcp.c
        ../../3rd-party/lwip/core/src/core/tcp_in.c
        ../../3rd-party/lwip/core/src/core/tcp_out.c
        ../../3rd-party/lwip/core/src/core/timeouts.c
        ../../3rd-party/lwip/core/src/core/udp.c
        )
set (LWIP_IPV4_SRC
        ../../3rd-party/lwip/core/src/core/ipv4/acd.c
        ../../3rd-party/lwip/core/src/core/ipv4/dhcp.c
        ../../3rd-party/lwip/core/src/core/ipv4/etharp.c
        ../../3rd-party/lwip/core/src/core/ipv4/icmp.c
        ../../3rd-party/lwip/core/src/core/ipv4/ip4.c
        ../../3rd-party/lwip/core/src/core/ipv4/ip4_addr.c
        ../../3rd-party/lwip/core/src/core/ipv4/ip4_frag.c
        )
set (LWIP_NETIF_SRC
        ../../3rd-party/lwip/core/src/netif/ethernet.c
        )
set (LWIP_HTTPD
        ../../3rd-party/lwip/core/src/apps/http/altcp_proxyconnect.c
        ../../3rd-party/lwip/core/src/apps/http/fs.c
        ../../3rd-party/lwip/core/src/apps/http/httpd.c
        )
set (LWIP_DHCPD
        ../../3rd-party/lwip/dhcp-server/dhserver.c
        )
set (LWIP_PORT
        ../../platform/lwip/port/sys_arch.c
        ../../platform/lwip/bnep_lwip.c
        )

set (SOURCES_LWIP ${LWIP_CORE_SRC} ${LWIP_IPV4_SRC} ${LWIP_NETIF_SRC} ${LWIP_HTTPD} ${LWIP_DHCPD} ${LWIP_PORT})

file(GLOB SOURCES_BLE_OFF "../../src/ble/le_device_db_memory.c")
list(REMOVE_ITEM SOURCES_BLE   ${SOURCES_BLE_OFF})

file(GLOB SOURCES_POSIX_OFF "../../platform/posix/le_device_db_fs.c")
list(REMOVE_ITEM SOURCES_POSIX ${SOURCES_POSIX_OFF})

set(SOURCES
        ${SOURCES_MD5}
        ${SOURCES_YXML}
        ${SOURCES_BLUEDROID}
        ${SOURCES_POSIX}
        ${SOURCES_RIJNDAEL}
        ${SOURCES_SRC}
        ${SOURCES_BLE}
        ${SOURCES_GATT}
        ${SOURCES_MESH}
        ${SOURCES_CLASSIC}
        ${SOURCES_UECC}
        ${SOURCES_HXCMOD}
        ${SOURCES_LC3_GOOGLE}
        )
list(SORT SOURCES)

add_compile_options( -g -fsanitize=fuzzer,address -fprofile-instr-generate -fcoverage-mapping)
add_link_options(       -fsanitize=fuzzer,address -fprofile-instr-generate -fcoverage-mapping)

# create static lib
add_library(btstack STATIC ${SOURCES})

# create fuzz targets
file(GLOB TARGETS_C "fuzz_*.c")

# create targets
foreach(TARGET_FILE ${TARGETS_C})
    get_filename_component(EXAMPLE ${TARGET_FILE} NAME_WE)

    if ( "fuzz_l2cap" MATCHES ${EXAMPLE} )
        add_executable(${EXAMPLE} ${TARGET_FILE}
            ${BTSTACK_ROOT}/src/l2cap.c
            ${BTSTACK_ROOT}/src/l2cap_signaling.c
            ${BTSTACK_ROOT}/src/btstack_run_loop.c
            ${BTSTACK_ROOT}/src/btstack_util.c
            ${BTSTACK_ROOT}/src/btstack_linked_list.c
            ${BTSTACK_ROOT}/src/btstack_memory.c
            ${BTSTACK_ROOT}/src/btstack_memory_pool.c
            ${BTSTACK_ROOT}/src/hci_cmd.c
            ${BTSTACK_ROOT}/src/hci_dump.c
            ${SOURCES_POSIX}
        )
    else()
        add_executable(${EXAMPLE} ${TARGET_FILE} )
        target_link_libraries(${EXAMPLE} btstack)
    endif()
endforeach(TARGET_FILE)
