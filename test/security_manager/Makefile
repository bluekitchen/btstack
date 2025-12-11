include ../common.make

DEFINES := -D HAVE_EMBEDDED_TIME_MS
INCLUDES := -I${BTSTACK_ROOT}/src
INCLUDES += -I${BTSTACK_ROOT}/platform/embedded
INCLUDES += -I${BTSTACK_ROOT}/platform/posix
INCLUDES += -I${BTSTACK_ROOT}/3rd-party/mbedtls/include
INCLUDES += -I${BTSTACK_ROOT}/3rd-party/micro-ecc
INCLUDES += -I${BTSTACK_ROOT}/3rd-party/rijndael
INCLUDES += -I${BTSTACK_ROOT}/test/include/coverage-ble

CFLAGS += ${INCLUDES} ${DEFINES}
CXXFLAGS += ${INCLUDES} ${DEFINES}

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
	hci_event.c					\
	le_device_db_memory.c       \
	mock.c 				        \
	rijndael.c 					\
	sm.c     					\
	btstack_util.c			            \
	btstack_tlv.c

COMMON_OBJ_COVERAGE = $(addprefix build-coverage/,$(COMMON:.c=.o)) build-coverage/uECC.o
COMMON_OBJ_ASAN     = $(addprefix build-asan/,    $(COMMON:.c=.o)) build-asan/uECC.o

all: build-coverage/security_manager build-asan/security_manager

build-coverage/security_manager: ${COMMON_OBJ_COVERAGE}
build-asan/security_manager: ${COMMON_OBJ_ASAN}

test: all
	build-asan/security_manager
	
coverage: all
	rm -f build-coverage/*.gcda
	build-coverage/security_manager

clean:
	rm -rf build-coverage build-asan
