CC = g++

# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..

CFLAGS  = -DUNIT_TEST -x c++ -g -Wall -Wno-unused -I. -I.. -I${BTSTACK_ROOT}/src -I${BTSTACK_ROOT}/ble -I${BTSTACK_ROOT}/include
LDFLAGS +=  -lCppUTest -lCppUTestExt

VPATH += ${BTSTACK_ROOT}/ble 
VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/platforms/posix/src

COMMON = \
    utils.c			            \
    btstack_memory.c			\
    memory_pool.c			    \
    linked_list.c			    \
    sdp_util.c			        \
    remote_device_db_memory.c	\
    run_loop.c					\
    run_loop_posix.c			\
    hci_cmds.c					\
    hci_dump.c					\
    sm.c     					\
    le_device_db_memory.c       \
    rijndael.c 										\
    mock.c
	
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
	