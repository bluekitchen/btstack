cmake_minimum_required (VERSION 3.5)

project(test-tlv-test)

set (BTSTACK_ROOT ${CMAKE_SOURCE_DIR}/../../)

# add CppUTest
include_directories("/usr/local/include")
link_directories("/usr/local/lib")
link_libraries( CppUTest )
link_libraries( CppUTestExt )

include_directories(../../3rd-party/micro-ecc)
include_directories(../../3rd-party/rijndael)
include_directories(../../platform/embedded)
include_directories(../../platform/posix)
include_directories(../../src)
include_directories(..)

# test ENABLE_TLV_FLASH_WRITE_ONCE
add_compile_definitions(ENABLE_TLV_FLASH_WRITE_ONCE)

# Enable ASAN
add_compile_options( -g -fsanitize=address)
add_link_options(       -fsanitize=address)

add_executable(tlv_test
        tlv_test.cpp
        ${BTSTACK_ROOT}/src/btstack_util.c
        ${BTSTACK_ROOT}/src/hci_dump.c
        ${BTSTACK_ROOT}/src/classic/btstack_link_key_db_tlv.c
        ${BTSTACK_ROOT}/platform/embedded/btstack_tlv_flash_bank.c
        ${BTSTACK_ROOT}/platform/embedded/hal_flash_bank_memory.c
        ${BTSTACK_ROOT}/platform/posix/hci_dump_posix_fs.c
)
