
include ../common.make

# Link Key DB and LE Device DB using TLV on top of Flash Sector interface
DEFINES := -D NVM_NUM_DEVICE_DB_ENTRIES=16

INCLUDES := -Ibuild-coverage
INCLUDES += -I${BTSTACK_ROOT}/src
INCLUDES += -I${BTSTACK_ROOT}/platform/embedded
INCLUDES += -I${BTSTACK_ROOT}/test/include/coverage-ble

CFLAGS += ${INCLUDES} ${DEFINES}
CXXFLAGS += ${INCLUDES} ${DEFINES}

VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/ble
VPATH += ${BTSTACK_ROOT}/platform/posix
VPATH += ${BTSTACK_ROOT}/platform/embedded

COMMON = \
	btstack_linked_list.c       \
	btstack_memory.c            \
	btstack_memory_pool.c       \
	btstack_util.c              \
	hci_dump.c                  \
	le_device_db_tlv.c          \
	btstack_tlv_flash_bank.c    \
	hal_flash_bank_memory.c

COMMON_OBJ_COVERAGE = $(addprefix build-coverage/,$(COMMON:.c=.o))
COMMON_OBJ_ASAN     = $(addprefix build-asan/,    $(COMMON:.c=.o))

all: build-coverage/le_device_db_tlv_test build-asan/le_device_db_tlv_test

build-coverage/le_device_db_tlv_test: ${COMMON_OBJ_COVERAGE}

build-asan/le_device_db_tlv_test: ${COMMON_OBJ_ASAN}

test: all
	build-asan/le_device_db_tlv_test
		
coverage: all
	rm -f build-coverage/*.gcda
	build-coverage/le_device_db_tlv_test

clean:
	rm -rf build-coverage build-asan
