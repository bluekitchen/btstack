
include ../common.make

DEFINES := -DUNIT_TEST
INCLUDES := -I${BTSTACK_ROOT}/src
INCLUDES += -I${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/include
INCLUDES += -I${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/include
INCLUDES += -I..

CFLAGS += ${INCLUDES} ${DEFINES}
CXXFLAGS += ${INCLUDES} ${DEFINES}

VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/classic
VPATH += ${BTSTACK_ROOT}/platform/posix

COMMON = \
	btstack_util.c		  \
	btstack_linked_list.c \
	hci_dump.c 			  \
    avdtp_util.c		  \

COMMON_OBJ_COVERAGE = $(addprefix build-coverage/,$(COMMON:.c=.o))
COMMON_OBJ_ASAN     = $(addprefix build-asan/,    $(COMMON:.c=.o))

all: build-coverage/avdtp_util_test build-asan/avdtp_util_test

build-coverage/avdtp_util_test: ${COMMON_OBJ_COVERAGE}

build-asan/avdtp_util_test: ${COMMON_OBJ_ASAN}

test: all
	build-asan/avdtp_util_test

coverage: all
	rm -f build-coverage/*.gcda
	build-coverage/avdtp_util_test

clean:
	rm -rf build-coverage build-asan
	
