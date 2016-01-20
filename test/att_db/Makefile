CC = g++

# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..

CFLAGS  = -g -Wall \
		  -I.. \
		  -I${BTSTACK_ROOT}/example/libusb \
		  -I${BTSTACK_ROOT}/src
		  
LDFLAGS += -lCppUTest -lCppUTestExt 

VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/ble
VPATH += ${BTSTACK_ROOT}/platform/posix

COMMON = \
    btstack_util.c		  \
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
	