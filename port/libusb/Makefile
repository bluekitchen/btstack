# Makefile for libusb based examples
BTSTACK_ROOT = ../..

CORE += main.c btstack_stdin_posix.c btstack_tlv_posix.c

COMMON  += hci_transport_h2_libusb.c btstack_run_loop_posix.c le_device_db_fs.c btstack_link_key_db_fs.c wav_util.c btstack_network_posix.c

include ${BTSTACK_ROOT}/example/Makefile.inc

CFLAGS  += -g -Wall -Wmissing-prototypes -Wstrict-prototypes -Wshadow -Werror -Wunused-parameter -Wredundant-decls -Wsign-compare
# CFLAGS += -Werror

CFLAGS += -I${BTSTACK_ROOT}/platform/posix \
		  -I${BTSTACK_ROOT}/platform/embedded

VPATH += ${BTSTACK_ROOT}/platform/embedded
VPATH += ${BTSTACK_ROOT}/platform/posix
VPATH += ${BTSTACK_ROOT}/platform/libusb

# use pkg-config
CFLAGS  += $(shell pkg-config libusb-1.0 --cflags)
LDFLAGS += $(shell pkg-config libusb-1.0 --libs)

EXAMPLES += 
	le_data_channel_client  \
	le_data_channel_server  \

# use pkg-config for portaudio
# CFLAGS  += $(shell pkg-config portaudio-2.0 --cflags) -DHAVE_PORTAUDIO
# LDFLAGS += $(shell pkg-config portaudio-2.0 --libs)

# hard coded flags for portaudio in /usr/local/lib
# CFLAGS  += -I/usr/local/include -DHAVE_PORTAUDIO
# LDFLAGS += -L/sw/lib -lportaudio -Wl,-framework,CoreAudio -Wl,-framework,AudioToolbox -Wl,-framework,AudioUnit -Wl,-framework,Carbon

clean_src:
	rm -rf * ${BTSTACK_ROOT}/src/*.o
	rm -rf * ${BTSTACK_ROOT}/src/classic/*.o
	rm -rf * ${BTSTACK_ROOT}/src/ble/*.o
	rm -rf * ${BTSTACK_ROOT}/platform/embedded/*.o

all: ${EXAMPLES}

