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
	mesh_iv_index_seq_number.c \
	mesh_node.c \
	le_device_db_fs.c \
	l2cap.c \
	uECC.c \
	rijndael.c \
	


# examples
include ${BTSTACK_ROOT}/example/Makefile.inc
include ${BTSTACK_ROOT}/src/ble/gatt-service/Makefile.inc


CFLAGS  += -g -DUNIT_TEST -Wall \
	-I$(BTSTACK_ROOT)/platform/embedded \
	-I$(BTSTACK_ROOT)/platform/posix \
	-I$(BTSTACK_ROOT)/3rd-party/tinydir \
	-I$(BTSTACK_ROOT)/3rd-party/rijndael \

CFLAGS += -fprofile-arcs -ftest-coverage -fsanitize=address,undefined

CFLAGS += -Wmissing-prototypes -Wstrict-prototypes -Wshadow -Wunused-parameter -Wredundant-decls -Wsign-compare

VPATH += ${BTSTACK_ROOT}/3rd-party/rijndael
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


mesh_pts: mesh_pts.h ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_SERVER_OBJ} ${SM_OBJ} ${MESH_OBJ} main.o mesh_pts.o
	${CC} $(filter-out mesh_pts.h,$^) ${CFLAGS} ${LDFLAGS} -o $@

provisioner: ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${SM_OBJ} main.o  pb_adv.o mesh_crypto.o provisioning_provisioner.o mesh_keys.o mesh_foundation.o mesh_network.o provisioner.o
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

mesh_message_test: mesh_message_test.cpp mesh_foundation.o mesh_node.o  mesh_iv_index_seq_number.o mesh_network.o mesh_peer.o mesh_lower_transport.o mesh_upper_transport.o mesh_virtual_addresses.o  mesh_keys.o  mesh_crypto.o btstack_memory.o btstack_memory_pool.o btstack_util.o btstack_crypto.o btstack_linked_list.o hci_dump.o uECC.o mock.o rijndael.o hci_cmd.o
	g++ $^ ${CFLAGS} ${LDFLAGS} -o $@

sniffer: ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${SM_OBJ} main.o mesh_keys.o mesh_network.o mesh_foundation.o sniffer.c 
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

provisioning_device_test: provisioning_device_test.cpp uECC.o mesh_crypto.o provisioning_device.o btstack_crypto.o btstack_util.o btstack_linked_list.o  mesh_node.o mock.o rijndael.o hci_cmd.o hci_dump.o
	${CC_UNIT} ${CFLAGS} ${LDFLAGS} $^ -lCppUTest -lCppUTestExt -o $@	

provisioning_provisioner_test: provisioning_provisioner_test.cpp uECC.o mesh_crypto.o provisioning_provisioner.o btstack_crypto.o btstack_util.o btstack_linked_list.o mock.o rijndael.o hci_cmd.o hci_dump.o
	${CC_UNIT} ${CFLAGS} ${LDFLAGS} $^ -lCppUTest -lCppUTestExt -o $@	

mesh_configuration_composition_data_message_test: ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${MESH_OBJ} mesh_configuration_composition_data_message_test.cpp 
	${CC_UNIT} ${CFLAGS} ${LDFLAGS} $^ -lCppUTest -lCppUTestExt -o $@

EXAMPLES = mesh_pts provisioner sniffer provisioning_device_test provisioning_provisioner_test mesh_message_test mesh_configuration_composition_data_message_test

all: ${EXAMPLES}

test: mesh_message_test
	./mesh_message_test

clean:
	rm -f  *.o *.out *.exe
	rm -rf *.dSYM
	rm -f ${EXAMPLES}
	rm -f  mesh_provisioning_device.h
	rm -f *.gcno *.gcda
	
