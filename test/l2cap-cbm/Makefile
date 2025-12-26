
include ../common.make

DEFINES  := -D FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION -D ENABLE_MALLOC_TEST
INCLUDES := -Ibuild-coverage
INCLUDES += -I${BTSTACK_ROOT}/src
INCLUDES += -I${BTSTACK_ROOT}/src/ble
INCLUDES += -I${BTSTACK_ROOT}/platform/posix
INCLUDES += -I${BTSTACK_ROOT}/platform/embedded
INCLUDES += -I${BTSTACK_ROOT}/test/include/coverage-ble

CFLAGS += ${INCLUDES} ${DEFINES}
CXXFLAGS += ${INCLUDES} ${DEFINES}

VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/ble 
VPATH += ${BTSTACK_ROOT}/platform/embedded
VPATH += ${BTSTACK_ROOT}/platform/posix

COMMON = \
	btstack_linked_list.c \
	btstack_util.c \
	hci.c \
	hci_cmd.c \
	ad_parser.c \
	l2cap.c \
	l2cap_signaling.c \
	btstack_memory.c \
	btstack_run_loop.c \
	btstack_run_loop_embedded.c \
	hci_dump.c \
	hci_dump_posix_stdout.c

COMMON_OBJ_COVERAGE = $(addprefix build-coverage/,$(COMMON:.c=.o))
COMMON_OBJ_ASAN     = $(addprefix build-asan/,    $(COMMON:.c=.o))

all: build-coverage/l2cap_cbm_test build-asan/l2cap_cbm_test 

build-coverage/l2cap_cbm_test: ${COMMON_OBJ_COVERAGE}

build-asan/l2cap_cbm_test: ${COMMON_OBJ_ASAN}

test: all
	build-asan/l2cap_cbm_test

coverage: all
	rm -f build-coverage/*.gcda
	build-coverage/l2cap_cbm_test

clean:
	rm -rf build-coverage build-asan

