#******************************************************************************
#
# Makefile - Rules for building the libraries, examples and docs.
#
# Copyright (c) 2017, Ambiq Micro
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
# 
# 1. Redistributions of source code must retain the above copyright notice,
# this list of conditions and the following disclaimer.
# 
# 2. Redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution.
# 
# 3. Neither the name of the copyright holder nor the names of its
# contributors may be used to endorse or promote products derived from this
# software without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#
# This is part of revision 1.2.11 of the AmbiqSuite Development Package.
#
#******************************************************************************
TARGET := EXAMPLE
COMPILERNAME := gcc
PROJECT := btstack_template_gcc
CONFIG := bin
AM_SoftwareRoot ?= ../../../../..

SHELL:=/bin/bash
#### Setup ####

TOOLCHAIN ?= arm-none-eabi
PART = apollo2
CPU = cortex-m4
FPU = fpv4-sp-d16
FABI = softfp

LINKER_FILE := ./btstack_template.ld
STARTUP_FILE := ./startup_$(COMPILERNAME).c

#### Required Executables ####
CC = $(TOOLCHAIN)-gcc
GCC = $(TOOLCHAIN)-gcc
CPP = $(TOOLCHAIN)-cpp
LD = $(TOOLCHAIN)-ld
CP = $(TOOLCHAIN)-objcopy
OD = $(TOOLCHAIN)-objdump
RD = $(TOOLCHAIN)-readelf
AR = $(TOOLCHAIN)-ar
SIZE = $(TOOLCHAIN)-size
RM = $(shell which rm 2>/dev/null)

EXECUTABLES = CC LD CP OD AR RD SIZE GCC
K := $(foreach exec,$(EXECUTABLES),\
        $(if $(shell which $($(exec)) 2>/dev/null),,\
        $(info $(exec) not found on PATH ($($(exec))).)$(exec)))
$(if $(strip $(value K)),$(info Required Program(s) $(strip $(value K)) not found))

ifneq ($(strip $(value K)),)
all clean:
	$(info Tools $(TOOLCHAIN)-$(COMPILERNAME) not installed.)
	$(RM) -rf bin
else

DEFINES = -DPART_$(PART)
DEFINES+= -DAM_PACKAGE_BGA
DEFINES += -DAM_PART_APOLLO2

INCLUDES = -I../src
INCLUDES += -I${AM_SoftwareRoot}/boards/apollo2_evb_am_ble/bsp
INCLUDES += -I${AM_SoftwareRoot}/utils
INCLUDES += -I${AM_SoftwareRoot}/devices
INCLUDES += -I${AM_SoftwareRoot}/mcu/apollo2
INCLUDES += -I${AM_SoftwareRoot}

VPATH = ${AM_SoftwareRoot}/utils
VPATH +=:${AM_SoftwareRoot}/devices
VPATH +=:../src

# BTstack
INCLUDES += -I${AM_SoftwareRoot}/third_party/btstack/src
INCLUDES += -I${AM_SoftwareRoot}/third_party/btstack/platform/embedded
INCLUDES += -I${AM_SoftwareRoot}/third_party/btstack/port/apollo2-em9304
INCLUDES += -I${AM_SoftwareRoot}/third_party/btstack/3rd-party/micro-ecc

VPATH += ${AM_SoftwareRoot}/third_party/btstack/3rd-party/micro-ecc
VPATH += ${AM_SoftwareRoot}/third_party/btstack/example
VPATH += ${AM_SoftwareRoot}/third_party/btstack/platform/embedded
VPATH += ${AM_SoftwareRoot}/third_party/btstack/port/apollo2-em9304
VPATH += ${AM_SoftwareRoot}/third_party/btstack/src
VPATH += ${AM_SoftwareRoot}/third_party/btstack/src/ble
VPATH += ${AM_SoftwareRoot}/third_party/btstack/src/ble/gatt-service

SRC += btstack_em9304_spi_embedded.c
SRC += btstack_port.c
SRC += btstack_run_loop_embedded.c
SRC += btstack_uart_block_embedded.c
SRC += hci_dump_embedded_stdout.c
SRC += uECC.c

