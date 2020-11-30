CC=g++

# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..
CPPUTEST_HOME = ${BTSTACK_ROOT}/test/cpputest

CFLAGS  = -g -Wall -I. -I../ -I${BTSTACK_ROOT}/src
CFLAGS  += -fprofile-arcs -ftest-coverage
LDFLAGS += -lCppUTest -lCppUTestExt

VPATH += ${BTSTACK_ROOT}/src

COMMON = \
    btstack_ring_buffer.c \

COMMON_OBJ = $(addprefix build-coverage/,$(COMMON:.c=.o))

all: build-coverage/btstack_ring_buffer_test

build-coverage:
	mkdir -p build-coverage

build-coverage/%.o: %.c | build-coverage
	${CC} -c $(CFLAGS) ${CPPFLAGS} $< -o $@

build-coverage/btstack_ring_buffer_test: ${COMMON_OBJ} build-coverage/btstack_ring_buffer_test.o | build-coverage
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

test: all
	build-coverage/btstack_ring_buffer_test
	
coverage: all
	build-coverage/btstack_ring_buffer_test

clean:
	rm -fr build-coverage *.dSYM *.o ../src/*.o *.gcda *.gcno
	rm -f *.gcno *.gcda
	
