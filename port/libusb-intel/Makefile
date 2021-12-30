# Makefile for libusb based examples
BTSTACK_ROOT ?= ../..

CORE += main.c btstack_stdin_posix.c btstack_tlv_posix.c hci_dump_posix_fs.c

COMMON += hci_transport_h2_libusb.c btstack_run_loop_posix.c le_device_db_tlv.c btstack_link_key_db_tlv.c wav_util.c btstack_network_posix.c
COMMON += btstack_audio_portaudio.c btstack_chipset_intel_firmware.c rijndael.c btstack_signal.c

include ${BTSTACK_ROOT}/example/Makefile.inc
include ${BTSTACK_ROOT}/chipset/intel/Makefile.inc

CFLAGS  += -g -std=c99 -Wall -Wmissing-prototypes -Wstrict-prototypes -Wshadow -Wunused-parameter -Wredundant-decls -Wsign-compare
# CFLAGS += -Werror
# CFLAGS += -pedantic

# only LLVM
# CFLAGS += -Wnewline-eof
# CFLAGS += -Wc11-extensions
# CFLAGS += -Wgnu-empty-initializer

CFLAGS += -I${BTSTACK_ROOT}/platform/posix \
		  -I${BTSTACK_ROOT}/platform/embedded \
		  -I${BTSTACK_ROOT}/3rd-party/tinydir \
          -I${BTSTACK_ROOT}/3rd-party/rijndael \
          -I${BTSTACK_ROOT}/chipset/intel

VPATH += ${BTSTACK_ROOT}/3rd-party/rijndael
VPATH += ${BTSTACK_ROOT}/platform/embedded
VPATH += ${BTSTACK_ROOT}/platform/posix
VPATH += ${BTSTACK_ROOT}/platform/libusb
VPATH += ${BTSTACK_ROOT}/chipset/intel

# use pkg-config
CFLAGS  += $(shell pkg-config libusb-1.0 --cflags)
LDFLAGS += $(shell pkg-config libusb-1.0 --libs)

# add pthread for ctrl-c signal handler
LDFLAGS += -lpthread

EXAMPLES = ${EXAMPLES_GENERAL} ${EXAMPLES_CLASSIC_ONLY} ${EXAMPLES_LE_ONLY} ${EXAMPLES_DUAL_MODE}
EXAMPLES += pan_lwip_http_server

# use pkg-config for portaudio
# CFLAGS  += $(shell pkg-config portaudio-2.0 --cflags) -DHAVE_PORTAUDIO
# LDFLAGS += $(shell pkg-config portaudio-2.0 --libs)

# hard coded flags for portaudio in /usr/local/lib
# CFLAGS  += -I/usr/local/include -DHAVE_PORTAUDIO
# LDFLAGS += -L/sw/lib -lportaudio -Wl,-framework,CoreAudio -Wl,-framework,AudioToolbox -Wl,-framework,AudioUnit -Wl,-framework,Carbon

clean: clean-intel

clean_src:
	rm -rf * ${BTSTACK_ROOT}/src/*.o
	rm -rf * ${BTSTACK_ROOT}/src/classic/*.o
	rm -rf * ${BTSTACK_ROOT}/src/ble/*.o
	rm -rf * ${BTSTACK_ROOT}/platform/embedded/*.o

all: all-intel ${EXAMPLES}

