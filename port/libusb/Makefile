# Makefile for libusb based examples
BTSTACK_ROOT = ../..

CORE += main.c stdin_support.c

COMMON += hci_transport_h2_libusb.c btstack_run_loop_posix.c btstack_link_key_db_fs.c

include ${BTSTACK_ROOT}/example/Makefile.inc

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

# use pkg-config for portaudio
# CFLAGS  += $(shell pkg-config portaudio-2.0 --cflags) -DHAVE_PORTAUDIO
# LDFLAGS += $(shell pkg-config portaudio-2.0 --libs)
# hard coded flags for portaudio in /usr/local/lib
# CFLAGS  += -I/usr/local/include -DHAVE_PORTAUDIO
# LDFLAGS += -L/sw/lib -lportaudio -Wl,-framework,CoreAudio -Wl,-framework,AudioToolbox -Wl,-framework,AudioUnit -Wl,-framework,Carbon

all: ${EXAMPLES}
