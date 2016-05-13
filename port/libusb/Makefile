# Makefile for libusb based examples
BTSTACK_ROOT = ../..

CORE += main.c stdin_support.c

COMMON += hci_transport_h2_libusb.c btstack_run_loop_posix.c btstack_link_key_db_fs.c

include ${BTSTACK_ROOT}/example/Makefile.inc

# custom
EXAMPLES += gap_le_connect_to_1111
gap_le_connect_to_1111: ${CORE_OBJ} ${COMMON_OBJ} ${ATT_OBJ} ${SM_REAL_OBJ} ad_parser.o gap_le_connect_to_1111.c
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@


# CC = gcc-fsf-4.9
CFLAGS  += -g -Wall -Wmissing-prototypes -Wstrict-prototypes -Wshadow -Werror
# CFLAGS += -Werror

CFLAGS += -I${BTSTACK_ROOT}/platform/posix \
		  -I${BTSTACK_ROOT}/platform/embedded

VPATH += ${BTSTACK_ROOT}/platform/embedded
VPATH += ${BTSTACK_ROOT}/platform/posix

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

all: ${EXAMPLES}
