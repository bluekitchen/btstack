# Makefile for windows-h4 examples
BTSTACK_ROOT ?= ../..

CORE += main.c btstack_stdin_windows.c btstack_tlv_windows.c hci_dump_windows_fs.c

COMMON  += \
			btstack_chipset_zephyr.c     \
			btstack_run_loop_windows.c   \
			btstack_uart_block_windows.c \
			hci_transport_h4.c           \
			le_device_db_tlv.c           \

# examples
CLASSIC=
include ${BTSTACK_ROOT}/example/Makefile.inc

# CC = gcc-fsf-4.9
CFLAGS  += -g -Wall -Wmissing-prototypes -Wstrict-prototypes -Wshadow -Werror

CFLAGS += -I${BTSTACK_ROOT}/platform/posix \
		  -I${BTSTACK_ROOT}/platform/windows \
		  -I${BTSTACK_ROOT}/platform/embedded \
		  -I$(BTSTACK_ROOT)/chipset/zephyr \
		  -I${BTSTACK_ROOT}/3rd-party/tinydir

VPATH += ${BTSTACK_ROOT}/platform/windows
VPATH += ${BTSTACK_ROOT}/platform/posix
VPATH += ${BTSTACK_ROOT}/chipset/zephyr

EXAMPLES = ${EXAMPLES_GENERAL} ${EXAMPLES_LE_ONLY}

all: ${EXAMPLES}
