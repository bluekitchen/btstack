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
	btstack_memory.c	\
	btstack_run_loop.c	\
    btstack_util.c		\
    hci_dump.c			\
    l2cap_signaling.c   \
    hci_cmd.c			\
    hci_event.c         \

COMMON_OBJ_COVERAGE = $(addprefix build-coverage/,$(COMMON:.c=.o))
COMMON_OBJ_ASAN     = $(addprefix build-asan/,    $(COMMON:.c=.o))

all: coverage test

build-coverage/btstack_event_test: ${COMMON_OBJ_COVERAGE}
build-asan/btstack_event_test: ${COMMON_OBJ_ASAN}

test: build-asan/btstack_event_test
	$<

coverage: build-coverage/btstack_event_test.info

clean: clean-common
