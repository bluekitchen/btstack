CC=g++

# Requirements: http://www.cpputest.org/ should be placed in btstack/test

BTSTACK_ROOT =  ../..
CPPUTEST_HOME = ${BTSTACK_ROOT}/test/cpputest

CFLAGS  = -g -Wall -I. -I../ -I${BTSTACK_ROOT}/src -I${BTSTACK_ROOT}/include -I$(CPPUTEST_HOME)/include
LDFLAGS += -L$(CPPUTEST_HOME) -lCppUTest -lCppUTestExt

COMMON = \
    ${BTSTACK_ROOT}/src/memory_pool.c			  \
    ${BTSTACK_ROOT}/src/remote_device_db_memory.c \
    ${BTSTACK_ROOT}/src/btstack_memory.c		  \
    ${BTSTACK_ROOT}/src/linked_list.c             \
	${BTSTACK_ROOT}/src/utils.c                   \


COMMON_OBJ = $(COMMON:.c=.o)

all: remote-memory

remote-memory: ${COMMON_OBJ} remote_device_db_memory_test.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

clean:
	rm -f remote-memory *.o ../src/*.o
	
