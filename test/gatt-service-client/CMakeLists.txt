cmake_minimum_required (VERSION 3.5)
project(gatt-client-test)

# pkgconfig
find_package(PkgConfig REQUIRED)

# CppuTest
pkg_check_modules(CPPUTEST REQUIRED CppuTest)
include_directories(${CPPUTEST_INCLUDE_DIRS})
link_directories(${CPPUTEST_LIBRARY_DIRS})
link_libraries(${CPPUTEST_LIBRARIES})

# set include paths
include_directories(..)
include_directories(../../src)
include_directories(../mock)
include_directories( ${CMAKE_CURRENT_BINARY_DIR})

add_compile_options(--coverage)
add_link_options(   --coverage)
add_definitions(    -DCOVERAGE)
add_definitions(    -DHAVE_ASSERT)

# common files
set(SOURCES
	../../src/btstack_linked_list.c
	../../src/btstack_util.c
	../../src/hci_dump.c
	../../src/ble/gatt-service/ancs_client.c
	../../src/ble/gatt-service/battery_service_client.c
	../../src/ble/gatt-service/device_information_service_client.c
	../mock/mock_gatt_client.c
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
