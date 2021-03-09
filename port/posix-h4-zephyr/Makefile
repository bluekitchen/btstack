# Makefile for posix-h4 based examples
BTSTACK_ROOT ?= ../..

CORE += \
	btstack_run_loop_posix.c \
	btstack_tlv_posix.c \
	btstack_uart_posix.c \
	hci_dump_posix_fs.c \
	hci_transport_h4.c \
	le_device_db_tlv.c \
	main.c \
	btstack_stdin_posix.c \
	btstack_chipset_zephyr.c \

# examples
CLASSIC =
include ${BTSTACK_ROOT}/example/Makefile.inc

CFLAGS  += -g -Wall -Werror \
	-I$(BTSTACK_ROOT)/platform/embedded \
	-I$(BTSTACK_ROOT)/platform/posix \
	-I$(BTSTACK_ROOT)/chipset/zephyr \
	-I${BTSTACK_ROOT}/3rd-party/tinydir
	
VPATH += ${BTSTACK_ROOT}/platform/posix
VPATH += ${BTSTACK_ROOT}/platform/embedded
VPATH += ${BTSTACK_ROOT}/chipset/zephyr

all: ${EXAMPLES_LE_ONLY}

