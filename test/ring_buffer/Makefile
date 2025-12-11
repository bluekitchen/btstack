
include ../common.make

INCLUDES := -I${BTSTACK_ROOT}/src
INCLUDES += -I${BTSTACK_ROOT}/test/include/coverage-ble

CFLAGS += ${INCLUDES}
CXXFLAGS += ${INCLUDES}

VPATH += ${BTSTACK_ROOT}/src

COMMON = btstack_ring_buffer.c

COMMON_OBJ_COVERAGE = $(addprefix build-coverage/,$(COMMON:.c=.o))
COMMON_OBJ_ASAN     = $(addprefix build-asan/,    $(COMMON:.c=.o))

all: build-coverage/btstack_ring_buffer_test build-asan/btstack_ring_buffer_test

build-coverage/btstack_ring_buffer_test: ${COMMON_OBJ_COVERAGE}
build-asan/btstack_ring_buffer_test: ${COMMON_OBJ_ASAN}

test: all
	build-asan/btstack_ring_buffer_test
	
coverage: all
	rm -f build-coverage/*.gcda
	build-coverage/btstack_ring_buffer_test

clean:
	rm -rf build-coverage build-asan
	
