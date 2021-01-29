CC=g++

# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..
CPPUTEST_HOME = ${BTSTACK_ROOT}/test/cpputest

CFLAGS  = -g -Wall -I. -I../ -I${BTSTACK_ROOT}/src -I${BTSTACK_ROOT}/include

VPATH += ${BTSTACK_ROOT}/src/ble 
VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/platform/posix

COMMON = \
    btstack_linked_list.c \
    hci_dump.c \
    btstack_util.c \

CFLAGS_COVERAGE = ${CFLAGS} -fprofile-arcs -ftest-coverage
CFLAGS_ASAN     = ${CFLAGS} -fsanitize=address -DHAVE_ASSERT

LDFLAGS += -lCppUTest -lCppUTestExt
LDFLAGS_COVERAGE = ${LDFLAGS} -fprofile-arcs -ftest-coverage
LDFLAGS_ASAN     = ${LDFLAGS} -fsanitize=address

COMMON_OBJ_COVERAGE = $(addprefix build-coverage/,$(COMMON:.c=.o))
COMMON_OBJ_ASAN     = $(addprefix build-asan/,    $(COMMON:.c=.o))

all: build-coverage/btstack_linked_list_test build-asan/btstack_linked_list_test

build-%:
	mkdir -p $@

build-coverage/%.o: %.c | build-coverage
	${CC} -c $(CFLAGS_COVERAGE) $< -o $@

build-asan/%.o: %.c | build-asan
	${CC} -c $(CFLAGS_ASAN) $< -o $@

build-coverage/btstack_linked_list_test: ${COMMON_OBJ_COVERAGE} build-coverage/btstack_linked_list_test.o | build-coverage
	${CC} $^ ${LDFLAGS_COVERAGE} -o $@

build-asan/btstack_linked_list_test: ${COMMON_OBJ_ASAN} build-asan/btstack_linked_list_test.o | build-asan
	${CC} $^ ${LDFLAGS_ASAN} -o $@

test: all
	build-asan/btstack_linked_list_test
	
coverage: all
	rm -f build-coverage/*.gcda
	build-coverage/btstack_linked_list_test

clean:
	rm -rf build-coverage build-asan
