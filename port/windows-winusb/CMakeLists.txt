cmake_minimum_required (VERSION 3.12)

project(BTstack-windows-winusb)

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
find_package(Python3 REQUIRED COMPONENTS Interpreter)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# local dir for btstack_config.h after build dir to avoid using .h from Makefile
include_directories(.)

include_directories(../../3rd-party/bluedroid/decoder/include)
include_directories(../../3rd-party/bluedroid/encoder/include)
include_directories(../../3rd-party/micro-ecc)
include_directories(../../3rd-party/lc3-google/include)
include_directories(../../3rd-party/md5)
include_directories(../../3rd-party/hxcmod-player)
include_directories(../../3rd-party/hxcmod-player/mod)
include_directories(../../3rd-party/rijndael)
include_directories(../../3rd-party/yxml)
include_directories(../../src)
include_directories(../../chipset/zephyr)
include_directories(../../platform/embedded)
include_directories(../../platform/posix)
include_directories(../../platform/windows)

file(GLOB SOURCES_SRC       "../../src/*.c" "../../example/sco_demo_util.c" "../../platform/posix/wav_util.c")
file(GLOB SOURCES_BLUEDROID "../../3rd-party/bluedroid/encoder/srce/*.c" "../../3rd-party/bluedroid/decoder/srce/*.c")
file(GLOB SOURCES_CLASSIC   "../../src/classic/*.c")
file(GLOB SOURCES_BLE       "../../src/ble/*.c")
file(GLOB SOURCES_GATT      "../../src/ble/gatt-service/*.c")
file(GLOB SOURCES_UECC      "../../3rd-party/micro-ecc/uECC.c")
file(GLOB SOURCES_HXCMOD    "../../3rd-party/hxcmod-player/*.c"  "../../3rd-party/hxcmod-player/mods/*.c")
file(GLOB SOURCES_MD5       "../../3rd-party/md5/md5.c")
file(GLOB SOURCES_RIJNDAEL  "../../3rd-party/rijndael/rijndael.c")
file(GLOB SOURCES_WINDOWS   "../../platform/windows/*.c")
file(GLOB SOURCES_ZEPHYR    "../../chipset/zephyr/*.c")
file(GLOB SOURCES_LC3_GOOGLE "../../3rd-party/lc3-google/src/*.c")
file(GLOB SOURCES_PORT      "main.c" "../../platform/posix/btstack_audio_portaudio.c")
file(GLOB SOURCES_YXML      "../../3rd-party/yxml/yxml.c")

file(GLOB SOURCES_BLE_OFF "../../src/ble/le_device_db_memory.c")
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

# LHDC V5 portable codec libraries
file(GLOB SOURCES_LHDCV5_DECODER "../../3rd-party/lhdcv5-decoder/decoder/*.c")
add_library(lhdcv5_decoder STATIC ${SOURCES_LHDCV5_DECODER})
target_include_directories(lhdcv5_decoder PUBLIC "${BTSTACK_ROOT}/3rd-party/lhdcv5-decoder/decoder")
target_compile_definitions(lhdcv5_decoder PRIVATE LHDC_HOST_BUILD)
if(MSVC)
    target_compile_options(lhdcv5_decoder PRIVATE /FI"${CMAKE_CURRENT_SOURCE_DIR}/lhdcv5_msvc_compat.h")
endif()

file(GLOB SOURCES_LHDCV5_ENCODER "../../3rd-party/lhdcv5-encoder/src/*.c")
add_library(lhdcv5_encoder STATIC ${SOURCES_LHDCV5_ENCODER})
target_include_directories(lhdcv5_encoder PUBLIC "${BTSTACK_ROOT}/3rd-party/lhdcv5-encoder/include" PRIVATE "${BTSTACK_ROOT}/3rd-party/lhdcv5-encoder/src")

# PortAudio: use system pkg-config when available, otherwise build the vendored copy.
find_package(PkgConfig QUIET)
if (PkgConfig_FOUND)
    pkg_check_modules(PORTAUDIO portaudio-2.0)
endif()

if(PORTAUDIO_FOUND)
    message("HAVE_PORTAUDIO (pkg-config)")
    target_include_directories(btstack PUBLIC ${PORTAUDIO_INCLUDE_DIRS})
    target_link_directories(btstack PUBLIC ${PORTAUDIO_LIBRARY_DIRS})
    target_link_libraries(btstack ${PORTAUDIO_LIBRARIES})
else()
    message("HAVE_PORTAUDIO (vendored)")
    set(PA_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(PA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(PA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    add_subdirectory("${BTSTACK_ROOT}/3rd-party/portaudio" "${CMAKE_CURRENT_BINARY_DIR}/portaudio")
    target_include_directories(btstack PUBLIC "${BTSTACK_ROOT}/3rd-party/portaudio/include")
    target_link_libraries(btstack portaudio)
endif()
add_compile_definitions(HAVE_PORTAUDIO)

# get list of examples, skipping mesh_node_demo
include(../../example/CMakeLists.txt)
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
   COMMAND ${Python3_EXECUTABLE}
   ARGS ${BTSTACK_ROOT}/tool/compile_gatt.py ${BTSTACK_ROOT}/example/${EXAMPLE}.gatt ${CMAKE_CURRENT_BINARY_DIR}/${EXAMPLE}.h
  )
        list(APPEND SOURCES_EXAMPLE ${CMAKE_CURRENT_BINARY_DIR}/${EXAMPLE}.h)
    else()
        message("example ${EXAMPLE}")
    endif()
    add_executable(${EXAMPLE} ${SOURCES_EXAMPLE})
    target_link_libraries(${EXAMPLE} btstack setupapi winusb)
endforeach(EXAMPLE)

# Windows A2DP Sink with LHDC V5 software decoder
add_executable(a2dp_sink_lhdcv5_demo a2dp_sink_lhdcv5_demo.c)
target_link_libraries(a2dp_sink_lhdcv5_demo btstack lhdcv5_decoder setupapi winusb)

# Offline LHDC V5 capture replay / decoder validation
add_executable(lhdcv5_replay lhdcv5_replay.c)
target_link_libraries(lhdcv5_replay lhdcv5_decoder)
