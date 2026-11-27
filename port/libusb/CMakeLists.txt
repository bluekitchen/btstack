cmake_minimum_required (VERSION 3.12)

SET(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(BTstack-libusb)

SET(BTSTACK_ROOT ${CMAKE_SOURCE_DIR}/../..)

# Get main BTstack CMake helper
include(${BTSTACK_ROOT}/tool/cmake/BTstack.cmake)

# Set up port
btstack_declare_port(libusb)
include(${BTSTACK_ROOT}/platform/lwip/btstack_lwip.cmake)
# Port specific files
set(SOURCES_PORT
    main.c
)

# Get libraries needed for this port

find_package(PkgConfig REQUIRED)
include(${CMAKE_CURRENT_LIST_DIR}/libusb.cmake)
btstack_configure_libusb()

# pthread
find_package(Threads)

# portaudio
pkg_check_modules(PORTAUDIO portaudio-2.0)
if(PORTAUDIO_FOUND)
    message("HAVE_PORTAUDIO")
    btstack_declare_port_capability(CAPABILITY audio
        DEFINES HAVE_PORTAUDIO
        INCLUDE_DIRECTORIES ${PORTAUDIO_INCLUDE_DIRS}
        LINK_DIRECTORIES ${PORTAUDIO_LIBRARY_DIRS}
        LINK_LIBRARIES ${PORTAUDIO_LIBRARIES}
        GROUPS platform-posix-audio-portaudio platform-posix-wav
    )
endif()
btstack_declare_port_capability(CAPABILITY led
    GROUPS platform-posix-hal-led
    PUBLIC_INCLUDE_DIRECTORIES "${BTSTACK_ROOT}/platform/embedded"
)


# Common baseline selected by the libusb port's main.c and shared by every
# example. This dual-mode port enables BLE in btstack_config.h. All Classic
# examples support pairing, and cross-transport key derivation requires SM.
set(BTSTACK_LIBUSB_GROUPS
        platform-posix-run-loop
        platform-posix-signal
        platform-posix-stdin
        ble
        le-device-db-tlv
        link-key-db-tlv
        tlv-none
        platform-posix-tlv
        platform-posix-hci-dump
        platform-libusb-h2
        chipset-realtek
        chipset-zephyr
        third-party-micro-ecc
        third-party-rijndael
)

# Use the C preprocessor to obtain the effective BTstack configuration macros.
# This lets the resolver use the same conditional configuration that
# compilation sees, without requiring btstack_config.h to be generated.
btstack_configure_port(
    CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/btstack_config.h")


# Get the declared examples supported by this port configuration and providers.
include(${BTSTACK_ROOT}/example/requirements.cmake)

set(EXAMPLES ${btstack_declared_applications})
btstack_filter_examples_for_port(EXAMPLES
    CANDIDATES ${EXAMPLES})

# Every selected example is defined with its resolved source closure. EXAMPLES
# is derived from the shared requirements manifest and any port exclusions.
btstack_set_port_baseline(
        GROUPS ${BTSTACK_LIBUSB_GROUPS}
        SOURCES ${SOURCES_PORT}
        PUBLIC_INCLUDE_DIRECTORIES
            # btstack_config.h is the port configuration seen by applications.
            "${CMAKE_CURRENT_SOURCE_DIR}"
            ${BTSTACK_LIBUSB_INCLUDE_DIRS}
        LINK_LIBRARIES
            btstack_libusb_dependency
            ${CMAKE_THREAD_LIBS_INIT}
            m
    )

foreach(EXAMPLE IN LISTS EXAMPLES)
    btstack_add_application_library(
        TARGET btstack_${EXAMPLE}_library
        APPLICATION ${EXAMPLE})
    if(TARGET btstack_libusb_build)
        add_dependencies(btstack_${EXAMPLE}_library btstack_libusb_build)
    endif()

    btstack_add_application_executable(${EXAMPLE} APPLICATION ${EXAMPLE})


    # Configure warnings
    btstack_target_warnings(${EXAMPLE} STRICT)

    # Add all libraries
    target_link_libraries(${EXAMPLE} btstack_${EXAMPLE}_library)
endforeach()

if(TARGET btstack_libusb_build)
    # Shared group object targets compile separately from application libraries.
    # Ensure the downloaded header is installed before the USB transport builds.
    get_property(port_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
    foreach(port_target IN LISTS port_targets)
        get_target_property(port_sources ${port_target} SOURCES)
        if("${port_sources}" MATCHES "(^|;)[^;]*platform/libusb/hci_transport_h2_libusb\\.c(;|$)")
            add_dependencies(${port_target} btstack_libusb_build)
        endif()
    endforeach()
endif()

btstack_add_info_targets()
