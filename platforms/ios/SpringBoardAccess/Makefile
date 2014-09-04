# lipoplastic setup for armv6 + arm64 compilation
TARGET := iphone:clang
THEOS_PLATFORM_SDK_ROOT_armv6 = /Applications/Xcode-4.4.1.app/Contents/Developer
SDKVERSION_armv6 = 5.1
TARGET_IPHONEOS_DEPLOYMENT_VERSION = 3.0
TARGET_IPHONEOS_DEPLOYMENT_VERSION_arm64 = 7.0
ARCHS = armv6 arm64

TWEAK_NAME = SpringBoardAccess
SpringBoardAccess_FILES = Tweak.xm
SpringBoardAccess_LIBRARIES = substrate
SpringBoardAccess_FRAMEWORKS = UIKit
SpringBoardAccess_PRIVATE_FRAMEWORKS = BluetoothManager

TOOL_NAME = SpringBoardAccess-test
SpringBoardAccess-test_FILES = SpringBoardAccess-test.c SpringBoardAccess.c
SpringBoardAccess-test_FRAMEWORKS = CoreFoundation Foundation

include $(THEOS)/makefiles/common.mk
include $(THEOS_MAKE_PATH)/tool.mk
include $(THEOS_MAKE_PATH)/tweak.mk
