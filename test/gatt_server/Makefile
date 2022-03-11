# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..

CFLAGS  = -DUNIT_TEST -g -Wall -Wnarrowing -Wconversion-null
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/rijndael
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/micro-ecc
CFLAGS += -I${BTSTACK_ROOT}/platform/embedded
CFLAGS += -I${BTSTACK_ROOT}/platform/posix
CFLAGS += -I${BTSTACK_ROOT}/src
CFLAGS += -I${BTSTACK_ROOT}/test/mock
CFLAGS += -I.

VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/ble 
VPATH += ${BTSTACK_ROOT}/src/ble/gatt-service
VPATH += ${BTSTACK_ROOT}/platform/posix
VPATH += ${BTSTACK_ROOT}/3rd-party/micro-ecc
VPATH += ${BTSTACK_ROOT}/3rd-party/rijndael
VPATH += ${BTSTACK_ROOT}/test/mock

COMMON = \
	ad_parser.c                 \
	att_db.c                    \
	att_db_util.c 				\
	att_server.c                \
	battery_service_server.c \
	btstack_crypto.c            \
	btstack_linked_list.c       \
	btstack_memory.c            \
	btstack_memory_pool.c       \
	btstack_tlv.c               \
	btstack_util.c              \
	cycling_power_service_server.c \
	cycling_speed_and_cadence_service_server.c \
	device_information_service_server.c \
	hci_cmd.c                   \
	hci_dump.c                  \
	heart_rate_service_server.c \
	hids_device.c \
	le_device_db_memory.c       \
	mock.c                      \
	mock_btstack_tlv.c          \
	nordic_spp_service_server.c \
	rijndael.c 					\
	ublox_spp_service_server.c \

CFLAGS_COVERAGE = ${CFLAGS} -fprofile-arcs -ftest-coverage
CFLAGS_ASAN     = ${CFLAGS} -fsanitize=address -DHAVE_ASSERT

LDFLAGS += -lCppUTest -lCppUTestExt
LDFLAGS_COVERAGE = ${LDFLAGS} -fprofile-arcs -ftest-coverage
LDFLAGS_ASAN     = ${LDFLAGS} -fsanitize=address

COMMON_OBJ_COVERAGE = $(addprefix build-coverage/,$(COMMON:.c=.o)) build-coverage/uECC.o
COMMON_OBJ_ASAN     = $(addprefix build-asan/,    $(COMMON:.c=.o)) build-asan/uECC.o


all: build-coverage/gatt_server_test build-asan/gatt_server_test

build-%:
	mkdir -p $@

# compile .ble description
build-%/profile.h: profile.gatt | build-%
	python3 ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@

build-coverage/%.o: %.c | build-coverage
	${CC} -c $(CFLAGS_COVERAGE) $< -o $@

build-coverage/%.o: %.cpp | build-coverage
	${CXX} -c $(CFLAGS_COVERAGE) $< -o $@

build-asan/%.o: %.c | build-asan
	${CC} -c $(CFLAGS_ASAN) $< -o $@

build-asan/%.o: %.cpp | build-asan
	${CXX} -c $(CFLAGS_ASAN) $< -o $@

build-coverage/gatt_server_test: ${COMMON_OBJ_COVERAGE} build-coverage/profile.h build-coverage/gatt_server_test.o | build-coverage
	${CXX} $(filter-out build-coverage/profile.h,$^) ${LDFLAGS_COVERAGE} -o $@

build-asan/gatt_server_test: ${COMMON_OBJ_ASAN} build-asan/profile.h build-asan/gatt_server_test.o | build-asan
	${CXX} $(filter-out build-asan/profile.h,$^) ${LDFLAGS_ASAN} -o $@

test: all
	build-asan/gatt_server_test
		
coverage: all
	rm -f build-coverage/*.gcda
	build-coverage/gatt_server_test

clean:
	rm -rf build-coverage build-asan

