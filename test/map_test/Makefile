# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..
CPPUTEST_HOME = ${BTSTACK_ROOT}/test/cpputest

CFLAGS  = -DUNIT_TEST -x c++ -g -Wall -Wnarrowing -Wconversion-null
CFLAGS += -I${BTSTACK_ROOT}/src
CFLAGS += -I..

LDFLAGS += -lCppUTest -lCppUTestExt

VPATH += ${BTSTACK_ROOT}/src/ble 
VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/platform/posix

VPATH  += ${BTSTACK_ROOT}/3rd-party/yxml
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/yxml

VPATH  += ${BTSTACK_ROOT}/test/map_client
CFLAGS += -I${BTSTACK_ROOT}/test/map_client

COMMON = \
    yxml.c \
    btstack_util.c \
    hci_dump.c \
    map_util.c  \
    btstack_util.c \

CFLAGS_COVERAGE = ${CFLAGS} -fprofile-arcs -ftest-coverage
CFLAGS_ASAN     = ${CFLAGS} -fsanitize=address -DHAVE_ASSERT

LDFLAGS += -lCppUTest -lCppUTestExt
LDFLAGS_COVERAGE = ${LDFLAGS} -fprofile-arcs -ftest-coverage
LDFLAGS_ASAN     = ${LDFLAGS} -fsanitize=address

COMMON_OBJ_COVERAGE = $(addprefix build-coverage/,$(COMMON:.c=.o))
COMMON_OBJ_ASAN     = $(addprefix build-asan/,    $(COMMON:.c=.o))

all: build-coverage/map_xml_test build-asan/map_xml_test


build-%:
	mkdir -p $@

build-coverage/%.o: %.c | build-coverage
	${CC} -c $(CFLAGS_COVERAGE) $< -o $@

build-coverage/%.o: %.cpp | build-coverage
	${CXX} -c $(CFLAGS_COVERAGE) $< -o $@

build-asan/%.o: %.c | build-asan
	${CC} -c $(CFLAGS_ASAN) $< -o $@

build-asan/%.o: %.cpp | build-asan
	${CXX} -c $(CFLAGS_ASAN) $< -o $@


build-coverage/map_xml_test: ${COMMON_OBJ_COVERAGE} build-coverage/map_xml_test.o | build-coverage
	${CXX} $^ ${LDFLAGS_COVERAGE} -o $@

build-asan/map_xml_test: ${COMMON_OBJ_ASAN} build-asan/map_xml_test.o | build-asan
	${CXX} $^ ${LDFLAGS_ASAN} -o $@


test: all
	build-asan/map_xml_test
	
coverage: all
	rm -f build-coverage/*.gcda
	build-coverage/map_xml_test

clean:
	rm -rf build-coverage build-asan
	
