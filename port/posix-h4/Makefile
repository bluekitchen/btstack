# Makefile for libusb based examples
BTSTACK_ROOT = ../..

CORE += main.c stdin_support.c

COMMON += \
	hci_transport_h4_posix.c  \
	btstack_run_loop_posix.c \
	remote_device_db_fs.c \

CORE += \
	bluetooth_init_cc2564B_1.2_BT_Spec_4.1.c \
	btstack_chipset_cc256x.c \
	btstack_chipset_csr.c \
	btstack_chipset_em9301.c \
	btstack_chipset_stlc2500d.c \
	btstack_chipset_tc3566x.c \
#	btstack_chipset_bcm.c \

# TI-WL183x requires TIInit_11.8.32.c

# examples
include ${BTSTACK_ROOT}/example/embedded/Makefile.inc

# fetch and convert TI init scripts
include ${BTSTACK_ROOT}/chipset/cc256x/Makefile.inc

CFLAGS  += -g -Wall -Werror \
	-I$(BTSTACK_ROOT)/platform/embedded \
	-I$(BTSTACK_ROOT)/platform/posix \
	-I$(BTSTACK_ROOT)/chipset/bcm \
	-I$(BTSTACK_ROOT)/chipset/cc256x \
	-I$(BTSTACK_ROOT)/chipset/csr \
	-I$(BTSTACK_ROOT)/chipset/em9301 \
	-I$(BTSTACK_ROOT)/chipset/stlc2500d \
	-I$(BTSTACK_ROOT)/chipset/tc3566x \

VPATH += ${BTSTACK_ROOT}/platform/posix
VPATH += ${BTSTACK_ROOT}/platform/embedded

VPATH += ${BTSTACK_ROOT}/chipset/bcm	
VPATH += ${BTSTACK_ROOT}/chipset/cc256x
VPATH += ${BTSTACK_ROOT}/chipset/csr
VPATH += ${BTSTACK_ROOT}/chipset/em9301
VPATH += ${BTSTACK_ROOT}/chipset/stlc2500d
VPATH += ${BTSTACK_ROOT}/chipset/tc3566x

ifeq ($(OS),Windows_NT)
LDFLAGS += -lws2_32
endif

# Command Line examples require porting to win32, so only build on other unix-ish hosts
ifneq ($(OS),Windows_NT)
EXAMPLES += ${EXAMPLES_CLI}
endif

all: ${BTSTACK_ROOT}/src/btstack_version.h ${EXAMPLES}
