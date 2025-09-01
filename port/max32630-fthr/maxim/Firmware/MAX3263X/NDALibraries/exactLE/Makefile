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
# $Id: Makefile 20727 2015-12-21 21:38:41Z jeremy.brodt $
#
################################################################################

# This is the name of the build output file
PROJECT=libexactle

ifeq "$(TARGET)" ""
$(error TARGET must be specified)
endif

ifeq "$(COMPILER)" ""
$(error COMPILER must be specified)
endif

# This is the path to the peripheral drivers
ifeq "$(PERIPH_DRIVER_DIR)" ""
$(error PERIPH_DRIVER_DIR must be specified)
endif

ifeq "$(BUILD_DIR)" ""
BUILD_DIR=./build
endif

# This is the path to the CMSIS root directory
ifeq "$(CMSIS_ROOT)" ""
CMSIS_ROOT=../CMSIS
endif

# Source files for this test (add path to VPATH below)
SRCS = \
  app_disc.c \
  app_main.c \
  app_server.c \
  app_slave.c \
  app_db.c \
  app_hw.c \
  app_ui.c \
  hci_cmd.c \
  hci_core.c \
  hci_evt.c \
  hci_tr.c \
  hci_vs.c \
  hci_drv.c \
  gatt_main.c \
  wspc_main.c \
  bas_main.c \
  hrps_main.c \
  wsps_main.c \
  glpc_main.c \
  wpc_main.c \
  dis_main.c \
  tipc_main.c \
  htps_main.c \
  glps_main.c \
  glps_db.c \
  anpc_main.c \
  hrpc_main.c \
  htpc_main.c \
  fmpl_main.c \
  blpc_main.c \
  paspc_main.c \
  blps_main.c \
  svc_batt.c \
  svc_bps.c \
  svc_core.c \
  svc_dis.c \
  svc_gls.c \
  svc_hid.c \
  svc_hrs.c \
  svc_hts.c \
  svc_px.c \
  svc_wp.c \
  svc_wss.c \
  cfg_stack.c \
  hci_main.c \
  wsf_buf.c \
  wsf_msg.c \
  wsf_queue.c \
  wsf_sec.c \
  wsf_timer.c \
  wsf_assert.c \
  wsf_os.c \
  wsf_trace.c

# Where to find source files for this project
VPATH = \
  src/apps/app \
  src/apps/app/generic \
  src/hci/em9301/max3263x \
  src/hci/em9301 \
  src/hci/dual_chip \
  src/profiles/anpc \
  src/profiles/bas \
  src/profiles/blpc \
  src/profiles/blps \
  src/profiles/dis \
  src/profiles/fmpl \
  src/profiles/gatt \
  src/profiles/glpc \
  src/profiles/glps \
  src/profiles/hrpc \
  src/profiles/hrps \
  src/profiles/htpc \
  src/profiles/htps \
  src/profiles/paspc \
  src/profiles/tipc \
  src/profiles/wpc \
  src/profiles/wspc \
  src/profiles/wsps \
  src/services \
  src/stack/cfg \
  src/stack/hci \
  src/wsf/common \
  src/wsf/generic

# Where to find header files for this project
IPATH += \
  src/apps/app \
  src/apps/app/include \
  src/hci/em9301/max3263x \
  src/hci/em9301 \
  src/hci/dual_chip \
  src/hci/include \
  src/profiles/anpc \
  src/profiles/bas \
  src/profiles/blpc \
  src/profiles/blps \
  src/profiles/dis \
  src/profiles/fmpl \
  src/profiles/gatt \
  src/profiles/glpc \
  src/profiles/glps \
  src/profiles/hrpc \
  src/profiles/hrps \
  src/profiles/htpc \
  src/profiles/htps \
  src/profiles/paspc \
  src/profiles/tipc \
  src/profiles/wpc \
  src/profiles/wspc \
  src/profiles/wsps \
  src/services \
  src/stack/att \
  src/stack/cfg \
  src/stack/dm \
  src/stack/hci \
  src/stack/include \
  src/stack/l2c \
  src/stack/smp \
  src/util \
  src/wsf/common \
  src/wsf/generic \
  src/wsf/include

IPATH += $(CMSIS_ROOT)/Device/Maxim/$(TARGET)/Include
IPATH += $(CMSIS_ROOT)/Include
IPATH += $(PERIPH_DRIVER_DIR)/Include

# Include the rules for building for this target
TARGET_UC:=$(shell echo $(TARGET) | tr a-z A-Z)
TARGET_LC:=$(shell echo $(TARGET) | tr A-Z a-z)
include $(CMSIS_ROOT)/Device/Maxim/$(TARGET_UC)/Source/$(COMPILER)/$(TARGET_LC).mk

# Build this as a library
.DEFAULT_GOAL := lib
