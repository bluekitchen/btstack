#
# Copyright (c) 2011 Atmel Corporation. All rights reserved.
#
# \asf_license_start
#
# \page License
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# 3. The name of Atmel may not be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# 4. This software may only be redistributed and used in connection with an
#    Atmel microcontroller product.
#
# THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR IMPLIED
# WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
# EXPRESSLY AND SPECIFICALLY DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
# STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
# ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#
# \asf_license_stop
#

# Path to top level ASF directory relative to this project directory.
PRJ_PATH = ../../ASF

# Target CPU architecture: cortex-m3, cortex-m4
ARCH = cortex-m7

# Target part: none, sam3n4 or sam4l4aa
PART = samv71q21

# Application target name. Given with suffix .a for library and .elf for a
# standalone application.
TARGET_FLASH=le_counter_flash.elf
TARGET_SRAM=le_counter_sram.elf

# template main file + firmware file
BTSTACK_ROOT_CONFIG = ../../../
CSRCS+=${BTSTACK_ROOT_CONFIG}/example/le_counter.c

# List of C source files.
CSRCS+= \
       ../main.c 						                 \
       common/services/clock/samv71/sysclk.c              \
       common/services/serial/usart_serial.c              \
       common/utils/interrupt/interrupt_sam_nvic.c        \
       common/utils/stdio/read.c                          \
       common/utils/stdio/write.c                         \
       sam/boards/samv71_xplained_ultra/init.c            \
       sam/drivers/matrix/matrix.c                        \
       sam/drivers/mpu/mpu.c                              \
       sam/drivers/pio/pio.c                              \
       sam/drivers/pio/pio_handler.c                      \
       sam/drivers/pmc/pmc.c                              \
       sam/drivers/pmc/sleep.c                            \
       sam/drivers/tc/tc.c                                \
       sam/drivers/uart/uart.c                            \
       sam/drivers/usart/usart.c                          \
       sam/utils/cmsis/samv71/source/templates/gcc/startup_samv71.c \
       sam/utils/cmsis/samv71/source/templates/system_samv71.c \
       sam/utils/syscalls/gcc/syscalls.c                  \

# List of assembler source files.
ASSRCS =

# List of include paths.
INC_PATH = \
       common/boards                                      \
       common/services/clock                              \
       common/services/gpio                               \
       common/services/ioport                             \
       common/services/delay                              \
       common/services/serial                             \
       common/services/serial/sam_uart                    \
       common/utils                                       \
       common/utils/stdio/stdio_serial                    \
       sam/boards                                         \
       sam/boards/samv71_xplained_ultra                   \
       sam/drivers/matrix                                 \
       sam/drivers/mpu                                    \
       sam/drivers/pio                                    \
       sam/drivers/pmc                                    \
       sam/drivers/tc                                     \
       sam/drivers/uart                                   \
       sam/drivers/usart                                  \
       sam/drivers/xdmac                                  \
       sam/utils                                          \
       sam/utils/cmsis/samv71/include                     \
       sam/utils/cmsis/samv71/source/templates            \
       sam/utils/fpu                                      \
       sam/utils/header_files                             \
       sam/utils/preprocessor                             \
       thirdparty/CMSIS/Include                           \
       thirdparty/CMSIS/Lib/GCC  						  \
	   ..

INC_PATH += ${BTSTACK_ROOT_CONFIG}/src/ble
INC_PATH += ${BTSTACK_ROOT_CONFIG}/src/ble/gatt-service
INC_PATH += ${BTSTACK_ROOT_CONFIG}/src
INC_PATH += ${BTSTACK_ROOT_CONFIG}/3rd-party/micro-ecc
INC_PATH += ${BTSTACK_ROOT_CONFIG}/platform/embedded
INC_PATH += ${BTSTACK_ROOT_CONFIG}/chipset/atwilc3000
INC_PATH += ${BTSTACK_ROOT_CONFIG}/port/samv71-xplained-atwilc3000/example/template


# VPATH += ${BTSTACK_ROOT_CONFIG}/src
# VPATH += ${BTSTACK_ROOT_CONFIG}/src/ble
# VPATH += ${BTSTACK_ROOT_CONFIG}/src/ble/gatt-service
# VPATH += ${BTSTACK_ROOT_CONFIG}/src/classic
# VPATH += ${BTSTACK_ROOT_CONFIG}/platform/embedded
# VPATH += ${BTSTACK_ROOT_CONFIG}/example
# VPATH += ${BTSTACK_ROOT_CONFIG}/3rd-party/micro-ecc

