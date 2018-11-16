# Makefile for libusb based examples
BTSTACK_ROOT = ../..

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
VPATH += ${BTSTACK_ROOT}/src/ble/mesh
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

mesh: ${CORE_OBJ} ${COMMON_OBJ} ${SM_OBJ} pb_adv.o mesh_crypto.o provisioning_device.o mesh_network.o mesh.o
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

provisioner: ${CORE_OBJ} ${COMMON_OBJ} ${SM_OBJ} pb_adv.o mesh_crypto.o provisioning_provisioner.o provisioner.o
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

sniffer: ${CORE_OBJ} ${COMMON_OBJ} ${SM_OBJ} sniffer.c 
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

provisioning_device_test: provisioning_device_test.cpp uECC.o mesh_crypto.o provisioning_device.o btstack_crypto.o btstack_util.o btstack_linked_list.o mock.o rijndael.o hci_cmd.o hci_dump.o
	${CC_UNIT} ${CFLAGS} ${LDFLAGS} $^ -lCppUTest -lCppUTestExt -o $@	

provisioning_provisioner_test: provisioning_provisioner_test.cpp uECC.o mesh_crypto.o provisioning_provisioner.o btstack_crypto.o btstack_util.o btstack_linked_list.o mock.o rijndael.o hci_cmd.o hci_dump.o
	${CC_UNIT} ${CFLAGS} ${LDFLAGS} $^ -lCppUTest -lCppUTestExt -o $@	

mesh_provisioning_device.h: mesh_provisioning_device.gatt
	python ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@ 
	
mesh_provisioning_device: mesh_provisioning_device.h ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${GATT_SERVER_OBJ} ${SM_OBJ} mesh_crypto.o provisioning_device.o mesh_provisioning_service_server.o mesh_provisioning_device.o
	${CC} $(filter-out mesh_provisioning_device.h,$^) ${CFLAGS} ${LDFLAGS} -o $@

EXAMPLES = mesh provisioner sniffer provisioning_device_test provisioning_provisioner_test mesh_provisioning_device

all: ${EXAMPLES}

clean:
	rm -f  *.o *.out *.exe
	rm -rf *.dSYM
	rm -f ${EXAMPLES}
	rm -f  mesh_provisioning_device.h
	
