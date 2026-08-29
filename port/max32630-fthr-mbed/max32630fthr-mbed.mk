BTSTACK_ROOT = ./

VPATH += .
VPATH += $(abspath ../../../../btstack)

BTSTACK_CORE_OBJ = \
	$(BTSTACK_ROOT)/src/ad_parser.o \
	$(BTSTACK_ROOT)/src/btstack_linked_list.o \
	$(BTSTACK_ROOT)/src/btstack_memory.o \
	$(BTSTACK_ROOT)/src/btstack_memory_pool.o \
	$(BTSTACK_ROOT)/src/btstack_run_loop.o \
	$(BTSTACK_ROOT)/src/btstack_util.o \
	$(BTSTACK_ROOT)/src/l2cap.o \
	$(BTSTACK_ROOT)/src/hci_transport_h4.o \
	$(BTSTACK_ROOT)/src/l2cap_signaling.o \
	$(BTSTACK_ROOT)/platform/embedded/btstack_run_loop_embedded.o \
	src/initscripts-TIInit_6.7.16_bt_spec_4.1.o \
	src/btstack_port.o \
	src/hal_tick.o \
	src/spp_counter.o

BTSTACK_COMMON_OBJ = \
	$(BTSTACK_ROOT)/chipset/cc256x/btstack_chipset_cc256x.o \
    $(BTSTACK_ROOT)/src/hci.o \
    $(BTSTACK_ROOT)/src/hci_cmd.o \
    $(BTSTACK_ROOT)/src/hci_dump.o \
    $(BTSTACK_ROOT)/platform/embedded/btstack_uart_block_embedded.o

BTSTACK_CLASSIC_OBJ = \
	$(BTSTACK_ROOT)/src/classic/rfcomm.o \
	$(BTSTACK_ROOT)/src/classic/sdp_util.o \
	$(BTSTACK_ROOT)/src/classic/spp_server.o \
	$(BTSTACK_ROOT)/src/classic/sdp_server.o \
	$(BTSTACK_ROOT)/src/classic/sdp_client.o  \
	$(BTSTACK_ROOT)/src/classic/sdp_client_rfcomm.o

BTSTACK_BLE_OBJ = \
	$(BTSTACK_ROOT)/src/ble/att_db.o \
	$(BTSTACK_ROOT)/src/ble/att_server.o \
	$(BTSTACK_ROOT)/src/ble/le_device_db_memory.o \
	$(BTSTACK_ROOT)/src/ble/att_dispatch.o \
	$(BTSTACK_ROOT)/src/ble/sm.o


BTSTACK_OBJ = $(BTSTACK_CORE_OBJ)
BTSTACK_OBJ += $(BTSTACK_COMMON_OBJ)
BTSTACK_OBJ += $(BTSTACK_CLASSIC_OBJ)
BTSTACK_OBJ += $(BTSTACK_BLE_OBJ)

OBJECTS += $(BTSTACK_OBJ)

BTSTACK_INC_ROOT = ../../..

INCLUDE_PATHS += $(BTSTACK_INC)
INCLUDE_PATHS += \
	-I$(BTSTACK_INC_ROOT)/src \
	-I$(BTSTACK_INC_ROOT)/src/ble \
	-I$(BTSTACK_INC_ROOT)/src/classic \
	-I$(BTSTACK_INC_ROOT)/chipset/cc256x \
	-I$(BTSTACK_INC_ROOT)/platform/embedded \
    -I${BTSTACK_INC_ROOT}/src/ble/gatt-service/ \
    -I${BTSTACK_INC_ROOT}/port/max32630-fthr-mbed/ \
    -I${BTSTACK_INC_ROOT}/port/max32630-fthr-mbed/src

C_FLAGS += -g3 -ggdb -DDEBUG
C_FLAGS += -g3 -ggdb -D__STACK_SIZE=0X30000
CXX_FLAGS += -g3 -ggdb -DDEBUG

