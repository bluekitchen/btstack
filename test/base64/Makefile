
include ../common.make

DEFINES := -DUNIT_TEST
INCLUDES := -I${BTSTACK_ROOT}/src
INCLUDES += -I..

CFLAGS += ${DEFINES} ${INCLUDES}
CXXFLAGS += ${DEFINES} ${INCLUDES}

VPATH += ${BTSTACK_ROOT}/src

COMMON = \
    btstack_base64_decoder.c \
    btstack_util.c \
    hci_dump.c \

COMMON_OBJ_COVERAGE = $(addprefix build-coverage/,$(COMMON:.c=.o))
COMMON_OBJ_ASAN     = $(addprefix build-asan/,    $(COMMON:.c=.o))

all: build-coverage/base64_decoder_test build-asan/base64_decoder_test

build-coverage/base64_decoder_test: ${COMMON_OBJ_COVERAGE}

build-asan/base64_decoder_test: ${COMMON_OBJ_ASAN}

test: all
	build-asan/base64_decoder_test
	
coverage: all
	rm -f build-coverage/*.gcda
	build-coverage/base64_decoder_test

clean:
	rm -rf build-coverage build-asan
