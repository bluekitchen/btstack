cmake_minimum_required (VERSION 3.5)
project(test-gap)

# find pkgconfig
find_package(PkgConfig REQUIRED)

# pkgconfig required to link cpputest
find_package(PkgConfig REQUIRED)

# CppuTest
pkg_check_modules(CPPUTEST REQUIRED CppuTest)
include_directories(${CPPUTEST_INCLUDE_DIRS})
link_directories(${CPPUTEST_LIBRARY_DIRS})
link_libraries(${CPPUTEST_LIBRARIES})

# ASAN
add_compile_options( -DHAVE_ASSERT)
add_compile_options( -g -fsanitize=address)
add_link_options(       -fsanitize=address)

include_directories(.)
include_directories(../../src)
include_directories(../../platform/posix)
include_directories( ${CMAKE_CURRENT_BINARY_DIR})

add_definitions(-DFUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION)

set(SOURCES
	../../src/ad_parser.c
	../../src/ble/att_db.c
	../../src/ble/att_dispatch.c
	../../src/ble/gatt_client.c
	../../src/ble/le_device_db_memory.c
	../../src/btstack_linked_list.c
	../../src/btstack_run_loop.c
	../../src/btstack_memory.c
	../../src/btstack_memory_pool.c
	../../src/btstack_util.c
	../../src/hci.c
	../../src/hci_cmd.c
	../../src/hci_dump.c
	../../platform/posix/btstack_run_loop_posix.c
	../../platform/posix/hci_dump_posix_fs.c
)

# create static lib
add_library(btstack STATIC ${SOURCES})

# create targets
foreach(EXAMPLE_FILE test_le_scan.cpp hci_test.cpp)
	get_filename_component(EXAMPLE ${EXAMPLE_FILE} NAME_WE)
	set (SOURCE_FILES ${EXAMPLE_FILE})
	add_executable(${EXAMPLE} ${SOURCE_FILES} )
	target_link_libraries(${EXAMPLE} btstack)
endforeach(EXAMPLE_FILE)
