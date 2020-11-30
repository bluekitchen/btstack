CC=g++

# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..
CPPUTEST_HOME = ${BTSTACK_ROOT}/test/cpputest

CFLAGS  = -g -Wall \
		  -I. \
		  -I.. \
		  -I${BTSTACK_ROOT}/src \
		  -I${BTSTACK_ROOT}/platform/posix \
		  -I${BTSTACK_ROOT}/3rd-party/tinydir
CFLAGS += -fprofile-arcs -ftest-coverage

LDFLAGS += -lCppUTest -lCppUTestExt

VPATH += ${BTSTACK_ROOT}/src/classic
VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/platform/posix

FS = \
    btstack_util.c                   \
    hci_dump.c                \
	btstack_link_key_db_fs.c


MEMORY = \
	btstack_util.c               \
	btstack_memory_pool.c	     \
    btstack_memory.c		     \
    hci_dump.c                   \
    btstack_link_key_db_memory.c \
    btstack_linked_list.c             

FS_OBJ = $(FS:.c=.o)
MEMORY_OBJ = $(MEMORY:.c=.o)

all:  build-coverage/btstack_link_key_db_memory_test build-coverage/btstack_link_key_db_fs_test

build-coverage:
	mkdir -p build-coverage

build-coverage/btstack_link_key_db_memory_test: ${MEMORY_OBJ} btstack_link_key_db_memory_test.c | build-coverage
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

build-coverage/btstack_link_key_db_fs_test: ${FS_OBJ} btstack_link_key_db_fs_test.c | build-coverage
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

test: all
	build-coverage/btstack_link_key_db_memory_test
	build-coverage/btstack_link_key_db_fs_test

coverage: all
	build-coverage/btstack_link_key_db_memory_test
	build-coverage/btstack_link_key_db_fs_test

clean:
	rm -f  *.o
	rm -f  *.gcno *.gcda
	rm -rf *.dSYM build-coverage

