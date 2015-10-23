CC = g++

# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..

CFLAGS  = -g -Wall -I.. -I${BTSTACK_ROOT}/example/libusb -I${BTSTACK_ROOT}/src -I${BTSTACK_ROOT}/ble -I${BTSTACK_ROOT}/include 
LDFLAGS += -lCppUTest -lCppUTestExt 

VPATH += ${BTSTACK_ROOT}/ble 
VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/platforms/posix/src

COMMON = \
    utils.c		  \
    hci_dump.c    \
    att_db_util.c \
	
COMMON_OBJ = $(COMMON:.c=.o)

all: att_db_util_test

att_db_util_test: ${COMMON_OBJ} att_db_util_test.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

test: all
	./att_db_util_test

clean:
	rm -f  att_db_util_test
	rm -f  *.o
	rm -rf *.dSYM
	