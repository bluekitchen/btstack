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
PROJECT=spp_and_le_streamer

# Specify the target processor
TARGET=MAX3263x
PROJ_CFLAGS+=-DRO_FREQ=96000000
PROJ_CFLAGS+=-g3 -ggdb -DDEBUG
CPPFLAGS+=-g3 -ggdb -DDEBUG

# Create Target name variables
TARGET_UC:=$(shell echo $(TARGET) | tr a-z A-Z)
TARGET_LC:=$(shell echo $(TARGET) | tr A-Z a-z)

CC2564B = bluetooth_init_cc2564B_1.8_BT_Spec_4.1.o

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
VPATH= . ../../src

# Where to find header files for this test
IPATH= . ../../src

BOARD_DIR=$(LIBS_DIR)/Boards

IPATH += ../../board/
VPATH += ../../board/

# Source files for this test (add path to VPATH below)
SRCS = main.c
SRCS += hal_tick.c
SRCS += btstack_port.c
SRCS += ${PROJECT}.c
SRCS += board.c
SRCS += stdio.c
SRCS += led.c
SRCS += pb.c
SRCS += max14690n.c

# Where to find BSP source files
VPATH += $(BOARD_DIR)/Source

# Where to find BSP header files
IPATH += $(BOARD_DIR)/Include

