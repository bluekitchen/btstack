# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..

# CppuTest from pkg-config
CFLAGS  += ${shell pkg-config --cflags CppuTest}
LDFLAGS += ${shell pkg-config --libs   CppuTest}

CFLAGS += -DUNIT_TEST -g -Wall -Wnarrowing -Wconversion-null
CFLAGS += -I. -I.. -I${BTSTACK_ROOT}/src
CFLAGS += -I${BTSTACK_ROOT}/platform/embedded
CFLAGS += -I${BTSTACK_ROOT}/platform/posix
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/mbedtls/include
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/micro-ecc
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/rijndael
LDFLAGS +=  -lCppUTest -lCppUTestExt

VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/ble 
VPATH += ${BTSTACK_ROOT}/platform/embedded
VPATH += ${BTSTACK_ROOT}/platform/posix
VPATH += ${BTSTACK_ROOT}/3rd-party/micro-ecc
VPATH += ${BTSTACK_ROOT}/3rd-party/rijndael

COMMON = \
	btstack_crypto.c    		\
	btstack_linked_list.c		\
	btstack_memory.c			\
	btstack_memory_pool.c		\
	btstack_run_loop.c			\
	btstack_run_loop_embedded.c \
	hci_cmd.c					\
	hci_dump.c					\
	hci_dump_posix_fs.c  		\
	le_device_db_memory.c       \
	mock.c 				        \
	rijndael.c 					\
	sm.c     					\
	btstack_util.c			            \
	btstack_tlv.c \
	
CFLAGS_COVERAGE = ${CFLAGS} -fprofile-arcs -ftest-coverage
CFLAGS_ASAN     = ${CFLAGS} -fsanitize=address -DHAVE_ASSERT

LDFLAGS += -lCppUTest -lCppUTestExt
LDFLAGS_COVERAGE = ${LDFLAGS} -fprofile-arcs -ftest-coverage
LDFLAGS_ASAN     = ${LDFLAGS} -fsanitize=address

COMMON_OBJ_COVERAGE = $(addprefix build-coverage/,$(COMMON:.c=.o)) build-coverage/uECC.o
COMMON_OBJ_ASAN     = $(addprefix build-asan/,    $(COMMON:.c=.o)) build-asan/uECC.o

all: build-coverage/security_manager build-asan/security_manager

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


build-coverage/security_manager: ${COMMON_OBJ_COVERAGE} build-coverage/security_manager.o | build-coverage
	${CXX} $^ ${LDFLAGS_COVERAGE} -o $@

build-asan/security_manager: ${COMMON_OBJ_ASAN} build-asan/security_manager.o | build-asan
	${CXX} $^ ${LDFLAGS_ASAN} -o $@


test: all
	build-asan/security_manager
	
coverage: all
	rm -f build-coverage/*.gcda
	build-coverage/security_manager

clean:
	rm -rf build-coverage build-asan
