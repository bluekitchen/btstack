CC = g++

# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..

CFLAGS  = -DUNIT_TEST -x c++ -g -Wall -Wnarrowing -Wconversion-null -I. -Ibuild-coverage -I${BTSTACK_ROOT}/src

VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/ble 
VPATH += ${BTSTACK_ROOT}/src/ble/gatt-service 
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

CFLAGS_COVERAGE = ${CFLAGS} -fprofile-arcs -ftest-coverage
CFLAGS_ASAN     = ${CFLAGS} -fsanitize=address -DHAVE_ASSERT

LDFLAGS += -lCppUTest -lCppUTestExt
LDFLAGS_COVERAGE = ${LDFLAGS} -fprofile-arcs -ftest-coverage
LDFLAGS_ASAN     = ${LDFLAGS} -fsanitize=address

COMMON_OBJ_COVERAGE = $(addprefix build-coverage/,$(COMMON:.c=.o))
COMMON_OBJ_ASAN     = $(addprefix build-asan/,    $(COMMON:.c=.o))

all: build-coverage/gatt_client_test build-coverage/le_central build-asan/gatt_client_test build-asan/le_central

build-%:
	mkdir -p $@

# compile .ble description
build-%/profile.h: profile.gatt | build-%
	python3 ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@ 

build-coverage/%.o: %.c | build-coverage
	${CC} -c $(CFLAGS_COVERAGE) $< -o $@

build-asan/%.o: %.c | build-asan
	${CC} -c $(CFLAGS_ASAN) $< -o $@

build-coverage/gatt_client_test: ${COMMON_OBJ_COVERAGE} build-coverage/profile.h build-coverage/gatt_client_test.o expected_results.h | build-coverage
	${CC} $(filter-out build-coverage/profile.h expected_results.h,$^) ${LDFLAGS_COVERAGE} -o $@

build-coverage/le_central: ${COMMON_OBJ_COVERAGE} build-coverage/le_central.o | build-coverage
	${CC} $^ ${LDFLAGS_COVERAGE} -o $@

build-asan/gatt_client_test: ${COMMON_OBJ_ASAN} build-asan/profile.h  build-asan/gatt_client_test.o expected_results.h | build-asan
	${CC} $(filter-out build-asan/profile.h expected_results.h,$^) ${LDFLAGS_ASAN} -o $@

build-asan/le_central: ${COMMON_OBJ_ASAN} build-asan/le_central.o | build-asan
	${CC} $^ ${LDFLAGS_ASAN} -o $@

test: all
	build-asan/gatt_client_test
	build-asan/le_central
		
coverage: all
	rm -f build-coverage/*.gcda
	build-coverage/gatt_client_test
	build-coverage/le_central

clean:
	rm -rf build-coverage build-asan

