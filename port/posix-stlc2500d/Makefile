# Makefile for libusb based examples
BTSTACK_ROOT = ../..

CORE += main.c stdin_support.c btstack_chipset_stlc2500d.c

COMMON += hci_transport_h4_posix.c btstack_run_loop_posix.c remote_device_db_fs.c

include ${BTSTACK_ROOT}/example/embedded/Makefile.inc


CFLAGS += -I$(BTSTACK_ROOT)/chipset/stlc2500d

CFLAGS  += -g -Wall	\
		   -I$(BTSTACK_ROOT)/platform/embedded \
		   -I$(BTSTACK_ROOT)/platform/posix 

VPATH += ${BTSTACK_ROOT}/platform/posix
VPATH += ${BTSTACK_ROOT}/platform/embedded
VPATH += ${BTSTACK_ROOT}/chipset/stlc2500d

ifeq ($(OS),Windows_NT)
LDFLAGS += -lws2_32
# assume libusb was installed into /usr/local
CFLAGS  += -I/usr/local/include/libusb-1.0
LDFLAGS += -L/usr/local/lib -lusb-1.0
else
# use pkg-config
CFLAGS  += $(shell pkg-config libusb-1.0 --cflags)
LDFLAGS += $(shell pkg-config libusb-1.0 --libs)
endif

# Command Line examples require porting to win32, so only build on other unix-ish hosts
ifneq ($(OS),Windows_NT)
EXAMPLES += ${EXAMPLES_CLI}
endif

# no BLE here
EXAMPLES:= $(filter-out ${EXAMPLES_USING_LE},$(EXAMPLES))

all: ${BTSTACK_ROOT}/src/btstack_version.h ${EXAMPLES}
