CC ?= cc
CFLAGS += -std=c11 -Wall -Wextra -Werror -ffunction-sections -fdata-sections
CFLAGS += -I../../port/posix-h4 -I../../src
ifeq ($(shell uname),Darwin)
LDFLAGS += -Wl,-dead_strip
else
LDFLAGS += -Wl,--gc-sections
endif

.PHONY: test coverage coverage-all clean

test: bnep_test bnep_lwip_test
	./bnep_test
	./bnep_lwip_test

coverage coverage-all: test

bnep_test: bnep_test.c ../../src/classic/bnep.c ../../src/btstack_util.c
	$(CC) $(CFLAGS) bnep_test.c ../../src/btstack_util.c $(LDFLAGS) -o $@

bnep_lwip_test: bnep_lwip_test.c ../../platform/lwip/bnep_lwip.c ../../src/btstack_util.c ../../src/btstack_ring_buffer.c
	$(CC) $(CFLAGS) -I../../platform/lwip -I../../platform/lwip/port -I../../3rd-party/lwip/core/src/include bnep_lwip_test.c ../../src/btstack_util.c ../../src/btstack_ring_buffer.c $(LDFLAGS) -o $@

clean:
	$(RM) bnep_test bnep_lwip_test
