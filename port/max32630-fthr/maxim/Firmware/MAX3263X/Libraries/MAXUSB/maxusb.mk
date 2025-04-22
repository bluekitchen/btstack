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
 # $Date: 2018-06-11 10:11:56 -0500 (Mon, 11 Jun 2018) $ 
 # $Revision: 35430 $
 #
 ###############################################################################

ifeq "$(MAXUSB_DIR)" ""
$(error MAXUSB_DIR must be specified")
endif

# Specify the build directory if not defined by the project
ifeq "$(BUILD_DIR)" ""
MAXUSB_BUILD_DIR=$(CURDIR)/build/MAXUSB
else
MAXUSB_BUILD_DIR=$(BUILD_DIR)/MAXUSB
endif

# Export paths needed by the peripheral driver makefile. Since the makefile to
# build the library will execute in a different directory, paths must be
# specified absolutely
MAXUSB_BUILD_DIR := ${abspath ${MAXUSB_BUILD_DIR}}
export TOOL_DIR := ${abspath ${TOOL_DIR}}
export CMSIS_ROOT := ${abspath ${CMSIS_ROOT}}
export PERIPH_DRIVER_DIR := ${abspath ${PERIPH_DRIVER_DIR}}

# Export other variables needed by the peripheral driver makefile
export TARGET
export COMPILER
export TARGET_MAKEFILE
export PROJ_CFLAGS
export PROJ_LDFLAGS

# Add to library list
LIBS += ${MAXUSB_BUILD_DIR}/maxusb.a

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

# Add to include directory list
ifeq "$(TARGET_USB)" "MUSBHSFC"
IPATH += ${MAXUSB_DIR}/include/core/musbhsfc
else
IPATH += ${MAXUSB_DIR}/include/core/arm
endif
IPATH += ${MAXUSB_DIR}/include
IPATH += ${MAXUSB_DIR}/include/core
IPATH += ${MAXUSB_DIR}/include/enumerate
IPATH += ${MAXUSB_DIR}/include/devclass
IPATH += ${MAXUSB_DIR}/include/dbg_log

# Add rule to build the Driver Library
${MAXUSB_BUILD_DIR}/maxusb.a: FORCE
	$(MAKE) -C ${MAXUSB_DIR} lib BUILD_DIR=${MAXUSB_BUILD_DIR}
