
include ../common.make

INCLUDES := -I ${BTSTACK_ROOT}/src
INCLUDES += -I ${BTSTACK_ROOT}/platform/posix
INCLUDES += -I ${BTSTACK_ROOT}/test/include/coverage-ble

CFLAGS += ${INCLUDES}
CXXFLAGS += ${INCLUDES}

VPATH += ${BTSTACK_ROOT}/src ${BTSTACK_ROOT}/platform/posix

COMMON = \
	btstack_util.c \
	btstack_hid_parser.c \
	hci_dump.c \
	hci_dump_posix_fs.c

COMMON_OBJ_COVERAGE = $(addprefix build-coverage/,$(COMMON:.c=.o))
COMMON_OBJ_ASAN     = $(addprefix build-asan/,    $(COMMON:.c=.o))

all: coverage test

build-coverage/hid_parser_test: ${COMMON_OBJ_COVERAGE}
build-asan/hid_parser_test: ${COMMON_OBJ_ASAN}

test: build-asan/hid_parser_test
	build-asan/hid_parser_test
	
coverage: build-coverage/hid_parser_test.info

clean: clean-common

