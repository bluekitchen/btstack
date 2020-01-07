CC=g++

# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..
CPPUTEST_HOME = ${BTSTACK_ROOT}/test/cpputest

CFLAGS  = -g -Wall -I. -I../ -I${BTSTACK_ROOT}/src
CFLAGS  += -fprofile-arcs -ftest-coverage -fsanitize=address,undefined
LDFLAGS += -lCppUTest -lCppUTestExt

VPATH += ${BTSTACK_ROOT}/src

COMMON = \
    btstack_ring_buffer.c \

COMMON_OBJ = $(COMMON:.c=.o)

all: btstack_ring_buffer_test

btstack_ring_buffer_test: ${COMMON_OBJ} btstack_ring_buffer_test.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

test: all
	./btstack_ring_buffer_test
	
clean:
	rm -fr btstack_ring_buffer_test *.dSYM *.o ../src/*.o *.gcda *.gcno
	rm -f *.gcno *.gcda
	