# BTstack
BTSTACK_ROOT ?= ../../../..
VPATH += $(BTSTACK_ROOT)/chipset/cc256x
VPATH += $(BTSTACK_ROOT)/example
VPATH += $(BTSTACK_ROOT)/port/pegasus-max3263x
VPATH += $(BTSTACK_ROOT)/src
VPATH += $(BTSTACK_ROOT)/src/ble
VPATH += $(BTSTACK_ROOT)/src/classic
VPATH += ${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/srce 
VPATH += ${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/srce
VPATH += ${BTSTACK_ROOT}/3rd-party/hxcmod-player
VPATH += ${BTSTACK_ROOT}/3rd-party/hxcmod-player/mods
VPATH += ${BTSTACK_ROOT}/3rd-party/lwip/core/src/core/
VPATH += ${BTSTACK_ROOT}/3rd-party/lwip/core/src/core/ipv4
VPATH += ${BTSTACK_ROOT}/3rd-party/lwip/core/src/core/ipv6
VPATH += ${BTSTACK_ROOT}/3rd-party/lwip/core/src/netif
VPATH += ${BTSTACK_ROOT}/3rd-party/lwip/core/src/apps/http
VPATH += ${BTSTACK_ROOT}/3rd-party/lwip/dhcp-server
VPATH += ${BTSTACK_ROOT}/3rd-party/md5
VPATH += ${BTSTACK_ROOT}/3rd-party/yxml
VPATH += ${BTSTACK_ROOT}/3rd-party/micro-ecc
VPATH += ${BTSTACK_ROOT}/platform/embedded
VPATH += ${BTSTACK_ROOT}/platform/lwip
VPATH += ${BTSTACK_ROOT}/platform/lwip/port
VPATH += ${BTSTACK_ROOT}/src/ble/gatt-service/

PROJ_CFLAGS += \
    -I$(BTSTACK_ROOT)/src \
    -I$(BTSTACK_ROOT)/src/ble \
    -I$(BTSTACK_ROOT)/src/classic \
    -I$(BTSTACK_ROOT)/chipset/cc256x \
    -I$(BTSTACK_ROOT)/platform/embedded \
    -I$(BTSTACK_ROOT)/platform/lwip \
    -I$(BTSTACK_ROOT)/platform/lwip/port \
    -I${BTSTACK_ROOT}/port/pegasus-max3263x \
    -I${BTSTACK_ROOT}/src/ble/gatt-service/ \
    -I${BTSTACK_ROOT}/example \
    -I${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/include \
	-I${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/include \
    -I${BTSTACK_ROOT}/3rd-party/md5 \
    -I${BTSTACK_ROOT}/3rd-party/yxml \
	-I${BTSTACK_ROOT}/3rd-party/micro-ecc \
	-I${BTSTACK_ROOT}/3rd-party/hxcmod-player \
	-I${BTSTACK_ROOT}/3rd-party/lwip/core/src/include \
	-I${BTSTACK_ROOT}/3rd-party/lwip/dhcp-server \


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
    hci_dump_embedded_stdout.o    \
    btstack_uart_block_embedded.o \
    hal_flash_bank_mxc.o      \
    btstack_audio.o           \
    btstack_tlv.o             \
    btstack_tlv_flash_bank.o  \
    btstack_stdin_embedded.o  \
    btstack_crypto.o          \
    
CLASSIC = \
    btstack_link_key_db_tlv.o \
    hid_device.o              \
    hid_host.o                \
    rfcomm.o                  \
    sdp_util.o              \
    spp_server.o            \
    sdp_server.o              \
    sdp_client.o              \
    sdp_client_rfcomm.o

BLE = \
    att_db.o                      \
    att_server.o              \
    le_device_db_tlv.o  \
    att_dispatch.o            \
    sm.o \
    ancs_client.o \
    gatt_client.o \
    hid_device.o \
    battery_service_server.o \
    uECC.o \

AVDTP += \
	avdtp_util.c  		\
	avdtp.c  			\
	avdtp_initiator.c 	\
	avdtp_acceptor.c  	\
	avdtp_source.c 		\
	avdtp_sink.c  		\
	a2dp_source.c 		\
	a2dp_sink.c  		\
	btstack_ring_buffer.c \
    btstack_resample.c  \
	avrcp.c \
	avrcp_target.c \
	avrcp_controller.c \

HFP_OBJ += sco_demo_util.o btstack_ring_buffer.o hfp.o hfp_gsm_model.o hfp_ag.o hfp_hf.o

# List of files for Bluedroid SBC codec
include ${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/Makefile.inc
include ${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/Makefile.inc

SBC_DECODER += \
	btstack_sbc_plc.c \
	btstack_sbc_decoder_bluedroid.c \

SBC_ENCODER += \
	btstack_sbc_encoder_bluedroid.c \
	hfp_msbc.c \

HXCMOD_PLAYER = \
	hxcmod.c 						\
	nao-deceased_by_disease.c 	\

LWIP_CORE_SRC  = init.c mem.c memp.c netif.c udp.c ip.c pbuf.c inet_chksum.c def.c tcp.c tcp_in.c tcp_out.c timeouts.c sys_arch.c
LWIP_IPV4_SRC  = acd.c dhcp.c etharp.c icmp.c ip4.c ip4_frag.c ip4_addr.c
LWIP_NETIF_SRC = ethernet.c
LWIP_HTTPD = altcp_proxyconnect.c fs.c httpd.c
LWIP_SRC = ${LWIP_CORE_SRC} ${LWIP_IPV4_SRC} ${LWIP_NETIF_SRC} ${LWIP_HTTPD} dhserver.c

ADDITION =

CORE_OBJ   = $(CORE:.c=.o)
COMMON_OBJ = $(COMMON:.c=.o)
BLE_OBJ    = $(BLE:.c=.o)
CLASSIC_OBJ = $(CLASSIC:.c=.o)
AVDTP_OBJ   = $(AVDTP:.c=.o)
SBC_DECODER_OBJ  = $(SBC_DECODER:.c=.o) 
SBC_ENCODER_OBJ  = $(SBC_ENCODER:.c=.o)
CVSD_PLC_OBJ = $(CVSD_PLC:.c=.o)
HXCMOD_PLAYER_OBJ = $(HXCMOD_PLAYER:.c=.o)

SRCS += $(CORE_OBJ)
SRCS += $(COMMON_OBJ)
SRCS += $(BLE_OBJ)
SRCS += $(CLASSIC_OBJ)
SRCS += $(AVDTP_OBJ)
SRCS += $(SBC_DECODER_OBJ)
SRCS += $(SBC_ENCODER_OBJ)
SRCS += $(CVSD_PLC_OBJ)
SRCS += $(HXCMOD_PLAYER_OBJ)
SRCS += $(HFP_OBJ)
SRCS += hsp_hs.o hsp_ag.o 
SRCS += obex_iterator.o goep_client.o pbap_client.o md5.o yxml.o
SRCS +=  pan.c bnep.c bnep_lwip.c
SRCS += ${LWIP_SRC}

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

%.h: %.gatt
	python3 ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@

all: spp_and_le_streamer.h

# Include the peripheral driver
PERIPH_DRIVER_DIR=$(LIBS_DIR)/$(TARGET_UC)PeriphDriver
include $(PERIPH_DRIVER_DIR)/periphdriver.mk

################################################################################
# Include the rules for building for this target. All other makefiles should be
# included before this one.
include $(CMSIS_ROOT)/Device/Maxim/$(TARGET_UC)/Source/$(COMPILER)/$(TARGET_LC).mk

# fetch and convert init scripts
# use bluetooth_init_cc2564B_1.8_BT_Spec_4.1.c
include ${BTSTACK_ROOT}/chipset/cc256x/Makefile.inc

rm-compiled-gatt-file:
	rm -f spp_and_le_counter.h

clean: rm-compiled-gatt-file

# The rule to clean out all the build products.
distclean: clean
	$(MAKE) -C ${PERIPH_DRIVER_DIR} clean
