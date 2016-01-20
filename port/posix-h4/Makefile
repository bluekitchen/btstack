# Makefile for libusb based examples
BTSTACK_ROOT = ../..

CORE += main.c stdin_support.c

COMMON += hci_transport_h4_posix.c btstack_run_loop_posix.c remote_device_db_fs.c

include ${BTSTACK_ROOT}/example/embedded/Makefile.inc

CFLAGS  += -g -Wall \
		  -I$(BTSTACK_ROOT)/platform/embedded \
		  -I$(BTSTACK_ROOT)/platform/posix 

# CFLAGS += -Werror

VPATH += ${BTSTACK_ROOT}/platform/posix
VPATH += ${BTSTACK_ROOT}/platform/embedded

ifeq ($(OS),Windows_NT)
LDFLAGS += -lws2_32
endif

# Command Line examples require porting to win32, so only build on other unix-ish hosts
ifneq ($(OS),Windows_NT)
EXAMPLES += ${EXAMPLES_CLI}
endif

all: ${BTSTACK_ROOT}/src/version.h ${EXAMPLES}
