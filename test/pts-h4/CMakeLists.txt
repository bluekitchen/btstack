cmake_minimum_required (VERSION 3.12)
project(BTstack-PTS-H4)

SET(BTSTACK_ROOT ${CMAKE_SOURCE_DIR}/../..)

# fink pkgconfig
find_package(PkgConfig REQUIRED)

# portaudio
pkg_check_modules(PORTAUDIO portaudio-2.0)
if(PORTAUDIO_FOUND)
    include_directories(${PORTAUDIO_INCLUDE_DIRS})
    link_directories(${PORTAUDIO_LIBRARY_DIRS})
    link_libraries(${PORTAUDIO_LIBRARIES})
    add_compile_definitions(HAVE_PORTAUDIO)
endif()

# lc3plus
pkg_check_modules(LC3PLUS LC3plus)
if(LC3PLUS_FOUND)
	message("HAVE_LC3PLUS")
	include_directories(${LC3PLUS_INCLUDE_DIRS})
	link_directories(${LC3PLUS_LIBRARY_DIRS})
	link_libraries(${LC3PLUS_LIBRARIES})
	add_definitions(${LC3PLUS_CFLAGS})
	add_compile_definitions(HAVE_LC3PLUS)
endif()

# enable optional features
add_compile_definitions(ENABLE_TESTING_SUPPORT ENABLE_LE_SIGNED_WRITE)

# to find generated .h from .gatt files
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# local dir for btstack_config.h after build dir to avoid using .h from Makefile
include_directories(.)

include_directories(../../3rd-party/micro-ecc)
include_directories(../../3rd-party/bluedroid/decoder/include)
include_directories(../../3rd-party/bluedroid/encoder/include)
include_directories(../../3rd-party/md5)
include_directories(../../3rd-party/hxcmod-player)
include_directories(../../3rd-party/hxcmod-player/mod)
include_directories(../../3rd-party/lwip/core/src/include)
include_directories(../../3rd-party/lwip/dhcp-server)
include_directories(../../3rd-party/rijndael)
include_directories(../../3rd-party/yxml)
include_directories(../../3rd-party/lc3-google/include)
include_directories(../../3rd-party/tinydir)
include_directories(../../src)
include_directories(../../example)
include_directories(../../chipset/bcm)
include_directories(../../chipset/csr)
include_directories(../../chipset/cc256x)
include_directories(../../chipset/csr)
include_directories(../../chipset/em9301)
include_directories(../../chipset/zephyr)
include_directories(../../chipset/realtek)
include_directories(../../chipset/tc3566x)
include_directories(../../chipset/stlc2500d)
include_directories(../../platform/posix)
include_directories(../../platform/embedded)
include_directories(../../platform/lwip)
include_directories(../../platform/lwip/port)

file(GLOB SOURCES_BLE       "../../src/ble/*.c")
file(GLOB SOURCES_BCM       "../../chipset/bcm/*.c")
file(GLOB SOURCES_BLUEDROID "../../3rd-party/bluedroid/encoder/srce/*.c" "../../3rd-party/bluedroid/decoder/srce/*.c")
file(GLOB SOURCES_CLASSIC   "../../src/classic/*.c")
file(GLOB SOURCES_CSR       "../../chipset/csr/*.c")
file(GLOB SOURCES_EM9301    "../../chipset/em9301/*.c")
file(GLOB SOURCES_GATT      "../../src/ble/gatt-service/*.c")
file(GLOB SOURCES_HXCMOD    "../../3rd-party/hxcmod-player/*.c"  "../../3rd-party/hxcmod-player/mods/*.c")
file(GLOB SOURCES_LE_AUDIO  "../../src/le-audio/*.c" "../../src/le-audio/gatt-service/*.c")
file(GLOB SOURCES_MD5       "../../3rd-party/md5/md5.c")
file(GLOB SOURCES_MESH      "../../src/mesh/*.c")
file(GLOB SOURCES_POSIX     "../../platform/posix/*.c" "../../port/posix-h4/main.c")
file(GLOB SOURCES_REALTEK   "../../chipset/realtek/*.c")
file(GLOB SOURCES_RIJNDAEL  "../../3rd-party/rijndael/rijndael.c")
file(GLOB SOURCES_SRC       "../../src/*.c" "../../example/sco_demo_util.c")
file(GLOB SOURCES_STLC2500D "../../chipset/stlc2500d/*.c")
file(GLOB SOURCES_TC2566X   "../../chipset/tc3566x/*.c")
file(GLOB SOURCES_UECC      "../../3rd-party/micro-ecc/uECC.c")
file(GLOB SOURCES_YXML      "../../3rd-party/yxml/yxml.c")
file(GLOB SOURCES_ZEPHYR    "../../chipset/zephyr/*.c")

