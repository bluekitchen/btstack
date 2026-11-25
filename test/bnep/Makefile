CC ?= cc
CFLAGS += -std=c11 -Wall -Wextra -Werror -ffunction-sections -fdata-sections
CFLAGS += -I../../port/posix-h4 -I../../src
ifeq ($(shell uname),Darwin)
LDFLAGS += -Wl,-dead_strip
else
LDFLAGS += -Wl,--gc-sections
endif

.PHONY: test coverage coverage-all clean

test: bnep_test bnep_lwip_test bnep_lwip_rtos_test pan_lwip_startup_test pan_lwip_startup_external_test
	./bnep_test
	./bnep_lwip_test
	./bnep_lwip_rtos_test
	./bnep_lwip_rtos_test init-failure
	./pan_lwip_startup_test
	./pan_lwip_startup_test dhcp-failure
	./pan_lwip_startup_external_test
	./pan_lwip_startup_external_test dhcp-failure

coverage coverage-all: test

bnep_test: bnep_test.c ../../src/classic/bnep.c ../../src/btstack_util.c
	$(CC) $(CFLAGS) bnep_test.c ../../src/btstack_util.c $(LDFLAGS) -o $@

bnep_lwip_test: bnep_lwip_test.c ../../platform/lwip/bnep_lwip.c ../../src/btstack_util.c ../../src/btstack_ring_buffer.c
	$(CC) $(CFLAGS) -I../../platform/lwip -I../../platform/lwip/port -I../../3rd-party/lwip/core/src/include bnep_lwip_test.c ../../src/btstack_util.c ../../src/btstack_ring_buffer.c $(LDFLAGS) -o $@

bnep_lwip_rtos_test: bnep_lwip_rtos_test.c ../../platform/lwip/bnep_lwip.c ../../src/btstack_util.c
	$(CC) -Irtos $(CFLAGS) -I../../platform/lwip -I../../platform/lwip/port -I../../3rd-party/lwip/core/src/include bnep_lwip_rtos_test.c ../../src/btstack_util.c $(LDFLAGS) -o $@

PAN_TEST_INCLUDES = -Irtos -I../../platform/lwip -I../../platform/lwip/port -I../../3rd-party/lwip/core/src/include -I../../3rd-party/lwip/dhcp-server -I../../3rd-party/hxcmod-player -I../../3rd-party/yxml -I../../3rd-party/bluedroid/decoder/include -I../../3rd-party/bluedroid/encoder/include

pan_lwip_startup_test: pan_lwip_startup_test.c ../../example/pan_lwip_http_server.c ../../src/btstack_util.c
	$(CC) $(PAN_TEST_INCLUDES) $(CFLAGS) pan_lwip_startup_test.c ../../src/btstack_util.c $(LDFLAGS) -o $@

pan_lwip_startup_external_test: pan_lwip_startup_test.c ../../example/pan_lwip_http_server.c ../../src/btstack_util.c
	$(CC) -DHAVE_LWIP $(PAN_TEST_INCLUDES) $(CFLAGS) pan_lwip_startup_test.c ../../src/btstack_util.c $(LDFLAGS) -o $@

clean:
	$(RM) bnep_test bnep_lwip_test bnep_lwip_rtos_test pan_lwip_startup_test pan_lwip_startup_external_test
