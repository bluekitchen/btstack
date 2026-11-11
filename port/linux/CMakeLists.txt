cmake_minimum_required(VERSION 3.18)

project(BTstack-linux-bluez)

set(BTSTACK_ROOT ${CMAKE_SOURCE_DIR}/../..)

include(${BTSTACK_ROOT}/tool/cmake/BTstack.cmake)
btstack_declare_port(linux)

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
    btstack_declare_port_capability(CAPABILITY audio
        GROUPS platform-posix-audio-portaudio platform-posix-wav
    )
elseif(ALSA_FOUND)
    message("ALSA audio port selected")
    add_compile_definitions(HAVE_ALSA)
    btstack_declare_port_capability(CAPABILITY audio
        GROUPS platform-linux-audio-alsa platform-posix-wav
    )
endif()
btstack_declare_port_capability(CAPABILITY led
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
btstack_configure_port(CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/btstack_config.h"
    DEFINITIONS ${BTSTACK_CONFIG_DEFINITIONS})


include(${BTSTACK_ROOT}/example/requirements.cmake)
set(EXAMPLES ${btstack_declared_applications})
btstack_filter_examples_for_port(EXAMPLES
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
        APPLICATION ${EXAMPLE}
        BASE_GROUPS ${BTSTACK_LINUX_BASE_GROUPS}
        PORT_SOURCES ${SOURCES_PORT}
        PUBLIC_INCLUDE_DIRECTORIES
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

    btstack_add_application_executable(${EXAMPLE} APPLICATION ${EXAMPLE})


    btstack_target_warnings(${EXAMPLE} STRICT)
    target_link_libraries(${EXAMPLE} btstack_${EXAMPLE}_library)
endforeach()

btstack_add_info_targets()
