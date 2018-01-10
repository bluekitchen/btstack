CC = g++

# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..

CFLAGS  = -DUNIT_TEST -g
CPPFLAGS =  -x c++ -Wall -Wno-unused
CFLAGS += -I. -I.. -I${BTSTACK_ROOT}/src
LDFLAGS +=  -lCppUTest -lCppUTestExt
VPATH += ${BTSTACK_ROOT}/src

COMMON = \
	
COMMON_OBJ = $(COMMON:.c=.o)


all: hid_parser_test

hid_parser_test: btstack_hid_parser.c btstack_util.c hid_parser_test.c hci_dump.c
	${CC} ${CFLAGS} ${CPPFLAGS} ${LDFLAGS} $^  -o $@

test: all
	./hid_parser_test
	
clean:
	rm -f  hid_parser_test
	rm -f  *.o
	rm -rf *.dSYM
	
