#==============================================================================
# (C) Copyright Albis Technologies Ltd 2011
#==============================================================================
#                STM32 Example Code
#==============================================================================
# File name:     Makefile
#
# Notes:         -
#==============================================================================

firsttarget: all

include project.settings

SHELL=/bin/bash.exe
GCC=arm-none-eabi-gcc.exe
AS=arm-none-eabi-as.exe
OBJCPY=arm-none-eabi-objcopy
RM=rm
ECHO=echo

#==============================================================================
#  Building.
#==============================================================================
AFLAGS=-mcpu=cortex-m3 \

CFLAGS=-Wall \
       -Wpointer-arith \
       -Wstrict-prototypes \
       -Werror \
       -mcpu=cortex-m3 \
       -mthumb \
       -g \
       -O2 \
       $(LED_DEFINES) \

LFLAGS=-nostartfiles \
       -nostdlib \
       -mcpu=cortex-m3 \

.s.o:
	$(AS) $(AFLAGS) -c $< -o $@

.c.o:
	$(GCC) $(CFLAGS) $(INCL) -c $< -o $@

#==============================================================================
#  Repository directories.
#==============================================================================
PATH_OS_SRC=../../../../uCOS-II/Source
PATH_CM3_SRC=../../../../uCOS-II/Ports/ARM-Cortex-M3/Generic/GNU
PATH_BSP_SRC=..
PATH_CPU_SRC=../../../../uC-CPU/ARM-Cortex-M3/GNU
PATH_CPU_INC=../../../../uC-CPU
PATH_UCLIB_SRC=../../../../uC-LIB
PATH_STM32_SRC=../../../../CPU/ST/STM32/src
PATH_STM32_INC=../../../../CPU/ST/STM32/inc
PATH_BTSTACK_SRC=../../../../../../BTstack/src
PATH_BTSTACK_INC=../../../../../../BTstack/include
PATH_APPL_SRC=../../../../../../Application
PATH_APPL_INC=../../../../../../Application

#==============================================================================
#  Board support package (BSP).
#==============================================================================
ASMS_START=vector_table.o \

OBJS_BSP=$(PATH_BSP_SRC)/app_hooks.o \
         $(PATH_BSP_SRC)/bsp.o \
         $(PATH_BSP_SRC)/bsp_debug.o \
         $(PATH_BSP_SRC)/bsp_i2c.o \
         $(PATH_BSP_SRC)/bsp_int.o \
         $(PATH_BSP_SRC)/bsp_periph.o \
         $(PATH_BSP_SRC)/bsp_stlm75.o \
         $(PATH_BSP_SRC)/main.o \
         $(PATH_BSP_SRC)/T32_Term.o \
         $(PATH_BSP_SRC)/uCOS-II/bsp_os.o \
         vector_funcs.o \

ifeq ($(STM32_USART_DMA),yes)
  OBJS_BSP+=$(PATH_BSP_SRC)/bsp_btuart_dma.o
else
  OBJS_BSP+=$(PATH_BSP_SRC)/bsp_btuart_nodma.o
endif

INCL_BSP=-I$(PATH_BSP_SRC) \
         -I$(PATH_BSP_SRC)/uCOS-II

#==============================================================================
#  uC/OS-II.
#==============================================================================
ASMS_UCOS=$(PATH_CM3_SRC)/os_cpu_a.o \
          $(PATH_CPU_SRC)/cpu_a.o \

OBJS_UCOS=$(PATH_OS_SRC)/os_core.o \
          $(PATH_OS_SRC)/os_flag.o \
          $(PATH_OS_SRC)/os_mbox.o \
          $(PATH_OS_SRC)/os_mem.o \
          $(PATH_OS_SRC)/os_mutex.o \
          $(PATH_OS_SRC)/os_q.o \
          $(PATH_OS_SRC)/os_sem.o \
          $(PATH_OS_SRC)/os_task.o \
          $(PATH_OS_SRC)/os_time.o \
          $(PATH_OS_SRC)/os_tmr.o \
          $(PATH_CM3_SRC)/os_cpu_c.o \
          $(PATH_CM3_SRC)/os_dbg.o \
          $(PATH_CPU_SRC)/cpu_c.o

INCL_UCOS=-I$(PATH_OS_SRC) \
          -I$(PATH_CM3_SRC) \
          -I$(PATH_CPU_SRC) \
          -I$(PATH_CPU_INC) \

#==============================================================================
#  uC/LIB.
#==============================================================================
OBJS_UCLIB=$(PATH_UCLIB_SRC)/lib_mem.o \
           $(PATH_UCLIB_SRC)/lib_str.o \
           $(PATH_UCLIB_SRC)/memmove.o \

