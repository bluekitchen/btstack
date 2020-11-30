CC = g++

# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..

CFLAGS  = -g -Wall \
		  -I. \
		  -I${BTSTACK_ROOT}/src
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/rijndael

CFLAGS += -fprofile-arcs -ftest-coverage

LDFLAGS += -lCppUTest -lCppUTestExt 

VPATH += ${BTSTACK_ROOT}/3rd-party/rijndael
VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/ble
VPATH += ${BTSTACK_ROOT}/platform/posix

COMMON = \
    btstack_util.c		  \
    hci_dump.c    \
    att_db_util.c \
    btstack_crypto.c \
    btstack_linked_list.c \
    hci_cmd.c \
    rijndael.c
	
COMMON_OBJ = $(COMMON:.c=.o)

all: build-coverage/att_db_util_test build-coverage/att_db_test

build-coverage:
	mkdir -p build-coverage

build-coverage/att_db_util_test: ${COMMON_OBJ} att_db_util_test.c | build-coverage/
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

build-coverage/att_db_test: att_db_test.c att_db.o btstack_util.o hci_dump.o att_db_util.o | build-coverage/
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

test: all
	build-coverage/att_db_util_test
	build-coverage/att_db_test

coverage: all
	build-coverage/att_db_util_test
	build-coverage/att_db_test

clean:
	rm -f  att_db_util_test
	rm -f  *.o
	rm -rf *.dSYM build-coverage
	rm -f *.gcno *.gcda
	