-include ${AM_SoftwareRoot}/third_party/btstack/src/Makefile.inc
-include ${AM_SoftwareRoot}/third_party/btstack/src/ble/Makefile.inc
-include ${AM_SoftwareRoot}/third_party/btstack/src/ble/gatt-service/Makefile.inc

SRC += $(SRC_FILES)
SRC += $(filter-out att_db_util.c le_device_db_tlv.c,$(SRC_BLE_FILES))
SRC += $(SRC_BLE_GATT_SERVICE_FILES)

SRC += $(TARGET).c

SRC += am_util_delay.c
SRC += am_util_faultisr.c
SRC += am_util_stdio.c
SRC += am_devices_button.c
# SRC += am_devices_em9304.c
SRC += startup_gcc.c

CSRC = $(filter %.c,$(SRC))
ASRC = $(filter %.s,$(SRC))

OBJS = $(CSRC:%.c=$(CONFIG)/%.o)
OBJS+= $(ASRC:%.s=$(CONFIG)/%.o)

DEPS = $(CSRC:%.c=$(CONFIG)/%.d)
DEPS+= $(ASRC:%.s=$(CONFIG)/%.d)

LIBS = ${AM_SoftwareRoot}/boards/apollo2_evb_am_ble/bsp/gcc/bin/libam_bsp.a
LIBS += ${AM_SoftwareRoot}/mcu/apollo2/hal/gcc/bin/libam_hal.a
INCS = ${AM_SoftwareRoot}/boards/apollo2_evb_am_ble/bsp/am_bsp_gpio.h
INCS += ${AM_SoftwareRoot}/mcu/apollo2/hal/am_hal_pin.h

CFLAGS = -mthumb -mcpu=$(CPU) -mfpu=$(FPU) -mfloat-abi=$(FABI)
CFLAGS+= -ffunction-sections -fdata-sections
CFLAGS+= -MMD -MP -std=c99 -Wall -g
CFLAGS+= -O0
CFLAGS+= $(DEFINES)
CFLAGS+= $(INCLUDES)
CFLAGS+= -Dgcc

AM_LFLAGS = -mthumb -mcpu=$(CPU) -mfpu=$(FPU) -mfloat-abi=$(FABI)
AM_LFLAGS+= -nostartfiles -static
AM_LFLAGS+= -Wl,--gc-sections,--entry,am_reset_isr,-Map,$(CONFIG)/$(TARGET).map
AM_LFLAGS+= -Wl,--start-group -lm -lc -lgcc -Wl,--end-group
AM_LFLAGS+= 

# Additional user specified CFLAGS
CFLAGS+=$(EXTRA_CFLAGS)

CPFLAGS = -Obinary

ODFLAGS = -S

#### Rules ####
all: directories $(CONFIG)/$(TARGET).bin

directories: $(CONFIG)

$(CONFIG):
	@mkdir -p $@

$(CONFIG)/%.o: %.c $(CONFIG)/%.d $(INCS)
	@echo " Compiling $(COMPILERNAME) $<" ;\
	$(CC) -c $(CFLAGS) $< -o $@

$(CONFIG)/%.o: %.s $(CONFIG)/%.d $(INCS)
	@echo " Assembling $(COMPILERNAME) $<" ;\
	$(CC) -c $(CFLAGS) $< -o $@


$(CONFIG)/$(TARGET).elf: $(OBJS) $(LIBS)
	@echo " Linking $(COMPILERNAME) $@" ;\
	$(CC) -Wl,-T,$(LINKER_FILE) -o $@ $(OBJS) $(LIBS) $(AM_LFLAGS)

$(CONFIG)/$(TARGET).bin: $(CONFIG)/$(TARGET).elf
	@echo " Copying $(COMPILERNAME) $@..." ;\
	$(CP) $(CPFLAGS) $< $@ ;\
	$(OD) $(ODFLAGS) $< > $(CONFIG)/$(TARGET).lst

clean:
	@echo "Cleaning..." ;\
	$(RM) -f $(OBJS) $(DEPS) \
	    $(CONFIG)/$(TARGET).bin $(CONFIG)/$(TARGET).elf \
	    $(CONFIG)/$(TARGET).lst $(CONFIG)/$(TARGET).map \
	    $(CONFIG)/$(TARGET).h

$(CONFIG)/%.d: ;

# Automatically include any generated dependencies
-include $(DEPS)
endif
.PHONY: all clean directories

