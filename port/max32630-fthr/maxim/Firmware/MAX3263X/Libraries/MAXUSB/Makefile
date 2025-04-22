################################################################################
 # Copyright (C) 2016 Maxim Integrated Products, Inc., All Rights Reserved.
 #
 # Permission is hereby granted, free of charge, to any person obtaining a
 # copy of this software and associated documentation files (the "Software"),
 # to deal in the Software without restriction, including without limitation
 # the rights to use, copy, modify, merge, publish, distribute, sublicense,
 # and/or sell copies of the Software, and to permit persons to whom the
 # Software is furnished to do so, subject to the following conditions:
 #
 # The above copyright notice and this permission notice shall be included
 # in all copies or substantial portions of the Software.
 #
 # THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 # OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 # MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 # IN NO EVENT SHALL MAXIM INTEGRATED BE LIABLE FOR ANY CLAIM, DAMAGES
 # OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 # ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 # OTHER DEALINGS IN THE SOFTWARE.
 #
 # Except as contained in this notice, the name of Maxim Integrated
 # Products, Inc. shall not be used except as stated in the Maxim Integrated
 # Products, Inc. Branding Policy.
 #
 # The mere transfer of this software does not imply any licenses
 # of trade secrets, proprietary technology, copyrights, patents,
 # trademarks, maskwork rights, or any other form of intellectual
 # property whatsoever. Maxim Integrated Products, Inc. retains all
 # ownership rights.
 #
 # $Date: 2018-07-02 16:17:59 -0500 (Mon, 02 Jul 2018) $
 # $Revision: 35946 $
 #
 ###############################################################################

# This is the name of the build output file
PROJECT=maxusb

ifeq "$(TARGET)" ""
$(error TARGET must be specified)
endif
TARGET_UC:=$(shell echo $(TARGET) | tr a-z A-Z)
TARGET_LC:=$(shell echo $(TARGET) | tr A-Z a-z)
$(info $(TARGET_UC))

ifeq "$(COMPILER)" ""
$(error COMPILER must be specified)
endif

# This is the path to the peripheral drivers
ifeq "$(PERIPH_DRIVER_DIR)" ""
PERIPH_DRIVER_DIR=../$(TARGET_UC)PeriphDriver
endif

ifeq "$(BUILD_DIR)" ""
BUILD_DIR=./Build
endif

# This is the path to the CMSIS root directory
ifeq "$(CMSIS_ROOT)" ""
CMSIS_ROOT=../CMSIS
endif

# Select Full Speed or High Speed Library
ifeq "$(TARGET_UC)" "MAX32650"
TARGET_USB=MUSBHSFC
endif
ifeq "$(TARGET_UC)" "MAX32660"
TARGET_USB=MUSBHSFC
endif
ifeq "$(TARGET_UC)" "MAX32665"
TARGET_USB=MUSBHSFC
endif
ifeq "$(TARGET_UC)" "MAX32666"
TARGET_USB=MUSBHSFC
endif
ifeq "$(TARGET_UC)" "MAX32667"
TARGET_USB=MUSBHSFC
endif
ifeq "$(TARGET_UC)" "MAX32668"
TARGET_USB=MUSBHSFC
endif
ifeq "$(TARGET_UC)" "MAX32655"
TARGET_USB=MUSBHSFC
endif
ifeq "$(TARGET_UC)" "MAX32656"
TARGET_USB=MUSBHSFC
endif

# Source files for this test (add path to VPATH below)
SRCS  = usb.c
SRCS += usb_event.c
SRCS += enumerate.c
SRCS += hid_kbd.c
SRCS += hid_raw.c
SRCS += cdc_acm.c
SRCS += fifo.c
SRCS += msc.c
# Generic debug log, useful for minimally-invasive tracing of the USB stack
#SRCS += dbg_log.c

# Where to find source files for this project
ifeq "$(TARGET_USB)" "MUSBHSFC"
VPATH  = src/core/musbhsfc
else
VPATH  = src/core/arm
endif
VPATH += src/core
VPATH += src/devclass
VPATH += src/enumerate
VPATH += src/dbg_log

# Where to find header files for this project
ifeq "$(TARGET_USB)" "MUSBHSFC"
IPATH  = include/core/musbhsfc
else
IPATH  = include/core/arm
endif
IPATH += include/core
IPATH += include/devclass
IPATH += include/enumerate
IPATH += include/dbg_log
IPATH += $(CMSIS_ROOT)/Device/Maxim/$(TARGET)/Include
IPATH += $(CMSIS_ROOT)/Include
IPATH += $(PERIPH_DRIVER_DIR)/Include

# Include the rules for building for this target
include $(CMSIS_ROOT)/Device/Maxim/$(TARGET_UC)/Source/$(COMPILER)/$(TARGET_LC).mk

# Build this as a library
.DEFAULT_GOAL := lib
