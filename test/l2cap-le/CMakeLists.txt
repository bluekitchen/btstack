cmake_minimum_required (VERSION 3.5)
project(gatt-client-test)

# add CppUTest
include_directories("/usr/local/include")
link_directories("/usr/local/lib")
link_libraries( CppUTest )
link_libraries( CppUTestExt )

# set include paths
include_directories(.)
include_directories(../../src)
include_directories(../mock)
include_directories(../../platform/embedded)
include_directories(../../platform/posix)
include_directories( ${CMAKE_CURRENT_BINARY_DIR})

# common files
set(SOURCES
		../../src/btstack_linked_list.c
		../../src/btstack_util.c
		../../src/hci.c
		../../src/hci_cmd.c
		../../src/ad_parser.c
		../../src/l2cap.c
		../../src/l2cap_signaling.c
		../../src/btstack_memory.c
		../../src/btstack_run_loop.c
		../../src/hci_dump.c
		../../platform/posix/hci_dump_posix_stdout.c
		../../platform/embedded/btstack_run_loop_embedded.c
)

# create static lib
add_library(btstack STATIC ${SOURCES})

# create targets
file(GLOB TEST_FILES_CPP "*_test.cpp")
foreach(TEST_FILE ${TEST_FILES_CPP})
	# Use C++ Compiler
	# set_source_files_properties(${TEST_FILE} PROPERTIES LANGUAGE CXX )
	set (SOURCE_FILES ${TEST_FILE})
	get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)
	message("- " ${TEST_NAME})
	add_executable(${TEST_NAME} ${SOURCE_FILES} )
	target_link_libraries(${TEST_NAME} btstack)
endforeach(TEST_FILE)
