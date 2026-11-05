cmake_minimum_required(VERSION 3.18)

project(BTstack-posix-h4-atwilc3000)

set(BTSTACK_ROOT ${CMAKE_SOURCE_DIR}/../..)

include(${BTSTACK_ROOT}/tool/cmake/BTstack.cmake)
include(${BTSTACK_ROOT}/tool/cmake/BTstackFirmware.cmake)
include(${BTSTACK_ROOT}/tool/cmake/BTstackPosixH4Le.cmake)

# main.c includes this controller firmware as a generated C header.
btstack_add_firmware_download(
    TARGET atwilc3000_ble_firmware
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/wilc3000_ble_firmware.h"
    URL "https://bluekitchen-gmbh.com/files/ble_firmware_3000.h"
    SHA256 "6c05d38e4b725c2558d0247f80f0bd82e9180f5840d7a7ed83d5eb005cd8e665"
)

btstack_add_posix_h4_le_port(
    PORT posix-h4-atwilc3000
    ROOT "${BTSTACK_ROOT}"
    CHIPSET_GROUPS chipset-atwilc3000
    PORT_SOURCES main.c
    PRIVATE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_BINARY_DIR}"
    FIRMWARE_TARGETS atwilc3000_ble_firmware
)
