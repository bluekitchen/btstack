cmake_minimum_required (VERSION 3.5)
project(BTstack)

# add libusb
find_path(LIBUSB_INCLUDE_DIR NAMES libusb.h PATH_SUFFIXES "include" "libusb" "libusb-1.0")
find_library(LIBUSB_LIBRARY NAMES usb-1.0 PATH_SUFFIXES "lib" "lib32" "lib64" "dylib")
get_filename_component(LIBUSB_LIBRARY_PATH ${LIBUSB_LIBRARY} DIRECTORY)
include_directories( ${LIBUSB_INCLUDE_DIR} )
link_directories( ${LIBUSB_LIBRARY_PATH} )
link_libraries( usb-1.0 )

# add all directories
include_directories(../../3rd-party/micro-ecc)
include_directories(../../3rd-party/bluedroid/decoder/include)
include_directories(../../3rd-party/bluedroid/encoder/include)
include_directories(../../3rd-party/md5)
include_directories(../../3rd-party/hxcmod-player)
include_directories(../../3rd-party/hxcmod-player/mod)
include_directories(../../3rd-party/yxml)
include_directories(../../3rd-party/tinydir)
include_directories(../../chipset/zephyr)
include_directories(../../src)
include_directories(../../platform/posix)
include_directories(../../platform/embedded)
include_directories(../../platform/lwip)
include_directories(../../platform/lwip/port)
include_directories(..)

# find src files
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
file(GLOB SOURCES_ZEPHYR    "../../chipset/zephyr/*.c")

# remove some
file(GLOB SOURCES_OFF "../../src/ble/le_device_db_memory.c")
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
	${SOURCES_ZEPHYR}
)
list(SORT SOURCES)

# create static lib
add_library(btstack STATIC ${SOURCES})

# create targets for all examples
file(GLOB EXAMPLE_FILE btpclient.c)

# create targets
get_filename_component(EXAMPLE ${EXAMPLE_FILE} NAME_WE)
set (SOURCE_FILES ${EXAMPLE_FILE})

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${EXAMPLE}.h
	COMMAND ${CMAKE_SOURCE_DIR}/../../tool/compile_gatt.py
	ARGS ${CMAKE_SOURCE_DIR}/${EXAMPLE}.gatt ${CMAKE_CURRENT_BINARY_DIR}/${EXAMPLE}.h
)
list(APPEND SOURCE_FILES ${CMAKE_CURRENT_BINARY_DIR}/${EXAMPLE}.h)
add_executable(${EXAMPLE} ${SOURCE_FILES} )
target_link_libraries(${EXAMPLE} btstack)
