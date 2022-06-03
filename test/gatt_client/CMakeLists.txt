cmake_minimum_required (VERSION 3.5)
project(gatt-client-test)

# fink pkgconfig
find_package(PkgConfig REQUIRED)

# add CppUTest
include_directories("/usr/local/include")
link_directories("/usr/local/lib")
link_libraries( CppUTest )
link_libraries( CppUTestExt )

include_directories(.)
include_directories(../../src)
include_directories( ${CMAKE_CURRENT_BINARY_DIR})

set(SOURCES
	../../src/ad_parser.c
	../../src/ble/att_db.c
	../../src/ble/att_dispatch.c
	../../src/ble/gatt_client.c
	../../src/ble/le_device_db_memory.c
	../../src/btstack_linked_list.c
	../../src/btstack_memory.c
	../../src/btstack_memory_pool.c
	../../src/btstack_util.c
	../../src/hci_cmd.c
	../../src/hci_dump.c
)

# create static lib
add_library(btstack STATIC ${SOURCES})

# create targets
foreach(EXAMPLE_FILE le_central.cpp gatt_client_test.cpp)
	get_filename_component(EXAMPLE ${EXAMPLE_FILE} NAME_WE)
	set (SOURCE_FILES ${EXAMPLE_FILE} mock.c)
	# profile.h
	add_custom_command(
		OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/profile.h
		COMMAND ${CMAKE_SOURCE_DIR}/../../tool/compile_gatt.py
		ARGS ${CMAKE_SOURCE_DIR}/profile.gatt ${CMAKE_CURRENT_BINARY_DIR}/profile.h
	)
	list(APPEND SOURCE_FILES ${CMAKE_CURRENT_BINARY_DIR}/profile.h)
	add_executable(${EXAMPLE} ${SOURCE_FILES} )
	target_link_libraries(${EXAMPLE} btstack)
endforeach(EXAMPLE_FILE)
