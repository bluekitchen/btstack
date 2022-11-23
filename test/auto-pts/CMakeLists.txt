cmake_minimum_required (VERSION 3.5)
project(BTstack-AutoPTS)

# fink pkgconfig
find_package(PkgConfig REQUIRED)

# libusb
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)
include_directories(${LIBUSB_INCLUDE_DIRS})
link_directories(${LIBUSB_LIBRARY_DIRS})
link_libraries(${LIBUSB_LIBRARIES})

# add all directories
include_directories((../../chipset/realtek))
include_directories(../../3rd-party/micro-ecc)
include_directories(../../3rd-party/lc3-google/include)
include_directories(../../3rd-party/rijndael)
include_directories(../../3rd-party/tinydir)
include_directories(../../chipset/zephyr)
include_directories(../../src)
include_directories(../../platform/posix)
include_directories(../../platform/embedded)
include_directories(../../platform/lwip)
include_directories(../../platform/lwip/port)
include_directories(.)
include_directories(..)

# find src files
file(GLOB SOURCES_SRC       "../../src/*.c" "../../example/sco_demo_util.c")
file(GLOB SOURCES_CLASSIC   ../../src/classic/btstack_link_key_db_tlv.c
							../../src/classic/rfcomm.c
							../../src/classic/sdp_server.c
							../../src/classic/sdp_util.c)
file(GLOB SOURCES_BLE       "../../src/ble/*.c")
file(GLOB SOURCES_GATT      "../../src/ble/gatt-service/*.c")
file(GLOB SOURCES_UECC      "../../3rd-party/micro-ecc/uECC.c")
file(GLOB SOURCES_POSIX     "../../platform/posix/*.c")
file(GLOB SOURCES_LIBUSB    "../../platform/libusb/*.c")
file(GLOB SOURCES_REALTEK   "../../chipset/realtek/*.c")
file(GLOB SOURCES_ZEPHYR    "../../chipset/zephyr/*.c")

# remove some
file(GLOB SOURCES_BLE_OFF "../../src/ble/le_device_db_memory.c")
list(REMOVE_ITEM SOURCES_BLE ${SOURCES_BLE_OFF})
file(GLOB SOURCES_POSIX_OFF "../../platform/posix/le_device_db_fs.c")
list(REMOVE_ITEM SOURCES_POSIX ${SOURCES_POSIX_OFF})

set(SOURCES 
	${SOURCES_POSIX}
	${SOURCES_LIBUSB}
	${SOURCES_SRC}
	${SOURCES_CLASSIC}
	${SOURCES_BLE}
	${SOURCES_GATT}
	${SOURCES_UECC}
	${SOURCES_REALTEK}
	${SOURCES_ZEPHYR}
	"../../3rd-party/rijndael/rijndael.c"
)

# Enable ASAN
add_compile_options( -g -fsanitize=address)
add_link_options(       -fsanitize=address)

# create static lib
add_library(btstack STATIC ${SOURCES})

# target
set (EXAMPLE btpclient)

# create targets
set (SOURCE_FILES ${EXAMPLE}.c ../../port/libusb/main.c btp_socket.c)

add_executable(${EXAMPLE} ${SOURCE_FILES} )
target_link_libraries(${EXAMPLE} btstack)
