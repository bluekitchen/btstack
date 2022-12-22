cmake_minimum_required (VERSION 3.5)

SET(CMAKE_OSX_SYSROOT /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk)
SET(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(BTstack-posix-h4-zephyr)

SET(BTSTACK_ROOT ${CMAKE_SOURCE_DIR}/../..)

# pkgconfig
find_package(PkgConfig QUIET)

# portaudio
if (PkgConfig_FOUND)
	pkg_check_modules(PORTAUDIO REQUIRED portaudio-2.0)
	if(PORTAUDIO_FOUND)
		message("HAVE_PORTAUDIO")
		include_directories(${PORTAUDIO_INCLUDE_DIRS})
		link_directories(${PORTAUDIO_LIBRARY_DIRS})
		link_libraries(${PORTAUDIO_LIBRARIES})
		# CMake 3.12 - add_compile_definitions(HAVE_PORTAUDIO)
		SET(CMAKE_C_FLAGS  "-DHAVE_PORTAUDIO")
	endif()
endif()

# pthread
find_package(Threads)
link_libraries(${CMAKE_THREAD_LIBS_INIT})

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
include_directories(../../3rd-party/lc3-google/include)
include_directories(../../3rd-party/md5)
include_directories(../../3rd-party/hxcmod-player)
include_directories(../../3rd-party/hxcmod-player/mod)
include_directories(../../3rd-party/rijndael)
include_directories(../../src)
include_directories(../../chipset/zephyr)
include_directories(../../platform/embedded)
include_directories(../../platform/posix)

file(GLOB SOURCES_SRC       "../../src/*.c")
file(GLOB SOURCES_BLE       "../../src/ble/*.c")
file(GLOB SOURCES_GATT      "../../src/ble/gatt-service/*.c")
file(GLOB SOURCES_MESH      "../../src/mesh/*.c")
file(GLOB SOURCES_UECC      "../../3rd-party/micro-ecc/uECC.c")
file(GLOB SOURCES_HXCMOD    "../../3rd-party/hxcmod-player/*.c"  "../../3rd-party/hxcmod-player/mods/*.c")
file(GLOB SOURCES_RIJNDAEL  "../../3rd-party/rijndael/rijndael.c")
file(GLOB SOURCES_POSIX     "../../platform/posix/*.c")
file(GLOB SOURCES_ZEPHYR    "../../chipset/zephyr/*.c")
file(GLOB SOURCES_LC3_GOOGLE "../../3rd-party/lc3-google/src/*.c")
file(GLOB SOURCES_PORT      "*.c")

file(GLOB SOURCES_BLE_OFF "../../src/ble/le_device_db_memory.c")
list(REMOVE_ITEM SOURCES_BLE   ${SOURCES_BLE_OFF})

file(GLOB SOURCES_POSIX_OFF "../../platform/posix/le_device_db_fs.c" "../../platform/posix/btstack_link_key_db_fs.c")
list(REMOVE_ITEM SOURCES_POSIX ${SOURCES_POSIX_OFF})

set(SOURCES
	${SOURCES_BLE}
	${SOURCES_GATT}
	${SOURCES_HXCMOD}
	${SOURCES_LC3_GOOGLE}
	${SOURCES_MESH}
	${SOURCES_PORT}
	${SOURCES_POSIX}
	${SOURCES_RIJNDAEL}
	${SOURCES_SRC}
	${SOURCES_UECC}
	${SOURCES_ZEPHYR}
)
list(SORT SOURCES)

# create static lib
add_library(btstack STATIC ${SOURCES})

# get list of examples
include(../../example/CMakeLists.txt)
set (EXAMPLES ${EXAMPLES_LE_ONLY} ${EXAMPLES_GENERAL})

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
	target_link_libraries(${EXAMPLE} btstack)
endforeach(EXAMPLE)
