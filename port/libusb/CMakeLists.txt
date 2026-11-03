cmake_minimum_required (VERSION 3.12)

SET(CMAKE_OSX_SYSROOT /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk)
SET(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(BTstack-libusb)

SET(BTSTACK_ROOT ${CMAKE_SOURCE_DIR}/../..)

# BTstack library source groups. Port-local and example utility sources remain
# below because they are selected by this port.
include(${BTSTACK_ROOT}/tool/cmake/BTstack.cmake)
set(BTSTACK_LIBRARY_GROUPS
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
    platform-posix
    platform-libusb-h2
    chipset-realtek
    chipset-zephyr
    third-party-bluedroid-sbc
    third-party-hxcmod
    third-party-lc3-google
    third-party-md5
    third-party-micro-ecc
    third-party-qr-code-generator
    third-party-rijndael
    third-party-yxml
)
btstack_collect_sources(SOURCES_BTSTACK
 ROOT "${BTSTACK_ROOT}"
 GROUPS ${BTSTACK_LIBRARY_GROUPS}
)
btstack_collect_include_directories(BTSTACK_INCLUDE_DIRECTORIES
 ROOT "${BTSTACK_ROOT}"
 GROUPS ${BTSTACK_LIBRARY_GROUPS}
)

# Example Util
file(GLOB SOURCES_EXAMPLE_UTIL "../../example/*_demo_util*.c")

# Port specific
set(SOURCES_PORT
    main.c
)

# lwIP is linked only by the examples that use it below.
btstack_collect_sources(SOURCES_LWIP
 ROOT "${BTSTACK_ROOT}"
 GROUPS lwip
)
btstack_collect_include_directories(LWIP_INCLUDE_DIRECTORIES
 ROOT "${BTSTACK_ROOT}"
 GROUPS lwip
)

set(SOURCES
 ${SOURCES_PORT}
 ${SOURCES_BTSTACK}
 ${SOURCES_EXAMPLE_UTIL}
)
list(SORT SOURCES)

# create static lib
add_library(btstack STATIC ${SOURCES})
btstack_target_warnings(btstack STRICT)
target_include_directories(btstack
 PUBLIC
    "${BTSTACK_ROOT}/src"
 PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${BTSTACK_ROOT}/platform/embedded"
    ${BTSTACK_INCLUDE_DIRECTORIES}
)

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
    target_include_directories(${EXAMPLE}
     PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${BTSTACK_ROOT}/platform/embedded"
        ${BTSTACK_INCLUDE_DIRECTORIES}
    )
    if ( "${LWIP_EXAMPLES}" MATCHES ${EXAMPLE} )
        target_include_directories(${EXAMPLE} PRIVATE ${LWIP_INCLUDE_DIRECTORIES})
    endif()

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
