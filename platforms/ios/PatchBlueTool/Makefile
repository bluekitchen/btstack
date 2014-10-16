# lipoplastic setup for armv6 + arm64 compilation
TARGET := iphone:clang
THEOS_PLATFORM_SDK_ROOT_armv6 = /Applications/Xcode-4.4.1.app/Contents/Developer
SDKVERSION_armv6 = 5.1
TARGET_IPHONEOS_DEPLOYMENT_VERSION = 3.0
TARGET_IPHONEOS_DEPLOYMENT_VERSION_arm64 = 7.0
ARCHS = armv7 arm64

TOOL_NAME = PatchBlueTool

PatchBlueTool_FILES = PatchBlueTool.c

include $(THEOS)/makefiles/common.mk
include $(THEOS_MAKE_PATH)/tool.mk
