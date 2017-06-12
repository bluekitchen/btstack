CC=g++

BTSTACK_ROOT = ../..
CPPUTEST_HOME = ${BTSTACK_ROOT}/test/cpputest

COMMON = \
	btstack_link_key_db_tlv.c \
	btstack_tlv_flash_sector.c \
	btstack_util.c \
	hal_flash_sector_memory.c \
	hci_dump.c \

COMMON_OBJ  = $(COMMON:.c=.o) 

VPATH = \
	${BTSTACK_ROOT}/src \
	${BTSTACK_ROOT}/src/classic \
	${BTSTACK_ROOT}/platform/embedded \

CFLAGS  = \
    -DBTSTACK_TEST \
    -g \
    -Wall \
    -Wmissing-prototypes \
    -Wnarrowing \
    -I. \
    -I.. \
    -I${BTSTACK_ROOT}/src \
    -I${BTSTACK_ROOT}/platform/embedded \

LDFLAGS += -lCppUTest -lCppUTestExt

TESTS = tlv_test

all: ${TESTS}

clean:
	rm -rf *.o $(TESTS) *.dSYM *.pklg

tlv_test: ${COMMON_OBJ} tlv_test.o  
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

test: all
	@echo Run all test
	@set -e; \
	for test in $(TESTS); do \
	  ./$$test; \
	done

