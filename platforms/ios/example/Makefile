
# lipoplastic setup for armv6 + arm64 compilation
TARGET := iphone:clang
THEOS_PLATFORM_SDK_ROOT_armv6 = /Applications/Xcode-4.4.1.app/Contents/Developer
SDKVERSION_armv6 = 5.1
TARGET_IPHONEOS_DEPLOYMENT_VERSION = 3.0
TARGET_IPHONEOS_DEPLOYMENT_VERSION_arm64 = 7.0
ARCHS = armv6 arm64

BTSTACK_ROOT=../../..
VPATH += $(BTSTACK_ROOT)/example/daemon

ADDITIONAL_CFLAGS =  -I.. -I$(BTSTACK_ROOT)/include
ADDITIONAL_LDFLAGS = -L../src/.theos/obj -lBTstack

TOOL_NAME = inquiry l2cap-test rfcomm-echo rfcomm-cat rfcomm-test l2cap-server l2cap-throughput

inquiry_FILES = inquiry.c
rfcomm-echo_FILES = rfcomm-echo.c
rfcomm-cat_FILES = rfcomm-cat.c
rfcomm-test_FILES = rfcomm-test.c
l2cap-test_FILES = test.c
l2cap-server_FILES = l2cap-server.c
l2cap-throughput_FILES = l2cap-throughput.c

include $(THEOS)/makefiles/common.mk
include $(THEOS_MAKE_PATH)/tool.mk
