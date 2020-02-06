CC=g++

# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..
CPPUTEST_HOME = ${BTSTACK_ROOT}/test/cpputest

CFLAGS  = -g -Wall -I. -I../ -I${BTSTACK_ROOT}/src -I${BTSTACK_ROOT}/include
LDFLAGS += -lCppUTest -lCppUTestExt

COMMON_OBJ = $(COMMON:.c=.o)

all: sqrt_test

sqrt_test: ${COMMON_OBJ} sqrt_test.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

test: all
	./sqrt_test
	
clean:
	rm -fr sqrt_test *.dSYM *.o ../src/*.o
	
