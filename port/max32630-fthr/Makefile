################################################################################
 # Copyright (C) 2016 Maxim Integrated Products, Inc., All Rights Reserved.
 # Ismail H. Kose <ismail.kose@maximintegrated.com>
 # Permission is hereby granted, free of charge, to any person obtaining a
 # copy of this software and associated documentation files (the "Software"),
 # to deal in the Software without restriction, including without limitation
 # the rights to use, copy, modify, merge, publish, distribute, sublicense,
 # and/or sell copies of the Software, and to permit persons to whom the
 # Software is furnished to do so, subject to the following conditions:
 #
 # The above copyright notice and this permission notice shall be included
 # in all copies or substantial portions of the Software.
 #
 # THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 # OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 # MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 # IN NO EVENT SHALL MAXIM INTEGRATED BE LIABLE FOR ANY CLAIM, DAMAGES
 # OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 # ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 # OTHER DEALINGS IN THE SOFTWARE.
 #
 # Except as contained in this notice, the name of Maxim Integrated
 # Products, Inc. shall not be used except as stated in the Maxim Integrated
 # Products, Inc. Branding Policy.
 #
 # The mere transfer of this software does not imply any licenses
 # of trade secrets, proprietary technology, copyrights, patents,
 # trademarks, maskwork rights, or any other form of intellectual
 # property whatsoever. Maxim Integrated Products, Inc. retains all
 # ownership rights.
 #
 # $Date: 2016-03-23 13:28:53 -0700 (Wed, 23 Mar 2016) $
 # $Revision: 22067 $
 #
 ###############################################################################

# Maxim ARM Toolchain and Libraries
# https://www.maximintegrated.com/en/products/digital/microcontrollers/MAX32630.html

# This is the name of the build output file
PROJECT=max3263x

# Specify the target processor
TARGET=MAX3263x
PROJ_CFLAGS+=-DRO_FREQ=96000000

# Create Target name variables
TARGET_UC:=$(shell echo $(TARGET) | tr a-z A-Z)
TARGET_LC:=$(shell echo $(TARGET) | tr A-Z a-z)

CC2564B = bluetooth_init_cc2564B_1.6_BT_Spec_4.1.o

# Select 'GCC' or 'IAR' compiler
COMPILER=GCC

ifeq "$(MAXIM_PATH)" ""
LIBS_DIR=/$(subst \,/,$(subst :,,$(HOME))/Maxim/Firmware/$(TARGET_UC)/Libraries)
$(warning "MAXIM_PATH need to be set. Please run setenv bash file in the Maxim Toolchain directory.")
else
LIBS_DIR=/$(subst \,/,$(subst :,,$(MAXIM_PATH))/Firmware/$(TARGET_UC)/Libraries)
endif

CMSIS_ROOT=$(LIBS_DIR)/CMSIS

# Where to find source files for this test
VPATH=.

# Where to find header files for this test
IPATH=.

BOARD_DIR=$(LIBS_DIR)/Boards

IPATH += board/
VPATH += board/

# Source files for this test (add path to VPATH below)
SRCS = main.c
SRCS += hal_tick.c
SRCS += btstack_port.c
SRCS += spp_and_le_streamer.c
SRCS += board.c
SRCS += stdio.c
SRCS += led.c
SRCS += pb.c
SRCS += max14690n.c

# Where to find BSP source files
VPATH += $(BOARD_DIR)/Source

# Where to find BSP header files
IPATH += $(BOARD_DIR)/Include

# Btstack
BTSTACK_ROOT = ../..
VPATH += $(BTSTACK_ROOT)/src
VPATH += $(BTSTACK_ROOT)/src/ble
VPATH += $(BTSTACK_ROOT)/src/classic
VPATH += $(BTSTACK_ROOT)/chipset/cc256x
VPATH += ${BTSTACK_ROOT}/platform/embedded
VPATH += ${BTSTACK_ROOT}/src/ble/gatt-service/
VPATH += $(BTSTACK_ROOT)/port/pegasus-max3263x
VPATH += $(BTSTACK_ROOT)/example

PROJ_CFLAGS += \
    -I$(BTSTACK_ROOT)/src \
    -I$(BTSTACK_ROOT)/src/ble \
    -I$(BTSTACK_ROOT)/src/classic \
    -I$(BTSTACK_ROOT)/chipset/cc256x \
    -I$(BTSTACK_ROOT)/platform/embedded \
    -I${BTSTACK_ROOT}/port/pegasus-max3263x \
    -I${BTSTACK_ROOT}/src/ble/gatt-service/ \
    -I${BTSTACK_ROOT}/example

CORE = \
        ad_parser.o \
        btstack_linked_list.o \
        btstack_memory.o \
        btstack_memory_pool.o \
        btstack_run_loop.o \
        btstack_util.o \
        l2cap.o \
        l2cap_signaling.o \
        btstack_run_loop_embedded.o \
		$(CC2564B) \
        hci_transport_h4.o

COMMON = \
    btstack_chipset_cc256x.o  \
    hci.o                     \
    hci_cmd.o                 \
    hci_dump.o                \
    btstack_uart_block_embedded.o

CLASSIC = \
        btstack_link_key_db_memory.o \
        rfcomm.o                  \
        sdp_util.o              \
        spp_server.o            \
        sdp_server.o              \
        sdp_client.o              \
        sdp_client_rfcomm.o

BLE = \
    att_db.o                      \
    att_server.o              \
    le_device_db_memory.o  \
    att_dispatch.o            \
    sm.o

ADDITION =

CORE_OBJ   = $(CORE:.c=.o)
COMMON_OBJ = $(COMMON:.c=.o)
BLE_OBJ    = $(BLE:.c=.o)
CLASSIC_OBJ = $(CLASSIC:.c=.o)

SRCS += $(CORE_OBJ)
SRCS += $(COMMON_OBJ)
SRCS += $(BLE_OBJ)
SRCS += $(CLASSIC_OBJ)

# Enable assertion checking for development
PROJ_CFLAGS+=-DMXC_ASSERT_ENABLE

# Use this variables to specify and alternate tool path
#TOOL_DIR=/opt/gcc-arm-none-eabi-4_8-2013q4/bin

# Use these variables to add project specific tool options
#PROJ_CFLAGS+=--specs=nano.specs
#PROJ_LDFLAGS+=--specs=nano.specs

# Point this variable to a startup file to override the default file
#STARTUPFILE=start.S

# Point this variable to a linker file to override the default file
# LINKERFILE=$(CMSIS_ROOT)/Device/Maxim/$(TARGET_UC)/Source/GCC/$(TARGET_LC).ld

# Include the peripheral driver
PERIPH_DRIVER_DIR=$(LIBS_DIR)/$(TARGET_UC)PeriphDriver
include $(PERIPH_DRIVER_DIR)/periphdriver.mk

################################################################################
# Include the rules for building for this target. All other makefiles should be
# included before this one.
include $(CMSIS_ROOT)/Device/Maxim/$(TARGET_UC)/Source/$(COMPILER)/$(TARGET_LC).mk

# fetch and convert init scripts
# use bluetooth_init_cc2564B_1.6_BT_Spec_4.1.c
include ${BTSTACK_ROOT}/chipset/cc256x/Makefile.inc

#include ${BTSTACK_ROOT}/example/Makefile.inc

spp_and_le_streamer.h: spp_and_le_streamer.gatt
	python ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@

generate_header_file: spp_and_le_streamer.h

# The rule to clean out all the build products.
distclean: clean
	$(MAKE) -C ${PERIPH_DRIVER_DIR} clean
