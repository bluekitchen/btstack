cmake_minimum_required (VERSION 3.18)

project(BTstack-posix-nxp)

set(BTSTACK_ROOT ${CMAKE_SOURCE_DIR}/../..)

include(${BTSTACK_ROOT}/tool/cmake/BTstack.cmake)
btstack_declare_port(posix-h4-nxp)

# Port specific files
set(SOURCES_PORT
    main.c
)

# PortAudio is optional. When present, it provides the generic audio
# capability requested by audio examples.
find_package(PkgConfig)
if (PkgConfig_FOUND)
    pkg_check_modules(PORTAUDIO portaudio-2.0)
    if(PORTAUDIO_FOUND)
        message("HAVE_PORTAUDIO")
        add_compile_definitions(HAVE_PORTAUDIO)
        btstack_declare_port_capability(CAPABILITY audio
            GROUPS platform-posix-audio-portaudio platform-posix-wav
        )
    endif()
endif()
btstack_declare_port_capability(CAPABILITY led
    GROUPS platform-posix-hal-led
    PUBLIC_INCLUDE_DIRECTORIES "${BTSTACK_ROOT}/platform/embedded"
)

find_package(Threads)

# Download the NXP UART firmware at build time, only for targets that use this
# port. The chipset module owns the version and checksum metadata.
include(${BTSTACK_ROOT}/chipset/nxp/nxp.cmake)
btstack_add_nxp_firmware_downloads(
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    TARGETS_VARIABLE NXP_FIRMWARE_TARGETS
    FILES_VARIABLE NXP_FIRMWARE_FILES
)

set(BTSTACK_CONFIG_DEFINITIONS)
if(PORTAUDIO_FOUND)
    list(APPEND BTSTACK_CONFIG_DEFINITIONS HAVE_PORTAUDIO)
endif()
btstack_configure_port(CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/btstack_config.h"
    DEFINITIONS ${BTSTACK_CONFIG_DEFINITIONS})


# Get the declared examples supported by this port configuration and providers.
include(${BTSTACK_ROOT}/example/requirements.cmake)
set(EXAMPLES ${btstack_declared_applications})
btstack_filter_examples_for_port(EXAMPLES
    CANDIDATES ${EXAMPLES})
list(REMOVE_ITEM EXAMPLES mesh_node_demo)

# Host runtime/platform facilities that can be shared by Bluetooth and
# standalone examples. Bluetooth controller setup remains separate below.
set(BTSTACK_POSIX_H4_NXP_RUNTIME_PLATFORM_GROUPS
    platform-posix-run-loop
    platform-posix-signal
    platform-posix-stdin
)
if(PORTAUDIO_FOUND)
    # main.c selects the PortAudio instances when HAVE_PORTAUDIO is enabled.
    list(APPEND BTSTACK_POSIX_H4_NXP_RUNTIME_PLATFORM_GROUPS platform-posix-audio-portaudio)
endif()

# This is a dual-mode port. All Classic examples support pairing, and
# cross-transport key derivation requires SM when BLE is enabled.
set(BTSTACK_POSIX_H4_NXP_BLUETOOTH_GROUPS
    ble
    le-device-db-tlv
    link-key-db-tlv
    tlv-none
    transport-h4
    platform-posix-main-config
    platform-posix-tlv
    platform-posix-uart
    platform-posix-hci-dump
    chipset-nxp
    third-party-micro-ecc
    third-party-rijndael
)
set(BTSTACK_POSIX_H4_NXP_BLUETOOTH_BASE_GROUPS
    ${BTSTACK_POSIX_H4_NXP_RUNTIME_PLATFORM_GROUPS}
    ${BTSTACK_POSIX_H4_NXP_BLUETOOTH_GROUPS}
)

# Every selected example is defined with its resolved source closure. EXAMPLES
# is derived from the shared requirements manifest and any port exclusions.
foreach(EXAMPLE IN LISTS EXAMPLES)
    btstack_add_application_library(
        TARGET btstack_${EXAMPLE}_library
        APPLICATION ${EXAMPLE}
        BASE_GROUPS ${BTSTACK_POSIX_H4_NXP_BLUETOOTH_BASE_GROUPS}
        PORT_SOURCES ${SOURCES_PORT}
        PUBLIC_INCLUDE_DIRECTORIES
            # btstack_config.h is the port configuration seen by applications.
            "${CMAKE_CURRENT_SOURCE_DIR}"
            ${PORTAUDIO_INCLUDE_DIRS}
        LINK_DIRECTORIES ${PORTAUDIO_LIBRARY_DIRS}
        LINK_LIBRARIES
            ${CMAKE_THREAD_LIBS_INIT}
            m
            ${PORTAUDIO_LIBRARIES}
    )
    add_dependencies(btstack_${EXAMPLE}_library ${NXP_FIRMWARE_TARGETS})

    btstack_add_application_executable(${EXAMPLE} APPLICATION ${EXAMPLE})


    btstack_target_warnings(${EXAMPLE} STRICT)
    target_link_libraries(${EXAMPLE} btstack_${EXAMPLE}_library pthread)
endforeach()

btstack_add_info_targets()
