cmake_minimum_required (VERSION 3.5)
project(BTstack)

# add libusb
find_path(LIBUSB_INCLUDE_DIR NAMES libusb.h PATH_SUFFIXES "include" "libusb" "libusb-1.0")
find_library(LIBUSB_LIBRARY NAMES usb-1.0 PATH_SUFFIXES "lib" "lib32" "lib64" "dylib")
get_filename_component(LIBUSB_LIBRARY_PATH ${LIBUSB_LIBRARY} DIRECTORY)
include_directories( ${LIBUSB_INCLUDE_DIR} )
link_directories( ${LIBUSB_LIBRARY_PATH} )
link_libraries( usb-1.0 )

include_directories(../../3rd-party/micro-ecc)
include_directories(../../3rd-party/bluedroid/decoder/include)
include_directories(../../3rd-party/bluedroid/encoder/include)
include_directories(../../3rd-party/md5)
include_directories(../../3rd-party/hxcmod-player)
include_directories(../../3rd-party/hxcmod-player/mod)
include_directories(../../3rd-party/yxml)
include_directories(../../3rd-party/tinydir)
include_directories(../../src)
include_directories(../../platform/posix)
include_directories(../../platform/embedded)
include_directories(.)

file(GLOB SOURCES_SRC       "../../src/*.c" "../../example/sco_demo_util.c")
file(GLOB SOURCES_BLE       "../../src/ble/*.c")
file(GLOB SOURCES_GATT      "../../src/ble/gatt-service/*.c")
file(GLOB SOURCES_CLASSIC   "../../src/classic/*.c")
file(GLOB SOURCES_BLUEDROID "../../3rd-party/bluedroid/encoder/srce/*.c" "../../3rd-party/bluedroid/decoder/srce/*.c")
file(GLOB SOURCES_MD5       "../../3rd-party/md5/md5.c")
file(GLOB SOURCES_UECC      "../../3rd-party/micro-ecc/uECC.c")
file(GLOB SOURCES_YXML      "../../3rd-party/yxml/yxml.c")
file(GLOB SOURCES_HXCMOD    "../../3rd-party/hxcmod-player/*.c"  "../../3rd-party/hxcmod-player/mods/*.c")
file(GLOB SOURCES_POSIX     "../../platform/posix/*.c")
file(GLOB SOURCES_LIBUSB    "../../port/libusb/*.c" "../../platform/libusb/*.c")

file(GLOB SOURCES_OFF "../../src/ble/le_device_db_memory.c" "../../src/ble/le_device_db_tlv.c")
list(REMOVE_ITEM SOURCES_BLE ${SOURCES_OFF})

set(SOURCES 
	${SOURCES_MD5}
	${SOURCES_YXML}
	${SOURCES_BLUEDROID}
	${SOURCES_POSIX}
	${SOURCES_LIBUSB}
	${SOURCES_SRC}
	${SOURCES_BLE}
	${SOURCES_GATT}
	${SOURCES_CLASSIC} 
	${SOURCES_UECC}
	${SOURCES_HXCMOD}
)
list(SORT SOURCES)

# create static lib
add_library(btstack STATIC ${SOURCES})

# create targets for all examples
file(GLOB EXAMPLES_C    "../../example/*.c")
list(SORT EXAMPLES_C)
file(GLOB EXAMPLES_GATT "../../example/*.gatt")

# remove some
file(GLOB EXAMPLES_OFF  "../../example/sco_demo_util.c" "../../example/ant_test.c" "../../example/avrcp_browsing_client.c")
list(REMOVE_ITEM EXAMPLES_C ${EXAMPLES_OFF})

# create targets
foreach(EXAMPLE_FILE ${EXAMPLES_C})
	get_filename_component(EXAMPLE ${EXAMPLE_FILE} NAME_WE)
	if ( "${EXAMPLES_GATT}" MATCHES ${EXAMPLE} )
		message("example ${EXAMPLE} -- with GATT DB")
		add_custom_command(
		    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${EXAMPLE}.h
    		COMMAND ${CMAKE_SOURCE_DIR}/../../tool/compile_gatt.py ${CMAKE_SOURCE_DIR}/../../example/${EXAMPLE}.gatt ${CMAKE_CURRENT_BINARY_DIR}/${EXAMPLE}.h > /dev/null
		)
		add_executable(${EXAMPLE} ${EXAMPLE_FILE} ${CMAKE_CURRENT_BINARY_DIR}/${EXAMPLE}.h)
	else()
		message("example ${EXAMPLE}")
		add_executable(${EXAMPLE} ${EXAMPLE_FILE})
	endif()
	target_link_libraries(${EXAMPLE} btstack)
endforeach(EXAMPLE_FILE)
