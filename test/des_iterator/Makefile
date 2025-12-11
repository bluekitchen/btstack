# Requirements: cpputest.github.io

include ../common.make

DEFINES := -DUNIT_TEST

INCLUDES := -I${BTSTACK_ROOT}/src
INCLUDES += -I..

CFLAGS += ${DEFINES} ${INCLUDES}
CXXFLAGS += ${DEFINES} ${INCLUDES}

VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/classic
VPATH += ${BTSTACK_ROOT}/platform/posix

COMMON = \
    sdp_util.c	              \
    hci_dump.c    \
	btstack_util.c

COMMON_OBJ_COVERAGE = $(addprefix build-coverage/,$(COMMON:.c=.o))
COMMON_OBJ_ASAN     = $(addprefix build-asan/,    $(COMMON:.c=.o))

all: build-coverage/des_iterator_test build-asan/des_iterator_test

build-coverage/des_iterator_test: ${COMMON_OBJ_COVERAGE}

build-asan/des_iterator_test: ${COMMON_OBJ_ASAN}

test: all
	build-asan/des_iterator_test

coverage: all
	rm -f build-coverage/*.gcda
	build-coverage/des_iterator_test

clean:
	rm -rf build-coverage build-asan

