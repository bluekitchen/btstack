CC = g++

# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..

CFLAGS  = -g -Wall -I. -I${BTSTACK_ROOT}/src -I${BTSTACK_ROOT}/include
# CFLAGS += -fprofile-arcs -ftest-coverage
LDFLAGS += -lCppUTest -lCppUTestExt

VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/classic
VPATH += ${BTSTACK_ROOT}/platform/posix

COMMON = \
	btstack_util.c		    \
	hci_dump.c    			\
	btstack_memory_pool.c 	\
	btstack_memory.c 	    \

COMMON_OBJ = $(COMMON:.c=.o)

all: btstack_memory_pool_test btstack_memory_test

btstack_memory_pool_test: ${COMMON_OBJ} btstack_memory_pool_test.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

btstack_memory_test: ${COMMON_OBJ} btstack_memory_test.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

test: all
	./btstack_memory_pool_test
	./btstack_memory_test

clean:
	rm -f btstack_memory_pool_test *.o
	rm -f btstack_memory_test *.o
	rm -rf *.dSYM
	rm -f *.gcno *.gcda
	
