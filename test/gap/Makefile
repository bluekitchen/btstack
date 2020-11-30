CC = g++

# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..

CFLAGS  = -DUNIT_TEST -x c++ -g -Wall -Wnarrowing -Wconversion-null -I. -I${BTSTACK_ROOT}/src
CFLAGS += -DFUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
CFLAGS += -fprofile-arcs -ftest-coverage
LDFLAGS +=  -lCppUTest -lCppUTestExt

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
	le_device_db_memory.c       \

COMMON_OBJ = $(addprefix build-coverage/,$(COMMON:.c=.o))

all: build-coverage/test_le_scan

build-coverage:
	mkdir -p build-coverage

# compile .ble description
build-coverage/profile.h: profile.gatt | build-coverage
	python3 ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@ 

build-coverage/%.o: %.c | build-coverage
	${CC} -c $(CFLAGS) $< -o $@

build-coverage/test_le_scan: ${COMMON_OBJ} test_le_scan.o | build-coverage
	${CC} ${COMMON_OBJ} test_le_scan.o ${CFLAGS} ${LDFLAGS} -o $@

test: all
	build-coverage/test_le_scan

coverage: all
	build-coverage/test_le_scan

clean:
	rm -f  test_le_scan
	rm -f  *.o
	rm -rf *.dSYM build-coverage
	rm -f *.gcno *.gcda

