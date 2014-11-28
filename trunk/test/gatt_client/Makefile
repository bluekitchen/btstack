CC = g++

# Requirements: http://www.cpputest.org/ should be placed in btstack/test

BTSTACK_ROOT =  ../..
CPPUTEST_HOME = ${BTSTACK_ROOT}/test/cpputest

CFLAGS  = -DUNIT_TEST -x c++ -g -Wall -I. -I../ -I${BTSTACK_ROOT}/src -I${BTSTACK_ROOT}/ble -I${BTSTACK_ROOT}/include -I$(CPPUTEST_HOME)/include
LDFLAGS += -L$(CPPUTEST_HOME) -lCppUTest -lCppUTestExt

COMMON = \
    ${BTSTACK_ROOT}/src/utils.c			            \
    ${BTSTACK_ROOT}/src/btstack_memory.c			\
    ${BTSTACK_ROOT}/src/memory_pool.c			    \
    ${BTSTACK_ROOT}/src/linked_list.c			    \
    ${BTSTACK_ROOT}/src/sdp_util.c			        \
    ${BTSTACK_ROOT}/src/remote_device_db_memory.c	\
    ${BTSTACK_ROOT}/src/hci_cmds.c					\
    ${BTSTACK_ROOT}/src/hci.c     					\
    ${BTSTACK_ROOT}/src/hci_dump.c     				\
    ${BTSTACK_ROOT}/ble/att_dispatch.c       	    \
    ${BTSTACK_ROOT}/ble/att.c     					\
    ${BTSTACK_ROOT}/ble/ad_parser.c                 \
    ${BTSTACK_ROOT}/ble/gatt_client.c               \
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
	rm -f  *.o ${BTSTACK_ROOT}/src/*.o
	rm -rf *.dSYM
	