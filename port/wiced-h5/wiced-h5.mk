#
# BTstack port for WICED framework
#

ifndef BT_CHIP 
$(error BT_CHIP not set - WICED BTstack port only supported with Broadcom Bluetooth chipset)
endif

NAME := BTstack_for_BCM$(BT_CHIP)$(BT_CHIP_REVISION)

GLOBAL_INCLUDES += \
	. \
	../../src \
	../../platform/embedded \
	../../platform/wiced \
	../../chipset/bcm \
	../../../../

# micro-ecc of WICED tree used for SECP256R1 in LE Secure Connections
$(NAME)_COMPONENTS += $(MICRO_ECC)

# additional CFLAGS
$(NAME)_CFLAGS += $(BTSTACK_CFLAGS)

# core BTstack sources
$(NAME)_SOURCES += \
	../../src/ad_parser.c    		      \
	../../src/ble/att_db.c                \
	../../src/ble/att_dispatch.c 		  \
	../../src/ble/att_server.c   		  \
	../../src/ble/gatt_client.c   		  \
	../../src/ble/gatt-service/battery_service_server.c   \
	../../src/ble/gatt-service/device_information_service_server.c   \
	../../src/ble/sm.c          		  \
	../../src/classic/hfp.c 			  \
	../../src/classic/hfp_ag.c 			  \
	../../src/classic/hfp_hf.c 			  \
	../../src/classic/hsp_ag.c            \
	../../src/classic/hsp_hs.c            \
	../../src/classic/rfcomm.c            \
	../../src/classic/sdp_server.c        \
	../../src/classic/sdp_client.c        \
	../../src/classic/sdp_client_rfcomm.c \
	../../src/classic/sdp_util.c          \
	../../src/classic/spp_server.c        \
	../../src/btstack_crypto.c            \
	../../src/btstack_linked_list.c       \
	../../src/btstack_memory.c            \
	../../src/btstack_memory_pool.c       \
	../../src/btstack_resample.c          \
	../../src/btstack_run_loop.c          \
	../../src/btstack_util.c              \
	../../src/btstack_slip.c              \
	../../src/btstack_tlv.c               \
	../../src/hci.c                       \
	../../src/hci_cmd.c                   \
	../../src/hci_dump.c                  \
	../../src/hci_transport_h5.c          \
	../../src/l2cap.c                     \
	../../src/l2cap_signaling.c           \
	../../example/sco_demo_util.c         \

# WICED port incl. support for Broadcom chipset
$(NAME)_SOURCES += \
	main.c                                     			       \
	btstack_aes128_wiced.c 					  			       \
	../../platform/wiced/btstack_link_key_db_wiced_dct.c       \
	../../platform/wiced/btstack_run_loop_wiced.c              \
	../../platform/wiced/btstack_stdin_wiced.c                 \
	../../platform/wiced/btstack_uart_block_wiced.c 		   \
	../../platform/wiced/le_device_db_wiced_dct.c        	   \
	../../chipset/bcm/btstack_chipset_bcm.c    				   \
	../../chipset/bcm/btstack_chipset_bcm_download_firmware.c  \

ifeq ($(BT_CHIP_XTAL_FREQUENCY),)
$(NAME)_SOURCES += ../../../drivers/bluetooth/firmware/$(BT_CHIP)$(BT_CHIP_REVISION)/$(BT_FIRMWARE_FILE)
else
$(NAME)_SOURCES += ../../../drivers/bluetooth/firmware/$(BT_CHIP)$(BT_CHIP_REVISION)/$(BT_CHIP_XTAL_FREQUENCY)/$(BT_FIRMWARE_FILE)
endif
