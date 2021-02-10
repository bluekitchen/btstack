CC = g++

# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..

CFLAGS  = -g -Wall -I${BTSTACK_ROOT}/src

CFLAGS_COVERAGE = ${CFLAGS} -fprofile-arcs -ftest-coverage
CFLAGS_ASAN     = ${CFLAGS} -fsanitize=address -DHAVE_ASSERT

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

COMMON_OBJ_COVERAGE = $(addprefix build-coverage/,$(COMMON:.c=.o))
COMMON_OBJ_ASAN     = $(addprefix build-asan/,    $(COMMON:.c=.o))

all: build-coverage/btstack_memory_pool_test \
	 build-coverage-none/btstack_memory_test \
	 build-coverage-single/btstack_memory_test \
	 build-coverage-malloc/btstack_memory_test \
	 build-asan/btstack_memory_pool_test \
	 build-asan/btstack_memory_test

build-%:
	mkdir -p $@


build-coverage/%.o: %.c | build-coverage
	${CC} -c $(CFLAGS_COVERAGE) -I config_single $< -o $@

build-coverage-none/%.o: %.c | build-coverage-none
	${CC} -c $(CFLAGS_COVERAGE) -I config_none $< -o $@

build-coverage-single/%.o: %.c | build-coverage-single
	${CC} -c $(CFLAGS_COVERAGE) -I config_single $< -o $@

build-coverage-malloc/%.o: %.c | build-coverage-malloc
	${CC} -c $(CFLAGS_COVERAGE) -I config_malloc $< -o $@

build-asan/%.o: %.c | build-asan
	${CC} -c $(CFLAGS_ASAN) $< -I config_single -o $@


build-coverage/btstack_memory_pool_test: ${COMMON_OBJ_COVERAGE} build-coverage/btstack_memory.o build-coverage/btstack_memory_pool_test.o | build-coverage
	${CC} $^ ${LDFLAGS_COVERAGE} -o $@

build-coverage-none/btstack_memory_test: ${COMMON_OBJ_COVERAGE} build-coverage-none/btstack_memory.o build-coverage-none/btstack_memory_test.o | build-coverage-none
	${CC} $^ ${LDFLAGS_COVERAGE} -o $@

build-coverage-single/btstack_memory_test: ${COMMON_OBJ_COVERAGE} build-coverage-single/btstack_memory.o build-coverage-single/btstack_memory_test.o | build-coverage-single
	${CC} $^ ${LDFLAGS_COVERAGE} -o $@

build-coverage-malloc/btstack_memory_test: ${COMMON_OBJ_COVERAGE} build-coverage-malloc/btstack_memory.o build-coverage-malloc/btstack_memory_test.o | build-coverage-malloc
	${CC} $^ ${LDFLAGS_COVERAGE} -o $@

build-asan/btstack_memory_pool_test: ${COMMON_OBJ_ASAN} build-asan/btstack_memory.o build-asan/btstack_memory_pool_test.o | build-asan
	${CC} $^ ${LDFLAGS_ASAN} -o $@

build-asan/btstack_memory_test: ${COMMON_OBJ_ASAN} build-asan/btstack_memory.o build-asan/btstack_memory_test.o | build-asan
	${CC} $^ ${LDFLAGS_ASAN} -o $@


test: all
	build-asan/btstack_memory_pool_test
	build-asan/btstack_memory_test

coverage: all
	rm -f build-coverage/*.gcda
	build-coverage/btstack_memory_pool_test
	build-coverage-none/btstack_memory_test
	build-coverage-single/btstack_memory_test
	build-coverage-malloc/btstack_memory_test

clean:
	rm -rf build-*
