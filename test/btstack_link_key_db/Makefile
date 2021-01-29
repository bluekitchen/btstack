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

CFLAGS_COVERAGE = ${CFLAGS} -fprofile-arcs -ftest-coverage
CFLAGS_ASAN     = ${CFLAGS} -fsanitize=address -DHAVE_ASSERT

LDFLAGS += -lCppUTest -lCppUTestExt
LDFLAGS_COVERAGE = ${LDFLAGS} -fprofile-arcs -ftest-coverage
LDFLAGS_ASAN     = ${LDFLAGS} -fsanitize=address

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

FS_OBJ_COVERAGE = $(addprefix build-coverage/,$(FS:.c=.o))
FS_OBJ_ASAN     = $(addprefix build-asan/,    $(FS:.c=.o))

MEMORY_OBJ_COVERAGE = $(addprefix build-coverage/,$(MEMORY:.c=.o))
MEMORY_OBJ_ASAN     = $(addprefix build-asan/,    $(MEMORY:.c=.o))

all:  build-coverage/btstack_link_key_db_memory_test build-coverage/btstack_link_key_db_fs_test build-asan/btstack_link_key_db_memory_test build-asan/btstack_link_key_db_fs_test

build-%:
	mkdir -p $@

build-coverage/%.o: %.c | build-coverage
	${CC} -c $(CFLAGS_COVERAGE) $< -o $@

build-asan/%.o: %.c | build-asan
	${CC} -c $(CFLAGS_ASAN) $< -o $@

build-coverage/btstack_link_key_db_fs_test: ${FS_OBJ_COVERAGE} build-coverage/btstack_link_key_db_fs_test.o | build-coverage
	${CC} $^ ${LDFLAGS_COVERAGE} -o $@

build-coverage/btstack_link_key_db_memory_test: ${MEMORY_OBJ_COVERAGE} build-coverage/btstack_link_key_db_memory_test.o | build-coverage
	${CC} $^ ${LDFLAGS_COVERAGE} -o $@

build-asan/btstack_link_key_db_fs_test: ${FS_OBJ_ASAN} build-asan/btstack_link_key_db_fs_test.o | build-asan
	${CC} $^ ${LDFLAGS_ASAN} -o $@

build-asan/btstack_link_key_db_memory_test: ${MEMORY_OBJ_ASAN} build-asan/btstack_link_key_db_memory_test.o | build-asan
	${CC} $^ ${LDFLAGS_ASAN} -o $@


test: all
	build-asan/btstack_link_key_db_memory_test
	build-asan/btstack_link_key_db_fs_test

coverage: all
	rm -f build-coverage/*.gcda
	build-coverage/btstack_link_key_db_memory_test
	build-coverage/btstack_link_key_db_fs_test

clean:
	rm -rf build-coverage build-asan

