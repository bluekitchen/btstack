
include ../common.make

DEFINES := -DHAVE_HAL_AUDIO
INCLUDES := -I${BTSTACK_ROOT}/3rd-party/micro-ecc
INCLUDES += -I${BTSTACK_ROOT}/3rd-party/rijndael
INCLUDES += -I${BTSTACK_ROOT}/src
INCLUDES += -I${BTSTACK_ROOT}/platform/embedded
INCLUDES += -I${BTSTACK_ROOT}/test/include/coverage-ble

CFLAGS += ${INCLUDES} ${DEFINES}
CXXFLAGS += ${INCLUDES} ${DEFINES}

VPATH += ${BTSTACK_ROOT}/3rd-party/micro-ecc
VPATH += ${BTSTACK_ROOT}/3rd-party/rijndael
VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/ble
VPATH += ${BTSTACK_ROOT}/platform/embedded
VPATH += ${BTSTACK_ROOT}/platform/freertos

COMMON = \
    btstack_linked_list.c  \
	btstack_memory.c 	\
	btstack_run_loop.c 	\
    btstack_util.c		\
    hci_dump.c    		\
    l2cap_signaling.c   \
    hci_cmd.c 			\
    hci_event.c         \
    
FREERTOS = btstack_run_loop_freertos.c

COMMON_OBJ_COVERAGE = $(addprefix build-coverage/,$(COMMON:.c=.o))
COMMON_OBJ_ASAN     = $(addprefix build-asan/,    $(COMMON:.c=.o))

FREERTOS_OBJ_COVERAGE = $(addprefix build-coverage/,$(FREERTOS:.c=.o))
FREERTOS_OBJ_ASAN     = $(addprefix build-asan/,    $(FREERTOS:.c=.o))

all: build-coverage/embedded_test build-asan/embedded_test \
	 build-coverage/run_loop_base_test build-asan/run_loop_base_test \
	 build-coverage/btstack_util_test build-asan/btstack_util_test \
	 build-coverage/l2cap_le_signaling_test build-asan/l2cap_le_signaling_test \
	 build-coverage/hci_cmd_test build-asan/hci_cmd_test \
	 build-coverage/hci_dump_test build-asan/hci_dump_test \
	 build-coverage/hci_event_test build-asan/hci_event_test \
	 build-coverage/freertos_test build-asan/freertos_test

build-coverage/embedded_test: ${COMMON_OBJ_COVERAGE} build-coverage/btstack_run_loop_embedded.o

build-asan/embedded_test: ${COMMON_OBJ_ASAN} build-asan/btstack_run_loop_embedded.o

build-coverage/freertos_test: ${COMMON_OBJ_COVERAGE} ${FREERTOS_OBJS}

build-asan/freertos_test: ${COMMON_OBJ_ASAN} ${FREERTOS_OBJS}

build-coverage/run_loop_base_test: ${COMMON_OBJ_COVERAGE}

build-asan/run_loop_base_test: ${COMMON_OBJ_ASAN}

build-coverage/btstack_util_test: ${COMMON_OBJ_COVERAGE}

build-asan/btstack_util_test: ${COMMON_OBJ_ASAN}

build-coverage/l2cap_le_signaling_test: ${COMMON_OBJ_COVERAGE}

build-asan/l2cap_le_signaling_test: ${COMMON_OBJ_ASAN}

build-coverage/hci_cmd_test: ${COMMON_OBJ_COVERAGE}

build-asan/hci_cmd_test: ${COMMON_OBJ_ASAN}

build-coverage/hci_dump_test: ${COMMON_OBJ_COVERAGE} build-coverage/hci_dump.o

build-asan/hci_dump_test: ${COMMON_OBJ_ASAN} build-asan/hci_dump.o

build-coverage/hci_event_test: ${COMMON_OBJ_COVERAGE}

build-asan/hci_event_test: ${COMMON_OBJ_ASAN}

test: all
	build-asan/embedded_test
	build-asan/freertos_test
	build-asan/run_loop_base_test
	build-asan/btstack_util_test
	build-asan/l2cap_le_signaling_test
	build-asan/hci_cmd_test
	build-asan/hci_dump_test
	build-asan/hci_event_test

coverage: all
	rm -f build-coverage/*.gcda
	build-coverage/embedded_test
	build-coverage/freertos_test
	build-coverage/run_loop_base_test
	build-coverage/btstack_util_test
	build-coverage/l2cap_le_signaling_test
	build-coverage/hci_cmd_test
	build-coverage/hci_dump_test
	build-coverage/hci_event_test

clean:
	rm -rf build-coverage build-asan *.dSYM
