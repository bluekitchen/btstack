CC = g++

# Requirements: http://www.cpputest.org/ should be placed in btstack/test

BTSTACK_ROOT =  ../..
CPPUTEST_HOME = ${BTSTACK_ROOT}/test/cpputest

CFLAGS  = -DUNIT_TEST -g -Wall -I. -I${BTSTACK_ROOT}/example/libusb -I${BTSTACK_ROOT}/src -I${BTSTACK_ROOT}/ble -I${BTSTACK_ROOT}/include -I$(CPPUTEST_HOME)/include
LDFLAGS += -L$(CPPUTEST_HOME) -lCppUTest -lCppUTestExt

COMMON = \
    ${BTSTACK_ROOT}/src/utils.c			            \
    ${BTSTACK_ROOT}/src/btstack_memory.c			\
    ${BTSTACK_ROOT}/src/memory_pool.c			    \
    ${BTSTACK_ROOT}/src/linked_list.c			    \
    ${BTSTACK_ROOT}/src/sdp_util.c			        \
    ${BTSTACK_ROOT}/src/remote_device_db_memory.c	\
    ${BTSTACK_ROOT}/src/run_loop.c					\
    ${BTSTACK_ROOT}/src/run_loop_posix.c			\
    ${BTSTACK_ROOT}/src/hci_cmds.c					\
    ${BTSTACK_ROOT}/example/libusb/ble_client.c     \
    mock.c
	
COMMON_OBJ = $(COMMON:.c=.o)

all: gatt_client

gatt_client: ${CORE_OBJ} ${COMMON_OBJ} gatt_client.c
	${CC} ${CORE_OBJ} ${COMMON_OBJ} gatt_client.c ${CFLAGS} ${LDFLAGS} -o $@

clean:
	rm -f  gatt_client
	rm -f  *.o ${BTSTACK_ROOT}/src/*.o
	rm -rf *.dSYM
	