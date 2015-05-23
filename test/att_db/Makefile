CC = g++

# Requirements: http://www.cpputest.org/ should be placed in btstack/test

BTSTACK_ROOT =  ../..
CPPUTEST_HOME = ${BTSTACK_ROOT}/test/cpputest

CFLAGS  = -g -Wall -I.. -I${BTSTACK_ROOT}/example/libusb -I${BTSTACK_ROOT}/src -I${BTSTACK_ROOT}/ble -I${BTSTACK_ROOT}/include -I$(CPPUTEST_HOME)/include
# -L$(CPPUTEST_HOME)/lib
LDFLAGS += -lCppUTest -lCppUTestExt 

VPATH += ${BTSTACK_ROOT}/ble 
VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/platforms/posix/src

COMMON = \
    utils.c		  \
    att_db_util.c \
	
COMMON_OBJ = $(COMMON:.c=.o)

all: att_db_util_test

att_db_util_test: ${COMMON_OBJ} att_db_util_test.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

test:
	./att_db_util_test

clean:
	rm -f  att_db_util_test
	rm -f  *.o
	rm -rf *.dSYM
	