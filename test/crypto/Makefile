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

MBEDTLS = \
	ecp.c \
	ecp_curves.c \
	bignum.c \

MICROECC = \
	uECC.c

all: aes_ccm_test aestest ecc_micro_ecc aes_cmac_test

aes_ccm_test: aes_ccm.o aes_ccm_test.o btstack_crypto.o btstack_linked_list.o hci_cmd.o btstack_util.o hci_dump.o aes_cmac.o rijndael.o mock.o
	${CC} ${CFLAGS} $^ -o $@

aestest: aestest.o rijndael.o
	${CC} ${CFLAGS} $^ -o $@

ecc_micro_ecc: ecc_micro_ecc.o ${MICROECC}
	gcc ${CFLAGS} $^ -o $@ 

aes_cmac_test: aes_cmac_test.o aes_cmac.o rijndael.o
	gcc ${CFLAGS} $^ -o $@ 

sm_mbedtls_allocator_test: sm_mbedtls_allocator.o hci_dump.o btstack_util.o sm_mbedtls_allocator_test.c
	${CC} sm_mbedtls_allocator.o btstack_util.o hci_dump.o sm_mbedtls_allocator_test.c ${CFLAGS} ${CPPFLAGS}  ${LDFLAGS} -o $@ 

test: all
	./aes_cmac_test
	./aestest
	./ecc_micro_ecc
	./aes_cmac_test
	
clean:
	rm -f  aestest ecc_micro_ecc aes_cmac_test
	rm -f  *.o
	rm -rf *.dSYM
	rm -f *.gcno *.gcda

