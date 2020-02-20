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

CFLAGS += -fprofile-arcs -ftest-coverage -fsanitize=address,undefined
LDFLAGS += -lCppUTest -lCppUTestExt

TESTS = tlv_test tlv_le_test

all: ${TESTS}

clean:
	rm -rf *.o $(TESTS) *.dSYM *.pklg
	rm -f *.gcno *.gcda

tlv_test: ${COMMON_OBJ} btstack_link_key_db_tlv.o tlv_test.o  
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

tlv_le_test: ${COMMON_OBJ} le_device_db_tlv.o tlv_le_test.o  
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

test: all
	@echo Run all test
	@set -e; \
	for test in $(TESTS); do \
	  ./$$test; \
	done

