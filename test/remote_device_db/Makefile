CC=g++

# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..
CPPUTEST_HOME = ${BTSTACK_ROOT}/test/cpputest

CFLAGS  = -g -Wall \
		  -I. \
		  -I.. \
		  -I${BTSTACK_ROOT}/src
		  
LDFLAGS += -lCppUTest -lCppUTestExt

VPATH += ${BTSTACK_ROOT}/src/classic
VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/platform/posix

FS = \
    btstack_util.c                   \
    hci_dump.c                \
	remote_device_db_fs.c


MEMORY = \
	btstack_util.c                   \
	btstack_memory_pool.c			  \
    btstack_memory.c		  \
    hci_dump.c                \
    remote_device_db_memory.c \
    btstack_linked_list.c             

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
	
