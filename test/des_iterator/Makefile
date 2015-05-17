CC = g++

# Requirements: http://www.cpputest.org/ should be placed in btstack/test

BTSTACK_ROOT =  ../..
CPPUTEST_HOME = ${BTSTACK_ROOT}/test/cpputest

CFLAGS  = -g -Wall -I. -I${BTSTACK_ROOT}/src -I${BTSTACK_ROOT}/include -I$(CPPUTEST_HOME)/include
LDFLAGS += -lCppUTest -lCppUTestExt
# -L$(CPPUTEST_HOME) 

VPATH += ${BTSTACK_ROOT}/ble 
VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/platforms/posix/src

COMMON = \
    sdp_util.c	              \
	utils.c			          
 
COMMON_OBJ = $(COMMON:.c=.o)

all: des_iterator_test

des_iterator_test: ${COMMON_OBJ} des_iterator_test.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

test:
	./des_iterator_test

clean:
	rm -f des_iterator_test *.o
	rm -rf *.dSYM
	