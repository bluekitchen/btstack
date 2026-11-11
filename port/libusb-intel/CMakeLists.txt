cmake_minimum_required(VERSION 3.12)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
project(BTstack-libusb-intel)

set(BTSTACK_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
include(${BTSTACK_ROOT}/tool/cmake/BTstack.cmake)
btstack_declare_port(libusb-intel)
btstack_collect_public_include_directories(BTSTACK_PUBLIC_INCLUDE_DIRECTORIES ROOT "${BTSTACK_ROOT}")

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)
find_package(Threads)
pkg_check_modules(PORTAUDIO portaudio-2.0)

set(SOURCES_PORT main.c)
if(PORTAUDIO_FOUND)
    add_compile_definitions(HAVE_PORTAUDIO)
    btstack_declare_port_capability(libusb-intel CAPABILITY audio
        GROUPS platform-posix-audio-portaudio platform-posix-wav)
endif()
btstack_declare_port_capability(libusb-intel CAPABILITY led
    GROUPS platform-posix-hal-led
    PUBLIC_INCLUDE_DIRECTORIES "${BTSTACK_ROOT}/platform/embedded")

set(BTSTACK_CONFIG_DEFINITIONS)
if(PORTAUDIO_FOUND)
    list(APPEND BTSTACK_CONFIG_DEFINITIONS HAVE_PORTAUDIO)
endif()
btstack_collect_port_configuration_macros(BTSTACK_CONFIG_MACROS
    CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/btstack_config.h"
    DEFINITIONS ${BTSTACK_CONFIG_DEFINITIONS})
btstack_set_port_configuration_macros(libusb-intel ${BTSTACK_CONFIG_MACROS})

include(${BTSTACK_ROOT}/example/requirements.cmake)
set(EXAMPLES ${btstack_declared_examples})
btstack_filter_examples_for_port(EXAMPLES
    PORT libusb-intel
    CANDIDATES ${EXAMPLES})
set(BTSTACK_LIBUSB_INTEL_GROUPS
    platform-posix-run-loop platform-posix-signal platform-posix-stdin
    ble le-device-db-tlv link-key-db-tlv tlv-none platform-posix-tlv
    platform-posix-hci-dump platform-libusb-h2 chipset-intel
    platform-posix-hal-led third-party-micro-ecc third-party-rijndael)
if(PORTAUDIO_FOUND)
    list(APPEND BTSTACK_LIBUSB_INTEL_GROUPS platform-posix-audio-portaudio)
endif()

foreach(EXAMPLE IN LISTS EXAMPLES)
    btstack_add_application_library(
        TARGET btstack_${EXAMPLE}_library ROOT "${BTSTACK_ROOT}"
        PORT libusb-intel APPLICATION ${EXAMPLE}
        BASE_GROUPS ${BTSTACK_LIBUSB_INTEL_GROUPS}
        PORT_SOURCES ${SOURCES_PORT}
        PUBLIC_INCLUDE_DIRECTORIES ${BTSTACK_PUBLIC_INCLUDE_DIRECTORIES}
            "${CMAKE_CURRENT_SOURCE_DIR}" ${LIBUSB_INCLUDE_DIRS} ${PORTAUDIO_INCLUDE_DIRS}
        LINK_DIRECTORIES ${LIBUSB_LIBRARY_DIRS} ${PORTAUDIO_LIBRARY_DIRS}
        LINK_LIBRARIES ${LIBUSB_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} m ${PORTAUDIO_LIBRARIES})

    set(SOURCES_EXAMPLE)
    foreach(EXAMPLE_SOURCE IN LISTS btstack_example_${EXAMPLE}_sources)
        list(APPEND SOURCES_EXAMPLE ${BTSTACK_ROOT}/example/${EXAMPLE_SOURCE})
    endforeach()
    add_executable(${EXAMPLE} ${SOURCES_EXAMPLE})
    if(btstack_example_${EXAMPLE}_gatt_db)
        btstack_target_gatt(${EXAMPLE} "${BTSTACK_ROOT}/example/${btstack_example_${EXAMPLE}_gatt_db}")
    endif()
    btstack_target_warnings(${EXAMPLE} STRICT)
    target_link_libraries(${EXAMPLE} btstack_${EXAMPLE}_library)
endforeach()

btstack_add_info_targets(PORT libusb-intel)
