TARGET := iphone:clang
TARGET_IPHONEOS_DEPLOYMENT_VERSION = 3.0
ARCHS = armv7 arm64 arm64e

BTSTACK_ROOT=../../..

ADDITIONAL_CFLAGS =  -I.. -I$(BTSTACK_ROOT)/include

ifeq ($(FINALPACKAGE),1)
ADDITIONAL_LDFLAGS = -L../.theos/obj -lBTstack
else 
ADDITIONAL_LDFLAGS = -L../.theos/obj/debug -lBTstack
endif

TOOL_NAME = inquiry l2cap-test rfcomm-echo rfcomm-cat rfcomm-test l2cap-server l2cap-throughput

inquiry_FILES          = $(BTSTACK_ROOT)/example/daemon/inquiry.c
rfcomm-echo_FILES      = $(BTSTACK_ROOT)/example/daemon/rfcomm-echo.c
rfcomm-cat_FILES       = $(BTSTACK_ROOT)/example/daemon/rfcomm-cat.c
rfcomm-test_FILES      = $(BTSTACK_ROOT)/example/daemon/rfcomm-test.c
l2cap-test_FILES       = $(BTSTACK_ROOT)/example/daemon/test.c
l2cap-server_FILES     = $(BTSTACK_ROOT)/example/daemon/l2cap-server.c
l2cap-throughput_FILES = $(BTSTACK_ROOT)/example/daemon/l2cap-throughput.c

inquiry_CODESIGN_FLAGS = -S../ent.xml
rfcomm-echo_CODESIGN_FLAGS = -S../ent.xml
rfcomm-cat_CODESIGN_FLAGS = -S../ent.xml
rfcomm-test_CODESIGN_FLAGS = -S../ent.xml
l2cap-test_CODESIGN_FLAGS = -S../ent.xml
l2cap-server_CODESIGN_FLAGS = -S../ent.xml
l2cap-throughput_CODESIGN_FLAGS = -S../ent.xml

include $(THEOS)/makefiles/common.mk
include $(THEOS_MAKE_PATH)/tool.mk
