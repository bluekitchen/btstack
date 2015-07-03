CC=g++

# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..
CPPUTEST_HOME = ${BTSTACK_ROOT}/test/cpputest

CFLAGS  = -g -Wall -I. -I../ -I${BTSTACK_ROOT}/src -I${BTSTACK_ROOT}/include
LDFLAGS += -lCppUTest -lCppUTestExt

VPATH += ${BTSTACK_ROOT}/ble 
VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/platforms/posix/src

COMMON = \
    linked_list.c \

COMMON_OBJ = $(COMMON:.c=.o)

all: linked_list_test

linked_list_test: ${COMMON_OBJ} linked_list_test.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

test: all
	./linked_list_test
	
clean:
	rm -fr linked_list_test *.dSYM *.o ../src/*.o
	
