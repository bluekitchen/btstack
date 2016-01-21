
# lipoplastic setup for armv6 + arm64 compilation
TARGET := iphone:clang
THEOS_PLATFORM_SDK_ROOT_armv6 = /Applications/Xcode-4.4.1.app/Contents/Developer
SDKVERSION_armv6 = 5.1
TARGET_IPHONEOS_DEPLOYMENT_VERSION = 3.0
TARGET_IPHONEOS_DEPLOYMENT_VERSION_arm64 = 7.0
ARCHS = armv6 arm64

BTSTACK_ROOT=../../..
VPATH += $(BTSTACK_ROOT)/example/daemon

ADDITIONAL_CFLAGS  = -I.. \
	-I$(BTSTACK_ROOT)/src \
	-I$(BTSTACK_ROOT)/platform/daemon \
	-I$(BTSTACK_ROOT)/platform/posix  \

ADDITIONAL_LDFLAGS = -L../src/.theos/obj -lBTstack

TOOL_NAME = inquiry l2cap_test rfcomm_echo rfcomm_cat rfcomm_test l2cap_server l2cap_throughput

inquiry_FILES = inquiry.c
rfcomm_echo_FILES = rfcomm_echo.c
rfcomm_cat_FILES  = rfcomm_cat.c
rfcomm_test_FILES = rfcomm_test.c
l2cap_test_FILES  = test.c
l2cap_server_FILES = l2cap_server.c
l2cap_throughput_FILES = l2cap_throughput.c

include $(THEOS)/makefiles/common.mk
include $(THEOS_MAKE_PATH)/tool.mk
