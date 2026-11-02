cmake_minimum_required (VERSION 3.12)

SET(CMAKE_OSX_SYSROOT /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk)
SET(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(BTstack-libusb)

SET(BTSTACK_ROOT ${CMAKE_SOURCE_DIR}/../..)

# local dir for btstack_config.h after build dir to avoid using .h from Makefile
include_directories(.)

include_directories(../../3rd-party/micro-ecc)
include_directories(../../3rd-party/bluedroid/decoder/include)
include_directories(../../3rd-party/bluedroid/encoder/include)
include_directories(../../3rd-party/lc3-google/include)
include_directories(../../3rd-party/md5)
include_directories(../../3rd-party/hxcmod-player)
include_directories(../../3rd-party/hxcmod-player/mod)
include_directories(../../3rd-party/lwip/core/src/include)
include_directories(../../3rd-party/lwip/dhcp-server)
include_directories(../../3rd-party/qr-code-generator)
include_directories(../../3rd-party/rijndael)
include_directories(../../3rd-party/yxml)
include_directories(../../3rd-party/tinydir)
include_directories(../../src)
include_directories(../../chipset/realtek)
include_directories(../../chipset/zephyr)
include_directories(../../platform/posix)
include_directories(../../platform/embedded)
include_directories(../../platform/lwip)
include_directories(../../platform/lwip/port)

# BTstack sources from src/ and platform. Third-party, and port-local sources
# remain below because they are still selected by this port.
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
)
btstack_collect_sources(SOURCES_PLATFORM
        ROOT "${BTSTACK_ROOT}"
        GROUPS
        platform-posix
        platform-libusb-h2
)

# Example Util
file(GLOB SOURCES_EXAMPLE_UTIL "../../example/*_demo_util*.c")

# 3rd-Party
file(GLOB SOURCES_BLUEDROID "../../3rd-party/bluedroid/encoder/srce/*.c" "../../3rd-party/bluedroid/decoder/srce/*.c")
file(GLOB SOURCES_MD5       "../../3rd-party/md5/md5.c")
file(GLOB SOURCES_UECC      "../../3rd-party/micro-ecc/uECC.c")
file(GLOB SOURCES_YXML      "../../3rd-party/yxml/yxml.c")
file(GLOB SOURCES_HXCMOD    "../../3rd-party/hxcmod-player/*.c"  "../../3rd-party/hxcmod-player/mods/*.c")
file(GLOB SOURCES_QRCODEGEN "../../3rd-party/qr-code-generator/qrcodegen.c")
file(GLOB SOURCES_RIJNDAEL  "../../3rd-party/rijndael/rijndael.c")

# Port specific
file(GLOB SOURCES_PORT      "../../port/libusb/*.c")

btstack_collect_sources(SOURCES_CHIPSETS
 ROOT "${BTSTACK_ROOT}"
 GROUPS
    chipset-realtek
    chipset-zephyr
)

# LC3
file(GLOB SOURCES_LC3_GOOGLE "../../3rd-party/lc3-google/src/*.c")

# lwIP is linked only by the examples that use it below.
btstack_collect_sources(SOURCES_LWIP
 ROOT "${BTSTACK_ROOT}"
 GROUPS lwip
)

set(SOURCES
 ${SOURCES_MD5}
 ${SOURCES_YXML}
 ${SOURCES_BLUEDROID}
 ${SOURCES_PLATFORM}
 ${SOURCES_RIJNDAEL}
 ${SOURCES_PORT}
 ${SOURCES_LC3_GOOGLE}
 ${SOURCES_BTSTACK}
 ${SOURCES_EXAMPLE_UTIL}
 ${SOURCES_UECC}
 ${SOURCES_HXCMOD}
 ${SOURCES_QRCODEGEN}
 ${SOURCES_CHIPSETS}
)
list(SORT SOURCES)

# create static lib
add_library(btstack STATIC ${SOURCES})
btstack_target_warnings(btstack STRICT)

# pkgconfig required to link libusb
find_package(PkgConfig REQUIRED)

# libusb
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)
target_include_directories(btstack PUBLIC ${LIBUSB_INCLUDE_DIRS})
target_link_directories(btstack PUBLIC ${LIBUSB_LIBRARY_DIRS})
target_link_libraries(btstack ${LIBUSB_LIBRARIES})

# portaudio
pkg_check_modules(PORTAUDIO portaudio-2.0)
if(PORTAUDIO_FOUND)
    message("HAVE_PORTAUDIO")
    target_include_directories(btstack PUBLIC ${PORTAUDIO_INCLUDE_DIRS})
    target_link_directories(btstack PUBLIC ${PORTAUDIO_LIBRARY_DIRS})
    target_link_libraries(btstack ${PORTAUDIO_LIBRARIES})
    add_compile_definitions(HAVE_PORTAUDIO)
endif()

# pthread
find_package(Threads)
target_link_libraries(btstack ${CMAKE_THREAD_LIBS_INIT})

# libm
target_link_libraries(btstack m)


# Get the declared examples. Feature-based filtering will be provided by the
# future resolver; for now this port builds every declared example.
include(${BTSTACK_ROOT}/example/requirements.cmake)
set(EXAMPLES ${btstack_declared_examples})

# on Mac 10.14, adding lwip to libstack results in a yet not understood link error
# workaround: add lwip sources only to lwip_examples
set (LWIP_EXAMPLES pan_lwip_http_server)

# .gatt files in src
file(GLOB GATT_FILES "../../src/*/gatt-service/*.gatt")

# create targets
foreach(EXAMPLE ${EXAMPLES})
    # Get the example sources from its declaration.
    set(SOURCES_EXAMPLE)
    foreach(EXAMPLE_SOURCE IN LISTS btstack_example_${EXAMPLE}_sources)
        list(APPEND SOURCES_EXAMPLE ${BTSTACK_ROOT}/example/${EXAMPLE_SOURCE})
    endforeach()

    # add lwip sources for lwip examples
    if ( "${LWIP_EXAMPLES}" MATCHES ${EXAMPLE} )
        list(APPEND SOURCES_EXAMPLE ${SOURCES_LWIP})
    endif()

    add_executable(${EXAMPLE} ${SOURCES_EXAMPLE} )

    # Compile GATT DB and add as dependency if needed
    set(EXAMPLE_GATT_DB ${btstack_example_${EXAMPLE}_gatt_db})
    if (EXAMPLE_GATT_DB)
        message("example ${EXAMPLE} -- with GATT DB")
        btstack_target_gatt(${EXAMPLE} "${BTSTACK_ROOT}/example/${EXAMPLE_GATT_DB}"
            DEPENDS ${GATT_FILES})
    else()
        message("example ${EXAMPLE}")
    endif()
    btstack_target_warnings(${EXAMPLE} STRICT)
    target_link_libraries(${EXAMPLE} btstack)
endforeach(EXAMPLE)
