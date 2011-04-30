include $(THEOS)/makefiles/common.mk

BUNDLE_NAME = BTstack
BTstack_FILES = PrefsViewController.m BluetoothController.m BluetoothTableViewAdapter.m
BTstack_INSTALL_PATH = /Library/PreferenceBundles
BTstack_FRAMEWORKS = UIKit
BTstack_CFLAGS = -I../include -g
BTstack_LDFLAGS = -L../src -lbtstack
BTstack_PRIVATE_FRAMEWORKS = Preferences

include $(THEOS_MAKE_PATH)/bundle.mk

internal-stage::
	$(ECHO_NOTHING)mkdir -p $(THEOS_STAGING_DIR)/Library/PreferenceLoader/Preferences$(ECHO_END)
	$(ECHO_NOTHING)cp entry.plist $(THEOS_STAGING_DIR)/Library/PreferenceLoader/Preferences/BTstack.plist$(ECHO_END)
