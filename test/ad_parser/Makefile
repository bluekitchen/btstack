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
	btstack_util.c		    \
	hci_dump.c    			\
	ad_parser.c 			\

COMMON_OBJ = $(COMMON:.c=.o)

all: ad_parser_test

ad_parser_test: ${COMMON_OBJ} ad_parser_test.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

test: all
	./ad_parser_test

clean:
	rm -f ad_parser_test *.o
	rm -rf *.dSYM
	rm -f *.gcno *.gcda
	
