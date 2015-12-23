
NAME := BTstack_for_BCM$(BT_CHIP)$(BT_CHIP_REVISION)

GLOBAL_INCLUDES += . ../../src ../../platform/embedded

$(NAME)_SOURCES += \
	main.c                                \
	run_loop_wiced.c                      \
	hci_transport_h4_wiced.c              \
	../../src/bk_linked_list.c            \
	../../src/btstack_memory.c            \
	../../src/classic/remote_device_db_memory.c \
	../../src/classic/rfcomm.c            \
	../../src/classic/sdp.c               \
	../../src/classic/sdp_client.c        \
	../../src/classic/sdp_parser.c        \
	../../src/classic/sdp_query_rfcomm.c  \
	../../src/classic/sdp_query_util.c    \
	../../src/classic/sdp_util.c          \
	../../src/hci.c                       \
	../../src/hci_cmds.c                  \
	../../src/hci_dump.c                  \
	../../src/l2cap.c                     \
	../../src/l2cap_signaling.c           \
	../../src/memory_pool.c               \
	../../src/run_loop.c                  \
	../../src/utils.c                     \
	../../../drivers/bluetooth/firmware/$(BT_CHIP)$(BT_CHIP_REVISION)/bt_firmware_image.c \
