cmake_minimum_required (VERSION 3.13)

project(BTstack-test-hid_parser)

set (BTSTACK_ROOT ${CMAKE_SOURCE_DIR}/../../)

include_directories(../../3rd-party/micro-ecc)
include_directories(../../3rd-party/rijndael)
include_directories(../../platform/embedded)
include_directories(../../platform/posix)
include_directories(../../src)
include_directories(.)

# Enable ASAN
add_compile_options( -g -fsanitize=address)
add_link_options(       -fsanitize=address)

# pkgconfig
find_package(PkgConfig REQUIRED)

# cpputest
pkg_check_modules(CPPUTEST REQUIRED cpputest)
include_directories(${CPPUTEST_INCLUDE_DIRS})
link_directories(${CPPUTEST_LIBRARY_DIRS})
link_libraries(${CPPUTEST_LIBRARIES})

add_executable(hid_parser_test
    hid_parser_test.cpp
    ${BTSTACK_ROOT}/src/btstack_util.c
    ${BTSTACK_ROOT}/src/btstack_hid_parser.c
    ${BTSTACK_ROOT}/src/hci_dump.c
    ${BTSTACK_ROOT}/platform/posix/hci_dump_posix_fs.c
    ${BTSTACK_ROOT}/platform/posix/hci_dump_posix_stdout.c
)



