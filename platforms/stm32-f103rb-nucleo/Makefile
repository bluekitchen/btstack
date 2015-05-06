#
# Makefile for STM32-F103RB Nucleo board with CC256x Bluetooth modules
#

# BINARY=led_counter
BINARY=spp_and_le_counter
OPENCM3_DIR = libopencm3
LDSCRIPT = stm32f1-nucleo.ld

BTSTACK_ROOT = ../..
VPATH  = $(BTSTACK_ROOT)/example/embedded
VPATH += $(BTSTACK_ROOT)/src
VPATH += $(BTSTACK_ROOT)/ble
VPATH += $(BTSTACK_ROOT)/chipset-cc256x

CORE   = \
	main.c 					  \
    btstack_memory.c          \
    linked_list.c	          \
    memory_pool.c             \
    run_loop.c		          \
    run_loop_embedded.c

COMMON = \
	bt_control_cc256x.c   \
	bluetooth_init_cc2564B_1.2_BT_Spec_4.1.c   \
    hci.c			          \
    hci_cmds.c		          \
    hci_dump.c		          \
    hci_transport_h4_ehcill_dma.c \
    l2cap.c			          \
    l2cap_signaling.c         \
	remote_device_db_memory.c \
	rfcomm.c			      \
    sdp.c			          \
    sdp_util.c                \
    utils.c			          \
    sdp_parser.c              \
    sdp_client.c              \
    sdp_query_util.c		  \
	sdp_query_rfcomm.c 		  \

BLE = \
	att.c 					  \
	att_server.c     		  \
	le_device_db_memory.c  \
	sm.c                      \
	att_dispatch.c            \
#	gatt_client.c             \

CORE_OBJ   = $(CORE:.c=.o)
COMMON_OBJ   = $(COMMON:.c=.o)
BLE_OBJ   = $(BLE:.c=.o)

OBJS += $(CORE_OBJ) $(COMMON_OBJ) $(BLE_OBJ)

CFLAGS = -I. -I$(BTSTACK_ROOT)/include -I$(BTSTACK_ROOT)/src -I$(BTSTACK_ROOT)/ble -I$(BTSTACK_ROOT)/chipset-cc256x

examples: libopencm3/lib/libopencm3_stm32f1.a spp_and_le_counter.h spp_and_le_counter.elf

include libopencm3.stm32f1.mk

# fetch and convert init scripts
include ${BTSTACK_ROOT}/chipset-cc256x/Makefile.inc

# clean:
#	rm -f *.map *.o *.d *.out *.elf *.bin *.hex ../driver/*.o ../../src/*.o ../../ble/*.o ../firmware/*.o ${BTSTACK_ROOT}/chipset-cc256x/*.o ${BTSTACK_ROOT}/src/*.o

# git clone and compile libopencm3
libopencm3/lib/libopencm3_stm32f1.a:
	git clone https://github.com/libopencm3/libopencm3.git
	make -C libopencm3

# compile GATT database
spp_and_le_counter.h: spp_and_le_counter.gatt
	python ${BTSTACK_ROOT}/ble/compile-gatt.py $< $@ 

