# lipoplastic setup for armv6 + arm64 compilation
TARGET := iphone:clang
THEOS_PLATFORM_SDK_ROOT_armv6 = /Applications/Xcode-4.4.1.app/Contents/Developer
SDKVERSION_armv6 = 5.1
TARGET_IPHONEOS_DEPLOYMENT_VERSION = 3.0
TARGET_IPHONEOS_DEPLOYMENT_VERSION_arm64 = 7.0
ARCHS = armv7 arm64

BUNDLE_NAME = BTstack
BTstack_FILES = PrefsViewController.m BluetoothController.m BluetoothTableViewAdapter.m
BTstack_INSTALL_PATH = /Library/PreferenceBundles
BTstack_FRAMEWORKS = UIKit
BTstack_CFLAGS = -I.. -I../../../include -g

BTstack_LDFLAGS = -L../src/.theos/obj -lBTstack
BTstack_PRIVATE_FRAMEWORKS = Preferences

include $(THEOS)/makefiles/common.mk
include $(THEOS_MAKE_PATH)/bundle.mk

internal-stage::
	$(ECHO_NOTHING)mkdir -p $(THEOS_STAGING_DIR)/Library/PreferenceLoader/Preferences$(ECHO_END)
	$(ECHO_NOTHING)cp entry.plist $(THEOS_STAGING_DIR)/Library/PreferenceLoader/Preferences/BTstack.plist$(ECHO_END)
