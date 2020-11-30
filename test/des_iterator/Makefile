CC = g++

# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..

CFLAGS  = -g -Wall -I.. -I${BTSTACK_ROOT}/src -I${BTSTACK_ROOT}/include
CFLAGS += -fprofile-arcs -ftest-coverage
LDFLAGS += -lCppUTest -lCppUTestExt

VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/classic
VPATH += ${BTSTACK_ROOT}/platform/posix

COMMON = \
    sdp_util.c	              \
    hci_dump.c    \
	btstack_util.c			          
 
COMMON_OBJ = $(COMMON:.c=.o)

all: build-coverage/des_iterator_test

build-coverage:
	mkdir -p build-coverage

build-coverage/des_iterator_test: ${COMMON_OBJ} des_iterator_test.c  | build-coverage
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

test: all
	build-coverage/des_iterator_test

coverage: all
	build-coverage/des_iterator_test

clean:
	rm -f *.o
	rm -rf *.dSYM build-coverage

