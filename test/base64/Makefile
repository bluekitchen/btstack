# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..

# CppuTest from pkg-config
CFLAGS  += ${shell pkg-config --cflags CppuTest}
LDFLAGS += ${shell pkg-config --libs   CppuTest}

CFLAGS += -DUNIT_TEST -x c++ -g -Wall -Wnarrowing -Wconversion-null
CFLAGS += -I${BTSTACK_ROOT}/src
CFLAGS += -I..

CFLAGS_COVERAGE = ${CFLAGS} -fprofile-arcs -ftest-coverage
CFLAGS_ASAN     = ${CFLAGS} -fsanitize=address -DHAVE_ASSERT

LDFLAGS += -lCppUTest -lCppUTestExt
LDFLAGS_COVERAGE = ${LDFLAGS} -fprofile-arcs -ftest-coverage
LDFLAGS_ASAN     = ${LDFLAGS} -fsanitize=address

VPATH += ${BTSTACK_ROOT}/src

COMMON = \
    btstack_base64_decoder.c \
    btstack_util.c \
    hci_dump.c \

COMMON_OBJ_COVERAGE = $(addprefix build-coverage/,$(COMMON:.c=.o))
COMMON_OBJ_ASAN     = $(addprefix build-asan/,    $(COMMON:.c=.o))

all: build-coverage/base64_decoder_test build-asan/base64_decoder_test

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

build-coverage/base64_decoder_test: ${COMMON_OBJ_COVERAGE} build-coverage/base64_decoder_test.o | build-coverage
	${CXX} $^ ${LDFLAGS_COVERAGE} -o $@

build-asan/base64_decoder_test: ${COMMON_OBJ_ASAN} build-asan/base64_decoder_test.o | build-asan
	${CXX} $^ ${LDFLAGS_ASAN} -o $@

test: all
	build-asan/base64_decoder_test
	
coverage: all
	rm -f build-coverage/*.gcda
	build-coverage/base64_decoder_test

clean:
	rm -rf build-coverage build-asan
