CC=g++

BTSTACK_ROOT = ../..
CPPUTEST_HOME = ${BTSTACK_ROOT}/test/cpputest

COMMON = \
	btstack_tlv_posix.c \
	btstack_util.c \
	btstack_linked_list.c \
	hci_dump.c \

COMMON_OBJ = $(addprefix build-coverage/,$(COMMON:.c=.o))

VPATH = \
	${BTSTACK_ROOT}/src \
	${BTSTACK_ROOT}/src/classic \
	${BTSTACK_ROOT}/src/ble \
	${BTSTACK_ROOT}/platform/posix \

CFLAGS  = \
    -DBTSTACK_TEST \
    -g \
    -Wall \
    -Wnarrowing \
    -I. \
    -I.. \
    -I${BTSTACK_ROOT}/src \
    -I${BTSTACK_ROOT}/platform/posix \

CFLAGS += -fprofile-arcs -ftest-coverage
LDFLAGS += -lCppUTest -lCppUTestExt

all: build-coverage/tlv_test

build-coverage:
	mkdir -p build-coverage

build-coverage/%.o: %.c | build-coverage
	${CC} -c $(CFLAGS) $< -o $@

build-coverage/tlv_test: ${COMMON_OBJ} build-coverage/tlv_test.o | build-coverage
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

test: all
	build-coverage/tlv_test

coverage: all
	build-coverage/tlv_test

clean:
	rm -rf *.o build-coverage *.dSYM *.pklg
	rm -f *.gcno *.gcda
