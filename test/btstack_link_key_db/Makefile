
include ../common.make

DEFINES := -DUNIT_TEST
INCLUDES := -I${BTSTACK_ROOT}/src
INCLUDES += -I${BTSTACK_ROOT}/platform/posix
INCLUDES += -I${BTSTACK_ROOT}/3rd-party/tinydir
INCLUDES += -I.

CFLAGS += ${DEFINES} ${INCLUDES}
CXXFLAGS += ${DEFINES} ${INCLUDES}

VPATH += ${BTSTACK_ROOT}/src/classic
VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/platform/posix

FS = \
    btstack_util.c            \
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

build-coverage/btstack_link_key_db_fs_test: ${FS_OBJ_COVERAGE}

build-coverage/btstack_link_key_db_memory_test: ${MEMORY_OBJ_COVERAGE}

build-asan/btstack_link_key_db_fs_test: ${FS_OBJ_ASAN}

build-asan/btstack_link_key_db_memory_test: ${MEMORY_OBJ_ASAN}

test: all
	build-asan/btstack_link_key_db_memory_test
	build-asan/btstack_link_key_db_fs_test

coverage: all
	rm -f build-coverage/*.gcda
	build-coverage/btstack_link_key_db_memory_test
	build-coverage/btstack_link_key_db_fs_test

clean:
	rm -rf build-coverage build-asan

