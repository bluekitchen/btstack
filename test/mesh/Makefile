 # Makefile for libusb based examples
BTSTACK_ROOT = ../..

ATT  += \
	att_dispatch.c       	    \
	att_db.c 				 	\
	att_server.c        	    \
	gatt_bearer.c               \
	mesh_proxy_service_server.c \

CORE += \
	adv_bearer.c \
	beacon.c \
	btstack_link_key_db_fs.c \
	btstack_run_loop_posix.c \
	btstack_stdin_posix.c \
	btstack_uart_block_posix_pty.c \
	hci_transport_h2_libusb.c \
	hci_transport_h4.c \
	btstack_tlv_posix.c \
	le_device_db_fs.c \
	l2cap.c \
	uECC.c \
	rijndael.c \
	main.c \

# examples
include ${BTSTACK_ROOT}/example/Makefile.inc
include ${BTSTACK_ROOT}/src/ble/gatt-service/Makefile.inc

CFLAGS  += -g -DUNIT_TEST -Wall \
	-I$(BTSTACK_ROOT)/platform/embedded \
	-I$(BTSTACK_ROOT)/platform/posix \
	-I$(BTSTACK_ROOT)/3rd-party/tinydir \
	-I$(BTSTACK_ROOT)/test/rijndael \

VPATH += ${BTSTACK_ROOT}/test/rijndael
VPATH += ${BTSTACK_ROOT}/src/mesh
VPATH += ${BTSTACK_ROOT}/src/classic
VPATH += ${BTSTACK_ROOT}/platform/posix
VPATH += ${BTSTACK_ROOT}/platform/embedded
VPATH += ${BTSTACK_ROOT}/platform/libusb
VPATH += ${BTSTACK_ROOT}/src/ble/gatt-service

# cpputest
CC_UNIT = g++

# libusb
CFLAGS  += $(shell pkg-config libusb-1.0 --cflags)
LDFLAGS += $(shell pkg-config libusb-1.0 --libs)

# cppUTest
LDFLAGS += -lCppUTest -lCppUTestExt


mesh: mesh.h ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_SERVER_OBJ} ${SM_OBJ} pb_adv.o pb_gatt.o mesh_foundation.o mesh_provisioning_service_server.o mesh_crypto.o provisioning_device.o mesh_network.o mesh_peer.o mesh_lower_transport.o mesh_upper_transport.o mesh_virtual_addresses.o mesh_keys.o mesh_proxy.o mesh_access.o mesh_configuration_server.o mesh_generic_server.o mesh_generic_level_server.o mesh_generic_client.o mesh.o
	${CC} $(filter-out mesh.h,$^) ${CFLAGS} ${LDFLAGS} -o $@

provisioner: ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${SM_OBJ} pb_adv.o mesh_crypto.o provisioning_provisioner.o mesh_keys.o provisioner.o
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

mesh_message_test: mesh_message_test.c mesh_network.o mesh_peer.o mesh_lower_transport.o mesh_upper_transport.o mesh_virtual_addresses.o  mesh_keys.o  mesh_crypto.o btstack_memory.o btstack_memory_pool.o btstack_util.o btstack_crypto.o btstack_linked_list.o hci_dump.o uECC.o mock.o rijndael.o hci_cmd.o
	g++ $^ ${CFLAGS} ${LDFLAGS} -o $@

sniffer: ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${SM_OBJ} mesh_keys.o sniffer.c 
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

provisioning_device_test: provisioning_device_test.cpp uECC.o mesh_crypto.o provisioning_device.o btstack_crypto.o btstack_util.o btstack_linked_list.o mock.o rijndael.o hci_cmd.o hci_dump.o
	${CC_UNIT} ${CFLAGS} ${LDFLAGS} $^ -lCppUTest -lCppUTestExt -o $@	

provisioning_provisioner_test: provisioning_provisioner_test.cpp uECC.o mesh_crypto.o provisioning_provisioner.o btstack_crypto.o btstack_util.o btstack_linked_list.o mock.o rijndael.o hci_cmd.o hci_dump.o
	${CC_UNIT} ${CFLAGS} ${LDFLAGS} $^ -lCppUTest -lCppUTestExt -o $@	


EXAMPLES = mesh provisioner sniffer provisioning_device_test provisioning_provisioner_test mesh_message_test

all: ${EXAMPLES}

clean:
	rm -f  *.o *.out *.exe
	rm -rf *.dSYM
	rm -f ${EXAMPLES}
	rm -f  mesh_provisioning_device.h
	
