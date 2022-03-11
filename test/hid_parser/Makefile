# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..

CFLAGS  = -DUNIT_TEST -x c++ -g -Wall -Wnarrowing -Wconversion-null

CFLAGS += -I.
CFLAGS += -I ${BTSTACK_ROOT}/src
CFLAGS += -I ${BTSTACK_ROOT}/platform/posix

VPATH += ${BTSTACK_ROOT}/src ${BTSTACK_ROOT}/platform/posix

COMMON = \
	btstack_util.c \
	btstack_hid_parser.c \
	hci_dump.c \
	hci_dump_posix_fs.c \
	
CFLAGS_COVERAGE = ${CFLAGS} -fprofile-arcs -ftest-coverage
CFLAGS_ASAN     = ${CFLAGS} -fsanitize=address -DHAVE_ASSERT

LDFLAGS += -lCppUTest -lCppUTestExt
LDFLAGS_COVERAGE = ${LDFLAGS} -fprofile-arcs -ftest-coverage
LDFLAGS_ASAN     = ${LDFLAGS} -fsanitize=address

COMMON_OBJ_COVERAGE = $(addprefix build-coverage/,$(COMMON:.c=.o))
COMMON_OBJ_ASAN     = $(addprefix build-asan/,    $(COMMON:.c=.o))

all: build-coverage/hid_parser_test build-asan/hid_parser_test

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

build-coverage/hid_parser_test: ${COMMON_OBJ_COVERAGE} build-coverage/hid_parser_test.o | build-coverage
	${CXX} $^ ${LDFLAGS_COVERAGE} -o $@

build-asan/hid_parser_test: ${COMMON_OBJ_ASAN} build-asan/hid_parser_test.o | build-asan
	${CXX} $^ ${LDFLAGS_ASAN} -o $@


test: all
	build-asan/hid_parser_test
	
coverage: all
	rm -f build-coverage/*.gcda
	build-coverage/hid_parser_test

clean:
	rm -rf build-coverage build-asan

