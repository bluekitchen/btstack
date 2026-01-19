include ../common.make

DEFINES := -DUNIT_TEST_ASSERT
INCLUDES = -I${BTSTACK_ROOT}/src
INCLUDES += -I${BTSTACK_ROOT}/test/include/coverage-ble

CFLAGS   += ${INCLUDES} ${DEFINES}
CXXFLAGS += ${INCLUDES} ${DEFINES}

VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/classic
VPATH += ${BTSTACK_ROOT}/platform/posix

COMMON = \
	btstack_util.c		    \
	hci_dump.c

COMMON_OBJ_COVERAGE = $(addprefix build-coverage/,$(COMMON:.c=.o))
COMMON_OBJ_ASAN     = $(addprefix build-asan/,    $(COMMON:.c=.o))

all: coverage test

build-coverage/btstack_util_test: $(COMMON_OBJ_COVERAGE)
build-asan/btstack_util_test: $(COMMON_OBJ_ASAN)

test: build-asan/btstack_util_test
	$<

coverage: build-coverage/btstack_util_test.info

clean: clean-common
