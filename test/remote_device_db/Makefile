CC=g++

# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..
CPPUTEST_HOME = ${BTSTACK_ROOT}/test/cpputest

CFLAGS  = -g -Wall -I. -I../ -I${BTSTACK_ROOT}/src -I${BTSTACK_ROOT}/include -I${BTSTACK_ROOT}/ble
LDFLAGS += -lCppUTest -lCppUTestExt

VPATH += ${BTSTACK_ROOT}/ble 
VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/platforms/posix/src

FS = \
    utils.c                   \
    hci_dump.c                \
	remote_device_db_fs.c


MEMORY = \
	utils.c                   \
	memory_pool.c			  \
    btstack_memory.c		  \
    hci_dump.c                \
    remote_device_db_memory.c \
    linked_list.c             

FS_OBJ = $(FS:.c=.o)
MEMORY_OBJ = $(MEMORY:.c=.o)

all:  remote_device_db_memory_test remote_device_db_fs_test

remote_device_db_memory_test: ${MEMORY_OBJ} remote_device_db_memory_test.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

remote_device_db_fs_test: ${FS_OBJ} remote_device_db_fs_test.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

test: all
	./remote_device_db_memory_test
	./remote_device_db_fs_test

clean:
	rm -f remote_device_db_memory_test remote_device_db_fs_test  *.o ../src/*.o 
	rm -rf *.dSYM
	
