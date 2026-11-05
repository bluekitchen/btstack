cmake_minimum_required(VERSION 3.18)

project(BTstack-posix-h4-da14585)

set(BTSTACK_ROOT ${CMAKE_SOURCE_DIR}/../..)

include(${BTSTACK_ROOT}/tool/cmake/BTstack.cmake)
include(${BTSTACK_ROOT}/tool/cmake/BTstackPosixH4Le.cmake)

btstack_add_posix_h4_le_port(
    PORT posix-h4-da14585
    ROOT "${BTSTACK_ROOT}"
    CHIPSET_GROUPS chipset-da145xx chipset-da145xx-585
    PORT_SOURCES main.c
)
