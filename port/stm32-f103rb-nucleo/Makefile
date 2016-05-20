#
# Makefile for STM32-F103RB Nucleo board with CC256x Bluetooth modules
#

# BINARY=led_counter
BINARY=spp_and_le_counter
OPENCM3_DIR = libopencm3
LDSCRIPT = stm32f1-nucleo.ld

BTSTACK_ROOT = ../..
VPATH  = $(BTSTACK_ROOT)/example
VPATH += $(BTSTACK_ROOT)/src
VPATH += $(BTSTACK_ROOT)/src/ble
VPATH += $(BTSTACK_ROOT)/src/classic
VPATH += $(BTSTACK_ROOT)/platform/embedded
VPATH += $(BTSTACK_ROOT)/chipset/cc256x

CORE = \
	main.c 					    \
    btstack_linked_list.c	    \
    btstack_memory.c            \
    btstack_memory_pool.c       \
    btstack_run_loop.c	        \
    btstack_run_loop_embedded.c \

COMMON = \
	bluetooth_init_cc2564B_1.4_BT_Spec_4.1.c   \
	btstack_chipset_cc256x.c   \
	btstack_link_key_db_memory.c \
	rfcomm.c			      \
	sdp_client_rfcomm.c 		  \
    btstack_util.c			  \
    hci.c			          \
    hci_cmd.c		          \
    hci_dump.c		          \
    btstack_uart_block_embedded.c \
    hci_transport_h4_ehcill_embedded.c \
    l2cap.c			          \
    l2cap_signaling.c         \
    sdp_client.c              \
    sdp_server.c			  \
    sdp_util.c                \
    spp_server.c			  \

BLE = \
	att_db.c 				  \
	att_dispatch.c            \
	att_server.c     		  \
	le_device_db_memory.c     \
	sm.c                      \
#	gatt_client.c             \

CORE_OBJ   = $(CORE:.c=.o)
COMMON_OBJ = $(COMMON:.c=.o)
BLE_OBJ    = $(BLE:.c=.o)

OBJS += $(CORE_OBJ) $(COMMON_OBJ) $(BLE_OBJ)

CFLAGS = -I. \
		 -I$(BTSTACK_ROOT)/platform/embedded \
		 -I$(BTSTACK_ROOT)/src \
		 -I$(BTSTACK_ROOT)/chipset/cc256x

examples: libopencm3/lib/libopencm3_stm32f1.a spp_and_le_counter.h spp_and_le_counter.elf

include libopencm3.stm32f1.mk

# fetch and convert init scripts
include ${BTSTACK_ROOT}/chipset/cc256x/Makefile.inc

clean:
	rm -f *.map *.o *.d *.out *.elf *.bin *.hex 

distclean: clean
	make -C libopencm3 clean

# git clone and compile libopencm3
libopencm3/lib/libopencm3_stm32f1.a:
	rm -rf libopencm3
	git clone https://github.com/libopencm3/libopencm3.git
	make -C libopencm3

# compile GATT database
spp_and_le_counter.h: spp_and_le_counter.gatt
	python ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@ 

