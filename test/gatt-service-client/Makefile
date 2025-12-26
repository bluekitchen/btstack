
include ../common.make

INCLUDES := -Ibuild-coverage
INCLUDES += -I${BTSTACK_ROOT}/src
INCLUDES += -I${BTSTACK_ROOT}/src/ble
INCLUDES += -I${BTSTACK_ROOT}/test/mock
INCLUDES += -I${BTSTACK_ROOT}/3rd-party/rijndael
INCLUDES += -I${BTSTACK_ROOT}/test/include/coverage-ble

CFLAGS += ${INCLUDES}
CXXFLAGS += ${INCLUDES}

VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/ble
VPATH += ${BTSTACK_ROOT}/src/ble/gatt-service
VPATH += ${BTSTACK_ROOT}/platform/posix
VPATH += ${BTSTACK_ROOT}/3rd-party/rijndael
VPATH += ${BTSTACK_ROOT}/test/mock

COMMON = \
	btstack_linked_list.c       \
	btstack_util.c              \
	hci_dump.c                  \
	hci_event_builder.c         \
	mock_gatt_client.c

COMMON_OBJ_COVERAGE = $(addprefix build-coverage/,$(COMMON:.c=.o))
COMMON_OBJ_ASAN     = $(addprefix build-asan/,    $(COMMON:.c=.o))

all: \
	build-coverage/battery_service_client_test build-asan/battery_service_client_test \
	build-coverage/ancs_client_test build-asan/ancs_client_test \
	build-coverage/device_information_service_client_test build-asan/device_information_service_client_test

build-coverage/battery_service_client_test: ${COMMON_OBJ_COVERAGE} build-coverage/battery_service_client.o
build-asan/battery_service_client_test: ${COMMON_OBJ_ASAN} build-asan/battery_service_client.o

build-coverage/ancs_client_test: ${COMMON_OBJ_COVERAGE} build-coverage/ancs_client.o
build-asan/ancs_client_test: ${COMMON_OBJ_ASAN} build-asan/ancs_client.o

build-coverage/device_information_service_client_test: ${COMMON_OBJ_COVERAGE} build-coverage/device_information_service_client.o
build-asan/device_information_service_client_test: ${COMMON_OBJ_ASAN} build-asan/device_information_service_client.o

test: all
	build-asan/battery_service_client_test
	build-asan/ancs_client_test
	build-asan/device_information_service_client_test
		
coverage: all
	rm -f build-coverage/*.gcda
	build-coverage/battery_service_client_test
	build-coverage/ancs_client_test
	build-coverage/device_information_service_client_test

clean:
	rm -rf build-coverage build-asan

