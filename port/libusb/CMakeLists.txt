cmake_minimum_required (VERSION 3.12)

SET(CMAKE_OSX_SYSROOT /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk)
SET(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(BTstack-libusb)

SET(BTSTACK_ROOT ${CMAKE_SOURCE_DIR}/../..)

# Get main BTstack CMake helper
include(${BTSTACK_ROOT}/tool/cmake/BTstack.cmake)

# Set up port
btstack_declare_port(libusb)
btstack_collect_public_include_directories(BTSTACK_PUBLIC_INCLUDE_DIRECTORIES)
# Port specific files
set(SOURCES_PORT
    main.c
)

# Get libraries needed for this port

# libusb: needs pkgconfig
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)

# pthread
find_package(Threads)

# portaudio
pkg_check_modules(PORTAUDIO portaudio-2.0)
if(PORTAUDIO_FOUND)
    message("HAVE_PORTAUDIO")
    add_compile_definitions(HAVE_PORTAUDIO)
    btstack_declare_port_capability(libusb
        CAPABILITY audio
        GROUPS platform-posix-audio-portaudio platform-posix-wav
    )
endif()
btstack_declare_port_capability(libusb
    CAPABILITY led
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
if(PORTAUDIO_FOUND)
    # main.c selects the PortAudio instances when HAVE_PORTAUDIO is enabled.
    list(APPEND BTSTACK_LIBUSB_GROUPS platform-posix-audio-portaudio)
endif()

# Use the C preprocessor to obtain the effective BTstack configuration macros.
# This lets the resolver use the same conditional configuration that
# compilation sees, without requiring btstack_config.h to be generated.
set(BTSTACK_CONFIG_DEFINITIONS)
if(PORTAUDIO_FOUND)
    # Keep this invocation consistent with add_compile_definitions above.
    list(APPEND BTSTACK_CONFIG_DEFINITIONS HAVE_PORTAUDIO)
endif()
btstack_collect_port_configuration_macros(BTSTACK_CONFIG_MACROS
    CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/btstack_config.h"
    DEFINITIONS ${BTSTACK_CONFIG_DEFINITIONS})
btstack_set_port_configuration_macros(libusb ${BTSTACK_CONFIG_MACROS})
string(JOIN ", " BTSTACK_CONFIG_MACROS_DISPLAY ${BTSTACK_CONFIG_MACROS})
message(STATUS "BTstack config ENABLE_*/HAVE_* macros: ${BTSTACK_CONFIG_MACROS_DISPLAY}")


# Get the declared examples supported by this port configuration and providers.
include(${BTSTACK_ROOT}/example/requirements.cmake)

set(EXAMPLES ${btstack_declared_applications})
btstack_filter_examples_for_port(EXAMPLES
    PORT libusb
    CANDIDATES ${EXAMPLES})

# Every selected example is defined with its resolved source closure. EXAMPLES
# is derived from the shared requirements manifest and any port exclusions.
foreach(EXAMPLE IN LISTS EXAMPLES)
    btstack_get_application_property(APPLICATION_SOURCES "${EXAMPLE}" SOURCES)
    btstack_get_application_property(APPLICATION_GATT_DB "${EXAMPLE}" GATT_DB)
    btstack_add_application_library(
        TARGET btstack_${EXAMPLE}_library
        PORT libusb
        APPLICATION ${EXAMPLE}
        BASE_GROUPS ${BTSTACK_LIBUSB_GROUPS}
        PORT_SOURCES ${SOURCES_PORT}
        PUBLIC_INCLUDE_DIRECTORIES
            ${BTSTACK_PUBLIC_INCLUDE_DIRECTORIES}
            # btstack_config.h is the port configuration seen by applications.
            "${CMAKE_CURRENT_SOURCE_DIR}"
            ${LIBUSB_INCLUDE_DIRS}
            ${PORTAUDIO_INCLUDE_DIRS}
        LINK_DIRECTORIES
            ${LIBUSB_LIBRARY_DIRS}
            ${PORTAUDIO_LIBRARY_DIRS}
        LINK_LIBRARIES
            ${LIBUSB_LIBRARIES}
            ${CMAKE_THREAD_LIBS_INIT}
            m
            ${PORTAUDIO_LIBRARIES}
    )

    # Construct full path to the example sources from its declaration.
    set(SOURCES_EXAMPLE)
    foreach(EXAMPLE_SOURCE IN LISTS APPLICATION_SOURCES)
        list(APPEND SOURCES_EXAMPLE ${EXAMPLE_SOURCE})
    endforeach()

    # Define example target
    add_executable(${EXAMPLE} ${SOURCES_EXAMPLE} )

    # Compile GATT DB if needed
    set(EXAMPLE_GATT_DB ${APPLICATION_GATT_DB})
    if (EXAMPLE_GATT_DB)
        message("example ${EXAMPLE} -- with GATT DB")
        btstack_target_gatt(${EXAMPLE} "${EXAMPLE_GATT_DB}")
    else()
        message("example ${EXAMPLE}")
    endif()

    # Configure warnings
    btstack_target_warnings(${EXAMPLE} STRICT)

    # Add all libraries
    target_link_libraries(${EXAMPLE} btstack_${EXAMPLE}_library)
endforeach()

btstack_add_info_targets(PORT libusb)
