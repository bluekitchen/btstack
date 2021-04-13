cmake_minimum_required (VERSION 3.5)
project(test-hfp)

# add CppUTest
include_directories("/usr/local/include")
link_directories("/usr/local/lib")
link_libraries( CppUTest )
link_libraries( CppUTestExt )

include_directories(.)
include_directories(..)
include_directories(../../3rd-party/bluedroid/decoder/include)
include_directories(../../3rd-party/bluedroid/encoder/include)
include_directories(../../3rd-party/md5)
include_directories(../../3rd-party/yxml)
include_directories(../../3rd-party/tinydir)
include_directories(../../src)
include_directories(../../chipset/zephyr)
include_directories(../../platform/posix)
include_directories(../../platform/embedded)
include_directories(../../port/libusb)

set (SOURCES_SRC
		../../src/btstack_linked_list.c
		../../src/btstack_util.c
		../../src/btstack_memory.c
		../../src/hci_cmd.c
		../../src/hci_dump.c
		)
file(GLOB SOURCES_CLASSIC   "../../src/classic/*.c")
file(GLOB SOURCES_POSIX     "../../platform/posix/*.c")

set(SOURCES 
	${SOURCES_POSIX}
	${SOURCES_SRC}
	${SOURCES_CLASSIC}
	mock.c
	test_sequences.c
)
list(SORT SOURCES)

# create static lib
add_library(btstack STATIC ${SOURCES})

# create targets for all tests
file(GLOB EXAMPLES_C "*_test.c")
foreach(EXAMPLE_FILE ${EXAMPLES_C})
	get_filename_component(EXAMPLE ${EXAMPLE_FILE} NAME_WE)
	# Use C++ Compiler
	set_source_files_properties(${EXAMPLE_FILE} PROPERTIES LANGUAGE CXX )
	set (SOURCE_FILES ${EXAMPLE_FILE})
	message("test/hfp tool: ${EXAMPLE}")
	add_executable(${EXAMPLE} ${SOURCE_FILES} )
	target_link_libraries(${EXAMPLE} btstack)
endforeach(EXAMPLE_FILE)
