CC = g++

# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..

CFLAGS  = -DUNIT_TEST -x c++ -g -Wall -Wnarrowing -Wconversion-null -Ibuild-coverage -I../
CFLAGS += -I${BTSTACK_ROOT}/src
CFLAGS += -I${BTSTACK_ROOT}/test/mock
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/rijndael
CFLAGS += -fprofile-arcs -ftest-coverage
LDFLAGS +=  -lCppUTest -lCppUTestExt

VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/ble 
VPATH += ${BTSTACK_ROOT}/src/ble/gatt-service
VPATH += ${BTSTACK_ROOT}/platform/posix
VPATH += ${BTSTACK_ROOT}/3rd-party/rijndael
VPATH += ${BTSTACK_ROOT}/test/mock

COMMON = \
	ad_parser.c                 \
	att_db.c 				 	    \
	battery_service_server.c \
	btstack_linked_list.c       \
	btstack_memory.c            \
	btstack_memory_pool.c       \
	btstack_util.c              \
	cycling_power_service_server.c \
	cycling_speed_and_cadence_service_server.c \
	device_information_service_server.c \
	hci_dump.c                  \
	heart_rate_service_server.c \
	hids_device.c \
	mock_att_server.c           \
	nordic_spp_service_server.c \
	ublox_spp_service_server.c \


COMMON_OBJ = $(addprefix build-coverage/,$(COMMON:.c=.o))

all: build-coverage/battery_service_test build-coverage/device_information_service_test

build-coverage:
	mkdir -p build-coverage

build-coverage/battery_service_profile.h: battery_service_profile.gatt | build-coverage
	python3 ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@

build-coverage/device_information_service_profile.h: device_information_service.gatt| build-coverage
	python3 ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@

build-coverage/%.o: %.c | build-coverage
	${CC} -c $(CFLAGS) $< -o $@

build-coverage/battery_service_test: build-coverage/battery_service_profile.h ${COMMON_OBJ} build-coverage/battery_service_test.o | build-coverage
	${CC} ${COMMON_OBJ} build-coverage//battery_service_test.o ${CFLAGS} ${LDFLAGS} -o $@

build-coverage/device_information_service_test: build-coverage/device_information_service_profile.h ${COMMON_OBJ} build-coverage/device_information_service_test.o  | build-coverage
	${CC} ${COMMON_OBJ} build-coverage//device_information_service_test.o ${CFLAGS} ${LDFLAGS} -o $@

test: all
	build-coverage/battery_service_test
	build-coverage/device_information_service_test
		
coverage: all
	build-coverage/battery_service_test
	build-coverage/device_information_service_test

clean:
	rm -f  *.o
	rm -rf *.dSYM build-coverage
	rm -f *.gcno *.gcda

