# Makefile for MSP-EXP430F5438 board with CC2560B/CC2564B/CC2567 modules
##
# mspgcc is used: http://sourceforge.net/apps/mediawiki/mspgcc/index.php?title=MSPGCC_Wiki
#

# init scripts
CC2560B = bluetooth_init_cc2560B_1.2_BT_Spec_4.1.o
CC2564B = bluetooth_init_cc2564B_1.2_BT_Spec_4.1.o
CC2567  = CC256x_BT_Service_Pack_2.8_ANT_1.16.o

BTSTACK_ROOT = ../..
VPATH += example
VPATH += firmware
VPATH += src
VPATH += ${BTSTACK_ROOT}/platforms/msp430
VPATH += $(BTSTACK_ROOT)/ble
VPATH += $(BTSTACK_ROOT)/chipset-cc256x
VPATH += $(BTSTACK_ROOT)/example/embedded
VPATH += $(BTSTACK_ROOT)/src

CC      = msp430-gcc
CFLAGS  = -mmcu=msp430f5438a -Os -Wall -fno-toplevel-reorder
CFLAGS += \
	-I. \
	-I src \
	-I firmware \
	-I$(BTSTACK_ROOT)/include \
  	-I$(BTSTACK_ROOT)/src \
	-I$(BTSTACK_ROOT)/ble \
	-I$(BTSTACK_ROOT)/chipset-cc256x \
    -I${BTSTACK_ROOT}/platforms/msp430 \

LDFLAGS = -mmcu=msp430f5438a

CORE   = \
    hal_tick.c	              \
    hal_cpu.c                 \
    hal_board.c	              \
    hal_led.c 				  \
    hal_compat.c              \
    hal_usb.c                 \
    hci_dump.c		          \
    btstack_memory.c          \
    linked_list.c	          \
    memory_pool.c             \
    run_loop.c		          \
    run_loop_embedded.c       \
    utils.c			          \
    main.c 					  \

COMMON = \
    hal_uart_dma.c            \
	bt_control_cc256x.c       \
    hci.c			          \
    hci_cmds.c		          \
    hci_transport_h4_ehcill_dma.c \
	remote_device_db_memory.c \

SPP = \
    l2cap.c			          \
    l2cap_signaling.c         \
	rfcomm.c			      \
    sdp_util.c                \

SDP_CLIENT += \
    sdp_client.o		      \
    sdp_parser.o		      \
    sdp_query_util.o		  \
	sdp_query_rfcomm.o		  \

BLE = \
	att.c 					  \
	att_server.c     		  \
	le_device_db_memory.c  \
	sm.c                      \
	att_dispatch.c            \
	l2cap_le.c 				  \
	${CC2564B}                \
#	gatt_client.c             \

LCD = hal_lcd.c hal_lcd_fonts.c

	
CORE_OBJ   = $(CORE:.c=.o)
COMMON_OBJ = $(COMMON:.c=.o)
SPP_OBJ    = $(SPP:.c=.o)
BLE_OBJ    = $(BLE:.c=.o)
LCD_OBJ    = $(LCD:.c=.o)

# create .hex file from .out
%.hex: %.out
	msp430-objcopy -O ihex $< $@    

# create firmware image from common objects and example source file

all: led_counter.hex  gap_inquiry.hex sdp_general_query.hex 

# fetch and convert init scripts
include ${BTSTACK_ROOT}/chipset-cc256x/Makefile.inc

# compiling requires a 20-bit mspgcc version
# hid_demo.hex 
# spp_and_le_counter.hex
# spp_accel.hex
# ble_server.hex
# spp_counter.hex
# spp_flowcontrol.hex 
# sdp_rfcomm_query.hex

# compiling ant-test requires special ant init script
# ant-test.hex

# compile GATT database
%.h: %.gatt
	python ${BTSTACK_ROOT}/ble/compile-gatt.py $< $@ 


led_counter.out: ${CORE_OBJ} ${COMMON_OBJ} ${CC2560B} led_counter.o 
	${CC} $^ ${LDFLAGS} -o $@

ant-test.out: ${CORE_OBJ} ${COMMON_OBJ} ${SPP_OBJ} profile.h sdp.o ant_cmds.o ${CC2567} ant-test.o
	${CC} $^ ${LDFLAGS} -o $@

ble_server.out: ${CORE_OBJ} ${COMMON_OBJ} ${BLE_OBJ} ${LCD_OBJ} profile.h ble_server.o
	${CC} $^ ${LDFLAGS} -o $@

hid_demo.out: ${CORE_OBJ} ${COMMON_OBJ} ${LCD_OBJ} ${SPP_OBJ} ${CC2560B} hid_demo.o 
	${CC} $^ ${LDFLAGS} -o $@

spp_accel.out: ${CORE_OBJ} ${COMMON_OBJ} ${SPP_OBJ} ${CC2560B} spp_accel.o sdp.o hal_adc.o
	${CC} $^ ${LDFLAGS} -o $@

spp_and_le_counter.out: ${CORE_OBJ} ${COMMON_OBJ} ${SPP_OBJ} ${BLE_OBJ} spp_and_le_counter.h spp_and_le_counter.o sdp.o 
	${CC} $^ ${LDFLAGS} -o $@

spp_counter.out: ${CORE_OBJ} ${COMMON_OBJ} ${SPP_OBJ} ${CC2560B} spp_counter.o sdp.o  
	${CC} $^ ${LDFLAGS} -o $@

spp_flowcontrol.out: ${CORE_OBJ} ${COMMON_OBJ} ${SPP_OBJ} ${CC2560B} spp_flowcontrol.o sdp.o  
	${CC} $^ ${LDFLAGS} -o $@

gap_inquiry.out: ${CORE_OBJ} ${COMMON_OBJ} ${SPP_OBJ} ${CC2560B} gap_inquiry.o 
	${CC} $^ ${LDFLAGS} -o $@

sdp_rfcomm_query.out: ${CORE_OBJ} ${COMMON_OBJ} ${SPP_OBJ} ${SDP_CLIENT} ${CC2560B} sdp_rfcomm_query.o 
	${CC} $^ ${LDFLAGS} -o $@

sdp_general_query.out: ${CORE_OBJ} ${COMMON_OBJ} ${SPP_OBJ} ${SDP_CLIENT} ${CC2560B} sdp_general_query.o
	${CC} $^ ${LDFLAGS} -o $@

clean:
	rm -f $ *.o *.out *.hex profile.h spp_and_le_counter.h

size: all
	msp430-size *.o
	msp430-size *.out 
