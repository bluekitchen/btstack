CC=g++

# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..
CPPUTEST_HOME = ${BTSTACK_ROOT}/test/cpputest

CFLAGS  = -g -Wall -I. -I../ -I${BTSTACK_ROOT}/src -I${BTSTACK_ROOT}/include
LDFLAGS += -lCppUTest -lCppUTestExt

VPATH += ${BTSTACK_ROOT}/src/ble 
VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/platform/posix/src

COMMON = \
    bk_linked_list.c \

COMMON_OBJ = $(COMMON:.c=.o)

all: bk_linked_list_test

bk_linked_list_test: ${COMMON_OBJ} bk_linked_list_test.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

test: all
	./bk_linked_list_test
	
clean:
	rm -fr bk_linked_list_test *.dSYM *.o ../src/*.o
	
