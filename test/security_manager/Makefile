CC = g++

# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..

CFLAGS  = -DUNIT_TEST -x c++ -g -Wall -Wno-unused
CFLAGS += -I. -I.. -I${BTSTACK_ROOT}/src -I${BTSTACK_ROOT}/ble -I${BTSTACK_ROOT}/platform/posix
LDFLAGS +=  -lCppUTest -lCppUTestExt

VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/ble 
VPATH += ${BTSTACK_ROOT}/platform/posix

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

all: security_manager aestest

security_manager: ${CORE_OBJ} ${COMMON_OBJ} security_manager.c
	${CC} ${CORE_OBJ} ${COMMON_OBJ} security_manager.c ${CFLAGS} ${LDFLAGS} -o $@

aestest: aestest.c rijndael.c
	${CC} ${CFLAGS} rijndael.c aestest.c -o $@

test: all
	./security_manager
	./aestest
	
clean:
	rm -f  security_manager
	rm -f  *.o
	rm -rf *.dSYM
	