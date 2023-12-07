# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..

# CppuTest from pkg-config
CFLAGS  += ${shell pkg-config --cflags CppuTest}
LDFLAGS += ${shell pkg-config --libs   CppuTest}

CFLAGS += -DUNIT_TEST -g -Wall -Wnarrowing -Wconversion-null -Ibuild-coverage -I../
CFLAGS += -I${BTSTACK_ROOT}/src
CFLAGS += -I${BTSTACK_ROOT}/src/ble
CFLAGS += -I${BTSTACK_ROOT}/test/mock
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/rijndael
# CFLAGS += -D ENABLE_TESTING_SUPPORT

VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/ble 
VPATH += ${BTSTACK_ROOT}/src/ble/gatt-service
VPATH += ${BTSTACK_ROOT}/platform/posix
VPATH += ${BTSTACK_ROOT}/3rd-party/rijndael
VPATH += ${BTSTACK_ROOT}/test/mock

COMMON = \
	btstack_linked_list.c       \
	btstack_util.c              \
	hci_dump.c                  \
	mock_gatt_client.c 			\

CFLAGS_COVERAGE = ${CFLAGS} -fprofile-arcs -ftest-coverage
CFLAGS_ASAN     = ${CFLAGS} -fsanitize=address -DHAVE_ASSERT

LDFLAGS += -lCppUTest -lCppUTestExt
LDFLAGS_COVERAGE = ${LDFLAGS} -fprofile-arcs -ftest-coverage
LDFLAGS_ASAN     = ${LDFLAGS} -fsanitize=address

COMMON_OBJ_COVERAGE = $(addprefix build-coverage/,$(COMMON:.c=.o))
COMMON_OBJ_ASAN     = $(addprefix build-asan/,    $(COMMON:.c=.o))


all: \
	build-coverage/battery_service_client_test build-asan/battery_service_client_test \
	build-coverage/ancs_client_test build-asan/ancs_client_test \
	build-coverage/device_information_service_client_test build-asan/device_information_service_client_test \

build-%:
	mkdir -p $@

build-%/ancs_client_profile.h: ancs_client_profile.gatt| build-%
	python3 ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@


build-coverage/%.o: %.c | build-coverage
	${CC} -c $(CFLAGS_COVERAGE) $< -o $@

build-coverage/%.o: %.cpp | build-coverage
	${CXX} -c $(CFLAGS_COVERAGE) $< -o $@

build-asan/%.o: %.c | build-asan
	${CC} -c $(CFLAGS_ASAN) $< -o $@

build-asan/%.o: %.cpp | build-asan
	${CXX} -c $(CFLAGS_ASAN) $< -o $@

build-coverage/battery_service_client_test: ${COMMON_OBJ_COVERAGE} build-coverage/battery_service_client_test.o  build-coverage/battery_service_client.o | build-coverage
	${CXX} $^ ${LDFLAGS_COVERAGE} -o $@

build-asan/battery_service_client_test: ${COMMON_OBJ_ASAN} build-asan/battery_service_client_test.o build-asan/battery_service_client.o | build-asan
	${CXX} $^ ${LDFLAGS_ASAN} -o $@

build-coverage/ancs_client_test: ${COMMON_OBJ_COVERAGE} build-coverage/ancs_client_test.o build-coverage/ancs_client.o | build-coverage
	${CXX} $^ ${LDFLAGS_COVERAGE} -o $@

build-asan/ancs_client_test: ${COMMON_OBJ_ASAN} build-asan/ancs_client_test.o build-asan/ancs_client.o | build-asan
	${CXX} $^ ${LDFLAGS_ASAN} -o $@

build-coverage/device_information_service_client_test: ${COMMON_OBJ_COVERAGE} build-coverage/device_information_service_client_test.o build-coverage/device_information_service_client.o | build-coverage
	${CXX} $^ ${LDFLAGS_COVERAGE} -o $@

build-asan/device_information_service_client_test: ${COMMON_OBJ_ASAN} build-asan/device_information_service_client_test.o build-asan/device_information_service_client.o | build-asan
	${CXX} $^ ${LDFLAGS_ASAN} -o $@


test: all
	build-asan/battery_service_client_test
	build-asan/ancs_client_test
	build-asan/device_information_service_client_test
		
coverage: all
	rm -f build-coverage/*.gcda
	build-coverage/battery_service_client_test
	build-coverage/ancs_client_test
	build-coverage/device_information_service_client_test

clean:
	rm -rf build-coverage build-asan

