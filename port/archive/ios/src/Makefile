TARGET := iphone:clang
TARGET_IPHONEOS_DEPLOYMENT_VERSION = 3.0
TARGET_IPHONEOS_DEPLOYMENT_VERSION_arm64 = 7.0
ARCHS = armv7 arm64

BTSTACK_ROOT=../../..

LIBRARY_NAME = libBTstack
libBTstack_FILES = \
	$(BTSTACK_ROOT)/src/btstack_linked_list.c \
	$(BTSTACK_ROOT)/src/btstack_run_loop.c \
	$(BTSTACK_ROOT)/src/hci_cmd.c \
	$(BTSTACK_ROOT)/src/hci_dump.c \
	$(BTSTACK_ROOT)/src/btstack_util.c \
	$(BTSTACK_ROOT)/platform/daemon/src/btstack.c \
 	$(BTSTACK_ROOT)/platform/daemon/src/daemon_cmds.c \
    $(BTSTACK_ROOT)/platform/daemon/src/socket_connection.c \
	$(BTSTACK_ROOT)/platform/corefoundation/btstack_run_loop_corefoundation.m \
    $(BTSTACK_ROOT)/platform/posix/btstack_run_loop_posix.c \
	$(BTSTACK_ROOT)/src/classic/sdp_util.c \
	$(BTSTACK_ROOT)/src/classic/spp_server.c \

libBTstack_CFLAGS = -I$(BTSTACK_ROOT)/src/ble -I$(BTSTACK_ROOT)/src -I..

TOOL_NAME = BTdaemon
BTdaemon_FILES = \
	$(libBTstack_FILES)  	  							  \
	$(BTSTACK_ROOT)/src/ad_parser.c                       \
	$(BTSTACK_ROOT)/src/btstack_memory.c                  \
	$(BTSTACK_ROOT)/src/btstack_memory_pool.c             \
	$(BTSTACK_ROOT)/src/classic/rfcomm.c                  \
	$(BTSTACK_ROOT)/src/classic/sdp_server.c              \
	$(BTSTACK_ROOT)/src/hci.c                             \
	$(BTSTACK_ROOT)/src/l2cap.c                           \
	$(BTSTACK_ROOT)/src/l2cap_signaling.c                 \
    $(BTSTACK_ROOT)/src/classic/sdp_client.c              \
    $(BTSTACK_ROOT)/src/classic/sdp_client_rfcomm.c       \
	$(BTSTACK_ROOT)/platform/corefoundation/btstack_device_name_db_corefoundation.m  \
	$(BTSTACK_ROOT)/platform/corefoundation/btstack_link_key_db_corefoundation.m  \
    $(BTSTACK_ROOT)/platform/corefoundation/rfcomm_service_db_corefoundation.m \
	$(BTSTACK_ROOT)/platform/daemon/src/daemon.c  		  \
	btstack_control_iphone.m  \
	hci_transport_h4_iphone.c \
	platform_iphone.m         \
	../SpringBoardAccess/SpringBoardAccess.c       \

BTdaemon_CFLAGS = \
	-I$(BTSTACK_ROOT)/src \
	-I$(BTSTACK_ROOT)/platform/posix \
	-I$(BTSTACK_ROOT)/platform/daemon/src \
	-I$(BTSTACK_ROOT)/platform/corefoundation \
	-I..
BTdaemon_PRIVATE_FRAMEWORKS = IOKit


include $(THEOS)/makefiles/common.mk
include $(THEOS_MAKE_PATH)/library.mk
include $(THEOS_MAKE_PATH)/tool.mk

before-all::
	../../../tool/get_version.sh

after-BTdaemon-stage::
	echo $(THEOS_STAGING_DIR)
	mkdir -p ../.theos/_/usr/local/lib
	mkdir -p ../.theos/_/usr/local/bin
	ln -s /usr/lib/libBTstack.dylib ../.theos/_/usr/local/lib
	ln -s /usr/bin/BTdaemon         ../.theos/_/usr/local/bin/BTdaemon