CSRCS += \
	${BTSTACK_ROOT_CONFIG}/chipset/atwilc3000/btstack_chipset_atwilc3000.c \
	${BTSTACK_ROOT_CONFIG}/platform/embedded/btstack_run_loop_embedded.c \
	${BTSTACK_ROOT_CONFIG}/platform/embedded/btstack_uart_block_embedded.c \
	${BTSTACK_ROOT_CONFIG}/platform/embedded/hci_dump_embedded_stdout.c \
	${BTSTACK_ROOT_CONFIG}/src/ad_parser.c \
	${BTSTACK_ROOT_CONFIG}/src/ble/att_db.c \
	${BTSTACK_ROOT_CONFIG}/src/ble/att_dispatch.c \
	${BTSTACK_ROOT_CONFIG}/src/ble/att_server.c \
       ${BTSTACK_ROOT_CONFIG}/src/ble/gatt-service/ancs_client.c \
	${BTSTACK_ROOT_CONFIG}/src/ble/gatt-service/battery_service_server.c \
	${BTSTACK_ROOT_CONFIG}/src/ble/gatt-service/device_information_service_server.c \
	${BTSTACK_ROOT_CONFIG}/src/ble/gatt-service/hids_device.c \
	${BTSTACK_ROOT_CONFIG}/src/ble/gatt_client.c \
	${BTSTACK_ROOT_CONFIG}/src/ble/le_device_db_memory.c \
	${BTSTACK_ROOT_CONFIG}/src/ble/sm.c \
	${BTSTACK_ROOT_CONFIG}/src/btstack_crypto.c \
	${BTSTACK_ROOT_CONFIG}/src/btstack_linked_list.c \
	${BTSTACK_ROOT_CONFIG}/src/btstack_memory.c \
	${BTSTACK_ROOT_CONFIG}/src/btstack_memory_pool.c \
	${BTSTACK_ROOT_CONFIG}/src/btstack_ring_buffer.c \
	${BTSTACK_ROOT_CONFIG}/src/btstack_run_loop.c \
	${BTSTACK_ROOT_CONFIG}/src/btstack_util.c \
	${BTSTACK_ROOT_CONFIG}/src/btstack_tlv.c \
	${BTSTACK_ROOT_CONFIG}/src/hci.c \
	${BTSTACK_ROOT_CONFIG}/src/hci_cmd.c \
	${BTSTACK_ROOT_CONFIG}/src/hci_dump.c \
	${BTSTACK_ROOT_CONFIG}/src/hci_transport_h4.c \
	${BTSTACK_ROOT_CONFIG}/src/l2cap.c \
	${BTSTACK_ROOT_CONFIG}/src/l2cap_signaling.c \

# Additional search paths for libraries.
LIB_PATH =  \
       thirdparty/CMSIS/Lib/GCC

# List of libraries to use during linking.
LIBS =  \
       arm_cortexM7lfsp_math_softfp                       \
       m

# Path relative to top level directory pointing to a linker script.
LINKER_SCRIPT_FLASH = sam/utils/linker_scripts/samv71/samv71q21/gcc/flash.ld
LINKER_SCRIPT_SRAM  = sam/utils/linker_scripts/samv71/samv71q21/gcc/sram.ld

# Path relative to top level directory pointing to a linker script.
DEBUG_SCRIPT_FLASH = sam/boards/samv71_xplained_ultra/debug_scripts/gcc/samv71_xplained_ultra_flash.gdb
DEBUG_SCRIPT_SRAM  = sam/boards/samv71_xplained_ultra/debug_scripts/gcc/samv71_xplained_ultra_sram.gdb

# Project type parameter: all, sram or flash
PROJECT_TYPE        = flash

# Additional options for debugging. By default the common Makefile.in will
# add -g3.
DBGFLAGS =

# Application optimization used during compilation and linking:
# -O0, -O1, -O2, -O3 or -Os
OPTIMIZATION = -O1

# Extra flags to use when archiving.
ARFLAGS =

# Extra flags to use when assembling.
ASFLAGS =  \
       -mfloat-abi=softfp                                 \
       -mfpu=fpv5-sp-d16

# Extra flags to use when compiling.
CFLAGS =  \
       -mfloat-abi=softfp                                 \
       -mfpu=fpv5-sp-d16

# Extra flags to use when preprocessing.
#
# Preprocessor symbol definitions
#   To add a definition use the format "-D name[=definition]".
#   To cancel a definition use the format "-U name".
#
# The most relevant symbols to define for the preprocessor are:
#   BOARD      Target board in use, see boards/board.h for a list.
#   EXT_BOARD  Optional extension board in use, see boards/board.h for a list.
CPPFLAGS = \
       -D ARM_MATH_CM7=true                               \
       -D BOARD=SAMV71_XPLAINED_ULTRA                     \
       -D __SAMV71Q21__                                   \
       -D printf=iprintf                                  \
       -D scanf=iscanf

# Extra flags to use when linking
LDFLAGS = \

# Pre- and post-build commands
PREBUILD_CMD =
POSTBUILD_CMD =
