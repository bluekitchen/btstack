# Makefile for windows-h4-da14585 examples
BTSTACK_ROOT ?= ../..

CORE += main.c btstack_stdin_windows.c btstack_tlv_posix.c hci_dump_posix_fs.c

COMMON  += \
			btstack_run_loop_windows.c   \
			hci_transport_h4.c           \
			btstack_uart_block_windows.c \
			le_device_db_tlv.c           \
			hci_585.c \
			btstack_chipset_da14581.c \
            rijndael.c \

# examples
CLASSIC =
include ${BTSTACK_ROOT}/example/Makefile.inc

# CC = gcc-fsf-4.9
CFLAGS  += -g -Wall -Wmissing-prototypes -Wstrict-prototypes -Wshadow -Werror
# CFLAGS += -Werror

CFLAGS += -I${BTSTACK_ROOT}/platform/posix \
		  -I${BTSTACK_ROOT}/platform/windows \
		  -I${BTSTACK_ROOT}/platform/embedded \
		  -I$(BTSTACK_ROOT)/chipset/da14581 \
          -I${BTSTACK_ROOT}/3rd-party/rijndael \
		  -I${BTSTACK_ROOT}/3rd-party/tinydir

VPATH += ${BTSTACK_ROOT}/3rd-party/rijndael

VPATH += ${BTSTACK_ROOT}/platform/windows
VPATH += ${BTSTACK_ROOT}/platform/posix

VPATH += ${BTSTACK_ROOT}/chipset/da14581

EXAMPLES = ${EXAMPLES_GENERAL} ${EXAMPLES_LE_ONLY}

all: ${EXAMPLES}
