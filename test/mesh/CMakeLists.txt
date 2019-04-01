cmake_minimum_required (VERSION 3.5)
project(BTstackMesh)

# add libusb
find_path(LIBUSB_INCLUDE_DIR NAMES libusb.h PATH_SUFFIXES "include" "libusb" "libusb-1.0")
find_library(LIBUSB_LIBRARY NAMES usb-1.0 PATH_SUFFIXES "lib" "lib32" "lib64" "dylib")
get_filename_component(LIBUSB_LIBRARY_PATH ${LIBUSB_LIBRARY} DIRECTORY)
include_directories( ${LIBUSB_INCLUDE_DIR} )
link_directories( ${LIBUSB_LIBRARY_PATH} )
link_libraries( usb-1.0 )

include_directories(../../3rd-party/micro-ecc)
include_directories(../../3rd-party/tinydir)
include_directories(../../src)
include_directories(../../platform/posix)
# include_directories(../../platform/embedded)
include_directories(.)

file(GLOB SOURCES_SRC       "../../src/*.c" "../../example/sco_demo_util.c")
file(GLOB SOURCES_BLE       "../../src/ble/*.c")
file(GLOB SOURCES_MESH      "../../src/ble/mesh/*.c")
file(GLOB SOURCES_GATT      "../../src/ble/gatt-service/*.c")
file(GLOB SOURCES_UECC      "../../3rd-party/micro-ecc/uECC.c")
file(GLOB SOURCES_POSIX     "../../platform/posix/*.c")
file(GLOB SOURCES_LIBUSB    "../../platform/libusb/*.c")

file(GLOB SOURCES_OFF "../../src/ble/le_device_db_memory.c" "../../src/ble/le_device_db_tlv.c")
list(REMOVE_ITEM SOURCES_BLE ${SOURCES_OFF})

set(SOURCES_MESH_NEW main.c mesh_transport.c provisioner.c provisioning.c provisioning_device.c provisioning_provisioner.c)

set(SOURCES 
	${SOURCES_POSIX}
	${SOURCES_LIBUSB}
	${SOURCES_SRC}
	${SOURCES_BLE}
	${SOURCES_MESH}
	${SOURCES_MESH_NEW}
	${SOURCES_GATT}
	${SOURCES_UECC}
)
list(SORT SOURCES)

# create static lib
add_library(btstack STATIC ${SOURCES})

# list of targets
set(TARGETS_C mesh.c)

# create targets
foreach(EXAMPLE_FILE ${TARGETS_C})
	get_filename_component(EXAMPLE ${EXAMPLE_FILE} NAME_WE)
	message("example ${EXAMPLE}")
	add_executable(${EXAMPLE} ${EXAMPLE_FILE})
	target_link_libraries(${EXAMPLE} btstack)
endforeach(EXAMPLE_FILE)
