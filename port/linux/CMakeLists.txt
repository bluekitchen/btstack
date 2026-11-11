cmake_minimum_required(VERSION 3.18)

project(BTstack-linux-bluez)

set(BTSTACK_ROOT ${CMAKE_SOURCE_DIR}/../..)

include(${BTSTACK_ROOT}/tool/cmake/BTstack.cmake)
btstack_declare_port(linux)
btstack_collect_public_include_directories(BTSTACK_PUBLIC_INCLUDE_DIRECTORIES)

set(SOURCES_PORT main.c)

# Prefer PortAudio when available, otherwise use the native ALSA backend.
find_package(PkgConfig)
if(PkgConfig_FOUND)
    pkg_check_modules(PORTAUDIO portaudio-2.0)
endif()
find_package(ALSA)

if(PORTAUDIO_FOUND)
    message("PortAudio audio port selected")
    add_compile_definitions(HAVE_PORTAUDIO)
    btstack_declare_port_capability(linux
        CAPABILITY audio
        GROUPS platform-posix-audio-portaudio platform-posix-wav
    )
elseif(ALSA_FOUND)
    message("ALSA audio port selected")
    add_compile_definitions(HAVE_ALSA)
    btstack_declare_port_capability(linux
        CAPABILITY audio
        GROUPS platform-linux-audio-alsa platform-posix-wav
    )
endif()
btstack_declare_port_capability(linux
    CAPABILITY led
    GROUPS platform-posix-hal-led
    PUBLIC_INCLUDE_DIRECTORIES "${BTSTACK_ROOT}/platform/embedded"
)

find_package(Threads)

set(BTSTACK_CONFIG_DEFINITIONS)
if(PORTAUDIO_FOUND)
    list(APPEND BTSTACK_CONFIG_DEFINITIONS HAVE_PORTAUDIO)
elseif(ALSA_FOUND)
    list(APPEND BTSTACK_CONFIG_DEFINITIONS HAVE_ALSA)
endif()
btstack_collect_port_configuration_macros(BTSTACK_CONFIG_MACROS
    CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/btstack_config.h"
    DEFINITIONS ${BTSTACK_CONFIG_DEFINITIONS})
btstack_set_port_configuration_macros(linux ${BTSTACK_CONFIG_MACROS})

include(${BTSTACK_ROOT}/example/requirements.cmake)
set(EXAMPLES ${btstack_declared_examples})
btstack_filter_examples_for_port(EXAMPLES
    PORT linux
    CANDIDATES ${EXAMPLES})
# Preserve the legacy port selection. Mesh is intentionally excluded.
list(REMOVE_ITEM EXAMPLES mesh_node_demo)

set(BTSTACK_LINUX_BASE_GROUPS
    ble
    le-device-db-tlv
    link-key-db-tlv
    tlv-none
    transport-linux
    platform-posix-run-loop
    platform-posix-signal
    platform-posix-stdin
    platform-posix-tlv
    platform-posix-hci-dump
    third-party-micro-ecc
    third-party-rijndael
)

foreach(EXAMPLE IN LISTS EXAMPLES)
    btstack_add_application_library(
        TARGET btstack_${EXAMPLE}_library
        PORT linux
        APPLICATION ${EXAMPLE}
        BASE_GROUPS ${BTSTACK_LINUX_BASE_GROUPS}
        PORT_SOURCES ${SOURCES_PORT}
        PUBLIC_INCLUDE_DIRECTORIES
            ${BTSTACK_PUBLIC_INCLUDE_DIRECTORIES}
            "${CMAKE_CURRENT_SOURCE_DIR}"
            ${PORTAUDIO_INCLUDE_DIRS}
            ${ALSA_INCLUDE_DIRS}
        LINK_DIRECTORIES
            ${PORTAUDIO_LIBRARY_DIRS}
            ${ALSA_LIBRARY_DIRS}
        LINK_LIBRARIES
            ${CMAKE_THREAD_LIBS_INIT}
            m
            ${PORTAUDIO_LIBRARIES}
            ${ALSA_LIBRARIES}
    )

    set(SOURCES_EXAMPLE)
    foreach(EXAMPLE_SOURCE IN LISTS btstack_example_${EXAMPLE}_sources)
        list(APPEND SOURCES_EXAMPLE ${BTSTACK_ROOT}/example/${EXAMPLE_SOURCE})
    endforeach()

    add_executable(${EXAMPLE} ${SOURCES_EXAMPLE})

    set(EXAMPLE_GATT_DB ${btstack_example_${EXAMPLE}_gatt_db})
    if(EXAMPLE_GATT_DB)
        message("example ${EXAMPLE} -- with GATT DB")
        btstack_target_gatt(${EXAMPLE} "${BTSTACK_ROOT}/example/${EXAMPLE_GATT_DB}")
    else()
        message("example ${EXAMPLE}")
    endif()

    btstack_target_warnings(${EXAMPLE} STRICT)
    target_link_libraries(${EXAMPLE} btstack_${EXAMPLE}_library)
endforeach()

btstack_add_info_targets(PORT linux)
