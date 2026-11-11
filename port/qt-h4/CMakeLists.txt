cmake_minimum_required(VERSION 3.12)
project(qt-h4 LANGUAGES C CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(BTSTACK_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)

find_package(Qt5 COMPONENTS Core REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(PORTAUDIO REQUIRED portaudio-2.0)

include(${BTSTACK_ROOT}/tool/cmake/BTstack.cmake)
btstack_declare_port(qt-h4)
add_compile_definitions(HAVE_PORTAUDIO)
btstack_declare_port_capability(CAPABILITY audio
    GROUPS platform-posix-audio-portaudio platform-posix-wav)
btstack_declare_port_capability(CAPABILITY led
    GROUPS platform-posix-hal-led
    PUBLIC_INCLUDE_DIRECTORIES "${BTSTACK_ROOT}/platform/embedded")

btstack_configure_port(CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/btstack_config.h"
    DEFINITIONS HAVE_PORTAUDIO)


include(${BTSTACK_ROOT}/example/requirements.cmake)
set(EXAMPLES ${btstack_declared_applications})
btstack_filter_examples_for_port(EXAMPLES
    CANDIDATES ${EXAMPLES})
set(BTSTACK_QT_H4_GROUPS
    ble le-device-db-tlv link-key-db-tlv tlv-none platform-posix-tlv
    platform-posix-hci-dump platform-posix-network platform-posix-uart
    transport-h4
    platform-posix-signal platform-posix-stdin platform-posix-hal-led
    platform-posix-audio-portaudio chipset-bcm chipset-cc256x chipset-csr
    third-party-micro-ecc third-party-rijndael)
set(SOURCES_PORT bluetooth_init_cc2564C_1.3.c)

foreach(EXAMPLE IN LISTS EXAMPLES)
    btstack_add_application_library(
        TARGET btstack_${EXAMPLE}_library
 APPLICATION ${EXAMPLE}
        BASE_GROUPS ${BTSTACK_QT_H4_GROUPS} PORT_SOURCES ${SOURCES_PORT}
        PUBLIC_INCLUDE_DIRECTORIES
            "${CMAKE_CURRENT_SOURCE_DIR}" ${PORTAUDIO_INCLUDE_DIRS}
        LINK_DIRECTORIES ${PORTAUDIO_LIBRARY_DIRS}
        LINK_LIBRARIES ${PORTAUDIO_LIBRARIES} Qt5::Core)
    btstack_add_application_executable(${EXAMPLE} APPLICATION ${EXAMPLE}
        SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"
            "${BTSTACK_ROOT}/platform/qt/btstack_run_loop_qt.cpp")
    btstack_target_warnings(${EXAMPLE} STRICT)
    target_include_directories(${EXAMPLE} PRIVATE
        "${BTSTACK_ROOT}/chipset/bcm"
        "${BTSTACK_ROOT}/chipset/cc256x"
        "${BTSTACK_ROOT}/chipset/csr"
        "${BTSTACK_ROOT}/platform/embedded"
        "${BTSTACK_ROOT}/platform/posix"
        "${BTSTACK_ROOT}/platform/qt")
    target_link_libraries(${EXAMPLE} btstack_${EXAMPLE}_library Qt5::Core)
endforeach()

btstack_add_info_targets()
