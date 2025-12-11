include ../common.make

DEFINES := -DUNIT_TEST

CFLAGS += ${DEFINES}
CXXFLAGS += ${DEFINES}

COMMON_OBJ_COVERAGE = $(addprefix build-coverage/,$(COMMON:.c=.o))
COMMON_OBJ_ASAN     = $(addprefix build-asan/,    $(COMMON:.c=.o))

all: build-coverage/sqrt_test build-asan/sqrt_test

build-coverage/sqrt_test: ${COMMON_OBJ_COVERAGE}
build-asan/sqrt_test: ${COMMON_OBJ_ASAN}

test: all
	build-asan/sqrt_test
	
coverage: all
	rm -f build-coverage/*.gcda
	build-coverage/sqrt_test

clean:
	rm -rf build-coverage build-asan
	
