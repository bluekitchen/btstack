cmake_minimum_required(VERSION 3.10)
project(libusb-static C)

set(LIBUSB_SOURCES
    libusb/core.c
    libusb/descriptor.c
    libusb/hotplug.c
    libusb/io.c
    libusb/strerror.c
    libusb/sync.c
)

if(APPLE)
    list(APPEND LIBUSB_SOURCES
        libusb/os/darwin_usb.c
        libusb/os/events_posix.c
        libusb/os/threads_posix.c
    )
    find_library(IOKIT_FRAMEWORK IOKit REQUIRED)
    find_library(COREFOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
    find_library(SECURITY_FRAMEWORK Security REQUIRED)
endif()

add_library(libusb_static STATIC ${LIBUSB_SOURCES})

target_include_directories(libusb_static PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/libusb
    ${CMAKE_CURRENT_SOURCE_DIR}/libusb/os
    ${CMAKE_CURRENT_SOURCE_DIR}/Xcode
)

if(APPLE)
    target_link_libraries(libusb_static PUBLIC
        ${IOKIT_FRAMEWORK}
        ${COREFOUNDATION_FRAMEWORK}
        ${SECURITY_FRAMEWORK}
        objc
    )
endif()
