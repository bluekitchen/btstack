# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..

CFLAGS  = -DUNIT_TEST -g -Wall -Wnarrowing -Wconversion-null -Ibuild-coverage -I./
CFLAGS += -I${BTSTACK_ROOT}/src
CFLAGS += -I${BTSTACK_ROOT}/src/ble
CFLAGS += -I${BTSTACK_ROOT}/platform/posix
CFLAGS += -I${BTSTACK_ROOT}/platform/embedded
# CFLAGS += -D ENABLE_TESTING_SUPPORT

VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/ble 
VPATH += ${BTSTACK_ROOT}/platform/embedded
VPATH += ${BTSTACK_ROOT}/platform/posix

COMMON = \
	btstack_linked_list.c \
	btstack_util.c \
	hci.c \
	hci_cmd.c \
	ad_parser.c \
	l2cap.c \
	l2cap_signaling.c \
	btstack_memory.c \
	btstack_run_loop.c \
	btstack_run_loop_embedded.c \
	hci_dump.c \
	hci_dump_posix_stdout.c \

CFLAGS_COVERAGE = ${CFLAGS} -fprofile-arcs -ftest-coverage
CFLAGS_ASAN     = ${CFLAGS} -fsanitize=address -DHAVE_ASSERT

LDFLAGS += -lCppUTest -lCppUTestExt
LDFLAGS_COVERAGE = ${LDFLAGS} -fprofile-arcs -ftest-coverage
LDFLAGS_ASAN     = ${LDFLAGS} -fsanitize=address

COMMON_OBJ_COVERAGE = $(addprefix build-coverage/,$(COMMON:.c=.o))
COMMON_OBJ_ASAN     = $(addprefix build-asan/,    $(COMMON:.c=.o))


all: \
	build-coverage/l2cap_cbm_test build-asan/l2cap_cbm_test \

build-%:
	mkdir -p $@

build-coverage/%.o: %.c | build-coverage
	${CC} -c $(CFLAGS_COVERAGE) $< -o $@

build-coverage/%.o: %.cpp | build-coverage
	${CXX} -c $(CFLAGS_COVERAGE) $< -o $@

build-asan/%.o: %.c | build-asan
	${CC} -c $(CFLAGS_ASAN) $< -o $@

build-asan/%.o: %.cpp | build-asan
	${CXX} -c $(CFLAGS_ASAN) $< -o $@

build-coverage/l2cap_cbm_test: ${COMMON_OBJ_COVERAGE} build-coverage/l2cap_cbm_test.o | build-coverage
	${CXX} $^ ${LDFLAGS_COVERAGE} -o $@

build-asan/l2cap_cbm_test: ${COMMON_OBJ_ASAN} build-asan/l2cap_cbm_test.o | build-asan
	${CXX} $^ ${LDFLAGS_ASAN} -o $@

test: all
	build-asan/l2cap_cbm_test

coverage: all
	rm -f build-coverage/*.gcda
	build-coverage/l2cap_cbm_test

clean:
	rm -rf build-coverage build-asan

