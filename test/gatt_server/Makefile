CC = g++

# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..

CFLAGS  = -DUNIT_TEST -x c++ -g -Wall -Wnarrowing -Wconversion-null -I. -I../ -I${BTSTACK_ROOT}/src
CFLAGS += -fprofile-arcs -ftest-coverage -fsanitize=address,undefined
LDFLAGS +=  -lCppUTest -lCppUTestExt

VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/ble 
VPATH += ${BTSTACK_ROOT}/src/ble/gatt-service
VPATH += ${BTSTACK_ROOT}/platform/posix

COMMON = \
	ad_parser.c                 \
	att_server.c                \
	att_db.c                    \
	att_dispatch.c              \
	btstack_linked_list.c       \
	btstack_memory.c            \
	btstack_memory_pool.c       \
	btstack_util.c              \
	hci_cmd.c                   \
	hci_dump.c                  \
	le_device_db_memory.c       \
	mock.c                      \
	btstack_tlv.c               \
	battery_service_server.c \
	cycling_power_service_server.c \
	cycling_speed_and_cadence_service_server.c \
	device_information_service_server.c \
	heart_rate_service_server.c \
	hids_device.c \
	nordic_spp_service_server.c \
	ublox_spp_service_server.c \

COMMON_OBJ = $(COMMON:.c=.o)

all: gatt_server_test

# compile .ble description
profile.h: profile.gatt
	python ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@ 

gatt_server_test: profile.h ${COMMON_OBJ} gatt_server_test.o
	${CC} ${COMMON_OBJ} gatt_server_test.o ${CFLAGS} ${LDFLAGS} -o $@

test: all
	./gatt_server_test
		
clean:
	rm -f  gatt_server_test
	rm -f  *.o
	rm -rf *.dSYM
	rm -f *.gcno *.gcda

