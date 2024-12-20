cmake_minimum_required (VERSION 3.12)

SET(CMAKE_OSX_SYSROOT /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk)
SET(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(BTstack-libusb-intel)

SET(BTSTACK_ROOT ${CMAKE_SOURCE_DIR}/../..)

# extra compiler warnings
if ("${CMAKE_C_COMPILER_ID}" MATCHES ".*Clang.*")
	# using Clang
	SET(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -Wunused-variable -Wswitch-default -Werror")
elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
	# using GCC
	SET(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -Wunused-but-set-variable -Wunused-variable -Wswitch-default -Werror")
elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "Intel")
	# using Intel C++
elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "MSVC")
	# using Visual Studio C++
endif()

# to generate .h from .gatt files
find_package (Python REQUIRED COMPONENTS Interpreter)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# local dir for btstack_config.h after build dir to avoid using .h from Makefile
include_directories(.)

include_directories(../../3rd-party/micro-ecc)
include_directories(../../3rd-party/bluedroid/decoder/include)
include_directories(../../3rd-party/bluedroid/encoder/include)
include_directories(../../3rd-party/lc3-google/include)
include_directories(../../3rd-party/md5)
include_directories(../../3rd-party/hxcmod-player)
include_directories(../../3rd-party/hxcmod-player/mod)
include_directories(../../3rdarty/lwip/core/src/include)
include_directories(../../3rd-party/lwip/dhcp-server)
include_directories(../../3rd-party/rijndael)
include_directories(../../3rd-party/yxml)
include_directories(../../3rd-party/tinydir)
include_directories(../../src)
include_directories(../../chipset/intel)
include_directories(../../platform/posix)
include_directories(../../platform/embedded)
include_directories(../../platform/lwip)
include_directories(../../platform/lwip/port)

file(GLOB SOURCES_SRC       "../../src/*.c" "../../example/sco_demo_util.c")
file(GLOB SOURCES_BLE       "../../src/ble/*.c")
file(GLOB SOURCES_GATT      "../../src/ble/gatt-service/*.c")
file(GLOB SOURCES_CLASSIC   "../../src/classic/*.c")
file(GLOB SOURCES_MESH      "../../src/mesh/*.c" "../../src/mesh/gatt-service/*.c")
file(GLOB SOURCES_BLUEDROID "../../3rd-party/bluedroid/encoder/srce/*.c" "../../3rd-party/bluedroid/decoder/srce/*.c")
file(GLOB SOURCES_MD5       "../../3rd-party/md5/md5.c")
file(GLOB SOURCES_UECC      "../../3rd-party/micro-ecc/uECC.c")
file(GLOB SOURCES_YXML      "../../3rd-party/yxml/yxml.c")
file(GLOB SOURCES_HXCMOD    "../../3rd-party/hxcmod-player/*.c"  "../../3rd-party/hxcmod-player/mods/*.c")
file(GLOB SOURCES_RIJNDAEL  "../../3rd-party/rijndael/rijndael.c")
file(GLOB SOURCES_POSIX     "../../platform/posix/*.c")
file(GLOB SOURCES_LIBUSB    "../../port/libusb-intel/*.c" "../../platform/libusb/*.c")
file(GLOB SOURCES_INTEL    "../../chipset/intel/*.c")
file(GLOB SOURCES_LC3_GOOGLE "../../3rd-party/lc3-google/src/*.c")

set(LWIP_CORE_SRC 
	../../3rd-party/lwip/core/src/core/def.c
	../../3rd-party/lwip/core/src/core/inet_chksum.c
	../../3rd-party/lwip/core/src/core/init.c
	../../3rd-party/lwip/core/src/core/ip.c
	../../3rd-party/lwip/core/src/core/mem.c
	../../3rd-party/lwip/core/src/core/memp.c
	../../3rd-party/lwip/core/src/core/netif.c
	../../3rd-party/lwip/core/src/core/pbuf.c
	../../3rd-party/lwip/core/src/core/tcp.c
	../../3rd-party/lwip/core/src/core/tcp_in.c
	../../3rd-party/lwip/core/src/core/tcp_out.c
	../../3rd-party/lwip/core/src/core/timeouts.c
	../../3rd-party/lwip/core/src/core/udp.c
)
set (LWIP_IPV4_SRC
	../../3rd-party/lwip/core/src/core/ipv4/acd.c
	../../3rd-party/lwip/core/src/core/ipv4/dhcp.c
	../../3rd-party/lwip/core/src/core/ipv4/etharp.c
	../../3rd-party/lwip/core/src/core/ipv4/icmp.c
	../../3rd-party/lwip/core/src/core/ipv4/ip4.c
	../../3rd-party/lwip/core/src/core/ipv4/ip4_addr.c
	../../3rd-party/lwip/core/src/core/ipv4/ip4_frag.c
)
set (LWIP_NETIF_SRC
	../../3rd-party/lwip/core/src/netif/ethernet.c
)
set (LWIP_HTTPD
	../../3rd-party/lwip/core/src/apps/http/altcp_proxyconnect.c
	../../3rd-party/lwip/core/src/apps/http/fs.c
	../../3rd-party/lwip/core/src/apps/http/httpd.c
)
set (LWIP_DHCPD
	../../3rd-party/lwip/dhcp-server/dhserver.c
)
set (LWIP_PORT
	../../platform/lwip/port/sys_arch.c
	../../platform/lwip/bnep_lwip.c
)

set (SOURCES_LWIP ${LWIP_CORE_SRC} ${LWIP_IPV4_SRC} ${LWIP_NETIF_SRC} ${LWIP_HTTPD} ${LWIP_DHCPD} ${LWIP_PORT})

file(GLOB SOURCES_SRC_OFF "../../src/hci_transport_*.c")
list(REMOVE_ITEM SOURCES_SRC   ${SOURCES_SRC_OFF})

file(GLOB SOURCES_BLE_OFF "../../src/ble/le_device_db_memory.c")
list(REMOVE_ITEM SOURCES_BLE   ${SOURCES_BLE_OFF})

file(GLOB SOURCES_POSIX_OFF "../../platform/posix/le_device_db_fs.c")
list(REMOVE_ITEM SOURCES_POSIX ${SOURCES_POSIX_OFF})

set(SOURCES 
	${SOURCES_MD5}
	${SOURCES_YXML}
	${SOURCES_BLUEDROID}
	${SOURCES_POSIX}
	${SOURCES_RIJNDAEL}
	${SOURCES_LIBUSB}
	${SOURCES_LC3_GOOGLE}
	${SOURCES_SRC}
	${SOURCES_BLE}
	${SOURCES_GATT}
	${SOURCES_MESH}
	${SOURCES_CLASSIC} 
	${SOURCES_UECC}
	${SOURCES_HXCMOD}
	${SOURCES_INTEL}
)
list(SORT SOURCES)

# create static lib
add_library(btstack STATIC ${SOURCES})

# pkgconfig required to link libusb
find_package(PkgConfig REQUIRED)

# libusb
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)
include_directories(${LIBUSB_INCLUDE_DIRS})
link_directories(${LIBUSB_LIBRARY_DIRS})
link_libraries(${LIBUSB_LIBRARIES})

