CC=g++

# Requirements: http://www.cpputest.org/ should be placed in btstack/test

BTSTACK_ROOT =  ../..
CPPUTEST_HOME = ${BTSTACK_ROOT}/test/cpputest

CFLAGS  = -g -Wall -I. -I../ -I${BTSTACK_ROOT}/src -I${BTSTACK_ROOT}/include -I${BTSTACK_ROOT}/ble -I$(CPPUTEST_HOME)/include
LDFLAGS += -L$(CPPUTEST_HOME) -lCppUTest -lCppUTestExt

FS = \
    ${BTSTACK_ROOT}/src/utils.c                   \
	${BTSTACK_ROOT}/platforms/posix/src/remote_device_db_fs.c


MEMORY = \
	${BTSTACK_ROOT}/src/utils.c                   \
	${BTSTACK_ROOT}/src/memory_pool.c			  \
    ${BTSTACK_ROOT}/src/btstack_memory.c		  \
    ${BTSTACK_ROOT}/src/remote_device_db_memory.c \
    ${BTSTACK_ROOT}/src/linked_list.c             

FS_OBJ = $(FS:.c=.o)
MEMORY_OBJ = $(MEMORY:.c=.o)

all:  remote_device_db_memory_test remote_device_db_fs_test

remote_device_db_memory_test: ${MEMORY_OBJ} remote_device_db_memory_test.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

remote_device_db_fs_test: ${FS_OBJ} remote_device_db_fs_test.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

clean:
	rm -f remote_device_db_memory_test remote_device_db_fs_test  *.o ../src/*.o
	
