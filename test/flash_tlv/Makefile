CC=g++

BTSTACK_ROOT = ../..
POSIX_ROOT= ${BTSTACK_ROOT}/platform/posix
CPPUTEST_HOME = ${BTSTACK_ROOT}/test/cpputest

COMMON = \
	hal_flash_sector_memory.c \
	btstack_tlv_flash_sector.c \
	hci_dump.c \
	btstack_util.c \

COMMON_OBJ  = $(COMMON:.c=.o) 

VPATH = \
	${BTSTACK_ROOT}/src

CFLAGS  = \
    -DBTSTACK_TEST \
    -g \
    -Wall \
    -Wmissing-prototypes \
    -Wnarrowing \
    -I. \
    -I.. \
    -I${BTSTACK_ROOT}/src \
    -I${BTSTACK_ROOT}/platform/posix \

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