INCL_UCLIB=-I$(PATH_UCLIB_SRC) \

#==============================================================================
#  STM32.
#==============================================================================
OBJS_STM32=$(PATH_STM32_SRC)/stm32f10x_flash.o \
           $(PATH_STM32_SRC)/stm32f10x_gpio.o \
           $(PATH_STM32_SRC)/stm32f10x_rcc.o \
           $(PATH_STM32_SRC)/stm32f10x_usart.o \

ifeq ($(STM32_USART_DMA),yes)
  OBJS_STM32+=$(PATH_STM32_SRC)/stm32f10x_dma.o $(PATH_STM32_SRC)/misc.o
endif

INCL_STM32=-I$(PATH_STM32_INC) \
           -I$(PATH_STM32_SRC) \

#==============================================================================
#  BTstack.
#==============================================================================
OBJS_BTSTACK=$(PATH_BTSTACK_SRC)/btstack_memory_ucos.o \
             $(PATH_BTSTACK_SRC)/daemon.o \
             $(PATH_BTSTACK_SRC)/hci.o \
             $(PATH_BTSTACK_SRC)/hci_cmds.o \
             $(PATH_BTSTACK_SRC)/hci_dump_ucos.o \
             $(PATH_BTSTACK_SRC)/hci_transport_h4_ucos.o \
             $(PATH_BTSTACK_SRC)/l2cap.o \
             $(PATH_BTSTACK_SRC)/l2cap_signaling.o \
             $(PATH_BTSTACK_SRC)/linked_list.o \
             $(PATH_BTSTACK_SRC)/port_ucos.o \
             $(PATH_BTSTACK_SRC)/remote_device_db_memory.o \
             $(PATH_BTSTACK_SRC)/rfcomm.o \
             $(PATH_BTSTACK_SRC)/run_loop_ucos.o \
             $(PATH_BTSTACK_SRC)/sdp.o \
             $(PATH_BTSTACK_SRC)/sdp_util.o \
             $(PATH_BTSTACK_SRC)/sdp_spp_record.o \
             $(PATH_BTSTACK_SRC)/utils.o \

ifeq ($(BT_CHIPSET),CSR_BC4)
  OBJS_BTSTACK+=$(PATH_BTSTACK_SRC)/bt_chipset_csrbc4.o
  CFLAGS+=-DBT_CHIPSET_CSR_BC4=1
  CFLAGS+=-DBT_CHIPSET_CSR_BC4_CHANGE_BAUDRATE=$(CSR_BC4_CHANGE_BAUDRATE_ID)
endif

ifeq ($(BT_CHIPSET),PAN1315)
  OBJS_BTSTACK+=$(PATH_BTSTACK_SRC)/bt_chipset_pan1315.o
  OBJS_BTSTACK+=$(PATH_BTSTACK_SRC)/pan1315_init.o
  CFLAGS+=-DBT_CHIPSET_PAN1315=1
endif

INCL_BTSTACK=-I$(PATH_BTSTACK_INC) \
             -I$(PATH_BTSTACK_SRC) \

#==============================================================================
#  Application.
#==============================================================================
OBJS_APPL=$(PATH_APPL_SRC)/api.o \
          $(PATH_APPL_SRC)/serial.o \

INCL_APPL=-I$(PATH_APPL_INC)

#==============================================================================
#  All objects for binary.
#==============================================================================
ASMS=$(ASMS_START) $(ASMS_UCOS)
OBJS=$(OBJS_BSP) $(OBJS_UCOS) $(OBJS_UCLIB) $(OBJS_STM32) $(OBJS_BTSTACK) $(OBJS_APPL)
INCL=$(INCL_BSP) $(INCL_UCOS) $(INCL_UCLIB) $(INCL_STM32) $(INCL_BTSTACK) $(INCL_APPL)
BIN=btstackfw

all: $(ASMS) $(OBJS)
	@$(ECHO) Linking Flash image...
	$(GCC) $(LFLAGS) -Wl,-Map,$(BIN).map -Tlinker.lds -o $(BIN).elf $(ASMS) $(OBJS)
	$(OBJCPY) -O binary $(BIN).elf $(BIN).bin
	@$(ECHO)
	@$(ECHO) Successfully built BTstack Flash image for $(BT_CHIPSET), STM32 UART DMA $(STM32_USART_DMA).
	@$(ECHO)

clean:
	$(RM) -f *.elf *.bin *.map $(ASMS) $(OBJS)
