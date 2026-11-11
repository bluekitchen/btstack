cmake_minimum_required(VERSION 3.18)

project(BTstack-windows-h4-da14585)

set(BTSTACK_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)

include(${BTSTACK_ROOT}/tool/cmake/BTstack.cmake)

btstack_declare_port(windows-h4-da14585)
btstack_collect_public_include_directories(BTSTACK_PUBLIC_INCLUDE_DIRECTORIES)

set(SOURCES_PORT main.c)

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(PORTAUDIO portaudio-2.0)
    if(PORTAUDIO_FOUND)
        message("HAVE_PORTAUDIO")
        add_compile_definitions(HAVE_PORTAUDIO)
        btstack_declare_port_capability(windows-h4-da14585
            CAPABILITY audio
            GROUPS platform-posix-audio-portaudio platform-posix-wav)
    endif()
endif()
btstack_declare_port_capability(windows-h4-da14585
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
btstack_set_port_configuration_macros(windows-h4-da14585 ${BTSTACK_CONFIG_MACROS})

include(${BTSTACK_ROOT}/example/requirements.cmake)
set(EXAMPLES ${btstack_declared_examples})
btstack_filter_examples_for_port(EXAMPLES
    PORT windows-h4-da14585
    CANDIDATES ${EXAMPLES})
list(REMOVE_ITEM EXAMPLES mesh_node_demo)

set(BTSTACK_WINDOWS_H4_DA14585_BASE_GROUPS
    ble
    le-device-db-tlv
    tlv-none
    transport-h4
    platform-windows
    chipset-da145xx
    chipset-da145xx-585
    third-party-micro-ecc
    third-party-rijndael)
if(PORTAUDIO_FOUND)
    list(APPEND BTSTACK_WINDOWS_H4_DA14585_BASE_GROUPS
        platform-posix-audio-portaudio)
endif()

foreach(EXAMPLE IN LISTS EXAMPLES)
    btstack_add_application_library(
        TARGET btstack_${EXAMPLE}_library
        PORT windows-h4-da14585
        APPLICATION ${EXAMPLE}
        BASE_GROUPS ${BTSTACK_WINDOWS_H4_DA14585_BASE_GROUPS}
        PORT_SOURCES ${SOURCES_PORT}
        PUBLIC_INCLUDE_DIRECTORIES
            ${BTSTACK_PUBLIC_INCLUDE_DIRECTORIES}
            "${CMAKE_CURRENT_SOURCE_DIR}"
            ${PORTAUDIO_INCLUDE_DIRS}
        LINK_DIRECTORIES ${PORTAUDIO_LIBRARY_DIRS}
        LINK_LIBRARIES ${PORTAUDIO_LIBRARIES})

    set(SOURCES_EXAMPLE)
    foreach(EXAMPLE_SOURCE IN LISTS btstack_example_${EXAMPLE}_sources)
        list(APPEND SOURCES_EXAMPLE "${BTSTACK_ROOT}/example/${EXAMPLE_SOURCE}")
    endforeach()
    add_executable(${EXAMPLE} ${SOURCES_EXAMPLE})

    if(btstack_example_${EXAMPLE}_gatt_db)
        message("example ${EXAMPLE} -- with GATT DB")
        btstack_target_gatt(${EXAMPLE}
            "${BTSTACK_ROOT}/example/${btstack_example_${EXAMPLE}_gatt_db}")
    else()
        message("example ${EXAMPLE}")
    endif()
    btstack_target_warnings(${EXAMPLE} STRICT)
    target_link_libraries(${EXAMPLE} PRIVATE btstack_${EXAMPLE}_library)
endforeach()

btstack_add_info_targets(PORT windows-h4-da14585)
