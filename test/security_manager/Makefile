CC = g++

# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..

CFLAGS  = -DUNIT_TEST -g
CPPFLAGS =  -x c++ -Wall -Wno-unused
CFLAGS += -I. -I.. -I${BTSTACK_ROOT}/src -I${BTSTACK_ROOT}/ble -I${BTSTACK_ROOT}/platform/posix
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/mbedtls/include
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/micro-ecc
LDFLAGS +=  -lCppUTest -lCppUTestExt

VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/ble 
VPATH += ${BTSTACK_ROOT}/platform/posix
VPATH += ${BTSTACK_ROOT}/3rd-party/mbedtls/library
VPATH += ${BTSTACK_ROOT}/3rd-party/micro-ecc

COMMON = \
    btstack_linked_list.c		\
    btstack_memory.c			\
    btstack_memory_pool.c		\
    btstack_run_loop.c			\
    btstack_run_loop_posix.c    \
    hci_cmd.c					\
    hci_dump.c					\
    le_device_db_memory.c       \
    mock.c 				        \
    rijndael.c 					\
    sm.c     					\
    btstack_util.c			            \
	
COMMON_OBJ = $(COMMON:.c=.o)

MBEDTLS = \
	ecp.c \
	ecp_curves.c \
	bignum.c \

MICROECC = \
	uECC.c

all: security_manager aestest ecc_mbed_tls ecc_micro_ecc aes_cmac_test
# sm_mbedtls_allocator_test

security_manager: ${CORE_OBJ} ${COMMON_OBJ} security_manager.c
	${CC} ${CORE_OBJ} ${COMMON_OBJ} security_manager.c ${CFLAGS} ${CPPFLAGS} ${LDFLAGS} -o $@

aestest: aestest.o rijndael.o
	${CC} ${CFLAGS} $^ -o $@

ecc_mbed_tls: ecc_mbed_tls.o ${MBEDTLS}
	gcc ${CFLAGS} $^ -o $@ 

ecc_micro_ecc: ecc_micro_ecc.o ${MICROECC}
	gcc ${CFLAGS} $^ -o $@ 

aes_cmac_test: aes_cmac_test.o aes_cmac.o rijndael.o
	gcc ${CFLAGS} $^ -o $@ 

sm_mbedtls_allocator_test: sm_mbedtls_allocator.o hci_dump.o btstack_util.o sm_mbedtls_allocator_test.c
	${CC} sm_mbedtls_allocator.o btstack_util.o hci_dump.o sm_mbedtls_allocator_test.c ${CFLAGS} ${CPPFLAGS}  ${LDFLAGS} -o $@ 

test: all
	./security_manager
	./aes_cmac_test
	./aestest
	./ecc_mbed_tls
	./ecc_micro_ecc
	./aes_cmac_test
	
clean:
	rm -f  security_manager
	rm -f  *.o
	rm -rf *.dSYM
	