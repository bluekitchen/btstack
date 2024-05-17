cmake_minimum_required (VERSION 3.5)

project(test-tlv-test)

set (BTSTACK_ROOT ${CMAKE_SOURCE_DIR}/../../)

# pkgconfig required to link cpputest
find_package(PkgConfig REQUIRED)

# CppuTest
pkg_check_modules(CPPUTEST REQUIRED CppuTest)
include_directories(${CPPUTEST_INCLUDE_DIRS})
link_directories(${CPPUTEST_LIBRARY_DIRS})
link_libraries(${CPPUTEST_LIBRARIES})

include_directories(../../3rd-party/micro-ecc)
include_directories(../../3rd-party/rijndael)
include_directories(../../platform/embedded)
include_directories(../../platform/posix)
include_directories(../../src)
include_directories(..)

# Enable ASAN
add_compile_options( -g -fsanitize=address)
add_link_options(       -fsanitize=address)

add_executable(tlv_test_main
        tlv_test.cpp
        ${BTSTACK_ROOT}/src/btstack_util.c
        ${BTSTACK_ROOT}/src/hci_dump.c
        ${BTSTACK_ROOT}/src/classic/btstack_link_key_db_tlv.c
        ${BTSTACK_ROOT}/platform/embedded/btstack_tlv_flash_bank.c
        ${BTSTACK_ROOT}/platform/embedded/hal_flash_bank_memory.c
        ${BTSTACK_ROOT}/platform/posix/hci_dump_posix_fs.c
)

# test ENABLE_TLV_FLASH_WRITE_ONCE
add_executable(tlv_test_write_once
        tlv_test.cpp
        ${BTSTACK_ROOT}/src/btstack_util.c
        ${BTSTACK_ROOT}/src/hci_dump.c
        ${BTSTACK_ROOT}/src/classic/btstack_link_key_db_tlv.c
        ${BTSTACK_ROOT}/platform/embedded/btstack_tlv_flash_bank.c
        ${BTSTACK_ROOT}/platform/embedded/hal_flash_bank_memory.c
        ${BTSTACK_ROOT}/platform/posix/hci_dump_posix_fs.c
)
target_compile_definitions(tlv_test_write_once PUBLIC ENABLE_TLV_FLASH_WRITE_ONCE)

# test ENABLE_TLV_FLASH_WRITE_ONCE
add_executable(tlv_test_delete_field
        tlv_test.cpp
        ${BTSTACK_ROOT}/src/btstack_util.c
        ${BTSTACK_ROOT}/src/hci_dump.c
        ${BTSTACK_ROOT}/src/classic/btstack_link_key_db_tlv.c
        ${BTSTACK_ROOT}/platform/embedded/btstack_tlv_flash_bank.c
        ${BTSTACK_ROOT}/platform/embedded/hal_flash_bank_memory.c
        ${BTSTACK_ROOT}/platform/posix/hci_dump_posix_fs.c
)
target_compile_definitions(tlv_test_delete_field PUBLIC ENABLE_TLV_FLASH_EXPLICIT_DELETE_FIELD)