file(GLOB SOURCES_BLE_OFF "../../src/ble/le_device_db_memory.c")
list(REMOVE_ITEM SOURCES_BLE   ${SOURCES_BLE_OFF})

file(GLOB SOURCES_POSIX_OFF "../../platform/posix/le_device_db_fs.c")
list(REMOVE_ITEM SOURCES_POSIX ${SOURCES_POSIX_OFF})

set(SOURCES 
	${SOURCES_BLE}
	${SOURCES_BCM}
	${SOURCES_BLUEDROID}
	${SOURCES_CLASSIC} 
	${SOURCES_CSR}
	${SOURCES_EM9301}
	${SOURCES_GATT}
	${SOURCES_HXCMOD}
	${SOURCES_LE_AUDIO}
	${SOURCES_MD5}
	${SOURCES_MESH}
	${SOURCES_POSIX}
	${SOURCES_RIJNDAEL}
	${SOURCES_SRC}
	${SOURCES_UECC}
	${SOURCES_YXML}
	${SOURCES_ZEPHYR}
    ${SOURCES_REALTEK}
    ${SOURCES_STLC2500D}
    ${SOURCES_TC2566X}
)
list(SORT SOURCES)

# Enable ASAN
add_compile_options( -g -fsanitize=address)
add_link_options(       -fsanitize=address)

# create static lib
add_library(btstack STATIC ${SOURCES})

# Add CC256x Support and specify init script
set (CC256X_INIT_SCRIPT bluetooth_init_cc2564C_1.5.c)
include(${BTSTACK_ROOT}/chipset/cc256x/cc256x.cmake)

# Add BCM Support and download PatchRAM files
include(${BTSTACK_ROOT}/chipset/bcm/bcm.cmake)

# create targets for all examples
file(GLOB EXAMPLES_C    "*.c")
list(SORT EXAMPLES_C)
file(GLOB EXAMPLES_GATT "*.gatt")

# create targets
foreach(EXAMPLE_FILE ${EXAMPLES_C})
	get_filename_component(EXAMPLE ${EXAMPLE_FILE} NAME_WE)
	set (SOURCE_FILES ${EXAMPLE_FILE})

	# add GATT DB creation
	if ( "${EXAMPLES_GATT}" MATCHES ${EXAMPLE} )
		message("PTS Tool: ${EXAMPLE} -- with GATT DB")
		add_custom_command(
		    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${EXAMPLE}.h
			DEPENDS ${CMAKE_SOURCE_DIR}/${EXAMPLE}.gatt
			COMMAND ${CMAKE_SOURCE_DIR}/../../tool/compile_gatt.py
			ARGS ${CMAKE_SOURCE_DIR}/${EXAMPLE}.gatt ${CMAKE_CURRENT_BINARY_DIR}/${EXAMPLE}.h
		)
		list(APPEND SOURCE_FILES ${CMAKE_CURRENT_BINARY_DIR}/${EXAMPLE}.h)
	else()
		message("PTS Tool: ${EXAMPLE}")
	endif()
	add_executable(${EXAMPLE} ${SOURCE_FILES} )
	target_link_libraries(${EXAMPLE} btstack)
endforeach(EXAMPLE_FILE)
