# lipoplastic setup for armv6 + arm64 compilation
TARGET := iphone:clang
THEOS_PLATFORM_SDK_ROOT_armv6 = /Applications/Xcode-4.4.1.app/Contents/Developer
SDKVERSION_armv6 = 5.1
TARGET_IPHONEOS_DEPLOYMENT_VERSION = 3.0
TARGET_IPHONEOS_DEPLOYMENT_VERSION_arm64 = 7.0
ARCHS = armv6 arm64

BTSTACK_ROOT=../../..

VPATH += $(BTSTACK_ROOT)/src \
		 $(BTSTACK_ROOT)/src/classic \
		 $(BTSTACK_ROOT)/platform/posix \
		 $(BTSTACK_ROOT)/platform/daemon \
		 $(BTSTACK_ROOT)/platform/cocoa \
		 $(BTSTACK_ROOT)/port/ios/SpringBoardAccess

LIBRARY_NAME = libBTstack
libBTstack_FILES = \
	btstack.c \
	btstack_linked_list.c \
	btstack_run_loop.c \
	btstack_run_loop_cocoa.m \
	btstack_run_loop_posix.c \
	daemon_cmds.c \
	hci_cmd.c \
	hci_dump.c \
	sdp_util.c \
	socket_connection.c \
	btstack_util.c \

libBTstack_CFLAGS = -I$(BTSTACK_ROOT)/src/ble -I$(BTSTACK_ROOT)/src -I..

TOOL_NAME = BTdaemon
BTdaemon_FILES = \
	$(libBTstack_FILES)  	  \
	btstack_control_iphone.m  \
	btstack_memory.c          \
	btstack_memory_pool.c     \
	daemon.c  				  \
	hci.c                     \
	hci_transport_h4_iphone.c \
	l2cap.c                   \
	l2cap_signaling.c         \
	platform_iphone.m         \
	remote_device_db_cocoa.m  \
	rfcomm.c                  \
	sdp.c                     \
	SpringBoardAccess.c       \
    rfcomm_service_db_cocoa.m \
    sdp_client.c              \
    sdp_parser.c              \
    sdp_query_rfcomm.c        \
    sdp_query_util.c          \

BTdaemon_CFLAGS = \
	-I$(BTSTACK_ROOT)/src \
	-I$(BTSTACK_ROOT)/platform/posix \
	-I$(BTSTACK_ROOT)/platform/daemon \
	-I..
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
