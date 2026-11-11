cmake_minimum_required(VERSION 3.12)
project(qt-usb LANGUAGES C CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(BTSTACK_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)

find_package(Qt5 COMPONENTS Core REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)
pkg_check_modules(PORTAUDIO REQUIRED portaudio-2.0)

include(${BTSTACK_ROOT}/tool/cmake/BTstack.cmake)
btstack_declare_port(qt-usb)
btstack_collect_public_include_directories(BTSTACK_PUBLIC_INCLUDE_DIRECTORIES)
add_compile_definitions(HAVE_PORTAUDIO)
btstack_declare_port_capability(qt-usb CAPABILITY audio
    GROUPS platform-posix-audio-portaudio platform-posix-wav)
btstack_declare_port_capability(qt-usb CAPABILITY led
    GROUPS platform-posix-hal-led
    PUBLIC_INCLUDE_DIRECTORIES "${BTSTACK_ROOT}/platform/embedded")

btstack_collect_port_configuration_macros(BTSTACK_CONFIG_MACROS
    CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/btstack_config.h"
    DEFINITIONS HAVE_PORTAUDIO)
btstack_set_port_configuration_macros(qt-usb ${BTSTACK_CONFIG_MACROS})

include(${BTSTACK_ROOT}/example/requirements.cmake)
set(EXAMPLES ${btstack_declared_examples})
btstack_filter_examples_for_port(EXAMPLES
    PORT qt-usb
    CANDIDATES ${EXAMPLES})
set(BTSTACK_QT_USB_GROUPS
    ble le-device-db-tlv link-key-db-tlv tlv-none platform-posix-tlv
    platform-posix-hci-dump platform-posix-network platform-posix-signal
    platform-posix-stdin platform-posix-hal-led platform-posix-audio-portaudio
    platform-libusb-h2 third-party-micro-ecc third-party-rijndael)

foreach(EXAMPLE IN LISTS EXAMPLES)
    btstack_add_application_library(
        TARGET btstack_${EXAMPLE}_library
        PORT qt-usb APPLICATION ${EXAMPLE}
        BASE_GROUPS ${BTSTACK_QT_USB_GROUPS}
        PUBLIC_INCLUDE_DIRECTORIES ${BTSTACK_PUBLIC_INCLUDE_DIRECTORIES}
            "${CMAKE_CURRENT_SOURCE_DIR}" ${LIBUSB_INCLUDE_DIRS} ${PORTAUDIO_INCLUDE_DIRS}
        LINK_DIRECTORIES ${LIBUSB_LIBRARY_DIRS} ${PORTAUDIO_LIBRARY_DIRS}
        LINK_LIBRARIES ${LIBUSB_LIBRARIES} ${PORTAUDIO_LIBRARIES} Qt5::Core)
    set(SOURCES_EXAMPLE ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
        ${BTSTACK_ROOT}/platform/qt/btstack_run_loop_qt.cpp)
    foreach(EXAMPLE_SOURCE IN LISTS btstack_example_${EXAMPLE}_sources)
        list(APPEND SOURCES_EXAMPLE ${BTSTACK_ROOT}/example/${EXAMPLE_SOURCE})
    endforeach()
    add_executable(${EXAMPLE} ${SOURCES_EXAMPLE})
    if(btstack_example_${EXAMPLE}_gatt_db)
        btstack_target_gatt(${EXAMPLE} "${BTSTACK_ROOT}/example/${btstack_example_${EXAMPLE}_gatt_db}")
    endif()
    btstack_target_warnings(${EXAMPLE} STRICT)
    target_include_directories(${EXAMPLE} PRIVATE
        "${BTSTACK_ROOT}/platform/embedded"
        "${BTSTACK_ROOT}/platform/posix"
        "${BTSTACK_ROOT}/platform/qt")
    target_link_libraries(${EXAMPLE} btstack_${EXAMPLE}_library Qt5::Core)
endforeach()

btstack_add_info_targets(PORT qt-usb)
