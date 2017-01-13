# ASMS = $(wildcard *.s)
SRCS = \
	../../src/ble/att_db.c \
	../../src/ble/att_dispatch.c \
	../../src/ble/att_server.c \
	../../src/ble/sm.c \
	../../src/ble/le_device_db_memory.c \
	../../src/ble/gatt-service/battery_service_server.c \
	../../src/btstack_linked_list.c \
	../../src/btstack_memory.c \
	../../src/btstack_memory_pool.c \
	../../src/btstack_run_loop.c \
	../../src/btstack_util.c \
	../../src/hci.c \
	../../src/hci_cmd.c \
	../../src/hci_dump.c \
	../../src/hci_dump.c \
	../../src/l2cap.c \
	../../src/l2cap_signaling.c \
	hci-ll.c \
	port.c \
	retarget_printf.c \

INCLUDES = \
	-I. \
	-I../../src \
	-I../../platform/embedded \
	-I../../../ble/ll \
	-I../../../util \
	-I../../../ticker \
	-I../../../hal \
	-I../../../external/arm/cmsis/include \
	-I../../../external/nordic/include \
	-I../../../arch/arm/cortex_m \
	-I../../../soc/nrf5/

include ../../../makefile.inc
