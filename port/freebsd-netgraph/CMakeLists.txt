cmake_minimum_required(VERSION 3.18)
project(BTstack-FreeBSD-Netgraph C)

get_filename_component(BTSTACK_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
include(${BTSTACK_ROOT}/tool/cmake/BTstack.cmake)

btstack_declare_port(freebsd-netgraph)
btstack_collect_public_include_directories(BTSTACK_PUBLIC_INCLUDE_DIRECTORIES)

# main.c supplies the LED implementation and uses PortAudio when available.
btstack_declare_port_capability(freebsd-netgraph
    CAPABILITY led
    PUBLIC_INCLUDE_DIRECTORIES "${BTSTACK_ROOT}/platform/embedded")
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(PORTAUDIO QUIET portaudio-2.0)
endif()
if(PORTAUDIO_FOUND)
    add_compile_definitions(HAVE_PORTAUDIO)
    btstack_declare_port_capability(freebsd-netgraph
        CAPABILITY audio
        GROUPS platform-posix-audio-portaudio platform-posix-wav)
endif()
find_package(Threads REQUIRED)

set(BTSTACK_CONFIG_DEFINITIONS)
if(PORTAUDIO_FOUND)
    list(APPEND BTSTACK_CONFIG_DEFINITIONS HAVE_PORTAUDIO)
endif()
btstack_collect_port_configuration_macros(BTSTACK_CONFIG_MACROS
    CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/btstack_config.h"
    DEFINITIONS ${BTSTACK_CONFIG_DEFINITIONS})
btstack_set_port_configuration_macros(freebsd-netgraph ${BTSTACK_CONFIG_MACROS})

# main.c sets up both persistent key databases and the POSIX event loop.
set(BTSTACK_NETGRAPH_BASE_GROUPS
    ble le-device-db-tlv link-key-db-tlv platform-posix-tlv
    platform-posix-run-loop platform-posix-signal platform-posix-stdin
    platform-posix-hci-dump third-party-micro-ecc third-party-rijndael)
if(PORTAUDIO_FOUND)
    # main.c uses the audio backend even when the example does not require it.
    list(APPEND BTSTACK_NETGRAPH_BASE_GROUPS platform-posix-audio-portaudio)
endif()
set(SOURCES_PORT main.c hci_transport_netgraph.c)
# The existing Netgraph callbacks have intentionally unused parameters.
if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    set_source_files_properties(${SOURCES_PORT} PROPERTIES COMPILE_OPTIONS -Wno-unused-parameter)
endif()

include(${BTSTACK_ROOT}/example/requirements.cmake)
set(EXAMPLES ${btstack_declared_examples})
btstack_filter_examples_for_port(EXAMPLES
    PORT freebsd-netgraph
    CANDIDATES ${EXAMPLES})
# Preserve the existing port's Mesh exclusion.
list(REMOVE_ITEM EXAMPLES mesh_node_demo)

foreach(EXAMPLE IN LISTS EXAMPLES)
    btstack_add_application_library(
        TARGET btstack_${EXAMPLE}_library
        PORT freebsd-netgraph
        APPLICATION ${EXAMPLE}
        BASE_GROUPS ${BTSTACK_NETGRAPH_BASE_GROUPS}
        PORT_SOURCES ${SOURCES_PORT}
        PUBLIC_INCLUDE_DIRECTORIES
            ${BTSTACK_PUBLIC_INCLUDE_DIRECTORIES}
            "${CMAKE_CURRENT_SOURCE_DIR}"
            "${BTSTACK_ROOT}/platform/posix"
            ${PORTAUDIO_INCLUDE_DIRS}
        LINK_DIRECTORIES ${PORTAUDIO_LIBRARY_DIRS}
        LINK_LIBRARIES Threads::Threads netgraph m ${PORTAUDIO_LIBRARIES})

    set(SOURCES_EXAMPLE)
    foreach(EXAMPLE_SOURCE IN LISTS btstack_example_${EXAMPLE}_sources)
        list(APPEND SOURCES_EXAMPLE "${BTSTACK_ROOT}/example/${EXAMPLE_SOURCE}")
    endforeach()
    add_executable(${EXAMPLE} ${SOURCES_EXAMPLE})
    if(btstack_example_${EXAMPLE}_gatt_db)
        btstack_target_gatt(${EXAMPLE} "${BTSTACK_ROOT}/example/${btstack_example_${EXAMPLE}_gatt_db}")
    endif()
    btstack_target_warnings(${EXAMPLE} STRICT)
    target_link_libraries(${EXAMPLE} btstack_${EXAMPLE}_library)
endforeach()

btstack_add_info_targets(PORT freebsd-netgraph)
