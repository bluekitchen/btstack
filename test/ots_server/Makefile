.DEFAULT_GOAL := all

include ../common.make

CFLAGS += -I${BTSTACK_ROOT}/src -I${BTSTACK_ROOT}/test/include/coverage-ble -fsanitize=undefined -ffunction-sections -fdata-sections
LDFLAGS += -fsanitize=undefined
VPATH += ${BTSTACK_ROOT}/src

# The test includes the server to access its static ATT callback. Discard unused
# entry points so the fixture only needs mocks for the write path.
ifeq ($(shell uname -s),Darwin)
LDFLAGS += -Wl,-dead_strip
else
LDFLAGS += -Wl,--gc-sections
endif

COMMON = btstack_util.o btstack_linked_list.o
build-asan/ots_server_test: $(addprefix build-asan/,${COMMON})
build-coverage/ots_server_test: $(addprefix build-coverage/,${COMMON})
build-asan/ots_server_test.o build-coverage/ots_server_test.o: ${BTSTACK_ROOT}/src/ble/gatt-service/object_transfer_service_server.c

all: test

test: build-asan/ots_server_test
	build-asan/ots_server_test

coverage: build-coverage/ots_server_test.info

clean: clean-common
