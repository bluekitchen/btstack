# Makefile for libusb based PTS tests
BTSTACK_ROOT = ../..

CORE += main.c stdin_support.c

COMMON += hci_transport_h2_libusb.c btstack_run_loop_posix.c btstack_link_key_db_fs.c le_device_db_fs.c

include ${BTSTACK_ROOT}/example/Makefile.inc

CFLAGS += -g -Wall -Wmissing-prototypes -Wstrict-prototypes -Wshadow -Werror \
          -I${BTSTACK_ROOT}/platform/posix \
          -I${BTSTACK_ROOT}/platform/embedded \
	      -I${BTSTACK_ROOT}/port/libusb

VPATH += ${BTSTACK_ROOT}/platform/posix
VPATH += ${BTSTACK_ROOT}/port/libusb

# use pkg-config for libusb
CFLAGS  += $(shell pkg-config libusb-1.0 --cflags)
LDFLAGS += $(shell pkg-config libusb-1.0 --libs)


AVDTP_TESTS = avdtp_test 

all: ${AVDTP_TESTS}

avdtp_test: ${CORE_OBJ} ${COMMON_OBJ} avdtp_sink.o avdtp_test.o
	${CC} $^ ${CFLAGS} ${LDFLAGS} -o $@

test: all

clean:
	rm -f *.pyc *.o $(AVDTP_TESTS) *.dSYM *_test