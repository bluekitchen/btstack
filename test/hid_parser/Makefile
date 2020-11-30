CC = g++

# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..

CPPFLAGS =  -x c++ -Wall -Wno-unused

CFLAGS  = -DUNIT_TEST -g
CFLAGS += -I. -I${BTSTACK_ROOT}/src
CFLAGS += -fprofile-arcs -ftest-coverage
LDFLAGS +=  -lCppUTest -lCppUTestExt
VPATH += ${BTSTACK_ROOT}/src

COMMON = \
	btstack_util.c \
	btstack_hid_parser.c \
	hci_dump.c
	
COMMON_OBJ = $(addprefix build-coverage/,$(COMMON:.c=.o))

all: build-coverage/hid_parser_test

build-coverage:
	mkdir -p build-coverage

build-coverage/%.o: %.c | build-coverage
	${CC} -c $(CFLAGS) ${CPPFLAGS} $< -o $@

build-coverage/hid_parser_test: ${COMMON_OBJ} build-coverage/hid_parser_test.o | build-coverage
	${CC} ${CFLAGS}  $^ ${LDFLAGS} -o $@

test: all
	build-coverage/hid_parser_test
	
coverage: all
	build-coverage/hid_parser_test

clean:
	rm -f  *.o
	rm -rf *.dSYM build-coverage
	rm -f *.gcno *.gcda

