cmake_minimum_required(VERSION 3.18)

project(BTstack-windows-winusb)

set(BTSTACK_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)

include(${BTSTACK_ROOT}/tool/cmake/BTstack.cmake)

btstack_declare_port(windows-winusb)
btstack_collect_public_include_directories(BTSTACK_PUBLIC_INCLUDE_DIRECTORIES)

set(SOURCES_PORT main.c)

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(PORTAUDIO portaudio-2.0)
    if(PORTAUDIO_FOUND)
        message("HAVE_PORTAUDIO")
        add_compile_definitions(HAVE_PORTAUDIO)
        btstack_declare_port_capability(windows-winusb
            CAPABILITY audio
            GROUPS platform-posix-audio-portaudio platform-posix-wav)
    endif()
endif()
btstack_declare_port_capability(windows-winusb
    CAPABILITY led
    GROUPS platform-posix-hal-led
    PUBLIC_INCLUDE_DIRECTORIES "${BTSTACK_ROOT}/platform/embedded")

set(BTSTACK_CONFIG_DEFINITIONS)
if(PORTAUDIO_FOUND)
    list(APPEND BTSTACK_CONFIG_DEFINITIONS HAVE_PORTAUDIO)
endif()
btstack_collect_port_configuration_macros(BTSTACK_CONFIG_MACROS
    CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/btstack_config.h"
    DEFINITIONS ${BTSTACK_CONFIG_DEFINITIONS})
btstack_set_port_configuration_macros(windows-winusb ${BTSTACK_CONFIG_MACROS})

include(${BTSTACK_ROOT}/example/requirements.cmake)
set(EXAMPLES ${btstack_declared_applications})
btstack_filter_examples_for_port(EXAMPLES
    PORT windows-winusb
    CANDIDATES ${EXAMPLES})
list(REMOVE_ITEM EXAMPLES mesh_node_demo)

set(BTSTACK_WINDOWS_WINUSB_BASE_GROUPS
    ble
    le-device-db-tlv
    link-key-db-tlv
    tlv-none
    platform-windows
    platform-winusb-h2
    chipset-zephyr
    third-party-micro-ecc
    third-party-rijndael)
if(PORTAUDIO_FOUND)
    list(APPEND BTSTACK_WINDOWS_WINUSB_BASE_GROUPS
        platform-posix-audio-portaudio)
endif()

foreach(EXAMPLE IN LISTS EXAMPLES)
    btstack_get_application_property(APPLICATION_SOURCES "${EXAMPLE}" SOURCES)
    btstack_get_application_property(APPLICATION_GATT_DB "${EXAMPLE}" GATT_DB)
    btstack_add_application_library(
        TARGET btstack_${EXAMPLE}_library
        PORT windows-winusb
        APPLICATION ${EXAMPLE}
        BASE_GROUPS ${BTSTACK_WINDOWS_WINUSB_BASE_GROUPS}
        PORT_SOURCES ${SOURCES_PORT}
        PUBLIC_INCLUDE_DIRECTORIES
            ${BTSTACK_PUBLIC_INCLUDE_DIRECTORIES}
            "${CMAKE_CURRENT_SOURCE_DIR}"
            ${PORTAUDIO_INCLUDE_DIRS}
        LINK_DIRECTORIES ${PORTAUDIO_LIBRARY_DIRS}
        LINK_LIBRARIES setupapi winusb ${PORTAUDIO_LIBRARIES})

    set(SOURCES_EXAMPLE)
    foreach(EXAMPLE_SOURCE IN LISTS APPLICATION_SOURCES)
        list(APPEND SOURCES_EXAMPLE "${EXAMPLE_SOURCE}")
    endforeach()
    add_executable(${EXAMPLE} ${SOURCES_EXAMPLE})

    if(APPLICATION_GATT_DB)
        message("example ${EXAMPLE} -- with GATT DB")
        btstack_target_gatt(${EXAMPLE}
            "${APPLICATION_GATT_DB}")
    else()
        message("example ${EXAMPLE}")
    endif()
    btstack_target_warnings(${EXAMPLE} STRICT)
    target_link_libraries(${EXAMPLE} PRIVATE btstack_${EXAMPLE}_library)
endforeach()

btstack_add_info_targets(PORT windows-winusb)
