CC = g++

# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..

CFLAGS  = -DUNIT_TEST -x c++ -g -Wall -Wnarrowing -Wconversion-null
CFLAGS += -I.
CFLAGS += -I${BTSTACK_ROOT}/src
CFLAGS += -I${BTSTACK_ROOT}/platform/embedded
# CFLAGS += -fprofile-arcs -ftest-coverage -fsanitize=address
LDFLAGS +=  -lCppUTest -lCppUTestExt

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
	hal_flash_bank_memory.c     \

COMMON_OBJ = $(COMMON:.c=.o)

all: le_device_db_tlv_test

le_device_db_tlv_test: ${COMMON_OBJ} le_device_db_tlv_test.o
	${CC} ${COMMON_OBJ} le_device_db_tlv_test.o ${CFLAGS} ${LDFLAGS} -o $@

test: all
	./le_device_db_tlv_test
		
clean:
	rm -f  le_device_db_tlv_test
	rm -f  *.o
	rm -rf *.dSYM
	rm -f *.gcno *.gcda

