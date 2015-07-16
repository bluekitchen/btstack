# uses Dustin Howett's theos-build system
# http://bit.ly/af0Evu
# http://hwtt.net/ths

include $(THEOS)/makefiles/common.mk

BTSTACK_ROOT=../../../..

APPLICATION_NAME = WiiMoteOpenGLDemo

VPATH += ../../CocoaTouch/src

WiiMoteOpenGLDemo_FILES = $(wildcard *.c) $(wildcard *.m) $(wildcard Classes/*.m)
WiiMoteOpenGLDemo_FILES += BTstackManager.m BTDiscoveryViewController.m BTDevice.m
WiiMoteOpenGLDemo_CFLAGS = -I. -I../.. -I Classes -I $(BTSTACK_ROOT)/include -I ../../CocoaTouch/include
WiiMoteOpenGLDemo_LDFLAGS = -L../../.theos/_/usr/lib/ -lbtstack
WiiMoteOpenGLDemo_FRAMEWORKS = Foundation UIKit CoreGraphics  OpenGLES QuartzCore

include $(FW_MAKEDIR)/application.mk
