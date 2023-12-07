BTSTACK_ROOT = ../..

# CppuTest from pkg-config
CFLAGS  += ${shell pkg-config --cflags CppuTest}
LDFLAGS += ${shell pkg-config --libs   CppuTest}

COMMON = \
	btstack_tlv_posix.c \
	btstack_util.c \
	btstack_linked_list.c \
	hci_dump.c \
	hci_dump_posix_fs.c \

VPATH = \
	${BTSTACK_ROOT}/src \
	${BTSTACK_ROOT}/src/classic \
	${BTSTACK_ROOT}/src/ble \
	${BTSTACK_ROOT}/platform/posix \


CFLAGS += -DUNIT_TEST -g -Wall -Wnarrowing -Wconversion-null
CFLAGS += -I${BTSTACK_ROOT}/src
CFLAGS += -I${BTSTACK_ROOT}/platform/posix
CFLAGS += -I..

LDFLAGS += -lCppUTest -lCppUTestExt

CFLAGS_COVERAGE = ${CFLAGS} -fprofile-arcs -ftest-coverage
CFLAGS_ASAN     = ${CFLAGS} -fsanitize=address -DHAVE_ASSERT

LDFLAGS += -lCppUTest -lCppUTestExt
LDFLAGS_COVERAGE = ${LDFLAGS} -fprofile-arcs -ftest-coverage
LDFLAGS_ASAN     = ${LDFLAGS} -fsanitize=address

COMMON_OBJ_COVERAGE = $(addprefix build-coverage/,$(COMMON:.c=.o))
COMMON_OBJ_ASAN     = $(addprefix build-asan/,    $(COMMON:.c=.o))

all: build-coverage/tlv_test build-asan/tlv_test

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


build-coverage/tlv_test: ${COMMON_OBJ_COVERAGE} build-coverage/tlv_test.o | build-coverage
	${CXX} $^ ${LDFLAGS_COVERAGE} -o $@

build-asan/tlv_test: ${COMMON_OBJ_ASAN} build-asan/tlv_test.o | build-asan
	${CXX} $^ ${LDFLAGS_ASAN} -o $@


test: all
	build-asan/tlv_test

coverage: all
	rm -f build-coverage/*.gcda
	build-coverage/tlv_test

clean:
	rm -rf build-coverage build-asan
