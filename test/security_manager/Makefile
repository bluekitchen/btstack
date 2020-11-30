CC = g++

# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..

CFLAGS  = -DUNIT_TEST -g
CPPFLAGS =  -x c++ -Wall -Wno-unused
CFLAGS += -I. -I.. -I${BTSTACK_ROOT}/src
#CFLAGS += -I${BTSTACK_ROOT}/platform/posix
CFLAGS += -I${BTSTACK_ROOT}/platform/embedded
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/mbedtls/include
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/micro-ecc
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/rijndael
CFLAGS += -fprofile-arcs -ftest-coverage
LDFLAGS +=  -lCppUTest -lCppUTestExt

VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/ble 
VPATH += ${BTSTACK_ROOT}/platform/embedded
# VPATH += ${BTSTACK_ROOT}/platform/posix
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
	le_device_db_memory.c       \
	mock.c 				        \
	rijndael.c 					\
	sm.c     					\
	btstack_util.c			            \
	btstack_tlv.c \
	
COMMON_OBJ = $(addprefix build-coverage/,$(COMMON:.c=.o))
CORE_OBJ = $(addprefix build-coverage/,$(CORE:.c=.o))

all: build-coverage/security_manager

build-coverage:
	mkdir -p build-coverage

build-coverage/%.o: %.c | build-coverage
	${CC} -c $(CFLAGS) ${CPPFLAGS} $< -o $@

build-coverage/security_manager: ${CORE_OBJ} ${COMMON_OBJ} build-coverage/security_manager.o | build-coverage
	${CC} ${CORE_OBJ} ${COMMON_OBJ} security_manager.c ${CFLAGS} ${CPPFLAGS} ${LDFLAGS} -o $@

test: all
	build-coverage/security_manager
	
coverage: all
	build-coverage/security_manager

clean:
	rm -f  *.o
	rm -rf *.dSYM build-coverage
	rm -f *.gcno *.gcda
	
