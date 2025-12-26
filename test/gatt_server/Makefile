
include ../common.make

INCLUDES := -I${BTSTACK_ROOT}/3rd-party/rijndael
INCLUDES += -I${BTSTACK_ROOT}/3rd-party/micro-ecc
INCLUDES += -I${BTSTACK_ROOT}/platform/embedded
INCLUDES += -I${BTSTACK_ROOT}/platform/posix
INCLUDES += -I${BTSTACK_ROOT}/src
INCLUDES += -I${BTSTACK_ROOT}/test/mock
INCLUDES += -I${BTSTACK_ROOT}/test/include/coverage-ble

CFLAGS += ${INCLUDES}
CXXFLAGS += ${INCLUDES}

VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/ble
VPATH += ${BTSTACK_ROOT}/src/ble/gatt-service
VPATH += ${BTSTACK_ROOT}/platform/posix
VPATH += ${BTSTACK_ROOT}/3rd-party/micro-ecc
VPATH += ${BTSTACK_ROOT}/3rd-party/rijndael
VPATH += ${BTSTACK_ROOT}/test/mock

COMMON = \
	ad_parser.c                 \
	att_db.c                    \
	att_db_util.c				\
	att_server.c                \
	btstack_hid_parser.c        \
	battery_service_server.c    \
	btstack_crypto.c            \
	btstack_linked_list.c       \
	btstack_memory.c            \
	btstack_memory_pool.c       \
	btstack_tlv.c               \
	btstack_util.c              \
	cycling_power_service_server.c \
	cycling_speed_and_cadence_service_server.c \
	device_information_service_server.c \
	hci_cmd.c                   \
	hci_dump.c                  \
	heart_rate_service_server.c \
	hids_device.c \
	le_device_db_memory.c       \
	mock.c                      \
	mock_btstack_tlv.c          \
	nordic_spp_service_server.c \
	rijndael.c					\
	ublox_spp_service_server.c

COMMON_OBJ_COVERAGE = $(addprefix build-coverage/,$(COMMON:.c=.o)) build-coverage/uECC.o
COMMON_OBJ_ASAN     = $(addprefix build-asan/,    $(COMMON:.c=.o)) build-asan/uECC.o

all: build-coverage/gatt_server_test build-asan/gatt_server_test

build-coverage/gatt_server_test: ${COMMON_OBJ_COVERAGE}

build-asan/gatt_server_test: ${COMMON_OBJ_ASAN}

test: all
	build-asan/gatt_server_test
		
coverage: all
	rm -f build-coverage/*.gcda
	build-coverage/gatt_server_test

clean:
	rm -rf build-coverage build-asan

