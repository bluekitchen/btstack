#
# Makefile for MSP-EXP430F5438 demos with PAN1315, PAN1317, PAN1325, PAN1323, or PAN1327 Bluetooth modules
#
# For the PAN1315/25 modules, please use bluetooth_init_cc2560_*.c in COMMON below
# Please check instructions at https://code.google.com/p/btstack/wiki/CC256x on where to download the init scripts and how to convert them to *.c files.
#
# mspgcc is used: http://sourceforge.net/apps/mediawiki/mspgcc/index.php?title=MSPGCC_Wiki
#

BTSTACK_ROOT = ../..

CC      = msp430-gcc
CFLAGS  = -mmcu=msp430f5438a -Os -Wall -fno-toplevel-reorder -I. -I../src -I../firmware -I${BTSTACK_ROOT} -I${BTSTACK_ROOT}/chipset-cc256x -I${BTSTACK_ROOT}/src -I${BTSTACK_ROOT}/include
LDFLAGS = -mmcu=msp430f5438a

CORE   = \
    ../src/hal_tick.c	                          \
    ../src/hal_cpu.c                              \
    ../firmware/hal_board.c	                      \
    ../firmware/hal_compat.c                      \
    ../firmware/hal_usb.c                         \
    ${BTSTACK_ROOT}/src/btstack_memory.c          \
    ${BTSTACK_ROOT}/src/linked_list.c	          \
    ${BTSTACK_ROOT}/src/memory_pool.c             \
    ${BTSTACK_ROOT}/src/run_loop.c		          \
    ${BTSTACK_ROOT}/src/run_loop_embedded.c

COMMON = \
    ../src/hal_uart_dma.c                         \
	${BTSTACK_ROOT}/chipset-cc256x/bt_control_cc256x.c   \
	${BTSTACK_ROOT}/chipset-cc256x/bluetooth_init_cc2560B_1.0_BT_Spec_4.1.c   \
    ${BTSTACK_ROOT}/src/hci.c			          \
    ${BTSTACK_ROOT}/src/hci_cmds.c		          \
    ${BTSTACK_ROOT}/src/hci_dump.c		          \
    ${BTSTACK_ROOT}/src/hci_transport_h4_ehcill_dma.c \
    ${BTSTACK_ROOT}/src/l2cap.c			          \
    ${BTSTACK_ROOT}/src/l2cap_signaling.c         \
	${BTSTACK_ROOT}/src/remote_device_db_memory.c \
	${BTSTACK_ROOT}/src/rfcomm.c			      \
    ${BTSTACK_ROOT}/src/sdp.c			          \
    ${BTSTACK_ROOT}/src/sdp_util.c                \
    ${BTSTACK_ROOT}/src/utils.c			          \
    ${BTSTACK_ROOT}/src/sdp_parser.c              \
    ${BTSTACK_ROOT}/src/sdp_client.c              \
    ${BTSTACK_ROOT}/src/sdp_query_util.c		  \
	${BTSTACK_ROOT}/src/sdp_query_rfcomm.c 		  \
#	${BTSTACK_ROOT}/chipset-cc256x/bluetooth_init_cc2560A_2.1.c \

LCD = ../firmware/hal_lcd.c ../firmware/hal_lcd_fonts.c

	
CORE_OBJ   = $(CORE:.c=.o)
COMMON_OBJ = $(COMMON:.c=.o)
LCD_OBJ    = $(LCD:.c=.o)

# create .hex file from .out
%.hex: %.out
	msp430-objcopy -O ihex $< $@    

# create firmware image from common objects and example source file

all: led_counter.hex spp_counter.hex spp_accel.hex gap_inquiry.hex spp_flowcontrol.hex \
	sdp_rfcomm_query.hex sdp_general_query.hex

# compiling hid_demo requires a 20-bit mspgcc version
# hid_demo.hex 

led_counter.out: ${CORE_OBJ} led_counter.o 
	${CC} $^ ${LDFLAGS} -o $@

hid_demo.out: ${CORE_OBJ} ${COMMON_OBJ} ${LCD_OBJ} hid_demo.o 
	${CC} $^ ${LDFLAGS} -o $@

spp_accel.out: ${CORE_OBJ} ${COMMON_OBJ} spp_accel.o ../firmware/hal_adc.o
	${CC} $^ ${LDFLAGS} -o $@

spp_counter.out: ${CORE_OBJ} ${COMMON_OBJ} spp_counter.o ../firmware/hal_adc.o
	${CC} $^ ${LDFLAGS} -o $@

spp_flowcontrol.out: ${CORE_OBJ} ${COMMON_OBJ} spp_flowcontrol.o ../firmware/hal_adc.o
	${CC} $^ ${LDFLAGS} -o $@

gap_inquiry.out: ${CORE_OBJ} ${COMMON_OBJ} gap_inquiry.o ../firmware/hal_adc.o
	${CC} $^ ${LDFLAGS} -o $@

sdp_rfcomm_query.out: ${CORE_OBJ} ${COMMON_OBJ} sdp_rfcomm_query.o ../firmware/hal_adc.o
	${CC} $^ ${LDFLAGS} -o $@

sdp_general_query.out: ${CORE_OBJ} ${COMMON_OBJ} sdp_general_query.o ../firmware/hal_adc.o
	${CC} $^ ${LDFLAGS} -o $@

clean:
	rm -f $ *.o *.out *.hex ../driver/*.o ../../src/*.o ../src/*.o ../firmware/*.o ${BTSTACK_ROOT}/chipset-cc256x/*.o ${BTSTACK_ROOT}/src/*.o

size: all
	msp430-size ../firmware/*.o
	msp430-size ../src/*.o
	msp430-size ${BTSTACK_ROOT}/chipset-cc256x/*.o
	msp430-size ${BTSTACK_ROOT}/src/*.o
	msp430-size *.o
	msp430-size *.out 
