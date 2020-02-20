CC=g++

# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..
CPPUTEST_HOME = ${BTSTACK_ROOT}/test/cpputest

CFLAGS  = -g -Wall -I. -I../ -I${BTSTACK_ROOT}/src -I${BTSTACK_ROOT}/include
CFLAGS += -fprofile-arcs -ftest-coverage -fsanitize=address,undefined
LDFLAGS += -lCppUTest -lCppUTestExt

VPATH += ${BTSTACK_ROOT}/src/ble 
VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/classic
VPATH += ${BTSTACK_ROOT}/platform/posix

COMMON = \
    btstack_util.c \
    hci_dump.c \
    obex_message_builder.c \

COMMON_OBJ = $(COMMON:.c=.o)

all: obex_message_builder_test

obex_message_builder_test: ${COMMON_OBJ} obex_message_builder_test.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

test: all
	./obex_message_builder_test
	
clean:
	rm -fr obex_message_builder_test *.dSYM *.o
	rm -f *.gcno *.gcda
	
