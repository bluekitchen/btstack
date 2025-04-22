################################################################################
# Copyright (C) 2014 Maxim Integrated Products, Inc., All Rights Reserved.
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
# $Id: exactle.mk 20727 2015-12-21 21:38:41Z jeremy.brodt $
#
################################################################################

################################################################################
# This file can be included in a project makefile to build the library for the 
# project.
################################################################################

ifeq "$(BLE_DIR)" ""
$(error BLE_DIR must be specified)
endif

ifeq "$(COMPILER)" ""
$(error COMPILER must be specified)
endif

# Specify the build directory if not defined by the project
ifeq "$(BUILD_DIR)" ""
BLE_BUILD_DIR=$(CURDIR)/build/exactLE
else
BLE_BUILD_DIR=$(BUILD_DIR)/exactLE
endif

# Export paths needed by the peripheral driver makefile. Since the makefile to
# build the library will execute in a different directory, paths must be
# specified absolutely
export BLE_BUILD_DIR := ${abspath ${BLE_BUILD_DIR}}
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
LIBS += $(BLE_DIR)/libstack.a
LIBS += ${BLE_BUILD_DIR}/libexactle.a

# Add to include directory list
IPATH += \
	$(BLE_DIR)/src/apps/app \
	$(BLE_DIR)/src/apps/app/include \
	$(BLE_DIR)/src/apps/app/generic \
	$(BLE_DIR)/src/apps/app \
	$(BLE_DIR)/src/apps/datc \
	$(BLE_DIR)/src/apps/dats \
	$(BLE_DIR)/src/apps/fit \
	$(BLE_DIR)/src/apps/gluc \
	$(BLE_DIR)/src/apps/medc \
	$(BLE_DIR)/src/apps/meds \
	$(BLE_DIR)/src/apps/tag \
	$(BLE_DIR)/src/apps/watch \
	$(BLE_DIR)/src/hci/em9301/max3263x \
	$(BLE_DIR)/src/hci/em9301 \
	$(BLE_DIR)/src/hci/dual_chip \
	$(BLE_DIR)/src/hci/include \
	$(BLE_DIR)/src/profiles/anpc \
	$(BLE_DIR)/src/profiles/bas \
	$(BLE_DIR)/src/profiles/blpc \
	$(BLE_DIR)/src/profiles/blps \
	$(BLE_DIR)/src/profiles/dis \
	$(BLE_DIR)/src/profiles/fmpl \
	$(BLE_DIR)/src/profiles/gatt \
	$(BLE_DIR)/src/profiles/glpc \
	$(BLE_DIR)/src/profiles/glps \
	$(BLE_DIR)/src/profiles/hrpc \
	$(BLE_DIR)/src/profiles/hrps \
	$(BLE_DIR)/src/profiles/htpc \
	$(BLE_DIR)/src/profiles/htps \
	$(BLE_DIR)/src/profiles/paspc \
	$(BLE_DIR)/src/profiles/tipc \
	$(BLE_DIR)/src/profiles/wpc \
	$(BLE_DIR)/src/profiles/wspc \
	$(BLE_DIR)/src/profiles/wsps \
	$(BLE_DIR)/src/services \
	$(BLE_DIR)/src/stack/att \
	$(BLE_DIR)/src/stack/cfg \
	$(BLE_DIR)/src/stack/dm \
	$(BLE_DIR)/src/stack/hci \
	$(BLE_DIR)/src/stack/include \
	$(BLE_DIR)/src/stack/l2c \
	$(BLE_DIR)/src/stack/smp \
	$(BLE_DIR)/src/util \
	$(BLE_DIR)/src/wsf/common \
	$(BLE_DIR)/src/wsf/generic \
	$(BLE_DIR)/src/wsf/include

VPATH += \
	$(BLE_DIR)/src/apps/datc \
	$(BLE_DIR)/src/apps/dats \
	$(BLE_DIR)/src/apps/fit \
	$(BLE_DIR)/src/apps/gluc \
	$(BLE_DIR)/src/apps/medc \
	$(BLE_DIR)/src/apps/meds \
	$(BLE_DIR)/src/apps/tag \
	$(BLE_DIR)/src/apps/watch

# Add rule to build the Peripheral Driver Library
${BLE_BUILD_DIR}/libexactle.a: FORCE
	make -C ${BLE_DIR} lib BUILD_DIR=${BLE_BUILD_DIR}
