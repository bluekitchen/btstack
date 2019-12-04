CC=g++

# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..
CPPUTEST_HOME = ${BTSTACK_ROOT}/test/cpputest

CFLAGS  = -g -Wall -I. -I../ -I${BTSTACK_ROOT}/src -I${BTSTACK_ROOT}/include
CFLAGS += -fprofile-arcs -ftest-coverage
LDFLAGS += -lCppUTest -lCppUTestExt

VPATH += ${BTSTACK_ROOT}/src/ble 
VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/platform/posix

COMMON = \
    btstack_linked_list.c \
    hci_dump.c \
    btstack_util.c \

COMMON_OBJ = $(COMMON:.c=.o)

all: btstack_linked_list_test

btstack_linked_list_test: ${COMMON_OBJ} btstack_linked_list_test.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

test: all
	./btstack_linked_list_test
	
clean:
	rm -fr btstack_linked_list_test *.dSYM *.o ../src/*.o 
	rm -f *.gcno *.gcda
	
