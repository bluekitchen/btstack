# Makefile for libusb based examples
BTSTACK_ROOT = ../..
POSIX_ROOT= ${BTSTACK_ROOT}/platforms/posix

CORE += main.c stdin_support.c bt_control_cc256x.c bluetooth_init_cc2564B_1.2_BT_Spec_4.1.c

COMMON += hci_transport_h4.c run_loop_posix.c remote_device_db_fs.c

include ${BTSTACK_ROOT}/example/embedded/Makefile.inc

# fetch and convert init scripts
include ${BTSTACK_ROOT}/chipset-cc256x/Makefile.inc

CFLAGS += -I$(BTSTACK_ROOT)/chipset-cc256x

CFLAGS  += -g -Wall 

# 
CFLAGS += -Wmissing-prototypes -Wstrict-prototypes -Wshadow -Werror

VPATH += ${BTSTACK_ROOT}/platforms/posix/src
VPATH += ${BTSTACK_ROOT}/chipset-cc256x

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
CFLAGS += -I${POSIX_ROOT}/src
endif

all: ${BTSTACK_ROOT}/include/btstack/version.h ${EXAMPLES}
