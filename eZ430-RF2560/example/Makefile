#
# Makefile for eZ430-RF2560
#
# mspgcc is used: http://sourceforge.net/apps/mediawiki/mspgcc/index.php?title=MSPGCC_Wiki
#

BTSTACK_ROOT = ../..

MCU = msp430x5438

CC      = msp430-gcc
CFLAGS  = -mmcu=${MCU} -g -Os -Wall -I. -I../src -I../firmware -I${BTSTACK_ROOT} -I${BTSTACK_ROOT}/chipset-cc256x -I${BTSTACK_ROOT}/src -I${BTSTACK_ROOT}/include
LDFLAGS = -mmcu=${MCU}

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
	${BTSTACK_ROOT}/chipset-cc256x/cc2560_init_script.c  \
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
	
CORE_OBJ   = $(CORE:.c=.o)
COMMON_OBJ = $(COMMON:.c=.o)

# create .hex file from .out
%.hex: %.out
	msp430-objcopy -O ihex $< $@    

# create firmware image from common objects and example source file

all: led_counter.hex spp_counter.hex spp_flowcontrol.hex
# missing: spp_accel.hex

led_counter.out: ${CORE_OBJ} led_counter.o 
	${CC} $^ ${LDFLAGS} -o $@

spp_accel.out: ${CORE_OBJ} ${COMMON_OBJ} spp_accel.o ../firmware/hal_adc.o
	echo "accelerometer code not adapted yet for eZ430-RF2560 target"
	${CC} $^ ${LDFLAGS} -o $@

spp_counter.out: ${CORE_OBJ} ${COMMON_OBJ} spp_counter.o ../firmware/hal_adc.o
	${CC} $^ ${LDFLAGS} -o $@

spp_flowcontrol.out: ${CORE_OBJ} ${COMMON_OBJ} spp_flowcontrol.o ../firmware/hal_adc.o
	${CC} $^ ${LDFLAGS} -o $@

clean:
	rm -f $ *.o *.out *.hex ../driver/*.o ../../src/*.o ../src/*.o ../firmware/*.o ${BTSTACK_ROOT}/chipset-cc256x/*.o

size: all
	msp430-size ../firmware/*.o
	msp430-size ../src/*.o
	msp430-size ${BTSTACK_ROOT}/chipset-cc256x/*.o
	msp430-size ${BTSTACK_ROOT}/src/*.o
	msp430-size *.o
	msp430-size *.out 
