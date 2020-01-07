CC = g++

# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..

CFLAGS  = -g -Wall \
		  -I. \
		  -I.. \
		  -I${BTSTACK_ROOT}/src \
		  -I${BTSTACK_ROOT}/platform/embedded
		  
CFLAGS += -fprofile-arcs -ftest-coverage -fsanitize=address,undefined

CFLAGS += -DHAVE_HAL_AUDIO

LDFLAGS += -lCppUTest -lCppUTestExt 

VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/ble
VPATH += ${BTSTACK_ROOT}/platform/embedded
VPATH += ${BTSTACK_ROOT}/platform/freertos

COMMON = \
	btstack_em9304_spi_embedded.c \
	btstack_run_loop.c \
	btstack_run_loop_embedded.c \
	btstack_stdin_embedded.c \
	btstack_tlv_flash_bank.c \
	btstack_uart_block_embedded.c \
	hal_flash_bank_memory.c \
    btstack_audio.c \
    btstack_audio_embedded.c \
    btstack_linked_list.c  \
    btstack_resample.c \
    btstack_ring_buffer.c \
    btstack_run_loop_base.c \
    btstack_slip.c \
    btstack_util.c		  \
    hci_dump.c    \
    hci_transport_em9304_spi.c \
    hci_transport_h4.c \
    hci_transport_h5.c \
	
COMMON_OBJ = $(COMMON:.c=.o)

FREERTOS_OBJS = \
	btstack_run_loop_freertos.o \
	btstack_uart_block_freertos.o \

all: embedded_test freertos_test

embedded_test: ${COMMON_OBJ} embedded_test.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

freertos_test: ${COMMON_OBJ} ${FREERTOS_OBJS} freertos_test.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

test: all
	./embedded_test
	./freertos_test

clean:
	rm -f  embedded_test
	rm -f  *.o
	rm -rf *.dSYM
	rm -f *.gcno *.gcda
	
