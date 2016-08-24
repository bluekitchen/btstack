#
# BTstack port for WICED framework
#

ifndef BT_CHIP 
$(error BT_CHIP not set - WICED BTstack port only supported with Broadcom Bluetooth chipset)
endif

NAME := BTstack_for_BCM$(BT_CHIP)$(BT_CHIP_REVISION)

GLOBAL_INCLUDES += . ../../src ../../platform/embedded ../../chipset/bcm ../../../../

# core BTstack sources
$(NAME)_SOURCES += \
	../../src/ble/ad_parser.c		      \
	../../src/ble/att_db.c                \
	../../src/ble/att_dispatch.c 		  \
	../../src/ble/att_server.c   		  \
	../../src/ble/le_device_db_memory.c   \
	../../src/ble/sm.c          		  \
	../../src/classic/hsp_hs.c            \
	../../src/classic/btstack_link_key_db_memory.c \
	../../src/classic/rfcomm.c            \
	../../src/classic/sdp_server.c        \
	../../src/classic/sdp_client.c        \
	../../src/classic/sdp_client_rfcomm.c \
	../../src/classic/sdp_util.c          \
	../../src/classic/spp_server.c        \
	../../src/btstack_linked_list.c       \
	../../src/btstack_memory.c            \
	../../src/btstack_memory_pool.c       \
	../../src/btstack_run_loop.c          \
	../../src/btstack_util.c              \
	../../src/hci.c                       \
	../../src/hci_cmd.c                   \
	../../src/hci_dump.c                  \
	../../src/l2cap.c                     \
	../../src/l2cap_signaling.c           \

# WICED port incl. support for Broadcom chipset
$(NAME)_SOURCES += \
	main.c                                     \
	btstack_run_loop_wiced.c                   \
	hci_transport_h4_wiced.c                   \
	../../chipset/bcm/btstack_chipset_bcm.c    \

ifeq ($(BT_CHIP_XTAL_FREQUENCY),)
$(NAME)_SOURCES += ../../../drivers/bluetooth/firmware/$(BT_CHIP)$(BT_CHIP_REVISION)/bt_firmware_image.c
else
$(NAME)_SOURCES += ../../../drivers/bluetooth/firmware/$(BT_CHIP)$(BT_CHIP_REVISION)/$(BT_CHIP_XTAL_FREQUENCY)/bt_firmware_image.c
endif
