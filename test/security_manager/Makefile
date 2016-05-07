CC = g++

# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..

CFLAGS  = -DUNIT_TEST -g
CPPFLAGS =  -x c++ -Wall -Wno-unused
CFLAGS += -I. -I.. -I${BTSTACK_ROOT}/src -I${BTSTACK_ROOT}/ble -I${BTSTACK_ROOT}/platform/posix
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/mbedtls/include
LDFLAGS +=  -lCppUTest -lCppUTestExt

VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/ble 
VPATH += ${BTSTACK_ROOT}/platform/posix
VPATH += ${BTSTACK_ROOT}/3rd-party/mbedtls/library

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
	# aes.c \
	# aesni.c \
	# asn1parse.c \
	# asn1write.c \
	# base64.c \
	# ctr_drbg.c \
	# entropy.c \
	# entropy_poll.c \
	# error.c \
	# md.c \
	# md5.c \
	# md_wrap.c \
	# oid.c \
	# pem.c \
	# pk.c \
	# pk_wrap.c \
	# pkwrite.c \
	# sha256.c \
	# sha512.c \
	# timing.c \

all: security_manager aestest ectest aes_cmac_test

security_manager: ${CORE_OBJ} ${COMMON_OBJ} security_manager.c
	${CC} ${CORE_OBJ} ${COMMON_OBJ} security_manager.c ${CFLAGS} ${CPPFLAGS} ${LDFLAGS} -o $@

aestest: aestest.c rijndael.c
	${CC} ${CFLAGS} rijndael.c aestest.c -o $@

ectest: ectest.c ${MBEDTLS} rijndael.c
	gcc ${CFLAGS} $^ -o $@ 

aes_cmac_test: aes_cmac_test.c rijndael.c
	gcc ${CFLAGS} $^ -o $@ 

test: all
	./security_manager
	./aestest
	
clean:
	rm -f  security_manager
	rm -f  *.o
	rm -rf *.dSYM
	