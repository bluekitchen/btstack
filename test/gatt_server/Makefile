CC = g++

# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..

CFLAGS  = -DUNIT_TEST -x c++ -g -Wall -Wnarrowing -Wconversion-null -I. -I../ 
CFLAGS += -I${BTSTACK_ROOT}/src
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/rijndael
CFLAGS += -fprofile-arcs -ftest-coverage
LDFLAGS +=  -lCppUTest -lCppUTestExt

VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/ble 
VPATH += ${BTSTACK_ROOT}/src/ble/gatt-service
VPATH += ${BTSTACK_ROOT}/platform/posix
VPATH += ${BTSTACK_ROOT}/3rd-party/rijndael

COMMON = \
	ad_parser.c                 \
	att_db.c                    \
	att_db_util.c 				\
	att_dispatch.c              \
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
	nordic_spp_service_server.c \
	rijndael.c 					\
	ublox_spp_service_server.c \

COMMON_OBJ = $(addprefix build-coverage/,$(COMMON:.c=.o))

all: build-coverage/gatt_server_test

build-coverage:
	mkdir -p build-coverage

# compile .ble description
build-coverage/profile.h: profile.gatt | build-coverage
	python3 ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@

build-coverage/%.o: %.c | build-coverage
	${CC} -c $(CFLAGS) $< -o $@

build-coverage/gatt_server_test: build-coverage/profile.h ${COMMON_OBJ} build-coverage/gatt_server_test.o | build-coverage
	${CC} ${COMMON_OBJ} build-coverage/gatt_server_test.o ${CFLAGS} ${LDFLAGS} -o $@

test: all
	build-coverage/gatt_server_test
		
coverage: all
	build-coverage/gatt_server_test

clean:
	rm -f  *.o
	rm -rf *.dSYM build-coverage
	rm -f *.gcno *.gcda

