CC = g++

# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..

CFLAGS  = -g -Wall -I.. -I${BTSTACK_ROOT}/src -I${BTSTACK_ROOT}/include
CFLAGS += -fprofile-arcs -ftest-coverage -fsanitize=address,undefined
LDFLAGS += -lCppUTest -lCppUTestExt

VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/classic
VPATH += ${BTSTACK_ROOT}/platform/posix

COMMON = \
    sdp_util.c	              \
    hci_dump.c    \
	btstack_util.c			          
 
COMMON_OBJ = $(COMMON:.c=.o)

all: des_iterator_test

des_iterator_test: ${COMMON_OBJ} des_iterator_test.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

test: all
	./des_iterator_test

clean:
	rm -f des_iterator_test *.o
	rm -rf *.dSYM
	
