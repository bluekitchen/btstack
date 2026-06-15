include ../common.make

INCLUDES := -I${BTSTACK_ROOT}/src
INCLUDES += -I${BTSTACK_ROOT}/test/include/coverage-ble

CFLAGS += ${INCLUDES}
CXXFLAGS += ${INCLUDES}

VPATH += ${BTSTACK_ROOT}/src

COMMON = btstack_resample.c

COMMON_OBJ_COVERAGE = $(addprefix build-coverage/,$(COMMON:.c=.o))
COMMON_OBJ_ASAN     = $(addprefix build-asan/,    $(COMMON:.c=.o))

all: coverage test

build-coverage/btstack_resample_test: ${COMMON_OBJ_COVERAGE}
build-asan/btstack_resample_test: ${COMMON_OBJ_ASAN}

test: build-asan/btstack_resample_test
	$<

coverage: build-coverage/btstack_resample_test.info

clean: clean-common
