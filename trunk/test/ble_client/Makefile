CC = g++

# Requirements: http://www.cpputest.org/ should be placed in btstack/test

BTSTACK_ROOT =  ../..
CPPUTEST_HOME = ${BTSTACK_ROOT}/test/cpputest

CFLAGS  = -g -Wall -I. -I${BTSTACK_ROOT}/example/libusb -I${BTSTACK_ROOT}/src -I${BTSTACK_ROOT}/ble -I${BTSTACK_ROOT}/include -I$(CPPUTEST_HOME)/include
LDFLAGS += -L$(CPPUTEST_HOME)/lib -lCppUTest -lCppUTestExt

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
    ${BTSTACK_ROOT}/src/hci_dump.c					\
    ${BTSTACK_ROOT}/src/hci.c                       \
    ${BTSTACK_ROOT}/ble/ad_parser.c                 \
	
COMMON_OBJ = $(COMMON:.c=.o)

all: ad_parser

ad_parser: ${CORE_OBJ} ${COMMON_OBJ} advertising_data_parser.c
	${CC} ${CORE_OBJ} ${COMMON_OBJ} advertising_data_parser.c ${CFLAGS} ${LDFLAGS} -o $@

clean:
	rm -f  ad_parser le_central
	rm -f  *.o ${BTSTACK_ROOT}/src/*.o
	rm -rf *.dSYM
	