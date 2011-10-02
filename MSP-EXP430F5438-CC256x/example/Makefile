#
# Makefile for MSP-EXP430F5438 demos with PAN1315, PAN1317, PAN1325, PAN1323, or PAN1327 Bluetooth modules
#
# For the PAN1315/25 modules, please use cc2560_init_scipt.c in COMMON below
# 
# mspgcc is used: http://sourceforge.net/apps/mediawiki/mspgcc/index.php?title=MSPGCC_Wiki
#

BTSTACK_ROOT = ../..

CC      = msp430-gcc
CFLAGS  = -mmcu=msp430x5438 -g -Os -Wall -I. -I../src -I../firmware -I${BTSTACK_ROOT} -I${BTSTACK_ROOT}/chipset-cc256x -I${BTSTACK_ROOT}/src -I${BTSTACK_ROOT}/include
LDFLAGS = -mmcu=msp430x5438

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
	${BTSTACK_ROOT}/chipset-cc256x/cc2567_init_script.c  \
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

LCD = ../firmware/hal_lcd.c ../firmware/hal_lcd_fonts.c

	
CORE_OBJ   = $(CORE:.c=.o)
COMMON_OBJ = $(COMMON:.c=.o)
LCD_OBJ    = $(LCD:.c=.o)

# create .hex file from .out
%.hex: %.out
	msp430-objcopy -O ihex $< $@    

# create firmware image from common objects and example source file

all: hid_demo.hex led_counter.hex spp_accel.hex spp_counter.hex spp_flowcontrol.hex

led_counter.out: ${CORE_OBJ} led_counter.o 
	${CC} $^ ${LDFLAGS} -o $@

hid_demo.out: ${CORE_OBJ} ${COMMON_OBJ} ${LCD_OBJ} hid_demo.o 
	${CC} $^ ${LDFLAGS} -o $@

spp_accel.out: ${CORE_OBJ} ${COMMON_OBJ} ${LCD_OBJ} spp_accel.o ../firmware/hal_adc.o
	${CC} $^ ${LDFLAGS} -o $@

spp_counter.out: ${CORE_OBJ} ${COMMON_OBJ} spp_counter.o ../firmware/hal_adc.o
	${CC} $^ ${LDFLAGS} -o $@

spp_flowcontrol.out: ${CORE_OBJ} ${COMMON_OBJ} ${LCD_OBJ} spp_flowcontrol.o ../firmware/hal_adc.o
	${CC} $^ ${LDFLAGS} -o $@

clean:
	rm -f $ *.o *.out *.hex ../driver/*.o ../../src/*.o ../src/*.o

size: all
	msp430-size ../firmware/*.o
	msp430-size ../src/*.o
	msp430-size ${BTSTACK_ROOT}/chipset-cc256x/*.o
	msp430-size ${BTSTACK_ROOT}/src/*.o
	msp430-size *.o
	msp430-size *.out 
