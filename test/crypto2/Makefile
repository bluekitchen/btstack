CC = g++

# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..

CFLAGS  = -DUNIT_TEST -g
CPPFLAGS =  -x c++ -Wall -Wno-unused -fno-exceptions
CFLAGS += -I. -I.. -I${BTSTACK_ROOT}/src -I${BTSTACK_ROOT}/platform/posix
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/mbedtls/include
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/micro-ecc
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/rijndael
CFLAGS += -fprofile-arcs -ftest-coverage
LDFLAGS +=  -lCppUTest -lCppUTestExt

VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/ble 
VPATH += ${BTSTACK_ROOT}/platform/posix
VPATH += ${BTSTACK_ROOT}/3rd-party/micro-ecc
VPATH += ${BTSTACK_ROOT}/3rd-party/rijndael

all: aes_cmac_test

aes_cmac_test: btstack_crypto.o btstack_linked_list.o hci_cmd.o btstack_util.o rijndael.o aes_cmac_test.c hci_dump.o
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

test: all
	./aes_cmac_test

clean:
	rm -f  aes_cmac_test
	rm -f  *.o
	rm -rf *.dSYM
	rm -f *.gcno *.gcda

