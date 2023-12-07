# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..

# CppuTest from pkg-config
CFLAGS  += ${shell pkg-config --cflags CppuTest}
LDFLAGS += ${shell pkg-config --libs   CppuTest}

CFLAGS += -DUNIT_TEST -x c++ -g -Wall -Wnarrowing -Wconversion-null 
CFLAGS += -I../

CFLAGS += -I${BTSTACK_ROOT}/src
CFLAGS += -I${BTSTACK_ROOT}/test/mock
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/rijndael

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
	cycling_power_service_server.c 				\
	cycling_speed_and_cadence_service_server.c 	\
	device_information_service_server.c 		\
	hci_dump.c                  				\
	heart_rate_service_server.c 				\
	hids_device.c 								\
	mock_att_server.c           				\
	nordic_spp_service_server.c 				\
	tx_power_service_server.c 					\
	ublox_spp_service_server.c 					\


CFLAGS_COVERAGE = ${CFLAGS} -Ibuild-coverage -fprofile-arcs -ftest-coverage 
CFLAGS_ASAN     = ${CFLAGS} -Ibuild-asan     -fsanitize=address -DHAVE_ASSERT

LDFLAGS += -lCppUTest -lCppUTestExt
LDFLAGS_COVERAGE = ${LDFLAGS} -fprofile-arcs -ftest-coverage
LDFLAGS_ASAN     = ${LDFLAGS} -fsanitize=address

COMMON_OBJ_COVERAGE = $(addprefix build-coverage/,$(COMMON:.c=.o))
COMMON_OBJ_ASAN     = $(addprefix build-asan/,    $(COMMON:.c=.o))


all: build-coverage/battery_service_server_test 			build-asan/battery_service_server_test 				\
	 build-coverage/device_information_service_server_test 	build-asan/device_information_service_server_test 	\
	 build-coverage/tx_power_service_server_test 			build-asan/tx_power_service_server_test 			\
	 build-coverage/bond_management_service_server_test 	build-asan/bond_management_service_server_test 		\
	 build-coverage/heart_rate_service_server_test 			build-asan/heart_rate_service_server_test 			\

build-%:
	mkdir -p $@

build-%/battery_service_server_test.h: battery_service_server_test.gatt | build-%
	python3 ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@

build-%/device_information_service_server_test.h: device_information_service_server_test.gatt| build-%
	python3 ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@

build-%/tx_power_service_server_test.h: tx_power_service_server_test.gatt| build-%
	python3 ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@

build-%/bond_management_service_server_test.h: bond_management_service_server_test.gatt| build-%
	python3 ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@

build-%/heart_rate_service_server_test.h: heart_rate_service_server_test.gatt| build-%
	python3 ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@

build-coverage/%.o: %.c | build-coverage
	${CC} -c $(CFLAGS_COVERAGE) $< -o $@

build-coverage/%.o: %.cpp | build-coverage
	${CXX} -c $(CFLAGS_COVERAGE) $< -o $@

build-asan/%.o: %.c | build-asan
	${CC} -c $(CFLAGS_ASAN) $< -o $@

build-asan/%.o: %.cpp | build-asan
	${CXX} -c $(CFLAGS_ASAN) $< -o $@

build-coverage/battery_service_server_test: build-coverage/battery_service_server_test.h ${COMMON_OBJ_COVERAGE} build-coverage/battery_service_server_test.o | build-coverage
	${CXX} $(filter-out build-coverage/battery_service_server_test.h,$^) ${LDFLAGS_COVERAGE} -o $@

build-asan/battery_service_server_test: build-asan/battery_service_server_test.h ${COMMON_OBJ_ASAN} build-asan/battery_service_server_test.o | build-asan
	${CXX} $(filter-out build-asan/battery_service_server_test.h,$^) ${LDFLAGS_ASAN} -o $@

build-coverage/device_information_service_server_test: build-coverage/device_information_service_server_test.h ${COMMON_OBJ_COVERAGE} build-coverage/device_information_service_server_test.o  | build-coverage
	${CXX} $(filter-out build-coverage/device_information_service_server_test.h,$^) ${LDFLAGS_COVERAGE} -o $@

build-asan/device_information_service_server_test: build-asan/device_information_service_server_test.h ${COMMON_OBJ_ASAN} build-asan/device_information_service_server_test.o  | build-asan
	${CXX} $(filter-out build-asan/device_information_service_server_test.h,$^) ${LDFLAGS_ASAN} -o $@

build-coverage/tx_power_service_server_test: build-coverage/tx_power_service_server_test.h ${COMMON_OBJ_COVERAGE} build-coverage/tx_power_service_server_test.o  | build-coverage
	${CXX} $(filter-out build-coverage/tx_power_service_server_test.h,$^) ${LDFLAGS_COVERAGE} -o $@

build-asan/tx_power_service_server_test: build-asan/tx_power_service_server_test.h ${COMMON_OBJ_ASAN} build-asan/tx_power_service_server_test.o  | build-asan
	${CXX} $(filter-out build-asan/tx_power_service_server_test.h,$^) ${LDFLAGS_ASAN} -o $@

build-coverage/bond_management_service_server_test: build-coverage/bond_management_service_server_test.h ${COMMON_OBJ_COVERAGE} build-coverage/bond_management_service_server.o build-coverage/bond_management_service_server_test.o  | build-coverage
	${CXX} $(filter-out build-coverage/bond_management_service_server_test.h,$^) ${LDFLAGS_COVERAGE} -o $@

build-asan/bond_management_service_server_test: build-asan/bond_management_service_server_test.h ${COMMON_OBJ_ASAN} build-asan/bond_management_service_server.o  build-asan/bond_management_service_server_test.o  | build-asan
	${CXX} $(filter-out build-asan/bond_management_service_server_test.h,$^) ${LDFLAGS_ASAN} -o $@

build-coverage/heart_rate_service_server_test: build-coverage/heart_rate_service_server_test.h ${COMMON_OBJ_COVERAGE} build-coverage/heart_rate_service_server.o build-coverage/heart_rate_service_server_test.o  | build-coverage
	${CXX} $(filter-out build-coverage/heart_rate_service_server_test.h,$^) ${LDFLAGS_COVERAGE} -o $@

build-asan/heart_rate_service_server_test: build-asan/heart_rate_service_server_test.h ${COMMON_OBJ_ASAN} build-asan/heart_rate_service_server.o  build-asan/heart_rate_service_server_test.o  | build-asan
	${CXX} $(filter-out build-asan/heart_rate_service_server_test.h,$^) ${LDFLAGS_ASAN} -o $@

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

