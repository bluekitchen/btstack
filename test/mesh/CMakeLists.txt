cmake_minimum_required (VERSION 3.5)
project(BTstackMesh)

# enable AddressSanitizer
set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
set (CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fsanitize=address")

# AddressSanitizer for Xcode
set(CMAKE_XCODE_GENERATE_SCHEME ON)
set(CMAKE_XCODE_SCHEME_ADDRESS_SANITIZER ON)
set(CMAKE_XCODE_SCHEME_ADDRESS_SANITIZER_USE_AFTER_RETURN ON)

# fink pkgconfig
find_package(PkgConfig REQUIRED)

# libusb
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)
include_directories(${LIBUSB_INCLUDE_DIRS})
link_directories(${LIBUSB_LIBRARY_DIRS})
link_libraries(${LIBUSB_LIBRARIES})

include_directories(../../3rd-party/bluedroid/decoder/include)
include_directories(../../3rd-party/bluedroid/encoder/include)
include_directories(../../3rd-party/micro-ecc)
include_directories(../../3rd-party/rijndael)
include_directories(../../3rd-party/tinydir)
include_directories(../../3rd-party/lc3-google/include/)
include_directories(../../src)
include_directories(../../platform/posix)
include_directories(.)

file(GLOB SOURCES_SRC       "../../src/*.c"                     "../../src/*.h" "../../example/sco_demo_util.c" "../../example/sco_demo_util.h")
file(GLOB SOURCES_BLE       "../../src/ble/*.c"                 "../../src/ble/*.h")
file(GLOB SOURCES_GATT      "../../src/ble/gatt-service/*.c"    "../../src/ble/gatt-service/*.h")
file(GLOB SOURCES_MESH      "../../src/mesh/*.c"                "../../src/mesh/*.h"   "../../src/mesh/gatt-service/*.c")
file(GLOB SOURCES_UECC      "../../3rd-party/micro-ecc/uECC.c"  "../../3rd-party/micro-ecc/uECC.h")
file(GLOB SOURCES_POSIX     "../../platform/posix/*.c"          "../../platform/posix/*.h")
file(GLOB SOURCES_LIBUSB    "../../platform/libusb/*.c"         "../../platform/libusb/*.h")


file(GLOB SOURCES_OFF "../../src/ble/le_device_db_memory.c" "../../src/ble/le_device_db_memory.h" "../../src/ble/le_device_db_tlv.c" "../../src/ble/le_device_db_tlv.h")
list(REMOVE_ITEM SOURCES_BLE ${SOURCES_OFF})

set(SOURCES_MESH_NEW main.c)

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
set(EXAMPLES_C mesh_pts.c)

file(GLOB EXAMPLES_GATT "*.gatt")

# create targets
foreach(EXAMPLE_FILE ${EXAMPLES_C})
	get_filename_component(EXAMPLE ${EXAMPLE_FILE} NAME_WE)
	if ( "${EXAMPLES_GATT}" MATCHES ${EXAMPLE} )
		message("example ${EXAMPLE} -- with GATT DB")
		add_custom_command(
		    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${EXAMPLE}.h
    		COMMAND ${CMAKE_SOURCE_DIR}/../../tool/compile_gatt.py ${CMAKE_SOURCE_DIR}/${EXAMPLE}.gatt ${CMAKE_CURRENT_BINARY_DIR}/${EXAMPLE}.h > /dev/null
		)
		add_executable(${EXAMPLE} ${EXAMPLE_FILE} ${CMAKE_CURRENT_BINARY_DIR}/${EXAMPLE}.h)
	else()
		message("example ${EXAMPLE}")
		add_executable(${EXAMPLE} ${EXAMPLE_FILE})
	endif()
	target_link_libraries(${EXAMPLE} btstack)
endforeach(EXAMPLE_FILE)

# pkgconfig required to link cpputest
find_package(PkgConfig REQUIRED)

# CppuTest
pkg_check_modules(CPPUTEST REQUIRED CppuTest)
include_directories(${CPPUTEST_INCLUDE_DIRS})
link_directories(${CPPUTEST_LIBRARY_DIRS})
link_libraries(${CPPUTEST_LIBRARIES})

message("example mesh_message_test")

add_executable(mesh_message_test
../../src/mesh/mesh_foundation.c
../../src/mesh/mesh_node.c
../../src/mesh/mesh_iv_index_seq_number.c
../../src/mesh/mesh_network.c
../../src/mesh/mesh_peer.c
../../src/mesh/mesh_lower_transport.c
../../src/mesh/mesh_upper_transport.c
../../src/mesh/mesh_virtual_addresses.c
../../src/mesh/mesh_keys.c
../../src/mesh/mesh_crypto.c
../../src/btstack_memory.c
../../src/btstack_memory_pool.c
../../src/btstack_util.c
../../src/btstack_crypto.c
../../src/btstack_linked_list.c
../../src/hci_dump.c
../../platform/posix/hci_dump_posix_fs.c
../../src/hci_cmd.c
../../3rd-party/micro-ecc/uECC.c
../../3rd-party/rijndael/rijndael.c
mock.c
mesh_message_test.cpp
)

message("example provisioning_device_test")
add_executable(provisioning_device_test
provisioning_device_test.cpp
mock.c
../../3rd-party/micro-ecc/uECC.c
../../3rd-party/rijndael/rijndael.c
../../src/mesh/mesh_crypto.c
../../src/mesh/provisioning_device.c
../../src/mesh/mesh_node.c
../../src/btstack_crypto.c
../../src/btstack_util.c
../../src/btstack_linked_list.c
../../src/hci_cmd.c
../../src/hci_dump.c
../../platform/posix/hci_dump_posix_fs.c
)

message("example provisioning_provisioner_test")
add_executable(provisioning_provisioner_test
provisioning_provisioner_test.cpp
mock.c
../../3rd-party/micro-ecc/uECC.c
../../3rd-party/rijndael/rijndael.c
../../src/btstack_crypto.c
../../src/mesh/provisioning_provisioner.c
 ../../src/mesh/mesh_crypto.c
../../src/btstack_crypto.c
../../src/btstack_util.c
../../src/btstack_linked_list.c
../../src/hci_cmd.c
../../src/hci_dump.c
../../platform/posix/hci_dump_posix_fs.c
)

message("example mesh_configuration_composition_data_message_test")
add_executable(mesh_configuration_composition_data_message_test
mesh_configuration_composition_data_message_test.cpp 
../../src/mesh/mesh_configuration_client.c
)
target_link_libraries(mesh_configuration_composition_data_message_test btstack)
