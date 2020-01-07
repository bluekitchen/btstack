CC=g++

BTSTACK_ROOT = ../..
CPPUTEST_HOME = ${BTSTACK_ROOT}/test/cpputest

COMMON_OBJ = \
	btstack_tlv_posix.o \
	btstack_util.o \
	btstack_linked_list.o \
	hci_dump.o \

VPATH = \
	${BTSTACK_ROOT}/src \
	${BTSTACK_ROOT}/src/classic \
	${BTSTACK_ROOT}/src/ble \
	${BTSTACK_ROOT}/platform/posix \

CFLAGS  = \
    -DBTSTACK_TEST \
    -g \
    -Wall \
    -Wnarrowing \
    -I. \
    -I.. \
    -I${BTSTACK_ROOT}/src \
    -I${BTSTACK_ROOT}/platform/posix \

CFLAGS += -fprofile-arcs -ftest-coverage -fsanitize=address,undefined
LDFLAGS += -lCppUTest -lCppUTestExt

TESTS = tlv_test

all: ${TESTS}

clean:
	rm -rf *.o $(TESTS) *.dSYM *.pklg
	rm -f *.gcno *.gcda

tlv_test: ${COMMON_OBJ} tlv_test.o  
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

test: all
	@echo Run all test
	@set -e; \
	for test in $(TESTS); do \
	  ./$$test; \
	done

