# Makefile for MSP-EXP430F5438 board with CC2560B/CC2564B/CC2567 modules
#
# mspgcc is used: http://sourceforge.net/apps/mediawiki/mspgcc/index.php?title=MSPGCC_Wiki
#

# init scripts
CC2560B = bluetooth_init_cc2560B_1.8_BT_Spec_4.1.o
CC2564B = bluetooth_init_cc2564B_1.8_BT_Spec_4.1.o
CC2567  = CC256x_BT_Service_Pack_2.8_ANT_1.16.o

BTSTACK_ROOT ?= ../..
VPATH += example
VPATH += firmware
VPATH += src
VPATH += ${BTSTACK_ROOT}/platform/msp430
VPATH += $(BTSTACK_ROOT)/src
VPATH += $(BTSTACK_ROOT)/src/ble
VPATH += $(BTSTACK_ROOT)/src/classic
VPATH += $(BTSTACK_ROOT)/chipset/cc256x
VPATH += $(BTSTACK_ROOT)/platform/embedded
VPATH += $(BTSTACK_ROOT)/example

MCU = msp430f5529

CC      = msp430-gcc
CFLAGS  = -mmcu=${MCU} -Os -Wall -fno-toplevel-reorder
CFLAGS += \
	-I. \
	-I src \
	-I firmware \
  	-I$(BTSTACK_ROOT)/src \
	-I$(BTSTACK_ROOT)/chipset/cc256x \
	-I$(BTSTACK_ROOT)/platform/embedded \
    -I${BTSTACK_ROOT}/platform/msp430 \

LDFLAGS = -mmcu=${MCU}

CORE   = \
    btstack_linked_list.c     \
    btstack_memory.c          \
    btstack_memory_pool.c       \
    btstack_run_loop.c		    \
    btstack_run_loop_embedded.c \
    btstack_tlv.c             \
    hal_board.c	              \
    hal_compat.c              \
    hal_cpu.c                 \
    hal_led.c 				  \
    hal_tick.c	              \
    hal_usb.c                 \
    hci_dump.c		          \
    main.c 					  \
    btstack_util.c			          \

COMMON = \
	ad_parser.c                        \
	btstack_chipset_cc256x.c           \
	btstack_link_key_db_memory.c       \
    btstack_uart_block_embedded.c      \
    hal_uart_dma.c            		   \
    hci.c			          		   \
    hci_cmd.c		          		   \
    hci_transport_h4.c                 \

SPP = \
    l2cap.c			          \
    l2cap_signaling.c         \
	rfcomm.c			      \
    sdp_util.c                \
    spp_server.c              \

SDP_CLIENT += \
    sdp_client.o		      \
	sdp_client_rfcomm.o		  \

BLE = \
	att_db.c 					  \
	att_server.c     		  \
	le_device_db_memory.c     \
	sm.c                      \
	att_dispatch.c            \
	l2cap.c 				  \
    l2cap_signaling.c         \
	${CC2564B}                \
#	gatt_client.c             \

LCD = hal_lcd.c hal_lcd_fonts.c

	
CORE_OBJ   = $(CORE:.c=.o)
COMMON_OBJ = $(COMMON:.c=.o)
SPP_OBJ    = $(SPP:.c=.o)
BLE_OBJ    = $(BLE:.c=.o)

# create .hex file from .out
%.hex: %.out
	msp430-objcopy -O ihex $< $@    

# create firmware image from common objects and example source file

all: led_counter.hex spp_counter.hex gap_inquiry.hex spp_flowcontrol.hex \
	 sdp_rfcomm_query.hex sdp_general_query.hex 
# ble_server.hex

# fetch and convert init scripts
include ${BTSTACK_ROOT}/chipset/cc256x/Makefile.inc

# compiling requires a 20-bit mspgcc version
# spp_and_gatt_counter.hex

# compiling ant-test requires special ant init script
# ant-test.hex

# compile GATT database
%.h: %.gatt
	python3 ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@ 


led_counter.out: ${CORE_OBJ} ${COMMON_OBJ} ${CC2560B} led_counter.o 
	${CC} $^ ${LDFLAGS} -o $@

ant-test.out: ${CORE_OBJ} ${COMMON_OBJ} ${SPP_OBJ} profile.h ant_cmds.o ${CC2567} ant-test.o
	${CC} $^ ${LDFLAGS} -o $@

ble_server.out: ${CORE_OBJ} ${COMMON_OBJ} ${BLE_OBJ} profile.h ble_server.o
	${CC} $^ ${LDFLAGS} -o $@

spp_and_gatt_counter.out: ${CORE_OBJ} ${COMMON_OBJ} ${SPP_OBJ} ${BLE_OBJ} spp_and_gatt_counter.h spp_and_gatt_counter.o sdp_server.o
	${CC} $^ ${LDFLAGS} -o $@

spp_counter.out: ${CORE_OBJ} ${COMMON_OBJ} ${SPP_OBJ} ${CC2560B} spp_counter.o sdp_server.o
	${CC} $^ ${LDFLAGS} -o $@

spp_flowcontrol.out: ${CORE_OBJ} ${COMMON_OBJ} ${SPP_OBJ} ${CC2560B} spp_flowcontrol.o sdp_server.o
	${CC} $^ ${LDFLAGS} -o $@

gap_inquiry.out: ${CORE_OBJ} ${COMMON_OBJ} ${SPP_OBJ} ${CC2560B} ad_parser.o gap_inquiry.o
	${CC} $^ ${LDFLAGS} -o $@

sdp_rfcomm_query.out: ${CORE_OBJ} ${COMMON_OBJ} ${SPP_OBJ} ${SDP_CLIENT} ${CC2560B} sdp_rfcomm_query.o
	${CC} $^ ${LDFLAGS} -o $@

sdp_general_query.out: ${CORE_OBJ} ${COMMON_OBJ} ${SPP_OBJ} ${SDP_CLIENT} ${CC2560B} sdp_general_query.o
	${CC} $^ ${LDFLAGS} -o $@

clean:
	rm -f $ *.o *.out *.hex profile.h spp_and_gatt_counter.h ../driver/*.o ../../src/*.o ../src/*.o ../firmware/*.o ${BTSTACK_ROOT}/chipset/cc256x/*.o ${BTSTACK_ROOT}/src/*.o

size: all
	msp430-size *.o
	msp430-size *.out 

%.flash: %.hex
	mspdebug tilib --force-reset "prog $^"

