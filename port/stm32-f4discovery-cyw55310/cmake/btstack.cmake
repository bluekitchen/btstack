SET(CMAKE_OSX_SYSROOT /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk)
SET(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#set(CMAKE_BUILD_TYPE RelWithDebInfo)

set(BTSTACK_ROOT ${CMAKE_SOURCE_DIR}/../..)
if( DEFINED ENV{BTSTACK_ROOT} )
    set(BTSTACK_ROOT $ENV{BTSTACK_ROOT})
endif()

# to generate .h from .gatt files
find_package(Python3 REQUIRED COMPONENTS Interpreter)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# local dir for btstack_config.h after build dir to avoid using .h from Makefile
set(INCLUDES_PATH "" )
list(APPEND INCLUDES_PATH 3rd-party/micro-ecc )
list(APPEND INCLUDES_PATH 3rd-party/bluedroid/decoder/include)
list(APPEND INCLUDES_PATH 3rd-party/bluedroid/encoder/include)
list(APPEND INCLUDES_PATH 3rd-party/lc3-google/include)
list(APPEND INCLUDES_PATH 3rd-party/md5)
list(APPEND INCLUDES_PATH 3rd-party/hxcmod-player)
list(APPEND INCLUDES_PATH 3rd-party/hxcmod-player/mod)
list(APPEND INCLUDES_PATH 3rd-party/lwip/core/src/include)
list(APPEND INCLUDES_PATH 3rd-party/lwip/dhcp-server)
list(APPEND INCLUDES_PATH 3rd-party/rijndael)
list(APPEND INCLUDES_PATH 3rd-party/segger-rtt)
list(APPEND INCLUDES_PATH 3rd-party/yxml)
list(APPEND INCLUDES_PATH 3rd-party/tinydir)
list(APPEND INCLUDES_PATH src)
list(APPEND INCLUDES_PATH chipset/bcm)
list(APPEND INCLUDES_PATH platform/embedded)
list(APPEND INCLUDES_PATH platform/lwip)
list(APPEND INCLUDES_PATH platform/lwip/port)
list(APPEND INCLUDES_PATH example )
list(TRANSFORM INCLUDES_PATH PREPEND ${BTSTACK_ROOT}/)
list(APPEND INCLUDES_PATH . )
list(APPEND INCLUDES_PATH port )
include_directories( ${INCLUDES_PATH} )

file(GLOB SOURCES_SRC            "${BTSTACK_ROOT}/src/*.c" )

file(GLOB SOURCES_BLE            "${BTSTACK_ROOT}/src/ble/*.c")
file(GLOB SOURCES_GATT           "${BTSTACK_ROOT}/src/ble/gatt-service/*.c")
file(GLOB SOURCES_CLASSIC        "${BTSTACK_ROOT}/src/classic/*.c")
file(GLOB SOURCES_LE_AUDIO       "${BTSTACK_ROOT}/src/le-audio/*.c" "${BTSTACK_ROOT}/src/le-audio/gatt-service/*.c")
file(GLOB SOURCES_MESH           "${BTSTACK_ROOT}/src/mesh/*.c" "${BTSTACK_ROOT}/src/mesh/gatt-service/*.c")
file(GLOB SOURCES_BLUEDROID      "${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/srce/*.c" "${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/srce/*.c")
file(GLOB SOURCES_MD5            "${BTSTACK_ROOT}/3rd-party/md5/md5.c")
file(GLOB SOURCES_UECC           "${BTSTACK_ROOT}/3rd-party/micro-ecc/uECC.c")
file(GLOB SOURCES_YXML           "${BTSTACK_ROOT}/3rd-party/yxml/yxml.c")
file(GLOB SOURCES_HXCMOD         "${BTSTACK_ROOT}/3rd-party/hxcmod-player/*.c"  "${BTSTACK_ROOT}/3rd-party/hxcmod-player/mods/*.c")
file(GLOB SOURCES_RIJNDAEL       "${BTSTACK_ROOT}/3rd-party/rijndael/rijndael.c")
file(GLOB SOURCES_EMBEDDED       "${BTSTACK_ROOT}/platform/embedded/*.c")
file(GLOB SOURCES_CHIPSET_BCM    "${BTSTACK_ROOT}/chipset/bcm/*.c")
file(GLOB SOURCES_LC3_GOOGLE     "${BTSTACK_ROOT}/3rd-party/lc3-google/src/*.c")

set( SOURCES_SEGGER_RTT
    ${BTSTACK_ROOT}/3rd-party/segger-rtt/SEGGER_RTT.c
    ${BTSTACK_ROOT}/3rd-party/segger-rtt/SEGGER_RTT_Syscalls_GCC.c
    ${BTSTACK_ROOT}/3rd-party/segger-rtt/SEGGER_RTT_printf.c
)

set(LWIP_CORE_SRC
    3rd-party/lwip/core/src/core/def.c
    3rd-party/lwip/core/src/core/inet_chksum.c
    3rd-party/lwip/core/src/core/init.c
    3rd-party/lwip/core/src/core/ip.c
    3rd-party/lwip/core/src/core/mem.c
    3rd-party/lwip/core/src/core/memp.c
    3rd-party/lwip/core/src/core/netif.c
    3rd-party/lwip/core/src/core/pbuf.c
    3rd-party/lwip/core/src/core/tcp.c
    3rd-party/lwip/core/src/core/tcp_in.c
    3rd-party/lwip/core/src/core/tcp_out.c
    3rd-party/lwip/core/src/core/timeouts.c
    3rd-party/lwip/core/src/core/udp.c
)
set (LWIP_IPV4_SRC
    3rd-party/lwip/core/src/core/ipv4/acd.c
    3rd-party/lwip/core/src/core/ipv4/dhcp.c
    3rd-party/lwip/core/src/core/ipv4/etharp.c
    3rd-party/lwip/core/src/core/ipv4/icmp.c
    3rd-party/lwip/core/src/core/ipv4/ip4.c
    3rd-party/lwip/core/src/core/ipv4/ip4_addr.c
    3rd-party/lwip/core/src/core/ipv4/ip4_frag.c
)
set (LWIP_NETIF_SRC
    3rd-party/lwip/core/src/netif/ethernet.c
)
set (LWIP_HTTPD
    3rd-party/lwip/core/src/apps/http/altcp_proxyconnect.c
    3rd-party/lwip/core/src/apps/http/fs.c
    3rd-party/lwip/core/src/apps/http/httpd.c
)
set (LWIP_DHCPD
    3rd-party/lwip/dhcp-server/dhserver.c
)
set (LWIP_PORT
    platform/lwip/port/sys_arch.c
    platform/lwip/bnep_lwip.c
)

set (SOURCES_LWIP ${LWIP_CORE_SRC} ${LWIP_IPV4_SRC} ${LWIP_NETIF_SRC} ${LWIP_HTTPD} ${LWIP_DHCPD} ${LWIP_PORT})
list(TRANSFORM SOURCES_LWIP PREPEND ${BTSTACK_ROOT}/)

#file(GLOB SOURCES_SRC_OFF "${BTSTACK_ROOT}/src/hci_transport_*.c")
#list(REMOVE_ITEM SOURCES_SRC   ${SOURCES_SRC_OFF})

file(GLOB SOURCES_BLE_OFF "${BTSTACK_ROOT}/src/ble/att_db_util.c")
list(REMOVE_ITEM SOURCES_BLE   ${SOURCES_BLE_OFF})

list(APPEND SOURCES_LE_AUDIO "${BTSTACK_ROOT}/example/le_audio_demo_util_sink.c")
list(APPEND SOURCES_LE_AUDIO "${BTSTACK_ROOT}/example/le_audio_demo_util_source.c")

set(SOURCES
    ${SOURCES_MD5}
    ${SOURCES_YXML}
    ${SOURCES_BLUEDROID}
    ${SOURCES_EMBEDDED}
    ${SOURCES_RIJNDAEL}
    ${SOURCES_LC3_GOOGLE}
    ${SOURCES_SRC}
    ${SOURCES_BLE}
    ${SOURCES_CLASSIC}
    ${SOURCES_GATT}
#    ${SOURCES_LE_AUDIO}
    ${SOURCES_UECC}
    ${SOURCES_SEGGER_RTT}
    ${SOURCES_HXCMOD}
    ${SOURCES_CHIPSET_BCM}
)
list(SORT SOURCES)

# create static lib
add_library(btstack OBJECT ${SOURCES})

# extra compiler warnings
target_compile_options(btstack PRIVATE
     $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:
            -Wunused-variable -Wswitch-default -Werror -Wall>
     $<$<CXX_COMPILER_ID:GNU>:
            -Wunused-but-set-variable -Wunused-variable -Wswitch-default -Werror -Wall>
     $<$<CXX_COMPILER_ID:MSVC>:
            /W4>)

set_target_properties( btstack PROPERTIES
    C_STANDARD 11
    C_EXTENSIONS ON
)
