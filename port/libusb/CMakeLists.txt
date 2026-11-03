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
    lwip
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

# Port specific files
set(SOURCES_PORT
    main.c
)

# Complete list of sources
set(SOURCES
 ${SOURCES_PORT}
 ${SOURCES_BTSTACK}
)

# create static 'btstack' lib
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

# Get libraries needed for this port

# libusb: needs pkgconfig
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)
target_include_directories(btstack PUBLIC ${LIBUSB_INCLUDE_DIRS})
target_link_directories(btstack PUBLIC ${LIBUSB_LIBRARY_DIRS})
target_link_libraries(btstack ${LIBUSB_LIBRARIES})

# pthread
find_package(Threads)
target_link_libraries(btstack ${CMAKE_THREAD_LIBS_INIT})

# libm
target_link_libraries(btstack m)

# portaudio
pkg_check_modules(PORTAUDIO portaudio-2.0)
if(PORTAUDIO_FOUND)
    message("HAVE_PORTAUDIO")
    target_include_directories(btstack PUBLIC ${PORTAUDIO_INCLUDE_DIRS})
    target_link_directories(btstack PUBLIC ${PORTAUDIO_LIBRARY_DIRS})
    target_link_libraries(btstack ${PORTAUDIO_LIBRARIES})
    add_compile_definitions(HAVE_PORTAUDIO)
endif()


# Get the declared examples. Feature-based filtering will be provided by the
# future resolver; for now this port builds every declared example.
include(${BTSTACK_ROOT}/example/requirements.cmake)
set(EXAMPLES ${btstack_declared_examples})

# create targets
foreach(EXAMPLE ${EXAMPLES})
    # Get the example sources from its declaration.
    set(SOURCES_EXAMPLE)
    foreach(EXAMPLE_SOURCE IN LISTS btstack_example_${EXAMPLE}_sources)
        list(APPEND SOURCES_EXAMPLE ${BTSTACK_ROOT}/example/${EXAMPLE_SOURCE})
    endforeach()

    # define example target
    add_executable(${EXAMPLE} ${SOURCES_EXAMPLE} )

    # add includes
    target_include_directories(${EXAMPLE}
     PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${BTSTACK_ROOT}/platform/embedded"
        ${BTSTACK_INCLUDE_DIRECTORIES}
    )

    # Compile GATT DB if needed
    set(EXAMPLE_GATT_DB ${btstack_example_${EXAMPLE}_gatt_db})
    if (EXAMPLE_GATT_DB)
        message("example ${EXAMPLE} -- with GATT DB")
        btstack_target_gatt(${EXAMPLE} "${BTSTACK_ROOT}/example/${EXAMPLE_GATT_DB}")
    else()
        message("example ${EXAMPLE}")
    endif()

    # configure warnings
    btstack_target_warnings(${EXAMPLE} STRICT)

    # link target with btstack library
    target_link_libraries(${EXAMPLE} btstack)
endforeach(EXAMPLE)
