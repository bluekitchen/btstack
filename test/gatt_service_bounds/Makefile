.DEFAULT_GOAL := all
include ../common.make

CFLAGS += -I${BTSTACK_ROOT}/src -I${BTSTACK_ROOT}/test/include/coverage-ble -fsanitize=undefined -ffunction-sections -fdata-sections
LDFLAGS += -fsanitize=undefined
VPATH += ${BTSTACK_ROOT}/src ${BTSTACK_ROOT}/src/ble

# Tests include individual servers to exercise their static ATT callbacks.
ifeq ($(shell uname -s),Darwin)
LDFLAGS += -Wl,-dead_strip
else
LDFLAGS += -Wl,--gc-sections
endif

TESTS = $(patsubst %.c,%,$(wildcard *_test.c))
COMMON = btstack_util.o att_db.o hci_dump.o
$(addprefix build-asan/,$(TESTS)): $(addprefix build-asan/,$(COMMON))
$(addprefix build-coverage/,$(TESTS)): $(addprefix build-coverage/,$(COMMON))
$(addprefix build-asan/,$(addsuffix .o,$(TESTS))) $(addprefix build-coverage/,$(addsuffix .o,$(TESTS))): read_bounds.h

# Included implementations must trigger a rebuild after a fix.
define SERVER_DEPENDENCY
build-asan/$(1)_test.o build-coverage/$(1)_test.o: $(BTSTACK_ROOT)/src/ble/gatt-service/$(1).c
endef
$(foreach server,$(filter-out att_server,$(patsubst %_test,%,$(TESTS))),$(eval $(call SERVER_DEPENDENCY,$(server))))

all: test

test: $(addprefix build-asan/,$(TESTS))
	@set -e; for test in $^; do echo "Running $$test"; $$test; done

coverage: $(addprefix build-coverage/,$(addsuffix .info,$(TESTS)))

clean: clean-common

build-asan/att_server_test.o build-coverage/att_server_test.o: ${BTSTACK_ROOT}/src/ble/att_server.c
build-asan/att_server_test: build-asan/eatt_mock.o build-asan/btstack_linked_list.o build-asan/le_device_db_memory.o
build-coverage/att_server_test: build-coverage/eatt_mock.o build-coverage/btstack_linked_list.o build-coverage/le_device_db_memory.o
build-asan/eatt_mock.o: ../gatt_server/mock.c | build-asan
	${CC} -c ${CFLAGS_ASAN} -DENABLE_GATT_OVER_EATT -DENABLE_L2CAP_ENHANCED_CREDIT_BASED_FLOW_CONTROL_MODE $< -o $@
build-coverage/eatt_mock.o: ../gatt_server/mock.c | build-coverage
	${CC} -c ${CFLAGS_COVERAGE} -DENABLE_GATT_OVER_EATT -DENABLE_L2CAP_ENHANCED_CREDIT_BASED_FLOW_CONTROL_MODE $< -o $@
