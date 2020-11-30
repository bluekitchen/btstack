CC = g++

# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..

CFLAGS  = -DUNIT_TEST -x c++ -g -Wall -Wnarrowing -Wconversion-null -I. -Ibuild-coverage -I${BTSTACK_ROOT}/src
CFLAGS += -fprofile-arcs -ftest-coverage
LDFLAGS +=  -lCppUTest -lCppUTestExt

VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/ble 
VPATH += ${BTSTACK_ROOT}/platform/posix

COMMON = \
	ad_parser.c                 \
	ancs_client.c               \
	att_db.c                    \
	att_dispatch.c              \
	btstack_linked_list.c       \
	btstack_memory.c            \
	btstack_memory_pool.c       \
	btstack_util.c              \
	gatt_client.c               \
	hci_cmd.c                   \
	hci_dump.c                  \
	le_device_db_memory.c       \
	mock.c                      \

COMMON_OBJ = $(addprefix build-coverage/,$(COMMON:.c=.o))

all: build-coverage/gatt_client_test build-coverage/le_central

build-coverage:
	mkdir -p build-coverage

# compile .ble description
build-coverage/profile.h: profile.gatt | build-coverage
	python3 ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@ 

build-coverage/%.o: %.c | build-coverage
	${CC} -c $(CFLAGS) $< -o $@

build-coverage/gatt_client_test: profile.h ${COMMON_OBJ} build-coverage/gatt_client_test.o expected_results.h | build-coverage
	${CC} ${COMMON_OBJ} build-coverage/gatt_client_test.o ${CFLAGS} ${LDFLAGS} -o $@

build-coverage/le_central: ${COMMON_OBJ} build-coverage/le_central.o | build-coverage
	${CC} ${COMMON_OBJ} build-coverage/le_central.o ${CFLAGS} ${LDFLAGS} -o $@

test: all
	build-coverage/gatt_client_test
	build-coverage/le_central
		
coverage: all
	build-coverage/gatt_client_test
	build-coverage/le_central

clean:
	rm -f  *.o
	rm -rf *.dSYM build-coverage
	rm -f *.gcno *.gcda

