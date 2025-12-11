
include ../common.make

DEFINES  := -DUNIT_TEST
INCLUDES := -I${BTSTACK_ROOT}/src

CFLAGS += ${DEFINES} ${INCLUDES}
CXXFLAGS += ${DEFINES} ${INCLUDES}

VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/classic
VPATH += ${BTSTACK_ROOT}/platform/posix

COMMON = \
	btstack_util.c		    \
	hci_dump.c    			\
	btstack_memory_pool.c 	\
	btstack_memory.c

COMMON_OBJ_COVERAGE_NONE   = $(addprefix build-coverage/,$(COMMON:.c=_none.o))
COMMON_OBJ_COVERAGE_SINGLE = $(addprefix build-coverage/,$(COMMON:.c=_single.o))
COMMON_OBJ_COVERAGE_MALLOC = $(addprefix build-coverage/,$(COMMON:.c=_malloc.o))
COMMON_OBJ_ASAN            = $(addprefix build-asan/,    $(COMMON:.c=_single.o))

all: build-coverage/btstack_memory_pool_test_single \
	 build-coverage/btstack_memory_test_single \
	 build-coverage/btstack_memory_test_none \
	 build-coverage/btstack_memory_test_malloc \
	 build-asan/btstack_memory_pool_test_single \
	 build-asan/btstack_memory_test_single

build-coverage/%_none.o: %.c
	${CC} -c $(CFLAGS_COVERAGE) -I config_none $< -o $@
build-coverage/%_none.o: %.cpp
	${CXX} -c $(CXXFLAGS_COVERAGE) -I config_none $< -o $@

build-coverage/%_single.o: %.c
	${CC} -c $(CFLAGS_COVERAGE) -I config_single $< -o $@
build-coverage/%_single.o: %.cpp
	${CXX} -c $(CXXFLAGS_COVERAGE) -I config_single $< -o $@

build-coverage/%_malloc.o: %.c
	${CC} -c $(CFLAGS_COVERAGE) -I config_malloc $< -o $@
build-coverage/%_malloc.o: %.cpp
	${CXX} -c $(CXXFLAGS_COVERAGE) -I config_malloc $< -o $@

build-asan/%_single.o: %.c
	${CC} -c $(CFLAGS_ASAN) -I config_single $< -o $@
build-asan/%_single.o: %.cpp
	${CXX} -c $(CXXFLAGS_ASAN) -I config_single $< -o $@

build-coverage/btstack_memory_pool_test_single: ${COMMON_OBJ_COVERAGE_SINGLE}

build-coverage/btstack_memory_test_single: ${COMMON_OBJ_COVERAGE_SINGLE}
build-coverage/btstack_memory_test_none: ${COMMON_OBJ_COVERAGE_NONE}
build-coverage/btstack_memory_test_malloc: ${COMMON_OBJ_COVERAGE_MALLOC}

build-asan/btstack_memory_pool_test_single: ${COMMON_OBJ_ASAN}
build-asan/btstack_memory_test_single: ${COMMON_OBJ_ASAN}

test: all
	build-asan/btstack_memory_pool_test_single
	build-asan/btstack_memory_test_single

coverage: all
	rm -f build-coverage/*.gcda
	build-coverage/btstack_memory_pool_test_single
	build-coverage/btstack_memory_test_none
	build-coverage/btstack_memory_test_single
	build-coverage/btstack_memory_test_malloc

clean:
	rm -rf build-*
