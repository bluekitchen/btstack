CC = g++

# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..

CFLAGS  = -g -Wall -I.. -I${BTSTACK_ROOT}/src -I${BTSTACK_ROOT}/include

CFLAGS_COVERAGE = ${CFLAGS} -fprofile-arcs -ftest-coverage
CFLAGS_ASAN     = ${CFLAGS} -fsanitize=address -DHAVE_ASSERT

LDFLAGS += -lCppUTest -lCppUTestExt
LDFLAGS_COVERAGE = ${LDFLAGS} -fprofile-arcs -ftest-coverage
LDFLAGS_ASAN     = ${LDFLAGS} -fsanitize=address

VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/classic
VPATH += ${BTSTACK_ROOT}/platform/posix

COMMON = \
    sdp_util.c	              \
    hci_dump.c    \
	btstack_util.c			          
 
COMMON_OBJ_COVERAGE = $(addprefix build-coverage/,$(COMMON:.c=.o))
COMMON_OBJ_ASAN     = $(addprefix build-asan/,    $(COMMON:.c=.o))

all: build-coverage/des_iterator_test build-asan/des_iterator_test

build-%:
	mkdir -p $@

build-coverage/%.o: %.c | build-coverage
	${CC} -c $(CFLAGS_COVERAGE) $< -o $@

build-asan/%.o: %.c | build-asan
	${CC} -c $(CFLAGS_ASAN) $< -o $@


build-coverage/des_iterator_test: ${COMMON_OBJ_COVERAGE} build-coverage/des_iterator_test.o  | build-coverage
	${CC} $^ ${LDFLAGS_COVERAGE} -o $@

build-asan/des_iterator_test: ${COMMON_OBJ_ASAN} build-asan/des_iterator_test.o  | build-asan
	${CC} $^ ${LDFLAGS_ASAN} -o $@


test: all
	build-asan/des_iterator_test

coverage: all
	rm -f build-coverage/*.gcda
	build-coverage/des_iterator_test

clean:
	rm -rf build-coverage build-asan

