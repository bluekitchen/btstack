cmake_minimum_required (VERSION 3.5)
project(gatt-client-test)

# pkgconfig
find_package(PkgConfig REQUIRED)

# CppuTest
pkg_check_modules(CPPUTEST REQUIRED cpputest)
include_directories(${CPPUTEST_INCLUDE_DIRS})
link_directories(${CPPUTEST_LIBRARY_DIRS})
link_libraries(${CPPUTEST_LIBRARIES})

# set include paths
include_directories(..)
include_directories(../../src)
include_directories(../mock)
include_directories( ${CMAKE_CURRENT_BINARY_DIR})

add_compile_options(-fsanitize=address,undefined)
add_link_options(   -fsanitize=address,undefined)
add_definitions(    -DCOVERAGE)
add_definitions(    -DHAVE_ASSERT)

# common files
set(SOURCES
        ../../src/btstack_linked_list.c
        ../../src/btstack_util.c
        ../../src/hci_dump.c
        ../../src/hci_event_builder.c
        ../../src/classic/avdtp_util.c
        ../../src/classic/avdtp_initiator.c
)


# create static lib
add_library(btstack STATIC ${SOURCES})

# create targets
file(GLOB TEST_FILES_C "*_test.cpp")
foreach(TEST_FILE ${TEST_FILES_C})
        # Use C++ Compiler
        set (SOURCE_FILES ${TEST_FILE})
        get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)
        add_executable(${TEST_NAME} ${SOURCE_FILES} )
        target_link_libraries(${TEST_NAME} btstack)
endforeach(TEST_FILE)

# Standalone C serialization test
add_executable(codec_serialization_test codec_serialization_test.c)
target_link_libraries(codec_serialization_test btstack)
