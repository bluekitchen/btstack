CC = g++

# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..

CFLAGS  = -g -Wall -I.. -I${BTSTACK_ROOT}/example/libusb -I${BTSTACK_ROOT}/src -I${BTSTACK_ROOT}/ble -I${BTSTACK_ROOT}/include
LDFLAGS += -L$(CPPUTEST_HOME)/lib -lCppUTest -lCppUTestExt

VPATH += ${BTSTACK_ROOT}/src/ble 
VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/platform/posix

COMMON = \
    ad_parser.c                 \
    btstack_linked_list.c	    \
    btstack_memory.c			\
    btstack_memory_pool.c		\
    btstack_run_loop.c			\
    btstack_run_loop_posix.c 	\
    btstack_util.c			    \
    hci.c                       \
    hci_cmd.c					\
    hci_dump.c					\
	
COMMON_OBJ = $(COMMON:.c=.o)

all: ad_parser

ad_parser: ${CORE_OBJ} ${COMMON_OBJ} advertising_data_parser.c
	${CC} ${CORE_OBJ} ${COMMON_OBJ} advertising_data_parser.c ${CFLAGS} ${LDFLAGS} -o $@

test: all
	./ad_parser

clean:
	rm -f  ad_parser le_central
	rm -f  *.o
	rm -rf *.dSYM
	