# portaudio
pkg_check_modules(PORTAUDIO portaudio-2.0)
if(PORTAUDIO_FOUND)
	message("HAVE_PORTAUDIO")
	include_directories(${PORTAUDIO_INCLUDE_DIRS})
	link_directories(${PORTAUDIO_LIBRARY_DIRS})
	link_libraries(${PORTAUDIO_LIBRARIES})
	add_compile_definitions(HAVE_PORTAUDIO)
endif()

# pthread
find_package(Threads)
link_libraries(${CMAKE_THREAD_LIBS_INIT})

# create targets for all examples
file(GLOB EXAMPLES_C    "../../example/*.c")
list(SORT EXAMPLES_C)
file(GLOB EXAMPLES_GATT "../../example/*.gatt")

# remove some
file(GLOB EXAMPLES_OFF  "../../example/*demo_util*.c" "../../example/ant_test.c" "../../example/avrcp_browsing_client.c")
list(REMOVE_ITEM EXAMPLES_C ${EXAMPLES_OFF})

# on Mac 10.14, adding lwip to libstack results in a yet not understood link error
# workaround: add lwip sources only to lwip_examples
set (LWIP_EXAMPLES pan_lwip_http_server)

# create targets
foreach(EXAMPLE_FILE ${EXAMPLES_C})
	get_filename_component(EXAMPLE ${EXAMPLE_FILE} NAME_WE)

	# get c file
	set (SOURCES_EXAMPLE ${BTSTACK_ROOT}/example/${EXAMPLE}.c)

	# add lwip sources for lwip examples
	if ( "${LWIP_EXAMPLES}" MATCHES ${EXAMPLE} )
		list(APPEND SOURCES_EXAMPLE ${SOURCES_LWIP})
	endif()

	# add GATT DB creation
	if ( "${EXAMPLES_GATT}" MATCHES ${EXAMPLE} )
		message("example ${EXAMPLE} -- with GATT DB")
		add_custom_command(
		    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${EXAMPLE}.h
			DEPENDS ${BTSTACK_ROOT}/example/${EXAMPLE}.gatt
			COMMAND ${Python_EXECUTABLE}
			ARGS ${BTSTACK_ROOT}/tool/compile_gatt.py ${BTSTACK_ROOT}/example/${EXAMPLE}.gatt ${CMAKE_CURRENT_BINARY_DIR}/${EXAMPLE}.h
		)
		list(APPEND SOURCES_EXAMPLE ${CMAKE_CURRENT_BINARY_DIR}/${EXAMPLE}.h)
	else()
		message("example ${EXAMPLE}")
	endif()
	add_executable(${EXAMPLE} ${SOURCES_EXAMPLE} )
	target_link_libraries(${EXAMPLE} btstack)
endforeach(EXAMPLE_FILE)
