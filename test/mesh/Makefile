 # Makefile for libusb based examples
BTSTACK_ROOT = ../..

# examples
include ${BTSTACK_ROOT}/example/Makefile.inc
include ${BTSTACK_ROOT}/src/ble/gatt-service/Makefile.inc

CXXFLAGS += ${CFLAGS}

include ../common.make

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
	btstack_uart_posix_pty.c \
	hci_transport_h2_libusb.c \
	hci_transport_h4.c \
	btstack_tlv_posix.c \
	mesh_iv_index_seq_number.c \
	mesh_node.c \
	le_device_db_fs.c \
	l2cap.c \
	uECC.c \
	rijndael.c \
	hci_dump_posix_fs.c

DEFINES := -DUNIT_TEST
INCLUDES := -I$(BTSTACK_ROOT)/platform/embedded
INCLUDES += -I$(BTSTACK_ROOT)/platform/posix
INCLUDES += -I$(BTSTACK_ROOT)/3rd-party/tinydir
INCLUDES += -I$(BTSTACK_ROOT)/3rd-party/rijndael

CFLAGS += ${INCLUDES} ${DEFINES}
CXXFLAGS += ${INCLUDES} ${DEFINES}

#CFLAGS += -Wmissing-prototypes -Wstrict-prototypes -Wshadow -Wunused-parameter -Wredundant-decls -Wsign-compare

VPATH += ${BTSTACK_ROOT}/3rd-party/rijndael
VPATH += ${BTSTACK_ROOT}/src/mesh
VPATH += ${BTSTACK_ROOT}/src/classic
VPATH += ${BTSTACK_ROOT}/platform/posix
VPATH += ${BTSTACK_ROOT}/platform/embedded
VPATH += ${BTSTACK_ROOT}/platform/libusb
VPATH += ${BTSTACK_ROOT}/src/ble/gatt-service

# libusb
CFLAGS  += $(shell pkg-config libusb-1.0 --cflags)
LDFLAGS += $(shell pkg-config libusb-1.0 --libs)

COMMON_OBJ_COVERAGE      = $(addprefix build-coverage/,$(COMMON_OBJ))
CORE_OBJ_COVERAGE        = $(addprefix build-coverage/,$(CORE_OBJ))
ATT_OBJ_COVERAGE         = $(addprefix build-coverage/,$(ATT_OBJ))
GATT_SERVER_OBJ_COVERAGE = $(addprefix build-coverage/,$(GATT_SERVER_OBJ))
SM_OB_COVERAGE           = $(addprefix build-coverage/,$(SM_OB))
MESH_OBJ_COVERAGE        = $(addprefix build-coverage/,$(MESH_OBJ))

COMMON_OBJ_ASAN          = $(addprefix build-asan/,$(COMMON_OBJ))
CORE_OBJ_ASAN            = $(addprefix build-asan/,$(CORE_OBJ))
ATT_OBJ_ASAN             = $(addprefix build-asan/,$(ATT_OBJ))
GATT_SERVER_OBJ_ASAN     = $(addprefix build-asan/,$(GATT_SERVER_OBJ))
SM_OB_ASAN               = $(addprefix build-asan/,$(SM_OB))
MESH_OBJ_ASAN            = $(addprefix build-asan/,$(MESH_OBJ))

TESTS_SRCS = mesh_message_test provisioning_device_test provisioning_provisioner_test mesh_configuration_composition_data_message_test
EXAMPLES =   mesh_pts provisioner sniffer

all:   $(addprefix build-asan/,$(EXAMPLES))
tests: $(addprefix build-asan/,$(TESTS_SRCS))

build-asan/mesh_pts: ${CORE_OBJ_ASAN} ${COMMON_OBJ_ASAN} ${ATT_OBJ_ASAN} ${GATT_SERVER_OBJ_ASAN} ${SM_OBJ_ASAN} ${MESH_OBJ_ASAN} build-asan/main.o

build-asan/provisioner: ${CORE_OBJ_ASAN} ${COMMON_OBJ_ASAN} ${ATT_OBJ_ASAN} ${SM_OBJ_ASAN} build-asan/main.o build-asan/pb_adv.o build-asan/mesh_crypto.o build-asan/provisioning_provisioner.o build-asan/mesh_keys.o build-asan/mesh_foundation.o build-asan/mesh_network.o

build-asan/sniffer: ${CORE_OBJ_ASAN} ${COMMON_OBJ_ASAN} ${ATT_OBJ_ASAN} ${SM_OBJ_ASAN} build-asan/main.o build-asan/mesh_keys.o build-asan/mesh_network.o build-asan/mesh_foundation.o

build-asan/mesh_message_test: $(addprefix build-asan/, mesh_foundation.o mesh_node.o  mesh_iv_index_seq_number.o mesh_network.o mesh_peer.o mesh_lower_transport.o mesh_upper_transport.o mesh_virtual_addresses.o  mesh_keys.o  mesh_crypto.o btstack_memory.o btstack_memory_pool.o btstack_util.o btstack_crypto.o btstack_linked_list.o hci_dump.o uECC.o mock.o rijndael.o hci_cmd.o hci_dump_posix_fs.o)

build-asan/provisioning_device_test:  $(addprefix build-asan/, uECC.o mesh_crypto.o provisioning_device.o btstack_crypto.o btstack_util.o btstack_linked_list.o  mesh_node.o mock.o rijndael.o hci_cmd.o hci_dump.o hci_dump_posix_fs.o)

build-asan/provisioning_provisioner_test:  $(addprefix build-asan/, uECC.o mesh_crypto.o provisioning_provisioner.o btstack_crypto.o btstack_util.o btstack_linked_list.o mock.o rijndael.o hci_cmd.o hci_dump.o hci_dump_posix_fs.o)

build-asan/mesh_configuration_composition_data_message_test: ${CORE_OBJ_ASAN} ${COMMON_OBJ_ASAN} ${ATT_OBJ_ASAN} ${MESH_OBJ_ASAN}

test: tests
	# Ignore leaks in mesh message test as tests stop before all PDUs are fully processed
	ASAN_OPTIONS=detect_leaks=0 build-asan/mesh_message_test
	build-asan/provisioning_device_test
	build-asan/provisioning_provisioner_test
	build-asan/mesh_configuration_composition_data_message_test

coverage: tests
	rm -f build-coverage/*.gcda
	@echo "no coverage here"

clean:
	rm -rf build-coverage build-asan
