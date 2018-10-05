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
	pb_adv.c \
	uECC.c \
	rijndael.c \
	main.c \

# examples
include ${BTSTACK_ROOT}/example/Makefile.inc

CFLAGS  += -g -DUNIT_TEST \
	-I$(BTSTACK_ROOT)/platform/embedded \
	-I$(BTSTACK_ROOT)/platform/posix \
	-I$(BTSTACK_ROOT)/3rd-party/tinydir \
	-I$(BTSTACK_ROOT)/3rd-party/rijndael \

VPATH += ${BTSTACK_ROOT}/3rd-party/rijndael
VPATH += ${BTSTACK_ROOT}/src/ble/mesh
VPATH += ${BTSTACK_ROOT}/src/classic
VPATH += ${BTSTACK_ROOT}/platform/posix
VPATH += ${BTSTACK_ROOT}/platform/embedded
VPATH += ${BTSTACK_ROOT}/platform/libusb

# cpputest
CC_UNIT = g++

# libusb
CFLAGS  += $(shell pkg-config libusb-1.0 --cflags)
LDFLAGS += $(shell pkg-config libusb-1.0 --libs)

mesh: ${CORE_OBJ} ${COMMON_OBJ} ${SM_OBJ} provisioning.o provisioning_device.o mesh.o
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

provisioner: ${CORE_OBJ} ${COMMON_OBJ} ${SM_OBJ} provisioning.o provisioning_provisioner.o provisioner.o
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

sniffer: ${CORE_OBJ} ${COMMON_OBJ} ${SM_OBJ} sniffer.c 
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

provisioning_device_test: provisioning_device_test.cpp uECC.o provisioning.o provisioning_device.o btstack_crypto.o btstack_util.o btstack_linked_list.o mock.o rijndael.o hci_cmd.o hci_dump.o
	${CC_UNIT} ${CFLAGS} ${LDFLAGS} $^ -lCppUTest -lCppUTestExt -o $@	

provisioning_provisioner_test: provisioning_provisioner_test.cpp uECC.o provisioning.o provisioning_provisioner.o btstack_crypto.o btstack_util.o btstack_linked_list.o mock.o rijndael.o hci_cmd.o hci_dump.o
	${CC_UNIT} ${CFLAGS} ${LDFLAGS} $^ -lCppUTest -lCppUTestExt -o $@	

EXAMPLES = mesh provisioner sniffer provisioning_device_test provisioning_provisioner_test

all: ${EXAMPLES}

