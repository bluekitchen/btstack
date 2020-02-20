CC = g++

# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..

CFLAGS  = -DUNIT_TEST -g
CPPFLAGS =  -x c++ -Wall -Wno-unused
CFLAGS += -I. -I.. -I${BTSTACK_ROOT}/src -I${BTSTACK_ROOT}/platform/posix
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/mbedtls/include
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/micro-ecc
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/rijndael
CFLAGS += -fprofile-arcs -ftest-coverage -fsanitize=address,undefined
LDFLAGS +=  -lCppUTest -lCppUTestExt

VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/ble 
VPATH += ${BTSTACK_ROOT}/platform/posix
VPATH += ${BTSTACK_ROOT}/3rd-party/micro-ecc
VPATH += ${BTSTACK_ROOT}/3rd-party/rijndael

COMMON = \
	btstack_crypto.c    		\
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
	btstack_tlv.c \
	
COMMON_OBJ = $(COMMON:.c=.o)

MBEDTLS = \
	ecp.c \
	ecp_curves.c \
	bignum.c \

MICROECC = \
	uECC.c

all: security_manager

security_manager: ${CORE_OBJ} ${COMMON_OBJ} security_manager.c
	${CC} ${CORE_OBJ} ${COMMON_OBJ} security_manager.c ${CFLAGS} ${CPPFLAGS} ${LDFLAGS} -o $@

test: all
	./security_manager
	
clean:
	rm -f  security_manager
	rm -f  *.o
	rm -rf *.dSYM
	rm -f *.gcno *.gcda
	
