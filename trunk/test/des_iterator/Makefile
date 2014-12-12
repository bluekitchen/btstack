CC = g++

# Requirements: http://www.cpputest.org/ should be placed in btstack/test

BTSTACK_ROOT =  ../..
CPPUTEST_HOME = ${BTSTACK_ROOT}/test/cpputest

CFLAGS  = -g -Wall -I. -I${BTSTACK_ROOT}/src -I${BTSTACK_ROOT}/include -I$(CPPUTEST_HOME)/include
LDFLAGS += -L$(CPPUTEST_HOME) -lCppUTest -lCppUTestExt

COMMON = \
    ${BTSTACK_ROOT}/src/sdp_util.c	              \
	${BTSTACK_ROOT}/src/utils.c			          
 
COMMON_OBJ = $(COMMON:.c=.o)

all: des_iterator_test

des_iterator_test: ${COMMON_OBJ} des_iterator_test.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@


clean:
	rm -f des_iterator_test *.o ${BTSTACK_ROOT}/src/*.o
	rm -rf *.dSYM
	