include ../common.make

COMMON = \
	btstack_link_key_db_tlv.c \
	btstack_util.c \
	hci_dump.c \
	hci_dump_posix_fs.c \
	hal_flash_bank_memory.c

VPATH = \
	${BTSTACK_ROOT}/src \
	${BTSTACK_ROOT}/src/ble \
	${BTSTACK_ROOT}/src/classic \
	${BTSTACK_ROOT}/platform/embedded \
	${BTSTACK_ROOT}/platform/posix

DEFINES := -DUNIT_TEST
INCLUDES += -I${BTSTACK_ROOT}/src
INCLUDES += -I${BTSTACK_ROOT}/platform/embedded
INCLUDES += -I${BTSTACK_ROOT}/platform/posix
INCLUDES += -I..

CFLAGS += ${INCLUDES} ${DEFINES}
CXXFLAGS += ${INCLUDES} ${DEFINES}

COMMON_OBJ_COVERAGE = $(addprefix build-coverage/,$(COMMON:.c=.o))
COMMON_OBJ_ASAN     = $(addprefix build-asan/,    $(COMMON:.c=.o))

all: build-coverage/tlv_test build-asan/tlv_test build-asan/tlv_test_write_once build-asan/tlv_test_delete_field

# write once sets ENABLE_TLV_FLASH_WRITE_ONCE
build-asan/%_write_once.o: %.c | build-asan
	${CC} -DENABLE_TLV_FLASH_WRITE_ONCE -c $(CFLAGS_ASAN) $< -o $@

build-asan/%_write_once.o: %.cpp | build-asan
	${CXX} -DENABLE_TLV_FLASH_WRITE_ONCE -c $(CXXFLAGS_ASAN) $< -o $@

# delete field sets ENABLE_TLV_FLASH_DELETE_FIELD
build-asan/%_delete_field.o: %.c | build-asan
	${CC} -DENABLE_TLV_FLASH_EXPLICIT_DELETE_FIELD -c $(CFLAGS_ASAN) $< -o $@

build-asan/%_delete_field.o: %.cpp | build-asan
	${CXX} -DENABLE_TLV_FLASH_EXPLICIT_DELETE_FIELD -c $(CXXFLAGS_ASAN) $< -o $@

# targets
build-coverage/tlv_test: ${COMMON_OBJ_COVERAGE} build-coverage/btstack_tlv_flash_bank.o

build-asan/tlv_test: ${COMMON_OBJ_ASAN} build-asan/btstack_tlv_flash_bank.o

build-asan/tlv_test_write_once: ${COMMON_OBJ_ASAN} build-asan/btstack_tlv_flash_bank_write_once.o

build-asan/tlv_test_delete_field: ${COMMON_OBJ_ASAN} build-asan/btstack_tlv_flash_bank_delete_field.o

test: all
	build-asan/tlv_test
	build-asan/tlv_test_write_once
	build-asan/tlv_test_delete_field

coverage: all
	rm -f build-coverage/*.gcda
	build-coverage/tlv_test

clean:
	rm -rf build-coverage build-asan
