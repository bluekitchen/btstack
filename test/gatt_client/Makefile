
include ../common.make

DEFINES := -DFUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
INCLUDES := -Ibuild-coverage
INCLUDES += -I${BTSTACK_ROOT}/src
INCLUDES += -I${BTSTACK_ROOT}/test/include/coverage-ble

CFLAGS += ${INCLUDES} ${DEFINES}
CXXFLAGS += ${INCLUDES} ${DEFINES}

VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/ble
VPATH += ${BTSTACK_ROOT}/src/ble/gatt-service
VPATH += ${BTSTACK_ROOT}/platform/posix

COMMON = \
	ad_parser.c                 \
	ancs_client.c               \
	att_db.c                    \
	att_dispatch.c              \
	btstack_linked_list.c       \
	btstack_memory.c            \
	btstack_memory_pool.c       \
	btstack_util.c              \
	gatt_client.c               \
	hci_cmd.c                   \
	hci_event_builder.c         \
	hci_dump.c                  \
	le_device_db_memory.c       \
	mock.c

COMMON_OBJ_COVERAGE = $(addprefix build-coverage/,$(COMMON:.c=.o))
COMMON_OBJ_ASAN     = $(addprefix build-asan/,    $(COMMON:.c=.o))

all: build-coverage/gatt_client_test build-coverage/le_central build-asan/gatt_client_test build-asan/le_central

build-coverage/gatt_client_test.o: build-coverage/profile.h
build-coverage/gatt_client_test: ${COMMON_OBJ_COVERAGE}
build-asan/gatt_client_test: ${COMMON_OBJ_ASAN}

build-coverage/le_central: ${COMMON_OBJ_COVERAGE}
build-asan/le_central: ${COMMON_OBJ_ASAN}

test: all
	build-asan/gatt_client_test
	build-asan/le_central
		
coverage: all
	rm -f build-coverage/*.gcda
	build-coverage/gatt_client_test
	build-coverage/le_central

clean:
	rm -rf build-coverage build-asan

