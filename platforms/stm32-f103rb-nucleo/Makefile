#
# Makefile for STM32-F103RB Nucleo board with CC256x Bluetooth modules
#

BINARY=led_counter
OPENCM3_DIR = libopencm3
LDSCRIPT = stm32f1-nucleo.ld

BTSTACK_ROOT = ../..

CORE   = \
	main.c 										  \
    ${BTSTACK_ROOT}/src/btstack_memory.c          \
    ${BTSTACK_ROOT}/src/linked_list.c	          \
    ${BTSTACK_ROOT}/src/memory_pool.c             \
    ${BTSTACK_ROOT}/src/run_loop.c		          \
    ${BTSTACK_ROOT}/src/run_loop_embedded.c

CORE_OBJ   = $(CORE:.c=.o)

OBJS += $(CORE_OBJ)

CFLAGS = -I. -I$(BTSTACK_ROOT)/include -I$(BTSTACK_ROOT)/src

examples: libopencm3/lib/libopencm3_stm32f1.a led_counter.elf

include libopencm3.stm32f1.mk

clean:
	rm -f *.map *.o *.d *.out *.elf *.bin *.hex ../driver/*.o ../../src/*.o ../src/*.o ../firmware/*.o ${BTSTACK_ROOT}/chipset-cc256x/*.o ${BTSTACK_ROOT}/src/*.o

# git clone and compile libopencm3
libopencm3/lib/libopencm3_stm32f1.a:
	git clone git@github.com:libopencm3/libopencm3.git
	make -C libopencm3

my_flash: $(BINARY).bin
	st-flash write $(BINARY).bin 0x8000000
