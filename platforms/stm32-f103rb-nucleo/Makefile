#
# Makefile for STM32-F103RB Nucleo board with CC256x Bluetooth modules
#

# BINARY=led_counter
# BINARY=spp_and_le_counter
OPENCM3_DIR = libopencm3
LDSCRIPT = stm32f1-nucleo.ld

BTSTACK_ROOT = ../..

CORE   = \
	main.c 										  \
    ${BTSTACK_ROOT}/src/btstack_memory.c          \
    ${BTSTACK_ROOT}/src/linked_list.c	          \
    ${BTSTACK_ROOT}/src/memory_pool.c             \
    ${BTSTACK_ROOT}/src/run_loop.c		          \
    ${BTSTACK_ROOT}/src/run_loop_embedded.c

COMMON = \
	${BTSTACK_ROOT}/chipset-cc256x/bt_control_cc256x.c   \
	${BTSTACK_ROOT}/chipset-cc256x/bluetooth_init_cc2564B_1.0_BT_Spec_4.1.c   \
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

BLE = \
	${BTSTACK_ROOT}/ble/att.c 					  \
	${BTSTACK_ROOT}/ble/att_server.c     		  \
	${BTSTACK_ROOT}/ble/central_device_db_memory.c  \
	${BTSTACK_ROOT}/ble/sm.c                      \
	${BTSTACK_ROOT}/ble/att_dispatch.c            \
#	${BTSTACK_ROOT}/ble/gatt_client.c             \

CORE_OBJ   = $(CORE:.c=.o)
COMMON_OBJ   = $(COMMON:.c=.o)
BLE_OBJ   = $(BLE:.c=.o)

OBJS += $(CORE_OBJ) $(COMMON_OBJ) $(BLE_OBJ)

CFLAGS = -I. -I$(BTSTACK_ROOT)/include -I$(BTSTACK_ROOT)/src -I$(BTSTACK_ROOT)/ble -I$(BTSTACK_ROOT)/chipset-cc256x

examples: libopencm3/lib/libopencm3_stm32f1.a spp_and_le_counter.h spp_and_le_counter.elf

include libopencm3.stm32f1.mk

clean:
	rm -f *.map *.o *.d *.out *.elf *.bin *.hex ../driver/*.o ../../src/*.o ../../ble/*.o ../firmware/*.o ${BTSTACK_ROOT}/chipset-cc256x/*.o ${BTSTACK_ROOT}/src/*.o

# git clone and compile libopencm3
libopencm3/lib/libopencm3_stm32f1.a:
	git clone git@github.com:libopencm3/libopencm3.git
	make -C libopencm3

# compile GATT database
spp_and_le_counter.h: spp_and_le_counter.gatt
	python ${BTSTACK_ROOT}/ble/compile-gatt.py $< $@ 

