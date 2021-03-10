CC = g++

# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..

CFLAGS  = -DUNIT_TEST -x c++ -g -Wall -Wnarrowing -Wconversion-null -I. -I${BTSTACK_ROOT}/src  -I${BTSTACK_ROOT}/platform/posix
CFLAGS += -DFUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION

VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/ble 
VPATH += ${BTSTACK_ROOT}/platform/posix

COMMON = \
	ad_parser.c                 \
	btstack_linked_list.c       \
	btstack_memory.c            \
	btstack_memory_pool.c       \
	btstack_util.c              \
	btstack_run_loop.c           \
	hci.c                       \
	hci_cmd.c                   \
	hci_dump.c                  \
	hci_dump_posix_fs.c         \
	le_device_db_memory.c       \

CFLAGS_COVERAGE = ${CFLAGS} -fprofile-arcs -ftest-coverage
CFLAGS_ASAN     = ${CFLAGS} -fsanitize=address -DHAVE_ASSERT

LDFLAGS += -lCppUTest -lCppUTestExt
LDFLAGS_COVERAGE = ${LDFLAGS} -fprofile-arcs -ftest-coverage
LDFLAGS_ASAN     = ${LDFLAGS} -fsanitize=address

COMMON_OBJ_COVERAGE = $(addprefix build-coverage/,$(COMMON:.c=.o))
COMMON_OBJ_ASAN     = $(addprefix build-asan/,    $(COMMON:.c=.o))

all: build-coverage/test_le_scan build-asan/test_le_scan

build-%:
	mkdir -p $@

# compile .ble description
build-%/profile.h: profile.gatt | build-%
	python3 ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@ 

build-coverage/%.o: %.c | build-coverage
	${CC} -c $(CFLAGS_COVERAGE) $< -o $@

build-asan/%.o: %.c | build-asan
	${CC} -c $(CFLAGS_ASAN) $< -o $@

build-coverage/test_le_scan: ${COMMON_OBJ_COVERAGE} build-coverage/test_le_scan.o | build-coverage
	${CC} $^ ${LDFLAGS_COVERAGE} -o $@

build-asan/test_le_scan: ${COMMON_OBJ_ASAN} build-asan/test_le_scan.o | build-asan
	${CC} $^ ${LDFLAGS_ASAN} -o $@

test: all
	build-asan/test_le_scan

coverage: all
	rm -f build-coverage/*.gcda
	build-coverage/test_le_scan

clean:
	rm -rf build-coverage build-asan

