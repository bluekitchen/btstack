
include ../common.make

INCLUDES := -I${BTSTACK_ROOT}/src
INCLUDES += -I${BTSTACK_ROOT}/test/mock
INCLUDES += -I${BTSTACK_ROOT}/3rd-party/rijndael
INCLUDES += -I${BTSTACK_ROOT}/test/include/coverage-ble

CFLAGS += ${INCLUDES}
CXXFLAGS += ${INCLUDES}

VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/ble
VPATH += ${BTSTACK_ROOT}/src/ble/gatt-service
VPATH += ${BTSTACK_ROOT}/src/le-audio/gatt-service
VPATH += ${BTSTACK_ROOT}/platform/posix
VPATH += ${BTSTACK_ROOT}/3rd-party/rijndael
VPATH += ${BTSTACK_ROOT}/test/mock

COMMON = 										\
	ad_parser.c                 				\
	att_db.c 				 	    			\
	btstack_hid_parser.c						\
	battery_service_server.c 					\
	btstack_linked_list.c       				\
	btstack_memory.c            				\
	btstack_memory_pool.c       				\
	btstack_util.c              				\
	cycling_speed_and_cadence_service_server.c 	\
	device_information_service_server.c 		\
	hci_dump.c                  				\
	heart_rate_service_server.c 				\
	hids_device.c 								\
	mock_att_server.c           				\
	nordic_spp_service_server.c 				\
	tx_power_service_server.c 					\
	ublox_spp_service_server.c

COMMON_OBJ_COVERAGE = $(addprefix build-coverage/,$(COMMON:.c=.o))
COMMON_OBJ_ASAN     = $(addprefix build-asan/,    $(COMMON:.c=.o))

all: build-coverage/battery_service_server_test 			build-asan/battery_service_server_test 				\
	 build-coverage/device_information_service_server_test 	build-asan/device_information_service_server_test 	\
	 build-coverage/tx_power_service_server_test 			build-asan/tx_power_service_server_test 			\
	 build-coverage/bond_management_service_server_test 	build-asan/bond_management_service_server_test 		\
	 build-coverage/heart_rate_service_server_test 			build-asan/heart_rate_service_server_test 			\

build-coverage/battery_service_server_test.o: build-coverage/battery_service_server_test.h
build-coverage/battery_service_server_test: ${COMMON_OBJ_COVERAGE}

build-asan/battery_service_server_test.o: build-asan/battery_service_server_test.h
build-asan/battery_service_server_test: ${COMMON_OBJ_ASAN}

build-coverage/device_information_service_server_test.o: build-coverage/device_information_service_server_test.h
build-coverage/device_information_service_server_test: ${COMMON_OBJ_COVERAGE}

build-asan/device_information_service_server_test.o: build-asan/device_information_service_server_test.h
build-asan/device_information_service_server_test: ${COMMON_OBJ_ASAN}

build-coverage/tx_power_service_server_test.o: build-coverage/tx_power_service_server_test.h 
build-coverage/tx_power_service_server_test: ${COMMON_OBJ_COVERAGE}

build-asan/tx_power_service_server_test.o: build-asan/tx_power_service_server_test.h 
build-asan/tx_power_service_server_test: ${COMMON_OBJ_ASAN}

build-coverage/bond_management_service_server_test.o: build-coverage/bond_management_service_server_test.h 
build-coverage/bond_management_service_server_test: ${COMMON_OBJ_COVERAGE} build-coverage/bond_management_service_server.o

build-asan/bond_management_service_server_test.o: build-asan/bond_management_service_server_test.h 
build-asan/bond_management_service_server_test: ${COMMON_OBJ_ASAN} build-asan/bond_management_service_server.o

build-coverage/microphone_control_service_server_test.o: build-coverage/microphone_control_service_server_test.h 
build-coverage/microphone_control_service_server_test: ${COMMON_OBJ_COVERAGE}

build-asan/microphone_control_service_server_test.o: build-asan/microphone_control_service_server_test.h 
build-asan/microphone_control_service_server_test: ${COMMON_OBJ_ASAN}

build-coverage/heart_rate_service_server_test.o: build-coverage/heart_rate_service_server_test.h 
build-coverage/heart_rate_service_server_test: ${COMMON_OBJ_COVERAGE} build-coverage/heart_rate_service_server.o

build-asan/heart_rate_service_server_test.o: build-asan/heart_rate_service_server_test.h 
build-asan/heart_rate_service_server_test: ${COMMON_OBJ_ASAN} build-asan/heart_rate_service_server.o

test: all
	build-asan/battery_service_server_test
	build-asan/device_information_service_server_test
	build-asan/tx_power_service_server_test
	build-asan/bond_management_service_server_test
	build-asan/heart_rate_service_server_test
		
coverage: all
	rm -f build-coverage/*.gcda
	build-coverage/battery_service_server_test
	build-coverage/device_information_service_server_test
	build-coverage/tx_power_service_server_test
	build-coverage/bond_management_service_server_test
	build-coverage/heart_rate_service_server_test

clean:
	rm -rf build-coverage build-asan

