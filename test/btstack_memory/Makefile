CC = g++

# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..

CFLAGS  = -g -Wall -I. -I${BTSTACK_ROOT}/src -I${BTSTACK_ROOT}/include
CFLAGS += -fprofile-arcs -ftest-coverage
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

all: build-coverage/btstack_memory_pool_test build-coverage/btstack_memory_test

build-coverage:
	mkdir -p build-coverage

build-coverage/btstack_memory_pool_test: ${COMMON_OBJ} btstack_memory_pool_test.c | build-coverage
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

build-coverage/btstack_memory_test: ${COMMON_OBJ} btstack_memory_test.c | build-coverage
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

test: all
	build-coverage/btstack_memory_pool_test
	build-coverage/btstack_memory_test

coverage: all
	build-coverage/btstack_memory_pool_test
	build-coverage/btstack_memory_test

clean:
	rm -f  *.o
	rm -rf *.dSYM build-coverage
	rm -f  *.gcno *.gcda
	
