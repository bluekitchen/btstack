cmake_minimum_required (VERSION 3.12)

project(BTstack-windows-winusb)

SET(BTSTACK_ROOT ${CMAKE_SOURCE_DIR}/${BTSTACK_ROOT})

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

include_directories(${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/include)
include_directories(${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/include)
include_directories(${BTSTACK_ROOT}/3rd-party/micro-ecc)
include_directories(${BTSTACK_ROOT}/3rd-party/lc3-google/include)
include_directories(${BTSTACK_ROOT}/3rd-party/md5)
include_directories(${BTSTACK_ROOT}/3rd-party/hxcmod-player)
include_directories(${BTSTACK_ROOT}/3rd-party/hxcmod-player/mod)
include_directories(${BTSTACK_ROOT}/3rd-party/rijndael)
include_directories(${BTSTACK_ROOT}/3rd-party/yxml)
include_directories(${BTSTACK_ROOT}/src)
include_directories(${BTSTACK_ROOT}/chipset/zephyr)
include_directories(${BTSTACK_ROOT}/platform/embedded)
include_directories(${BTSTACK_ROOT}/platform/posix)
include_directories(${BTSTACK_ROOT}/platform/windows)

file(GLOB SOURCES_SRC       "${BTSTACK_ROOT}/src/*.c" "${BTSTACK_ROOT}/example/sco_demo_util.c" "${BTSTACK_ROOT}/platform/posix/wav_util.c")
file(GLOB SOURCES_BLUEDROID "${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/srce/*.c" "${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/srce/*.c")
file(GLOB SOURCES_CLASSIC   "${BTSTACK_ROOT}/src/classic/*.c")
file(GLOB SOURCES_BLE       "${BTSTACK_ROOT}/src/ble/*.c")
file(GLOB SOURCES_GATT      "${BTSTACK_ROOT}/src/ble/gatt-service/*.c")
file(GLOB SOURCES_UECC      "${BTSTACK_ROOT}/3rd-party/micro-ecc/uECC.c")
file(GLOB SOURCES_HXCMOD    "${BTSTACK_ROOT}/3rd-party/hxcmod-player/*.c"  "${BTSTACK_ROOT}/3rd-party/hxcmod-player/mods/*.c")
file(GLOB SOURCES_MD5       "${BTSTACK_ROOT}/3rd-party/md5/md5.c")
file(GLOB SOURCES_RIJNDAEL  "${BTSTACK_ROOT}/3rd-party/rijndael/rijndael.c")
file(GLOB SOURCES_WINDOWS   "${BTSTACK_ROOT}/platform/windows/*.c")
file(GLOB SOURCES_ZEPHYR    "${BTSTACK_ROOT}/chipset/zephyr/*.c")
file(GLOB SOURCES_LC3_GOOGLE "${BTSTACK_ROOT}/3rd-party/lc3-google/src/*.c")
file(GLOB SOURCES_PORT      "main.c" "${BTSTACK_ROOT}/platform/posix/btstack_audio_portaudio.c")
file(GLOB SOURCES_YXML      "${BTSTACK_ROOT}/3rd-party/yxml/yxml.c")

file(GLOB SOURCES_BLE_OFF "${BTSTACK_ROOT}/src/ble/le_device_db_memory.c")
list(REMOVE_ITEM SOURCES_BLE   ${SOURCES_BLE_OFF})

set(SOURCES
	${SOURCES_BLE}
	${SOURCES_BLUEDROID}
	${SOURCES_CLASSIC}
	${SOURCES_GATT}
	${SOURCES_HXCMOD}
	${SOURCES_MD5}
	${SOURCES_PORT}
	${SOURCES_RIJNDAEL}
	${SOURCES_SRC}
	${SOURCES_UECC}
	${SOURCES_WINDOWS}
	${SOURCES_YXML}
	${SOURCES_ZEPHYR}
)
list(SORT SOURCES)

# create static lib
add_library(btstack STATIC ${SOURCES})


# Add custom command to generate btstack_event.h
add_custom_command(
    OUTPUT ${BTSTACK_ROOT}/src/btstack_event.h
    COMMAND ${Python_EXECUTABLE} ${BTSTACK_ROOT}/tool/btstack_event_generator.py
    DEPENDS ${BTSTACK_ROOT}/src/btstack_defines.h
    COMMENT "Generating btstack_event.h"
)

# Add custom target that depends on the generated header
add_custom_target(generate_btstack_event_h DEPENDS ${BTSTACK_ROOT}/src/btstack_event.h)

# Make sure the library depends on the generated header
add_dependencies(btstack generate_btstack_event_h)

# pkgconfig
find_package(PkgConfig QUIET)

# portaudio
if (PkgConfig_FOUND)
	pkg_check_modules(PORTAUDIO portaudio-2.0)
	if(PORTAUDIO_FOUND)
		message("HAVE_PORTAUDIO")
		include_directories(${PORTAUDIO_INCLUDE_DIRS})
		link_directories(${PORTAUDIO_LIBRARY_DIRS})
		link_libraries(${PORTAUDIO_LIBRARIES})
		add_compile_definitions(HAVE_PORTAUDIO)
	endif()
endif()

# get list of examples, skipping mesh_node_demo
include(${BTSTACK_ROOT}/example/CMakeLists.txt)
set (EXAMPLES ${EXAMPLES_GENERAL} ${EXAMPLES_CLASSIC_ONLY}  ${EXAMPLES_LE_ONLY} ${EXAMPLES_DUAL_MODE})
list(REMOVE_DUPLICATES EXAMPLES)
list(REMOVE_ITEM EXAMPLES "mesh_node_demo")

# create targets
foreach(EXAMPLE ${EXAMPLES})
	# get c file
	set (SOURCES_EXAMPLE ${BTSTACK_ROOT}/example/${EXAMPLE}.c)

	# add GATT DB creation
	if ( "${EXAMPLES_GATT_FILES}" MATCHES ${EXAMPLE} )
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
	add_executable(${EXAMPLE} ${SOURCES_EXAMPLE})
	target_link_libraries(${EXAMPLE} btstack setupapi winusb)
endforeach(EXAMPLE)
