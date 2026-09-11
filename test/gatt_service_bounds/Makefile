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
COMMON = btstack_util.o att_db.o
$(addprefix build-asan/,$(TESTS)): $(addprefix build-asan/,$(COMMON))
$(addprefix build-coverage/,$(TESTS)): $(addprefix build-coverage/,$(COMMON))
$(addprefix build-asan/,$(addsuffix .o,$(TESTS))) $(addprefix build-coverage/,$(addsuffix .o,$(TESTS))): read_bounds.h

# Included implementations must trigger a rebuild after a fix.
define SERVER_DEPENDENCY
build-asan/$(1)_test.o build-coverage/$(1)_test.o: $(BTSTACK_ROOT)/src/ble/gatt-service/$(1).c
endef
$(foreach server,$(patsubst %_test,%,$(TESTS)),$(eval $(call SERVER_DEPENDENCY,$(server))))

all: test

test: $(addprefix build-asan/,$(TESTS))
	@set -e; for test in $^; do echo "Running $$test"; $$test; done

coverage: $(addprefix build-coverage/,$(addsuffix .info,$(TESTS)))

clean: clean-common
