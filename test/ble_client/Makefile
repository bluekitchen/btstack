CC = g++

# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..

CFLAGS  = -g -Wall -I. -I${BTSTACK_ROOT}/src -I${BTSTACK_ROOT}/include
CFLAGS += -fprofile-arcs -ftest-coverage
LDFLAGS += -lCppUTest -lCppUTestExt

VPATH += ${BTSTACK_ROOT}/src/ble 
VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/platform/posix

COMMON = \
	btstack_linked_list.c	    \
	btstack_memory.c			\
	btstack_memory_pool.c		\
	btstack_run_loop.c			\
	btstack_run_loop_posix.c 	\
	btstack_util.c			    \
	hci.c                       \
	hci_cmd.c					\
	hci_dump.c					\
	
COMMON_OBJ = $(COMMON:.c=.o)

all: build-coverage/ad_parser_test

build-coverage:
	mkdir -p build-coverage

build-coverage/ad_parser_test: ${CORE_OBJ} ${COMMON_OBJ} ad_parser_test.c | build-coverage
	${CC} ${CORE_OBJ} ${COMMON_OBJ} ad_parser_test.c ${CFLAGS} ${LDFLAGS} -o $@

test: all
	build-coverage/ad_parser_test

coverage: all
	build-coverage/ad_parser_test

clean:
	rm -f  *.o
	rm -rf *.dSYM build-coverage
	rm -f *.gcno *.gcda

