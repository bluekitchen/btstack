CC = g++

# Requirements: cpputest.github.io

BTSTACK_ROOT =  ../..

CFLAGS  = -g -Wall \
		  -I. \
		  -I${BTSTACK_ROOT}/src \
		  -I${BTSTACK_ROOT}/platform/embedded
		  
CFLAGS += -DHAVE_HAL_AUDIO

CFLAGS_COVERAGE = ${CFLAGS} -fprofile-arcs -ftest-coverage
CFLAGS_ASAN     = ${CFLAGS} -fsanitize=address -DHAVE_ASSERT

LDFLAGS += -lCppUTest -lCppUTestExt
LDFLAGS_COVERAGE = ${LDFLAGS} -fprofile-arcs -ftest-coverage
LDFLAGS_ASAN     = ${LDFLAGS} -fsanitize=address

VPATH += ${BTSTACK_ROOT}/src
VPATH += ${BTSTACK_ROOT}/src/ble
VPATH += ${BTSTACK_ROOT}/platform/embedded
VPATH += ${BTSTACK_ROOT}/platform/freertos

COMMON = \
    btstack_linked_list.c  \
	btstack_memory.c \
	btstack_run_loop.c \
	btstack_run_loop_embedded.c \
    btstack_util.c		  \
    hci_dump.c    \

# not used yet
#	btstack_em9304_spi_embedded.c \
#	btstack_stdin_embedded.c \
#	btstack_tlv_flash_bank.c \
#	btstack_uart_block_embedded.c \
#	hal_flash_bank_memory.c \
#   btstack_audio.c \
#   btstack_audio_embedded.c \
#   btstack_resample.c \
#   btstack_ring_buffer.c \
#   btstack_slip.c \
#   hci_transport_em9304_spi.c \
#   hci_transport_h4.c \
#   hci_transport_h5.c \

FREERTOS = \
	btstack_run_loop_freertos.c \
#	btstack_uart_block_freertos.c \

COMMON_OBJ_COVERAGE = $(addprefix build-coverage/,$(COMMON:.c=.o))
COMMON_OBJ_ASAN     = $(addprefix build-asan/,    $(COMMON:.c=.o))

FREERTOS_OBJ_COVERAGE = $(addprefix build-coverage/,$(FREERTOS:.c=.o))
FREERTOS_OBJ_ASAN     = $(addprefix build-asan/,    $(FREERTOS:.c=.o))

all: build-coverage/embedded_test build-coverage/freertos_test build-asan/embedded_test build-asan/freertos_test

build-%:
	mkdir -p $@

build-coverage/%.o: %.c | build-coverage
	${CC} -c $(CFLAGS_COVERAGE) $< -o $@

build-asan/%.o: %.c | build-asan
	${CC} -c $(CFLAGS_ASAN) $< -o $@


build-coverage/embedded_test: ${COMMON_OBJ_COVERAGE} build-coverage/embedded_test.o | build-coverage
	${CC} $^ ${LDFLAGS_COVERAGE} -o $@

build-coverage/freertos_test: ${COMMON_OBJ_COVERAGE} ${FREERTOS_OBJS} build-coverage/freertos_test.o | build-coverage
	${CC} $^ ${LDFLAGS_COVERAGE} -o $@

build-asan/embedded_test: ${COMMON_OBJ_ASAN} build-asan/embedded_test.o | build-asan
	${CC} $^ ${LDFLAGS_ASAN} -o $@

build-asan/freertos_test: ${COMMON_OBJ_ASAN} ${FREERTOS_OBJS} build-asan/freertos_test.o | build-asan
	${CC} $^ ${LDFLAGS_ASAN} -o $@

test: all
	build-asan/embedded_test
	build-asan/freertos_test

coverage: all
	rm -f build-coverage/*.gcda
	build-coverage/embedded_test
	build-coverage/freertos_test

clean:
	rm -rf build-coverage build-asan
