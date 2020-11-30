CC=g++

BTSTACK_ROOT = ../..
CPPUTEST_HOME = ${BTSTACK_ROOT}/test/cpputest

COMMON_OBJ = \
	btstack_tlv_flash_bank.o \
	btstack_util.o \
	hal_flash_bank_memory.o \
	hci_dump.o \

VPATH = \
	${BTSTACK_ROOT}/src \
	${BTSTACK_ROOT}/src/classic \
	${BTSTACK_ROOT}/src/ble \
	${BTSTACK_ROOT}/platform/embedded \

CFLAGS  = \
    -DBTSTACK_TEST \
    -g \
    -Wall \
    -Wmissing-prototypes \
    -Wnarrowing \
    -I. \
    -I.. \
    -I${BTSTACK_ROOT}/src \
    -I${BTSTACK_ROOT}/platform/embedded \

CFLAGS += -fprofile-arcs -ftest-coverage
LDFLAGS += -lCppUTest -lCppUTestExt

all: build-coverage/tlv_test

build-coverage:
	mkdir -p build-coverage

clean:
	rm -rf *.o *.dSYM *.pklg build-coverage
	rm -f *.gcno *.gcda

build-coverage/tlv_test: ${COMMON_OBJ} btstack_link_key_db_tlv.o tlv_test.o | build-coverage
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

test: all
	build-coverage/tlv_test

coverage: all
	build-coverage/tlv_test
