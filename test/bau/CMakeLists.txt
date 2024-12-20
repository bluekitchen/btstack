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

# create test targets
file(GLOB TARGETS_CPP "*_test.cpp")
# create targets
foreach(TARGET_FILE ${TARGETS_CPP})
        get_filename_component(TEST ${TARGET_FILE} NAME_WE)
        message("test/btstack_util: ${TEST}")
        message("${TEST} - ${TARGET_FILE} - ${BTSTACK_ROOT}/src/le_audio/broadcast_audio_uri_builder.c")
        add_executable(${TEST}
                ${TARGET_FILE}
                ${BTSTACK_ROOT}/src/btstack_util.c
                ${BTSTACK_ROOT}/src/hci_dump.c
                ${BTSTACK_ROOT}/src/le-audio/broadcast_audio_uri_builder.c
        )
endforeach(TARGET_FILE)


