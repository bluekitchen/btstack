
include ../common.make

DEFINES := -DFUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
INCLUDES += -I${BTSTACK_ROOT}/src
INCLUDES += -I${BTSTACK_ROOT}/platform/posix
INCLUDES += -I${BTSTACK_ROOT}/test/include/coverage-ble

CFLAGS += ${DEFINES} ${INCLUDES}
CXXFLAGS += ${DEFINES} ${INCLUDES}

VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/ble
VPATH += ${BTSTACK_ROOT}/platform/posix

COMMON = \
	ad_parser.c                 \
	btstack_linked_list.c       \
	btstack_memory.c            \
	btstack_memory_pool.c       \
	btstack_util.c              \
	btstack_run_loop.c          \
	btstack_run_loop_posix.c    \
	hci.c                       \
	hci_cmd.c                   \
	hci_dump.c                  \
	hci_dump_posix_fs.c         \
	le_device_db_memory.c

COMMON_OBJ_COVERAGE = $(addprefix build-coverage/,$(COMMON:.c=.o))
COMMON_OBJ_ASAN     = $(addprefix build-asan/,    $(COMMON:.c=.o))

all: build-coverage/test_le_scan build-asan/test_le_scan build-coverage/hci_test build-asan/hci_test

build-coverage/test_le_scan: ${COMMON_OBJ_COVERAGE}

build-asan/test_le_scan: ${COMMON_OBJ_ASAN}

build-coverage/hci_test: ${COMMON_OBJ_COVERAGE}

build-asan/hci_test: ${COMMON_OBJ_ASAN}

test: all
	build-asan/test_le_scan
	build-asan/hci_test

coverage: all
	rm -f build-coverage/*.gcda
	build-coverage/test_le_scan
	build-coverage/hci_test

clean:
	rm -rf build-coverage build-asan

