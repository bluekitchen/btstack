CC = g++

# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..

CFLAGS  = -g -Wall \
		  -I. \
		  -I${BTSTACK_ROOT}/src
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/rijndael

CFLAGS_COVERAGE = ${CFLAGS} -fprofile-arcs -ftest-coverage
CFLAGS_ASAN     = ${CFLAGS} -fsanitize=address -DHAVE_ASSERT

LDFLAGS += -lCppUTest -lCppUTestExt
LDFLAGS_COVERAGE = ${LDFLAGS} -fprofile-arcs -ftest-coverage
LDFLAGS_ASAN     = ${LDFLAGS} -fsanitize=address

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
	
COMMON_OBJ_COVERAGE = $(addprefix build-coverage/,$(COMMON:.c=.o))
COMMON_OBJ_ASAN     = $(addprefix build-asan/,    $(COMMON:.c=.o))

all: build-coverage/att_db_util_test build-coverage/att_db_test build-asan/att_db_util_test build-asan/att_db_test

build-%:
	mkdir -p $@

build-coverage/%.o: %.c | build-coverage
	${CC} -c $(CFLAGS_COVERAGE) $< -o $@

build-asan/%.o: %.c | build-asan
	${CC} -c $(CFLAGS_ASAN) $< -o $@

build-coverage/att_db_util_test: ${COMMON_OBJ_COVERAGE} build-coverage/att_db_util_test.o | build-coverage/
	${CC} $^ ${LDFLAGS_COVERAGE} -o $@

build-coverage/att_db_test: build-coverage/att_db_test.o build-coverage/att_db.o build-coverage/btstack_util.o build-coverage/hci_dump.o build-coverage/att_db_util.o | build-coverage/
	${CC} $^ ${LDFLAGS_COVERAGE} -o $@

build-asan/att_db_util_test: ${COMMON_OBJ_ASAN} build-asan/att_db_util_test.o | build-asan/
	${CC} $^ ${LDFLAGS_ASAN} -o $@

build-asan/att_db_test: build-asan/att_db_test.o build-asan/att_db.o build-asan/btstack_util.o build-asan/hci_dump.o build-asan/att_db_util.o | build-asan/
	${CC} $^ ${LDFLAGS_ASAN} -o $@

test: all
	build-asan/att_db_util_test
	build-asan/att_db_test

coverage: all
	rm -f build-coverage/*.gcda
	build-coverage/att_db_util_test
	build-coverage/att_db_test

clean:
	rm -rf build-coverage build-asan
	
