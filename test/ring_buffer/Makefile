# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..

# CppuTest from pkg-config
CFLAGS  += ${shell pkg-config --cflags CppuTest}
LDFLAGS += ${shell pkg-config --libs   CppuTest}

CFLAGS += -DUNIT_TEST -g -Wall -Wnarrowing -Wconversion-null
CFLAGS += -I${BTSTACK_ROOT}/src
CFLAGS += -I..
LDFLAGS += -lCppUTest -lCppUTestExt

VPATH += ${BTSTACK_ROOT}/src

COMMON = \
    btstack_ring_buffer.c \

CFLAGS_COVERAGE = ${CFLAGS} -fprofile-arcs -ftest-coverage
CFLAGS_ASAN     = ${CFLAGS} -fsanitize=address -DHAVE_ASSERT

LDFLAGS += -lCppUTest -lCppUTestExt
LDFLAGS_COVERAGE = ${LDFLAGS} -fprofile-arcs -ftest-coverage
LDFLAGS_ASAN     = ${LDFLAGS} -fsanitize=address

COMMON_OBJ_COVERAGE = $(addprefix build-coverage/,$(COMMON:.c=.o))
COMMON_OBJ_ASAN     = $(addprefix build-asan/,    $(COMMON:.c=.o))

all: build-coverage/btstack_ring_buffer_test build-asan/btstack_ring_buffer_test

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


build-coverage/btstack_ring_buffer_test: ${COMMON_OBJ_COVERAGE} build-coverage/btstack_ring_buffer_test.o | build-coverage
	${CXX} $^  ${LDFLAGS_COVERAGE} -o $@

build-asan/btstack_ring_buffer_test: ${COMMON_OBJ_ASAN} build-asan/btstack_ring_buffer_test.o | build-asan
	${CXX} $^  ${LDFLAGS_ASAN} -o $@


test: all
	build-asan/btstack_ring_buffer_test
	
coverage: all
	rm -f build-coverage/*.gcda
	build-coverage/btstack_ring_buffer_test

clean:
	rm -rf build-coverage build-asan
	
