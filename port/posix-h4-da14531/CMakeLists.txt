cmake_minimum_required(VERSION 3.18)

project(BTstack-posix-h4-da14531)

set(BTSTACK_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)

include(${BTSTACK_ROOT}/tool/cmake/BTstack.cmake)

btstack_declare_port(posix-h4-da14531)
btstack_collect_public_include_directories(BTSTACK_PUBLIC_INCLUDE_DIRECTORIES)

set(SOURCES_PORT main.c)

find_package(PkgConfig)
if(PkgConfig_FOUND)
    pkg_check_modules(PORTAUDIO portaudio-2.0)
    if(PORTAUDIO_FOUND)
        message("HAVE_PORTAUDIO")
        add_compile_definitions(HAVE_PORTAUDIO)
        btstack_declare_port_capability(posix-h4-da14531
            CAPABILITY audio
            GROUPS platform-posix-audio-portaudio platform-posix-wav)
    endif()
endif()
btstack_declare_port_capability(posix-h4-da14531
    CAPABILITY led
    GROUPS platform-posix-hal-led
    PUBLIC_INCLUDE_DIRECTORIES "${BTSTACK_ROOT}/platform/embedded")

find_package(Threads)

btstack_collect_port_configuration_macros(BTSTACK_CONFIG_MACROS
    CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/btstack_config.h")
btstack_set_port_configuration_macros(posix-h4-da14531 ${BTSTACK_CONFIG_MACROS})

include(${BTSTACK_ROOT}/example/requirements.cmake)
set(EXAMPLES ${btstack_declared_examples})
btstack_filter_examples_for_port(EXAMPLES
    PORT posix-h4-da14531
    CANDIDATES ${EXAMPLES})

set(BTSTACK_POSIX_H4_LE_BASE_GROUPS
    ble le-device-db-tlv tlv-none transport-h4
    platform-posix-run-loop platform-posix-signal platform-posix-stdin
    platform-posix-main-config platform-posix-tlv platform-posix-uart
    platform-posix-hci-dump
    chipset-da145xx chipset-da145xx-531
    third-party-micro-ecc third-party-rijndael)
if(PORTAUDIO_FOUND)
    list(APPEND BTSTACK_POSIX_H4_LE_BASE_GROUPS platform-posix-audio-portaudio)
endif()

foreach(EXAMPLE IN LISTS EXAMPLES)
    btstack_add_application_library(
        TARGET btstack_${EXAMPLE}_library
        PORT posix-h4-da14531
        APPLICATION ${EXAMPLE}
        BASE_GROUPS ${BTSTACK_POSIX_H4_LE_BASE_GROUPS}
        PORT_SOURCES ${SOURCES_PORT}
        PUBLIC_INCLUDE_DIRECTORIES
            ${BTSTACK_PUBLIC_INCLUDE_DIRECTORIES}
            "${CMAKE_CURRENT_SOURCE_DIR}"
            ${PORTAUDIO_INCLUDE_DIRS}
        LINK_DIRECTORIES ${PORTAUDIO_LIBRARY_DIRS}
        LINK_LIBRARIES ${CMAKE_THREAD_LIBS_INIT} m ${PORTAUDIO_LIBRARIES})

    set(SOURCES_EXAMPLE)
    foreach(EXAMPLE_SOURCE IN LISTS btstack_example_${EXAMPLE}_sources)
        list(APPEND SOURCES_EXAMPLE ${BTSTACK_ROOT}/example/${EXAMPLE_SOURCE})
    endforeach()
    add_executable(${EXAMPLE} ${SOURCES_EXAMPLE})
    if(btstack_example_${EXAMPLE}_gatt_db)
        btstack_target_gatt(${EXAMPLE} "${BTSTACK_ROOT}/example/${btstack_example_${EXAMPLE}_gatt_db}")
    endif()
    btstack_target_warnings(${EXAMPLE} STRICT)
    target_link_libraries(${EXAMPLE} btstack_${EXAMPLE}_library pthread)
endforeach()

btstack_add_info_targets(PORT posix-h4-da14531)
