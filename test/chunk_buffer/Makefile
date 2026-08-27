include ../common.make

INCLUDES := -I${BTSTACK_ROOT}/src

CFLAGS += ${INCLUDES}
CXXFLAGS += ${INCLUDES}

VPATH += ${BTSTACK_ROOT}/src

COMMON = btstack_chunk_buffer.c

COMMON_OBJ_COVERAGE = $(addprefix build-coverage/,$(COMMON:.c=.o))
COMMON_OBJ_ASAN     = $(addprefix build-asan/,    $(COMMON:.c=.o))

all: coverage test

build-coverage/chunk_buffer_test: ${COMMON_OBJ_COVERAGE}
build-asan/chunk_buffer_test: ${COMMON_OBJ_ASAN}

test: build-asan/chunk_buffer_test
	build-asan/chunk_buffer_test

coverage: build-coverage/chunk_buffer_test.info

clean: clean-common
