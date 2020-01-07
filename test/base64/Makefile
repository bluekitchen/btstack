CC=g++

# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..
CPPUTEST_HOME = ${BTSTACK_ROOT}/test/cpputest

CFLAGS  = -g -Wall -I. -I../ -I${BTSTACK_ROOT}/src
CFLAGS += -fprofile-arcs -ftest-coverage -fsanitize=address,undefined
LDFLAGS += -lCppUTest -lCppUTestExt

VPATH += ${BTSTACK_ROOT}/src

COMMON_OBJ = \
    btstack_base64_decoder.o \
    btstack_util.o \
    hci_dump.o \

all: base64_decoder_test

base64_decoder_test: ${COMMON_OBJ} base64_decoder_test.o
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

test: all
	./base64_decoder_test
	
clean:
	rm -fr base64_decoder_test *.dSYM *.o
	rm -f *.gcno *.gcda
