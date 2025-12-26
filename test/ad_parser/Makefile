
include ../common.make

INCLUDES = -I${BTSTACK_ROOT}/src
INCLUDES += -I${BTSTACK_ROOT}/test/include/coverage-ble

CFLAGS   += ${INCLUDES}
CXXFLAGS += ${INCLUDES}

VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/classic
VPATH += ${BTSTACK_ROOT}/platform/posix

COMMON = \
	btstack_util.c				\
	hci_dump.c				\
	ad_parser.c				\

COMMON_OBJ_COVERAGE = $(addprefix build-coverage/,$(COMMON:.c=.o))
COMMON_OBJ_ASAN     = $(addprefix build-asan/,    $(COMMON:.c=.o))

all: build-coverage/ad_parser_test build-asan/ad_parser_test

build-coverage/ad_parser_test: $(COMMON_OBJ_COVERAGE)
build-asan/ad_parser_test: $(COMMON_OBJ_ASAN)

test: all
	build-asan/ad_parser_test

coverage: all
	rm -f build-coverage/*.gcda
	build-coverage/ad_parser_test

clean:
	rm -rf build-coverage
	rm -rf build-asan
