CC = g++

# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..

CFLAGS  = -g -Wall \
		  -I.. \
		  -I${BTSTACK_ROOT}/src
		  
CFLAGS += -fprofile-arcs -ftest-coverage -fsanitize=address,undefined

LDFLAGS += -lCppUTest -lCppUTestExt 

VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/classic
VPATH += ${BTSTACK_ROOT}/platform/posix

COMMON = \
	btstack_util.c		  \
	btstack_linked_list.c \
	hci_dump.c 			  \
    avdtp_util.c		  \
	
COMMON_OBJ = $(COMMON:.c=.o)

all: avdtp_util_test

avdtp_util_test: ${COMMON_OBJ} avdtp_util_test.o
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

test: all
	rm -f *.gcda
	./avdtp_util_test

clean:
	rm -f  avdtp_util_test
	rm -f  *.o
	rm -rf *.dSYM
	rm -f *.gcno *.gcda
	
