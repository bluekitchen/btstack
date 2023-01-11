# CMakeLists.txt for BTstack EK-RA6M4 + DA14531 port

cmake_minimum_required (VERSION 3.5)

SET(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# select arm-non-eabi toolchain
include (cmake/arm-none-eabi.cmake)

# Project
project(BTstack-ra6m4-da14531)

# BTstack root
SET(BTSTACK_ROOT ${CMAKE_SOURCE_DIR}/../..)

# Linker script
set(LINKER_SCRIPT "fsp.ld")

# CPU OPTIONS
set(CPU_OPTIONS
    -mcpu=cortex-m33
    -mthumb
    -mfloat-abi=hard
	-mfpu=fpv5-sp-d16
    -g
)

# Other Compile options from e2-studio debug/Makefile
add_compile_options(
    ${CPU_OPTIONS}
    -Og
    -g
    -fmessage-length=0
    -fsigned-char
    -ffunction-sections
    -fdata-sections
    # -Wunused
    -Wuninitialized
    -Wall
    -Wextra
    -Wmissing-declarations
    # -Wconversion
    -Wpointer-arith
    -Wshadow
    -Wlogical-op
    -Waggregate-return
    -Wfloat-equal
    --specs=nano.specs
    --specs=rdimon.specs    
)

# E2 project headers and sources
include_directories(
	${CMAKE_SOURCE_DIR}/e2-project/ra/arm/CMSIS_5/CMSIS/Core/Include/
	${CMAKE_SOURCE_DIR}/e2-project/ra/fsp/inc
	${CMAKE_SOURCE_DIR}/e2-project/ra/fsp/inc/api
	${CMAKE_SOURCE_DIR}/e2-project/ra/fsp/inc/instances/
	${CMAKE_SOURCE_DIR}/e2-project/ra_cfg/fsp_cfg
	${CMAKE_SOURCE_DIR}/e2-project/ra_cfg/fsp_cfg/bsp
	${CMAKE_SOURCE_DIR}/e2-project/ra_gen
	${CMAKE_SOURCE_DIR}/port
)

set (E2_SOURCES
	${CMAKE_SOURCE_DIR}/e2-project/ra/board/ra6m4_ek/board_init.c
	${CMAKE_SOURCE_DIR}/e2-project/ra/board/ra6m4_ek/board_leds.c
	${CMAKE_SOURCE_DIR}/e2-project/ra/fsp/src/bsp/cmsis/Device/RENESAS/Source/startup.c
	${CMAKE_SOURCE_DIR}/e2-project/ra/fsp/src/bsp/cmsis/Device/RENESAS/Source/system.c
	${CMAKE_SOURCE_DIR}/e2-project/ra/fsp/src/bsp/mcu/all/bsp_clocks.c
	${CMAKE_SOURCE_DIR}/e2-project/ra/fsp/src/bsp/mcu/all/bsp_common.c
	${CMAKE_SOURCE_DIR}/e2-project/ra/fsp/src/bsp/mcu/all/bsp_delay.c
	${CMAKE_SOURCE_DIR}/e2-project/ra/fsp/src/bsp/mcu/all/bsp_group_irq.c
	${CMAKE_SOURCE_DIR}/e2-project/ra/fsp/src/bsp/mcu/all/bsp_guard.c
	${CMAKE_SOURCE_DIR}/e2-project/ra/fsp/src/bsp/mcu/all/bsp_io.c
	${CMAKE_SOURCE_DIR}/e2-project/ra/fsp/src/bsp/mcu/all/bsp_irq.c
	${CMAKE_SOURCE_DIR}/e2-project/ra/fsp/src/bsp/mcu/all/bsp_register_protection.c
	${CMAKE_SOURCE_DIR}/e2-project/ra/fsp/src/bsp/mcu/all/bsp_rom_registers.c
	${CMAKE_SOURCE_DIR}/e2-project/ra/fsp/src/bsp/mcu/all/bsp_sbrk.c
	${CMAKE_SOURCE_DIR}/e2-project/ra/fsp/src/bsp/mcu/all/bsp_security.c
	${CMAKE_SOURCE_DIR}/e2-project/ra/fsp/src/r_dtc/r_dtc.c
	${CMAKE_SOURCE_DIR}/e2-project/ra/fsp/src/r_flash_hp/r_flash_hp.c
	${CMAKE_SOURCE_DIR}/e2-project/ra/fsp/src/r_gpt/r_gpt.c
	${CMAKE_SOURCE_DIR}/e2-project/ra/fsp/src/r_ioport/r_ioport.c
	${CMAKE_SOURCE_DIR}/e2-project/ra/fsp/src/r_sci_uart/r_sci_uart.c
	${CMAKE_SOURCE_DIR}/e2-project/ra_gen/common_data.c
	${CMAKE_SOURCE_DIR}/e2-project/ra_gen/hal_data.c
	${CMAKE_SOURCE_DIR}/e2-project/ra_gen/main.c
	${CMAKE_SOURCE_DIR}/e2-project/ra_gen/pin_data.c
	${CMAKE_SOURCE_DIR}/e2-project/ra_gen/vector_data.c
)

# BTstack headers and includes

# local dir for btstack_config.h after build dir to avoid using .h from Makefile
include_directories(
	${BTSTACK_ROOT}/3rd-party/micro-ecc
	${BTSTACK_ROOT}/3rd-party/segger-rtt
	${BTSTACK_ROOT}/3rd-party/hxcmod-player
	${BTSTACK_ROOT}/3rd-party/hxcmod-player/mod
	${BTSTACK_ROOT}/3rd-party/rijndael
	${BTSTACK_ROOT}/src
	${BTSTACK_ROOT}/platform/embedded
	.
)

# to generate .h from .gatt files
find_package (Python REQUIRED COMPONENTS Interpreter)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

file(GLOB SOURCES_SRC        "${BTSTACK_ROOT}/src/*.c")
file(GLOB SOURCES_BLE        "${BTSTACK_ROOT}/src/ble/*.c")
file(GLOB SOURCES_GATT       "${BTSTACK_ROOT}/src/ble/gatt-service/*.c")
file(GLOB SOURCES_SEGGER_RTT "${BTSTACK_ROOT}/3rd-party/segger-rtt/*.c")
file(GLOB SOURCES_UECC       "${BTSTACK_ROOT}/3rd-party/micro-ecc/uECC.c")
file(GLOB SOURCES_RIJNDAEL   "${BTSTACK_ROOT}/3rd-party/rijndael/rijndael.c")
file(GLOB SOURCES_EMBEDDED   "${BTSTACK_ROOT}/platform/embedded/*.c")

file(GLOB SOURCES_EMBEDDED_OFF ${BTSTACK_ROOT}/platform/embedded/btstack_em9304_spi_embedded.c)
list(REMOVE_ITEM SOURCES_EMBEDDED ${SOURCES_EMBEDDED_OFF})
file(GLOB SOURCES_BLE_OFF    ${BTSTACK_ROOT}/src/ble/le_device_db_memory.c ${BTSTACK_ROOT}/src/ble/att_db_util.c)
list(REMOVE_ITEM SOURCES_BLE ${SOURCES_BLE_OFF})
file(GLOB SOURCES_SRC_OFF    ${BTSTACK_ROOT}/src/btstack_lc3_google.c ${BTSTACK_ROOT}/btstack_tlv_none.c)
list(REMOVE_ITEM SOURCES_SRC ${SOURCES_SRC_OFF})

set(BTSTACK_SOURCES 
	${SOURCES_RIJNDAEL}
	${SOURCES_EMBEDDED}
	${SOURCES_SRC}
	${SOURCES_BLE}
	${SOURCES_GATT}
	${SOURCES_UECC}
	${SOURCES_SEGGER_RTT}
)

set (SOURCES
	${BTSTACK_SOURCES}
	${CMAKE_SOURCE_DIR}/port/hal_entry.c
	${CMAKE_SOURCE_DIR}/port/hal_flash_bank_fsp.c
)

# create static lib
add_library(btstack STATIC ${SOURCES})

# get list of examples, skipping some
include(${BTSTACK_ROOT}/example/CMakeLists.txt)
set (EXAMPLES ${EXAMPLES_GENERAL} ${EXAMPLES_LE_ONLY})
list(REMOVE_DUPLICATES EXAMPLES)
list(REMOVE_ITEM EXAMPLES mesh_node_demo mod_player sinus_player audio_duplex)

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

	# create ozone
	configure_file(ozone.jdebug.in ${EXAMPLE}.jdebug)

	add_executable(${EXAMPLE}
		${E2_SOURCES}
		${SOURCES_EXAMPLE}
	)

	target_link_libraries(${EXAMPLE}
			${CPU_OPTIONS}
			-L"${CMAKE_SOURCE_DIR}/e2-project/script"
			-L"${CMAKE_SOURCE_DIR}"
			-T${LINKER_SCRIPT}
			-nostartfiles
			m
			-nostdlib
			btstack
			)

	set_property(TARGET ${EXAMPLE} PROPERTY LINK_DEPENDS ${CMAKE_SOURCE_DIR}/memory_regions.ld)
	set_property(TARGET ${EXAMPLE} PROPERTY LINK_DEPENDS ${CMAKE_SOURCE_DIR}/e2-project/script/${LINKER_SCRIPT})
endforeach()



