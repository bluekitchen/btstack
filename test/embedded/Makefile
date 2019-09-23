CC = g++

# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..

CFLAGS  = -g -Wall \
		  -I.. \
		  -I${BTSTACK_ROOT}/src \
		  -I${BTSTACK_ROOT}/platform/embedded
		  
CFLAGS += -fprofile-arcs -ftest-coverage

LDFLAGS += -lCppUTest -lCppUTestExt 

VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/ble
VPATH += ${BTSTACK_ROOT}/platform/embedded

COMMON = \
    btstack_util.c		  \
    btstack_linked_list.c  \
    btstack_audio_embedded.c \
	btstack_em9304_spi_embedded.c \
	btstack_run_loop_embedded.c \
	btstack_stdin_embedded.c \
	btstack_tlv_flash_bank.c \
	btstack_uart_block_embedded.c \
	hal_flash_bank_memory.c \
	btstack_run_loop.c \
    hci_dump.c    \
	
COMMON_OBJ = $(COMMON:.c=.o)

all: embedded_test

embedded_test: ${COMMON_OBJ} embedded_test.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

test: all
	./embedded_test

clean:
	rm -f  embedded_test
	rm -f  *.o
	rm -rf *.dSYM
	rm -f *.gcno *.gcda
	
