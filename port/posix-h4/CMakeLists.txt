cmake_minimum_required (VERSION 3.18)

project(BTstack-posix-h4)

SET(BTSTACK_ROOT ${CMAKE_SOURCE_DIR}/../..)

# Enable undefined and address sanitizers
#  add_compile_options(
#      -fsanitize=undefined -fsanitize=address
#  )
#  add_link_options(
#      -fsanitize=undefined -fsanitize=address
#  )

# local dir for btstack_config.h after build dir to avoid using .h from Makefile
include_directories(.)


include_directories(${BTSTACK_ROOT}/3rd-party/micro-ecc)
include_directories(${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/include)
include_directories(${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/include)
include_directories(${BTSTACK_ROOT}/3rd-party/lc3-google/include)
include_directories(${BTSTACK_ROOT}/3rd-party/md5)
include_directories(${BTSTACK_ROOT}/3rd-party/hxcmod-player)
include_directories(${BTSTACK_ROOT}/3rd-party/hxcmod-player/mod)
include_directories(${BTSTACK_ROOT}/3rd-party/lwip/core/src/include)
include_directories(${BTSTACK_ROOT}/3rd-party/lwip/dhcp-server)
include_directories(${BTSTACK_ROOT}/3rd-party/rijndael)
include_directories(${BTSTACK_ROOT}/3rd-party/yxml)
include_directories(${BTSTACK_ROOT}/3rd-party/tinydir)
include_directories(${BTSTACK_ROOT}/3rd-party/qr-code-generator)
include_directories(${BTSTACK_ROOT}/src)
include_directories(${BTSTACK_ROOT}/chipset/bcm)
include_directories(${BTSTACK_ROOT}/chipset/csr)
include_directories(${BTSTACK_ROOT}/chipset/cc256x)
include_directories(${BTSTACK_ROOT}/chipset/em9301)
include_directories(${BTSTACK_ROOT}/chipset/realtek)
include_directories(${BTSTACK_ROOT}/chipset/tc3566x)
include_directories(${BTSTACK_ROOT}/chipset/stlc2500d)
include_directories(${BTSTACK_ROOT}/chipset/zephyr)
include_directories(${BTSTACK_ROOT}/platform/embedded)
include_directories(${BTSTACK_ROOT}/platform/lwip)
include_directories(${BTSTACK_ROOT}/platform/lwip/port)
include_directories(${BTSTACK_ROOT}/platform/posix)

# BTstack sources from src/. Platform, third-party, chipset, and port-local
# sources remain below because they are selected by this port.
include(${BTSTACK_ROOT}/tool/cmake/BTstack.cmake)
btstack_collect_sources(SOURCES_BTSTACK
 ROOT "${BTSTACK_ROOT}"
 GROUPS
    audio-demo
    ble
    bnep
    classic
    core
    gatt-services
    lc3
    le-audio
    le-device-db-tlv
    link-key-db-tlv
    mesh
    tlv-none
    transport-h4
)
file(GLOB SOURCES_EXAMPLE_UTIL "${BTSTACK_ROOT}/example/*_demo_util*.c")
file(GLOB SOURCES_BLUEDROID "${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/srce/*.c" "${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/srce/*.c")
file(GLOB SOURCES_UECC      "${BTSTACK_ROOT}/3rd-party/micro-ecc/uECC.c")
file(GLOB SOURCES_HXCMOD    "${BTSTACK_ROOT}/3rd-party/hxcmod-player/*.c"  "${BTSTACK_ROOT}/3rd-party/hxcmod-player/mods/*.c")
file(GLOB SOURCES_MD5       "${BTSTACK_ROOT}/3rd-party/md5/md5.c")
file(GLOB SOURCES_RIJNDAEL  "${BTSTACK_ROOT}/3rd-party/rijndael/rijndael.c")
file(GLOB SOURCES_YXML      "${BTSTACK_ROOT}/3rd-party/yxml/yxml.c")
file(GLOB SOURCES_QRCODEGEN "${BTSTACK_ROOT}/3rd-party/qr-code-generator/qrcodegen.c")
btstack_collect_platform_sources(SOURCES_PLATFORM
 ROOT "${BTSTACK_ROOT}"
 GROUPS platform-posix
)
file(GLOB SOURCES_BCM       "${BTSTACK_ROOT}/chipset/bcm/*.c")
file(GLOB SOURCES_CSR       "${BTSTACK_ROOT}/chipset/csr/*.c")
file(GLOB SOURCES_EM9301    "${BTSTACK_ROOT}/chipset/em9301/*.c")
file(GLOB SOURCES_STLC2500D "${BTSTACK_ROOT}/chipset/stlc2500d/*.c")
file(GLOB SOURCES_TC2566X   "${BTSTACK_ROOT}/chipset/tc3566x/*.c")
file(GLOB SOURCES_REALTEK   "${BTSTACK_ROOT}/chipset/realtek/*.c")
file(GLOB SOURCES_ZEPHYR    "${BTSTACK_ROOT}/chipset/zephyr/*.c")
file(GLOB SOURCES_LC3_GOOGLE "${BTSTACK_ROOT}/3rd-party/lc3-google/src/*.c")
file(GLOB SOURCES_PORT      "*.c")

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
  ${BTSTACK_ROOT}//platform/lwip/bnep_lwip.c
  )
set (SOURCES_LWIP ${LWIP_CORE_SRC} ${LWIP_IPV4_SRC} ${LWIP_NETIF_SRC} ${LWIP_HTTPD} ${LWIP_DHCPD} ${LWIP_PORT})

set(SOURCES
  ${SOURCES_BCM}
  ${SOURCES_BLUEDROID}
  ${SOURCES_CSR}
  ${SOURCES_EM9301}
  ${SOURCES_HXCMOD}
  ${SOURCES_LC3_GOOGLE}
  ${SOURCES_MD5}
  ${SOURCES_PORT}
  ${SOURCES_REALTEK}
  ${SOURCES_RIJNDAEL}
  ${SOURCES_STLC2500D}
  ${SOURCES_TC2566X}
  ${SOURCES_UECC}
  ${SOURCES_PLATFORM}
  ${SOURCES_BTSTACK}
  ${SOURCES_EXAMPLE_UTIL}
  ${SOURCES_YXML}
  ${SOURCES_ZEPHYR}
  ${SOURCES_QRCODEGEN}
)
list(SORT SOURCES)

# create static lib
add_library(btstack STATIC ${SOURCES})
btstack_target_warnings(btstack STRICT)

# pkgconfig
find_package(PkgConfig)

# portaudio
if (PkgConfig_FOUND)
    pkg_check_modules(PORTAUDIO portaudio-2.0)
    if(PORTAUDIO_FOUND)
        message("HAVE_PORTAUDIO")
        target_include_directories(btstack PUBLIC ${PORTAUDIO_INCLUDE_DIRS})
        target_link_directories(btstack PUBLIC ${PORTAUDIO_LIBRARY_DIRS})
        target_link_libraries(btstack ${PORTAUDIO_LIBRARIES})
        add_compile_definitions(HAVE_PORTAUDIO)
    endif()
endif()

# pthread
find_package(Threads)
target_link_libraries(btstack ${CMAKE_THREAD_LIBS_INIT})

# libm
target_link_libraries(btstack m)


# Add CC256x Support and specify init script
set (CC256X_INIT_SCRIPT bluetooth_init_cc2564C_1.5.c)
include(${BTSTACK_ROOT}/chipset/cc256x/cc256x.cmake)
target_sources(btstack PRIVATE
    ${BTSTACK_ROOT}/chipset/cc256x/btstack_chipset_cc256x.c
    ${CC256X_INIT_SCRIPT_SOURCES}
)
add_dependencies(btstack ${CC256X_INIT_SCRIPT_TARGETS})

# Add BCM Support and download PatchRAM files during the build.
include(${BTSTACK_ROOT}/chipset/bcm/btstack_bcm_patchram.cmake)
btstack_add_firmware_download(
    TARGET bcm43430a1_patchram
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${BTSTACK_BCM43430A1_PATCHRAM_FILE}"
    URL "${BTSTACK_BCM43430A1_PATCHRAM_URL}"
    SHA256 "${BTSTACK_BCM43430A1_PATCHRAM_SHA256}"
)
btstack_add_firmware_download(
    TARGET cyw55310_patchram
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${BTSTACK_CYW55310_PATCHRAM_FILE}"
    URL "${BTSTACK_CYW55310_PATCHRAM_URL}"
    SHA256 "${BTSTACK_CYW55310_PATCHRAM_SHA256}"
)
add_dependencies(btstack bcm43430a1_patchram cyw55310_patchram)

# Get the declared examples. Feature-based filtering will be provided by the
# future resolver; for now this port builds every declared example.
include(${BTSTACK_ROOT}/example/requirements.cmake)
set(EXAMPLES ${btstack_declared_examples})
list(REMOVE_ITEM EXAMPLES "mesh_node_demo")

file(GLOB GATT_FILES "${BTSTACK_ROOT}/src/*/gatt-service/*.gatt")

# create targets
foreach(EXAMPLE ${EXAMPLES})
    # Get the example sources from its declaration.
    set(SOURCES_EXAMPLE)
    foreach(EXAMPLE_SOURCE IN LISTS btstack_example_${EXAMPLE}_sources)
        list(APPEND SOURCES_EXAMPLE ${BTSTACK_ROOT}/example/${EXAMPLE_SOURCE})
    endforeach()

    add_executable(${EXAMPLE} ${SOURCES_EXAMPLE})

    # Compile the declared GATT DB, if any.
    set(EXAMPLE_GATT_DB ${btstack_example_${EXAMPLE}_gatt_db})
    if (EXAMPLE_GATT_DB)
        message("example ${EXAMPLE} -- with GATT DB")
        btstack_target_gatt(${EXAMPLE} "${BTSTACK_ROOT}/example/${EXAMPLE_GATT_DB}"
            DEPENDS ${GATT_FILES})
    else()
        message("example ${EXAMPLE}")
    endif()
    btstack_target_warnings(${EXAMPLE} STRICT)
    target_link_libraries(${EXAMPLE} btstack pthread)
endforeach(EXAMPLE)
