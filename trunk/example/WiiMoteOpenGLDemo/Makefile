# uses Dustin Howett's theos-build system
# http://bit.ly/af0Evu
# http://hwtt.net/ths

include $(THEOS)/makefiles/common.mk

APPLICATION_NAME = WiiMoteOpenGLDemo

WiiMoteOpenGLDemo_FILES = $(wildcard *.c) $(wildcard *.m) $(wildcard Classes/*.m)
WiiMoteOpenGLDemo_FILES += ../../CocoaTouch/src/BTstackManager.m ../../CocoaTouch/src/BTDiscoveryViewController.m ../../CocoaTouch/src/BTDevice.m
WiiMoteOpenGLDemo_CFLAGS = -I . -I Classes -I ../../include -I ../../CocoaTouch/include
WiiMoteOpenGLDemo_LDFLAGS = -L../../src -lbtstack
WiiMoteOpenGLDemo_FRAMEWORKS = Foundation UIKit CoreGraphics  OpenGLES QuartzCore

include $(FW_MAKEDIR)/application.mk
