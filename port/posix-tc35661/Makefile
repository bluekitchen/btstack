# Makefile for libusb based examples
BTSTACK_ROOT = ../..

CORE += main.c stdin_support.c btstack_chipset_tc3566x.c

COMMON += hci_transport_h4_posix.c btstack_run_loop_posix.c remote_device_db_fs.c

include ${BTSTACK_ROOT}/example/embedded/Makefile.inc

CFLAGS  += -g -Wall \
		   -I$(BTSTACK_ROOT)/chipset/tc3556x   \
		   -I$(BTSTACK_ROOT)/platform/embedded \
		   -I$(BTSTACK_ROOT)/platform/posix 

VPATH += ${BTSTACK_ROOT}/chipset/tc3556x
VPATH += ${BTSTACK_ROOT}/platform/embedded
VPATH += ${BTSTACK_ROOT}/platform/posix

# Command Line examples require porting to win32, so only build on other unix-ish hosts
ifneq ($(OS),Windows_NT)
EXAMPLES += ${EXAMPLES_CLI}
endif

# no BLE here
EXAMPLES:= $(filter-out ${EXAMPLES_USING_LE},$(EXAMPLES))

all: ${BTSTACK_ROOT}/src/btstack_version.h ${EXAMPLES}
