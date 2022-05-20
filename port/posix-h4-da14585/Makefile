# Makefile for libusb based examples
BTSTACK_ROOT ?= ../..

CORE += \
	btstack_chipset_da145xx.c \
	btstack_run_loop_posix.c \
	btstack_tlv_posix.c \
	btstack_uart_posix.c \
	hci_dump_posix_fs.c \
	hci_transport_h4.c \
	le_device_db_tlv.c \
	main.c \
	btstack_stdin_posix.c \
	btstack_signal.c \
	hci_585.c \
	# hci_581_active_uart.c \

# examples
CLASSIC =
include ${BTSTACK_ROOT}/example/Makefile.inc

CFLAGS  += -g -Wall -Werror \
	-I$(BTSTACK_ROOT)/platform/posix \
	-I$(BTSTACK_ROOT)/chipset/da145xx \
	-I$(BTSTACK_ROOT)/platform/embedded \
	-I${BTSTACK_ROOT}/3rd-party/tinydir

# add pthread for ctrl-c signal handler
LDFLAGS += -lpthread

VPATH += ${BTSTACK_ROOT}/platform/posix
VPATH += ${BTSTACK_ROOT}/chipset/da145xx

EXAMPLES = ${EXAMPLES_GENERAL} ${EXAMPLES_LE_ONLY}

all: ${EXAMPLES}

