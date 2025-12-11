
include ../common.make

INCLUDES = -I${BTSTACK_ROOT}/src
INCLUDES += -I${BTSTACK_ROOT}/3rd-party/micro-ecc
INCLUDES += -I${BTSTACK_ROOT}/3rd-party/rijndael
INCLUDES += -I${BTSTACK_ROOT}/test/include/coverage-ble

CFLAGS   += ${INCLUDES}
CXXFLAGS += ${INCLUDES}

VPATH += ${BTSTACK_ROOT}/3rd-party/micro-ecc
VPATH += ${BTSTACK_ROOT}/3rd-party/rijndael
VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/ble
VPATH += ${BTSTACK_ROOT}/platform/posix

COMMON = \
    btstack_util.c		  \
    hci_dump.c    \
    att_db_util.c \
    btstack_crypto.c \
    btstack_linked_list.c \
    hci_cmd.c \
    rijndael.c \
    uECC.c

COMMON_OBJ_COVERAGE = $(addprefix build-coverage/,$(COMMON:.c=.o))
COMMON_OBJ_ASAN     = $(addprefix build-asan/,    $(COMMON:.c=.o))

all: build-coverage/att_db_util_test build-coverage/att_db_test build-asan/att_db_util_test build-asan/att_db_test

build-coverage/att_db_util_test: ${COMMON_OBJ_COVERAGE}

build-coverage/att_db_test: build-coverage/att_db.o build-coverage/btstack_util.o build-coverage/hci_dump.o build-coverage/att_db_util.o

build-asan/att_db_util_test: ${COMMON_OBJ_ASAN}

build-asan/att_db_test: build-asan/att_db.o build-asan/btstack_util.o build-asan/hci_dump.o build-asan/att_db_util.o

test: all
	build-asan/att_db_util_test
	build-asan/att_db_test

coverage: all
	rm -f build-coverage/*.gcda
	build-coverage/att_db_util_test
	build-coverage/att_db_test

clean:
	rm -rf build-coverage build-asan
	
