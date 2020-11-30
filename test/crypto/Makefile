CC = g++

# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..

CFLAGS  = -DUNIT_TEST -g
CPPFLAGS =  -x c++ -Wall -Wno-unused -fno-exceptions
CFLAGS += -I. -I${BTSTACK_ROOT}/src -I${BTSTACK_ROOT}/platform/posix
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

MBEDTLS = \
	ecp.c \
	ecp_curves.c \
	bignum.c \

MICROECC = \
	uECC.c

all: build-coverage/aes_ccm_test build-coverage/aestest build-coverage/ecc_micro_ecc build-coverage/aes_cmac_test

build-coverage:
	mkdir -p build-coverage

build-coverage/aes_ccm_test: aes_ccm.o aes_ccm_test.o btstack_crypto.o btstack_linked_list.o hci_cmd.o btstack_util.o hci_dump.o aes_cmac.o rijndael.o mock.o | build-coverage
	${CC} ${CFLAGS} $^ -o $@

build-coverage/aestest: aestest.o rijndael.o | build-coverage
	${CC} ${CFLAGS} $^ -o $@

build-coverage/ecc_micro_ecc: ecc_micro_ecc.o ${MICROECC} | build-coverage
	gcc ${CFLAGS} $^ -o $@ 

build-coverage/aes_cmac_test: aes_cmac_test.o aes_cmac.o rijndael.o | build-coverage
	gcc ${CFLAGS} $^ -o $@ 

test: all
	build-coverage/aes_cmac_test
	build-coverage/aestest
	build-coverage/ecc_micro_ecc
	build-coverage/aes_cmac_test
	
coverage: all
	build-coverage/aes_cmac_test
	build-coverage/aestest
	build-coverage/ecc_micro_ecc
	build-coverage/aes_cmac_test

clean:
	rm -f  *.o
	rm -f *.gcno *.gcda
	rm -rf *.dSYM

