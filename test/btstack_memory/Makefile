CC = g++

# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..

CFLAGS  = -g -Wall -I. -I${BTSTACK_ROOT}/src -I${BTSTACK_ROOT}/include

CFLAGS_COVERAGE = ${CFLAGS} -fprofile-arcs -ftest-coverage
CFLAGS_ASAN     = ${CFLAGS} -fsanitize=address

LDFLAGS += -lCppUTest -lCppUTestExt
LDFLAGS_COVERAGE = ${LDFLAGS} -fprofile-arcs -ftest-coverage
LDFLAGS_ASAN     = ${LDFLAGS} -fsanitize=address

VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/classic
VPATH += ${BTSTACK_ROOT}/platform/posix

COMMON = \
	btstack_util.c		    \
	hci_dump.c    			\
	btstack_memory_pool.c 	\
	btstack_memory.c 	    \

COMMON_OBJ_COVERAGE = $(addprefix build-coverage/,$(COMMON:.c=.o))
COMMON_OBJ_ASAN     = $(addprefix build-asan/,    $(COMMON:.c=.o))

all: build-coverage/btstack_memory_pool_test build-coverage/btstack_memory_test build-asan/btstack_memory_pool_test build-asan/btstack_memory_test

build-%:
	mkdir -p $@

build-coverage/%.o: %.c | build-coverage
	${CC} -c $(CFLAGS_COVERAGE) $< -o $@

build-asan/%.o: %.c | build-asan
	${CC} -c $(CFLAGS_ASAN) $< -o $@


build-coverage/btstack_memory_pool_test: ${COMMON_OBJ_COVERAGE} build-coverage/btstack_memory_pool_test.o | build-coverage
	${CC} $^ ${LDFLAGS_COVERAGE} -o $@

build-coverage/btstack_memory_test: ${COMMON_OBJ_COVERAGE} build-coverage/btstack_memory_test.o | build-coverage
	${CC} $^ ${LDFLAGS_COVERAGE} -o $@

build-asan/btstack_memory_pool_test: ${COMMON_OBJ_ASAN} build-asan/btstack_memory_pool_test.o | build-asan
	${CC} $^ ${LDFLAGS_ASAN} -o $@

build-asan/btstack_memory_test: ${COMMON_OBJ_ASAN} build-asan/btstack_memory_test.o | build-asan
	${CC} $^ ${LDFLAGS_ASAN} -o $@

test: all
	build-asan/btstack_memory_pool_test
	build-asan/btstack_memory_test

coverage: all
	rm -f build-coverage/*.gcda
	build-coverage/btstack_memory_pool_test
	build-coverage/btstack_memory_test

clean:
	rm -rf build-coverage build-asan
