# this uses rpetrich's lipoplastic feature not present in dhowett's repository
# you need to install Xcode 4.4.1 as /Applications/Xcode-4.4.1.app
 
SUBPROJECTS = src example PatchBlueTool PrefsBundle

include $(THEOS)/makefiles/common.mk
include $(THEOS_MAKE_PATH)/aggregate.mk

distclean:
	rm -rf layout btstack_config.h src/Makefile example/Makefile Makefile
