cmake_minimum_required (VERSION 3.5)

SET(CMAKE_OSX_SYSROOT /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk)
SET(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#set(CMAKE_BUILD_TYPE RelWithDebInfo)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(BTstack-Zephyr)

set(BTSTACK_ROOT ${CMAKE_SOURCE_DIR}/../../)
if( DEFINED ENV{BTSTACK_ROOT} )
    set(BTSTACK_ROOT $ENV{BTSTACK_ROOT})
endif()

# pkgconfig
find_package(PkgConfig REQUIRED)

# to generate .h from .gatt files
find_package (Python REQUIRED COMPONENTS Interpreter)
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
list(APPEND INCLUDES_PATH 3rd-party/yxml)
list(APPEND INCLUDES_PATH 3rd-party/tinydir)
list(APPEND INCLUDES_PATH src)
list(APPEND INCLUDES_PATH chipset/zephyr)
list(APPEND INCLUDES_PATH platform/embedded)
list(APPEND INCLUDES_PATH platform/lwip)
list(APPEND INCLUDES_PATH platform/lwip/port)
list(TRANSFORM INCLUDES_PATH PREPEND ${BTSTACK_ROOT}/)
list(APPEND INCLUDES_PATH . )
include_directories( ${INCLUDES_PATH} )

file(GLOB SOURCES_SRC            "${BTSTACK_ROOT}/src/*.c" ) #"${BTSTACK_ROOT}/example/sco_demo_util.c")

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
file(GLOB SOURCES_CHIPSET_ZEPHYR "${BTSTACK_ROOT}/chipset/zephyr/*.c")
file(GLOB SOURCES_LC3_GOOGLE     "${BTSTACK_ROOT}/3rd-party/lc3-google/src/*.c")

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

file(GLOB SOURCES_SRC_OFF "${BTSTACK_ROOT}/src/hci_transport_*.c")
list(REMOVE_ITEM SOURCES_SRC   ${SOURCES_SRC_OFF})

file(GLOB SOURCES_BLE_OFF "${BTSTACK_ROOT}/src/ble/att_db_util.c")
list(REMOVE_ITEM SOURCES_BLE   ${SOURCES_BLE_OFF})

set(SOURCES
    ${SOURCES_MD5}
    ${SOURCES_YXML}
    ${SOURCES_BLUEDROID}
    ${SOURCES_EMBEDDED}
    ${SOURCES_RIJNDAEL}
    ${SOURCES_LC3_GOOGLE}
    ${SOURCES_SRC}
    ${SOURCES_BLE}
    ${SOURCES_GATT}
    ${SOURCES_LE_AUDIO}
    ${SOURCES_UECC}
    ${SOURCES_HXCMOD}
    ${SOURCES_CHIPSET_ZEPHYR}
)
list(SORT SOURCES)

# create static lib
add_library(btstack STATIC ${SOURCES})

# extra compiler warnings
target_compile_options(btstack PRIVATE
     $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:
            -Wunused-variable -Wswitch-default -Werror -Wall>
     $<$<CXX_COMPILER_ID:GNU>:
            -Wunused-but-set-variable -Wunused-variable -Wswitch-default -Werror -Wall>
     $<$<CXX_COMPILER_ID:MSVC>:
            /W4>)

# get zephyr's idea of build flags
zephyr_get_include_directories_for_lang(       C includes)
zephyr_get_system_include_directories_for_lang(C system_includes)
zephyr_get_compile_definitions_for_lang(       C definitions)
zephyr_get_compile_options_for_lang(           C options)

set(external_project_cflags
    ${includes}$<SEMICOLON>${definitions}$<SEMICOLON>${options}$<SEMICOLON>${system_includes}
  )

# prevent imacros argument from deduplication
string( REGEX REPLACE "\\$<TARGET_PROPERTY:compiler,imacros>\\$<SEMICOLON>([^\\$]+)"
    "\$<1:SHELL:-imacros \\1>"
     _all_options ${external_project_cflags} )

target_compile_options( btstack PUBLIC ${_all_options})
target_compile_options( btstack PUBLIC -std=c11 )

set(EXAMPLES "gatt_counter" )
if(DEFINED ENV{EXAMPLE})
    set( EXAMPLES $ENV{EXAMPLE} )
endif()

list(SORT EXAMPLES)
file(GLOB EXAMPLES_GATT "${BTSTACK_ROOT}/example/*.gatt")

# on Mac 10.14, adding lwip to libstack results in a yet not understood link error
# workaround: add lwip sources only to lwip_examples
set (LWIP_EXAMPLES pan_lwip_http_server)

# create targets
foreach(EXAMPLE ${EXAMPLES})
    #   get_filename_component(EXAMPLE ${EXAMPLE_FILE} NAME_WE)

    # get c file, either from examples or local
    if( EXISTS "${BTSTACK_ROOT}/example/${EXAMPLE}.c" )
        set (SOURCES_EXAMPLE ${BTSTACK_ROOT}/example/${EXAMPLE}.c)
    else()
        set (SOURCES_EXAMPLE ${EXAMPLE}.c)
    endif()
    list(APPEND SOURCES_EXAMPLE "${CMAKE_SOURCE_DIR}/src/main.c")
    message("${BTSTACK_ROOT}")
    message("${SOURCES_EXAMPLE}")
    # add lwip sources for lwip examples
    if ( "${LWIP_EXAMPLES}" MATCHES ${EXAMPLE} )
        list(APPEND SOURCES_EXAMPLE ${SOURCES_LWIP})
    endif()

    # add GATT DB creation
    if ( "${EXAMPLES_GATT}" MATCHES ${EXAMPLE} )
        message("example ${EXAMPLE} -- with GATT DB")
        add_custom_command(
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${EXAMPLE}.h
            DEPENDS ${BTSTACK_ROOT}/example/${EXAMPLE}.gatt
            COMMAND ${Python_EXECUTABLE}
            ARGS ${BTSTACK_ROOT}/tool/compile_gatt.py ${BTSTACK_ROOT}/example/${EXAMPLE}.gatt ${CMAKE_CURRENT_BINARY_DIR}/${EXAMPLE}.h
        )
        list(APPEND SOURCES_EXAMPLE ${CMAKE_CURRENT_BINARY_DIR}/${EXAMPLE}.h)
    else()
        message("example ${EXAMPLE}")
    endif()
    target_sources( app PRIVATE ${SOURCES_EXAMPLE} )
    target_link_libraries( app PRIVATE btstack )
endforeach(EXAMPLE)
