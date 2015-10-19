# Makefile for libusb based examples
BTSTACK_ROOT = ../..
POSIX_ROOT= ${BTSTACK_ROOT}/platforms/posix

CORE += main.c stdin_support.c

COMMON += hci_transport_h4.c run_loop_posix.c remote_device_db_fs.c

include ${BTSTACK_ROOT}/example/embedded/Makefile.inc

# CC = gcc-fsf-4.9
CFLAGS  += -g -Wall 
# CFLAGS += -Werror

VPATH += ${BTSTACK_ROOT}/platforms/posix/src

ifeq ($(OS),Windows_NT)
LDFLAGS += -lws2_32
endif

# Command Line examples require porting to win32, so only build on other unix-ish hosts
ifneq ($(OS),Windows_NT)
EXAMPLES += ${EXAMPLES_CLI}
CFLAGS += -I${POSIX_ROOT}/src
endif

all: ${BTSTACK_ROOT}/include/btstack/version.h ${EXAMPLES}
