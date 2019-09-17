CC=g++

# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..
CPPUTEST_HOME = ${BTSTACK_ROOT}/test/cpputest

CFLAGS  = -g -Wall -I. -I../ -I${BTSTACK_ROOT}/src -I${BTSTACK_ROOT}/include
LDFLAGS += -lCppUTest -lCppUTestExt

VPATH += ${BTSTACK_ROOT}/src/ble 
VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/platform/posix

VPATH  += ${BTSTACK_ROOT}/3rd-party/yxml
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/yxml

VPATH  += ${BTSTACK_ROOT}/test/map_client
CFLAGS += -I${BTSTACK_ROOT}/test/map_client

CFLAGS += -fprofile-arcs -ftest-coverage

COMMON = \
    yxml.c \
    btstack_util.c \
    hci_dump.c \
    map_util.c  \
    btstack_util.c \


COMMON_OBJ = $(COMMON:.c=.o)

all: map_xml_test

map_xml_test: ${COMMON_OBJ} map_xml_test.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

test: all
	./map_xml_test
	
clean:
	rm -fr map_xml_test *.dSYM *.o ../src/*.o
	rm -f *.gcno *.gcda
	
