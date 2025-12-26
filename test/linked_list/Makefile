
include ../common.make

INCLUDES := -Ibuild-coverage
INCLUDES += -I${BTSTACK_ROOT}/src
INCLUDES += -I${BTSTACK_ROOT}/include
INCLUDES += -I${BTSTACK_ROOT}/test/include/coverage-ble

CFLAGS += ${INCLUDES}
CXXFLAGS += ${INCLUDES}

VPATH += ${BTSTACK_ROOT}/src/ble
VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/platform/posix

COMMON = \
    btstack_linked_list.c \
    hci_dump.c \
    btstack_util.c

COMMON_OBJ_COVERAGE = $(addprefix build-coverage/,$(COMMON:.c=.o))
COMMON_OBJ_ASAN     = $(addprefix build-asan/,    $(COMMON:.c=.o))

all: build-coverage/btstack_linked_list_test build-asan/btstack_linked_list_test

build-coverage/btstack_linked_list_test: ${COMMON_OBJ_COVERAGE}

build-asan/btstack_linked_list_test: ${COMMON_OBJ_ASAN}

test: all
	build-asan/btstack_linked_list_test
	
coverage: all
	rm -f build-coverage/*.gcda
	build-coverage/btstack_linked_list_test

clean:
	rm -rf build-coverage build-asan
