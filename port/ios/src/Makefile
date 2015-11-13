# lipoplastic setup for armv6 + arm64 compilation
TARGET := iphone:clang
THEOS_PLATFORM_SDK_ROOT_armv6 = /Applications/Xcode-4.4.1.app/Contents/Developer
SDKVERSION_armv6 = 5.1
TARGET_IPHONEOS_DEPLOYMENT_VERSION = 3.0
TARGET_IPHONEOS_DEPLOYMENT_VERSION_arm64 = 7.0
ARCHS = armv6 arm64

BTSTACK_ROOT=../../..
POSIX_ROOT=$(BTSTACK_ROOT)/platform/posix
DAEMON_ROOT=$(BTSTACK_ROOT)/platform/daemon

VPATH += $(BTSTACK_ROOT)/src \
		 $(BTSTACK_ROOT)/src/classic 

LIBRARY_NAME = libBTstack
libBTstack_FILES = \
	hci_cmds.c \
	hci_dump.c \
	linked_list.c \
	run_loop.c \
	sdp_util.c \
	utils.c \
	$(BTSTACK_ROOT)/platform/cocoa/run_loop_cocoa.m \
	$(POSIX_ROOT)/src/run_loop_posix.c \
	$(DAEMON_ROOT)/src/btstack.c \
	$(DAEMON_ROOT)/src/socket_connection.c \

libBTstack_CFLAGS = -I$(BTSTACK_ROOT)/src/ble -I$(BTSTACK_ROOT)/src -I..

TOOL_NAME = BTdaemon
BTdaemon_FILES = \
	$(libBTstack_FILES)  	\
	btstack_memory.c        \
	hci.c                   \
	l2cap.c                 \
	l2cap_signaling.c       \
	memory_pool.c           \
	rfcomm.c                \
	sdp.c                   \
    sdp_client.c            \
    sdp_parser.c            \
    sdp_query_rfcomm.c      \
    sdp_query_util.c        \
	$(DAEMON_ROOT)/src/daemon.c  \
	$(POSIX_ROOT)/src/hci_transport_h4.c 		 	\
	$(BTSTACK_ROOT)/port/ios/src/bt_control_iphone.m     	\
	$(BTSTACK_ROOT)/port/ios/src/hci_transport_h4_iphone.c  \
	$(BTSTACK_ROOT)/port/ios/src/platform_iphone.m       	\
	$(BTSTACK_ROOT)/port/ios/SpringBoardAccess/SpringBoardAccess.c \
	$(BTSTACK_ROOT)/platform/cocoa/remote_device_db_cocoa.m  	\

BTdaemon_CFLAGS = -I$(BTSTACK_ROOT)/src -I$(BTSTACK_ROOT)/platform/daemon/src -I..
BTdaemon_PRIVATE_FRAMEWORKS = IOKIT

before-all::
	../../../tool/get_version.sh

after-BTdaemon-stage::
	echo $(THEOS_STAGING_DIR)
	mkdir -p ../.theos/_/usr/local/lib
	mkdir -p ../.theos/_/usr/local/bin
	ln -s /usr/lib/libBTstack.dylib ../.theos/_/usr/local/lib
	ln -s /usr/bin/BTdaemon         ../.theos/_/usr/local/bin/BTdaemon

include $(THEOS)/makefiles/common.mk
include $(THEOS_MAKE_PATH)/library.mk
include $(THEOS_MAKE_PATH)/tool.mk
