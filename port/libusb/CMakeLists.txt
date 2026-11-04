cmake_minimum_required (VERSION 3.12)

SET(CMAKE_OSX_SYSROOT /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk)
SET(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(BTstack-libusb)

SET(BTSTACK_ROOT ${CMAKE_SOURCE_DIR}/../..)

# BTstack library source groups. Port-local and example utility sources remain
# below because they are selected by this port.
include(${BTSTACK_ROOT}/tool/cmake/BTstack.cmake)
btstack_declare_port(libusb)
btstack_collect_public_include_directories(BTSTACK_PUBLIC_INCLUDE_DIRECTORIES
 ROOT "${BTSTACK_ROOT}"
)
set(BTSTACK_LIBRARY_GROUPS
    audio-demo
    ble
    bnep
    classic
    core
    gatt
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
    ${BTSTACK_PUBLIC_INCLUDE_DIRECTORIES}
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
    btstack_declare_port_provider(libusb
        CAPABILITY audio
        GROUPS platform-posix-audio-portaudio
    )
endif()


# Get the declared examples. Feature-based filtering will be provided by the
# future resolver; for now this port builds every declared example.
include(${BTSTACK_ROOT}/example/requirements.cmake)
set(EXAMPLES ${btstack_declared_examples})

# Support selected by the libusb port's common main.c. All examples need these
# groups independently of their own protocol/profile requirements.
set(BTSTACK_LIBUSB_BASE_GROUPS
    le-device-db-tlv
    link-key-db-tlv
    tlv-none
    platform-posix-run-loop
    platform-posix-signal
    platform-posix-stdin
    platform-posix-tlv
    platform-posix-hci-dump
    platform-libusb-h2
    chipset-realtek
    chipset-zephyr
    third-party-micro-ecc
    third-party-rijndael
)
if(PORTAUDIO_FOUND)
    # main.c configures the PortAudio instances whenever HAVE_PORTAUDIO is set.
    list(APPEND BTSTACK_LIBUSB_BASE_GROUPS platform-posix-audio-portaudio)
endif()

# The first scoped source-closure pilots. They share the port baseline above,
# but their protocol implementation sources come only from their declared
# requirements.
set(BTSTACK_SCOPED_EXAMPLES
    gatt_counter
    gatt_heart_rate_client
    gatt_battery_query
    hog_host_demo
    hog_boot_host_demo
    le_streamer_client
    ancs_client_demo
    att_delayed_response
    gatt_browser
    gatt_device_information_query
    gatt_streamer_server
    hog_keyboard_demo
    hog_mouse_demo
    le_credit_based_flow_control_mode_server
    mesh_node_demo
    nordic_spp_le_counter
    nordic_spp_le_streamer
    sm_pairing_central
    sm_pairing_peripheral
    ublox_spp_le_counter
)
foreach(example IN LISTS BTSTACK_SCOPED_EXAMPLES)
    btstack_add_scoped_example_library(
        TARGET btstack_${example}_library
        ROOT "${BTSTACK_ROOT}"
        PORT libusb
        EXAMPLE ${example}
        GROUP_OBJECT_TARGET_PREFIX btstack_libusb
        BASE_GROUPS ${BTSTACK_LIBUSB_BASE_GROUPS}
        PORT_SOURCES ${SOURCES_PORT}
        PUBLIC_INCLUDE_DIRECTORIES
            ${BTSTACK_PUBLIC_INCLUDE_DIRECTORIES}
            ${LIBUSB_INCLUDE_DIRS}
            ${PORTAUDIO_INCLUDE_DIRS}
        PRIVATE_INCLUDE_DIRECTORIES
            "${CMAKE_CURRENT_SOURCE_DIR}"
            "${BTSTACK_ROOT}/platform/embedded"
        LINK_DIRECTORIES
            ${LIBUSB_LIBRARY_DIRS}
            ${PORTAUDIO_LIBRARY_DIRS}
        LINK_LIBRARIES
            ${LIBUSB_LIBRARIES}
            ${CMAKE_THREAD_LIBS_INIT}
            m
            ${PORTAUDIO_LIBRARIES}
    )
endforeach()

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

    # Scoped pilots use their resolved source closures. Other examples continue
    # to use the compatibility all-sources library.
    if(EXAMPLE IN_LIST BTSTACK_SCOPED_EXAMPLES)
        target_link_libraries(${EXAMPLE} btstack_${EXAMPLE}_library)
    else()
        target_link_libraries(${EXAMPLE} btstack)
    endif()
endforeach(EXAMPLE)
