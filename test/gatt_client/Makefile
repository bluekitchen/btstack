CC = g++

# Requirements: http://www.cpputest.org/ should be placed in btstack/test

BTSTACK_ROOT =  ../..
CPPUTEST_HOME = ${BTSTACK_ROOT}/test/cpputest

CFLAGS  = -DUNIT_TEST -x c++ -g -Wall -I. -I../ -I${BTSTACK_ROOT}/src -I${BTSTACK_ROOT}/ble -I${BTSTACK_ROOT}/include -I$(CPPUTEST_HOME)/include
LDFLAGS += -L$(CPPUTEST_HOME) -lCppUTest -lCppUTestExt

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
    hci_cmds.c					\
    hci.c     					\
    hci_dump.c     				\
    att_dispatch.c       	    \
    att.c     					\
    ad_parser.c                 \
    gatt_client.c               \
    gatt_client_test.c          \
    mock.c
	

COMMON_OBJ = $(COMMON:.c=.o)

all: gatt_client le_central

# compile .ble description
profile.h: profile.gatt
	python ${BTSTACK_ROOT}/ble/compile-gatt.py $< $@ 

gatt_client: ${CORE_OBJ} ${COMMON_OBJ} gatt_client.o profile.h expected_results.h
	${CC} ${CORE_OBJ} ${COMMON_OBJ} gatt_client.o ${CFLAGS} ${LDFLAGS} -o $@

le_central: ${CORE_OBJ} ${COMMON_OBJ} le_central.o
	${CC} ${CORE_OBJ} ${COMMON_OBJ} le_central.o ${CFLAGS} ${LDFLAGS} -o $@

clean:
	rm -f  gatt_client le_central
	rm -f  *.o *.o ${BTSTACK_ROOT}/ble/*.o
	rm -rf *.dSYM
	