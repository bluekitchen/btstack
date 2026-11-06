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

include(${BTSTACK_ROOT}/tool/cmake/BTstack.cmake)
include(${BTSTACK_ROOT}/tool/cmake/BTstackFirmware.cmake)
btstack_declare_port(posix-h4)
btstack_collect_public_include_directories(BTSTACK_PUBLIC_INCLUDE_DIRECTORIES
  ROOT "${BTSTACK_ROOT}"
)
# Port specific files
set(SOURCES_PORT
    main.c
)

# Get libraries needed for this port

# portaudio: needs pkgconfig
find_package(PkgConfig)

if (PkgConfig_FOUND)
    pkg_check_modules(PORTAUDIO portaudio-2.0)
    if(PORTAUDIO_FOUND)
        message("HAVE_PORTAUDIO")
        add_compile_definitions(HAVE_PORTAUDIO)
        btstack_declare_port_provider(posix-h4
            CAPABILITY audio
            GROUPS platform-posix-audio-portaudio platform-posix-wav
        )
    endif()
endif()
btstack_declare_port_provider(posix-h4
    CAPABILITY led
    GROUPS platform-posix-hal-led
    PUBLIC_INCLUDE_DIRECTORIES "${BTSTACK_ROOT}/platform/embedded"
)

# pthread
find_package(Threads)

# libm


# Add CC256x Support and specify init script
set (CC256X_INIT_SCRIPT bluetooth_init_cc2564C_1.5.c)
include(${BTSTACK_ROOT}/chipset/cc256x/cc256x.cmake)
list(APPEND SOURCES_PORT ${CC256X_INIT_SCRIPT_SOURCES})

# Add BCM Support and download PatchRAM files.
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
set(BTSTACK_POSIX_H4_FIRMWARE_TARGETS
    bcm43430a1_patchram
    cyw55310_patchram
)

set(BTSTACK_CONFIG_DEFINITIONS)
if(PORTAUDIO_FOUND)
    list(APPEND BTSTACK_CONFIG_DEFINITIONS HAVE_PORTAUDIO)
endif()
btstack_collect_port_configuration_macros(BTSTACK_CONFIG_MACROS
    CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/btstack_config.h"
    DEFINITIONS ${BTSTACK_CONFIG_DEFINITIONS})
btstack_set_port_configuration_macros(posix-h4 ${BTSTACK_CONFIG_MACROS})

# Get the declared examples supported by this port configuration and providers.
include(${BTSTACK_ROOT}/example/requirements.cmake)
set(EXAMPLES ${btstack_declared_examples})
btstack_filter_examples_for_port(EXAMPLES
    PORT posix-h4
    CANDIDATES ${EXAMPLES})
list(REMOVE_ITEM EXAMPLES "mesh_node_demo")

# Host runtime/platform facilities that can be shared by Bluetooth and
# standalone examples. Bluetooth controller setup remains separate below.
set(BTSTACK_POSIX_H4_RUNTIME_PLATFORM_GROUPS
    platform-posix-run-loop
    platform-posix-signal
    platform-posix-stdin
)
if(PORTAUDIO_FOUND)
    # main.c selects the PortAudio instances when HAVE_PORTAUDIO is enabled.
    list(APPEND BTSTACK_POSIX_H4_RUNTIME_PLATFORM_GROUPS platform-posix-audio-portaudio)
endif()

# Scoped Bluetooth examples use the H4 port's controller setup plus their
# implementation closure resolved from each example declaration.
set(BTSTACK_POSIX_H4_BLUETOOTH_GROUPS
    # This dual-mode port enables BLE in btstack_config.h. All Classic examples
    # support pairing, and cross-transport key derivation requires SM.
    ble
    le-device-db-tlv
    link-key-db-tlv
    tlv-none
    transport-h4
    platform-posix-main-config
    platform-posix-tlv
    platform-posix-uart
    platform-posix-hci-dump
    chipset-bcm
    chipset-cc256x
    chipset-csr
    chipset-em9301
    chipset-realtek
    chipset-stlc2500d
    chipset-tc3566x
    chipset-zephyr
    third-party-micro-ecc
    third-party-rijndael
)
set(BTSTACK_POSIX_H4_BLUETOOTH_BASE_GROUPS
    ${BTSTACK_POSIX_H4_RUNTIME_PLATFORM_GROUPS}
    ${BTSTACK_POSIX_H4_BLUETOOTH_GROUPS}
)

# Every selected example is defined with its resolved source closure. EXAMPLES
# is derived from the shared requirements manifest and any port exclusions.
foreach(EXAMPLE IN LISTS EXAMPLES)
    btstack_add_scoped_example_library(
        TARGET btstack_${EXAMPLE}_library
        ROOT "${BTSTACK_ROOT}"
        PORT posix-h4
        EXAMPLE ${EXAMPLE}
        GROUP_OBJECT_TARGET_PREFIX btstack_posix_h4
        BASE_GROUPS ${BTSTACK_POSIX_H4_BLUETOOTH_BASE_GROUPS}
        PORT_SOURCES ${SOURCES_PORT}
        PUBLIC_INCLUDE_DIRECTORIES
            ${BTSTACK_PUBLIC_INCLUDE_DIRECTORIES}
            # btstack_config.h is the port configuration seen by applications.
            "${CMAKE_CURRENT_SOURCE_DIR}"
            ${PORTAUDIO_INCLUDE_DIRS}
        PRIVATE_INCLUDE_DIRECTORIES "${BTSTACK_ROOT}/chipset/cc256x"
        LINK_DIRECTORIES ${PORTAUDIO_LIBRARY_DIRS}
        LINK_LIBRARIES
            ${CMAKE_THREAD_LIBS_INIT}
            m
            ${PORTAUDIO_LIBRARIES}
    )
    add_dependencies(btstack_${EXAMPLE}_library ${CC256X_INIT_SCRIPT_TARGETS})

    # Get the example sources from its declaration.
    set(SOURCES_EXAMPLE)
    foreach(EXAMPLE_SOURCE IN LISTS btstack_example_${EXAMPLE}_sources)
        list(APPEND SOURCES_EXAMPLE ${BTSTACK_ROOT}/example/${EXAMPLE_SOURCE})
    endforeach()

    # define example target
    add_executable(${EXAMPLE} ${SOURCES_EXAMPLE})

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

    target_link_libraries(${EXAMPLE} btstack_${EXAMPLE}_library pthread)
    add_dependencies(${EXAMPLE} ${BTSTACK_POSIX_H4_FIRMWARE_TARGETS})
endforeach()
