cmake_minimum_required(VERSION 3.18)
project(btstack_standalone_gatt_counter C)

get_filename_component(BTSTACK_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
include("${BTSTACK_ROOT}/tool/cmake/BTstack.cmake")

btstack_add_library(standalone_btstack
    CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/btstack_config.h"
    GROUPS gatt-server transport-h4 platform-posix-run-loop platform-posix-uart
        le-device-db-memory third-party-rijndael)

add_executable(gatt_counter main.c "${BTSTACK_ROOT}/example/gatt_counter.c")
btstack_target_gatt(gatt_counter "${BTSTACK_ROOT}/example/gatt_counter.gatt")
target_link_libraries(gatt_counter PRIVATE standalone_btstack)
