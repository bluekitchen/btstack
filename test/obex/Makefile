
include ../common.make

DEFINES := -DUNIT_TEST
INCLUDES := -I${BTSTACK_ROOT}/src
INCLUDES += -I../

CFLAGS += ${INCLUDES} ${DEFINES}
CXXFLAGS += ${INCLUDES} ${DEFINES}

VPATH += ${BTSTACK_ROOT}/src/ble
VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/classic
VPATH += ${BTSTACK_ROOT}/platform/posix

COMMON = \
    btstack_util.c \
    obex_message_builder.c \
    obex_parser.c

COMMON_OBJ_COVERAGE = $(addprefix build-coverage/,$(COMMON:.c=.o))
COMMON_OBJ_ASAN     = $(addprefix build-asan/,    $(COMMON:.c=.o))

all: build-coverage/obex_message_builder_test build-asan/obex_message_builder_test \
	 build-coverage/obex_parser_test build-asan/obex_parser_test

build-coverage/obex_message_builder_test: ${COMMON_OBJ_COVERAGE}
build-asan/obex_message_builder_test: ${COMMON_OBJ_ASAN}

build-coverage/obex_parser_test: ${COMMON_OBJ_COVERAGE}
build-asan/obex_parser_test: ${COMMON_OBJ_ASAN}

test: all
	build-asan/obex_message_builder_test
	build-asan/obex_parser_test

coverage: all
	rm -f build-coverage/*.gcda
	build-coverage/obex_message_builder_test
	build-coverage/obex_parser_test

clean:
	rm -rf build-coverage build-asan
	
