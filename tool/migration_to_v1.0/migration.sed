# sed script

# DEFINES
s/ANCS_CLIENT_CONNECTED/ANCS_EVENT_CLIENT_CONNECTED/g
s/ANCS_CLIENT_DISCONNECTED/ANCS_EVENT_CLIENT_DISCONNECTED/g
s/ANCS_CLIENT_NOTIFICATION/ANCS_EVENT_CLIENT_NOTIFICATION/g
s/ATT_HANDLE_VALUE_INDICATION_COMPLETE/ATT_EVENT_HANDLE_VALUE_INDICATION_COMPLETE/g
s/ATT_MTU_EXCHANGE_COMPLETE/ATT_EVENT_MTU_EXCHANGE_COMPLETE/g
s/BNEP_EVENT_OPEN_CHANNEL_COMPLETE/BNEP_EVENT_CHANNEL_OPENED/g
s/BTSTACK_EVENT_REMOTE_NAME_CACHED/DAEMON_EVENT_REMOTE_NAME_CACHED/g
s/COMMAND_COMPLETE_EVENT/HCI_EVENT_IS_COMMAND_COMPLETE/g
s/COMMAND_STATUS_EVENT/HCI_EVENT_IS_COMMAND_STATUS/g
s/GAP_DEDICATED_BONDING_COMPLETED/GAP_EVENT_DEDICATED_BONDING_COMPLETED/g
s/GAP_LE_ADVERTISING_REPORT/GAP_EVENT_ADVERTISING_REPORT/g
s/GAP_SECURITY_LEVEL/GAP_EVENT_SECURITY_LEVEL/g
s/GATT_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_RESULT/GATT_EVENT_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_RESULT/g
s/GATT_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT/GATT_EVENT_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT/g
s/GATT_CHARACTERISTIC_QUERY_RESULT/GATT_EVENT_CHARACTERISTIC_QUERY_RESULT/g
s/GATT_CHARACTERISTIC_VALUE_QUERY_RESULT/GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT/g
s/GATT_INCLUDED_SERVICE_QUERY_RESULT/GATT_EVENT_INCLUDED_SERVICE_QUERY_RESULT/g
s/GATT_INDICATION/GATT_EVENT_INDICATION/g
s/GATT_LONG_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT/GATT_EVENT_LONG_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT/g
s/GATT_LONG_CHARACTERISTIC_VALUE_QUERY_RESULT/GATT_EVENT_LONG_CHARACTERISTIC_VALUE_QUERY_RESULT/g
s/GATT_MTU/GATT_EVENT_MTU/g
s/GATT_NOTIFICATION/GATT_EVENT_NOTIFICATION/g
s/GATT_QUERY_COMPLETE/GATT_EVENT_QUERY_COMPLETE/g
s/GATT_SERVICE_QUERY_RESULT/GATT_EVENT_SERVICE_QUERY_RESULT/g
s/HAVE_TIME_MS/HAVE_EMBEDDED_TIME_MS/g
s/HAVE_TICK/HAVE_EMBEDDED_TICK/g
s/HAVE_TIME/HAVE_POSIX_TIME/g
s/HAVE_STDIO/HAVE_POSIX_STDIN/g
s/RFCOMM_EVENT_OPEN_CHANNEL_COMPLETE/RFCOMM_EVENT_CHANNEL_OPENED/g
s/SDP_QUERY_ATTRIBUTE_BYTE/SDP_EVENT_QUERY_ATTRIBUTE_BYTE/g
s/SDP_QUERY_ATTRIBUTE_VALUE/SDP_EVENT_QUERY_ATTRIBUTE_VALUE/g
s/SDP_QUERY_COMPLETE/SDP_EVENT_QUERY_COMPLETE/g
s/SDP_QUERY_RFCOMM_SERVICE/SDP_EVENT_QUERY_RFCOMM_SERVICE/g
s/SDP_QUERY_SERVICE_RECORD_HANDLE/SDP_EVENT_QUERY_SERVICE_RECORD_HANDLE/g
s/SM_EVENT_AUTHORIZATION_REQUEST/SM_EVENT_AUTHORIZATION_REQUEST/g
s/SM_EVENT_AUTHORIZATION_RESULT/SM_EVENT_AUTHORIZATION_RESULT/g
s/SM_EVENT_IDENTITY_RESOLVING_FAILED/SM_EVENT_IDENTITY_RESOLVING_FAILED/g
s/SM_EVENT_IDENTITY_RESOLVING_STARTED/SM_EVENT_IDENTITY_RESOLVING_STARTED/g
s/SM_EVENT_IDENTITY_RESOLVING_SUCCEEDED/SM_EVENT_IDENTITY_RESOLVING_SUCCEEDED/g
s/SM_EVENT_JUST_WORKS_CANCEL/SM_EVENT_JUST_WORKS_CANCEL/g
s/SM_EVENT_JUST_WORKS_REQUEST/SM_EVENT_JUST_WORKS_REQUEST/g
s/SM_EVENT_PASSKEY_DISPLAY_CANCEL/SM_EVENT_PASSKEY_DISPLAY_CANCEL/g
s/SM_EVENT_PASSKEY_DISPLAY_NUMBER/SM_EVENT_PASSKEY_DISPLAY_NUMBER/g
s/SM_EVENT_PASSKEY_INPUT_CANCEL/SM_EVENT_PASSKEY_INPUT_CANCEL/g
s/SM_EVENT_PASSKEY_INPUT_NUMBER/SM_EVENT_PASSKEY_INPUT_NUMBER/g

# Functions ending with _internal
s/l2cap_accept_connection_internal/l2cap_accept_connection/g
s/l2cap_create_channel_internal/l2cap_create_channel/g
s/l2cap_decline_connection_internal/l2cap_decline_connection/g
s/l2cap_disconnect_internal/l2cap_disconnect/g
s/l2cap_le_register_service_internal/l2cap_cbm_register_service/g
s/l2cap_le_unregister_service_internal/l2cap_cbm_unregister_service/g
s/l2cap_register_service_internal/l2cap_register_service/g
s/l2cap_send_internal/l2cap_send/g
s/l2cap_unregister_service_internal/l2cap_unregister_service/g
s/rfcomm_accept_connection_internal/rfcomm_accept_connection/g
s/rfcomm_create_channel_internal/rfcomm_create_channel/g
s/rfcomm_create_channel_with_initial_credits_internal/rfcomm_create_channel_with_initial_credits/g
s/rfcomm_decline_connection_internal/rfcomm_decline_connection/g
s/rfcomm_disconnect_internal/rfcomm_disconnect/g
s/rfcomm_register_service_internal/rfcomm_register_service/g
s/rfcomm_register_service_with_initial_credits_internal/rfcomm_register_service_with_initial_credits/g
s/rfcomm_send_internal/rfcomm_send/g
s/rfcomm_unregister_service_internal/rfcomm_unregister_service/g
s/sdp_register_service_internal/sdp_register_service/g
s/sdp_unregister_service_internal/sdp_unregister_service/g

# Functions/Macros
s/att_server_can_send/att_server_can_send_packet_now/g
s/BD_ADDR_CMP/bd_addr_cmp/g
s/bt_store_16/little_endian_store_16/g
s/bt_store_24/little_endian_store_24/g
s/bt_store_32/little_endian_store_32/g
s/hci_discoverable_control/gap_discoverable_control/g
s/hci_ssp_set_io_capability/gap_ssp_set_io_capability/g
s/hci_set_class_of_device/gap_set_class_of_device/g
s/le_central_connect/gap_connect/g
s/le_central_connect_cancel/gap_connect_cancel/g
s/le_central_set_scan_parameters/gap_set_scan_parameters/g
s/le_central_start_scan/gap_start_scan/g
s/le_central_stop_scan/gap_stop_scan/g
s/net_store_16/big_endian_store_16/g
s/net_store_24/big_endian_store_24/g
s/net_store_32/big_endian_store_32/g
s/READ_BT_16/little_endian_read_16/g
s/READ_BT_24/little_endian_read_24/g
s/READ_BT_32/little_endian_read_32/g
s/READ_NET_16/big_endian_read_16/g
s/READ_NET_24/big_endian_read_24/g
s/READ_NET_32/big_endian_read_32/g
s/run_loop_add_timer/btstack_run_loop_add_timer/g
s/run_loop_get_time_ms/btstack_run_loop_get_time_ms/g
s/run_loop_set_timer/btstack_run_loop_set_timer/g
s/sdp_client_query_rfcomm_ready/sdp_client_ready/g
s/sdp_create_spp_service/spp_create_sdp_record/g
s/swap128/reverse_128/g
s/swap32/reverse_32/g
s/swap48/reverse_48/g
s/swap64/reverse_64/g
s/pan_create_panu_service/pan_create_panu_sdp_record/g
s/pan_create_gn_service/pan_create_gn_sdp_record/g
s/pan_create_nap_service/pan_create_nap_sdp_record/g

# Folder structure
s|/example/embedded|/example|g
s|/ble/compile-gatt.py|/tool/compile_gatt.py|g

# type renames
s/le_service_t/gatt_client_service_t/g
s/le_characteristic_t/gatt_client_characteristic_t/g

# header changes
s|"att.h"|"att_db.h"|g
s|"bnep.h"|"classic/bnep.h"|g
s|"gap_le.h"|"gap.h"|g
s|"hfp_ag.h"|"classic/hfp_ag.h"|g
s|"hfp_hf.h"|"classic/hfp_hf.h"|g
s|"hsp_ag.h"|"classic/hsp_ag.h"|g
s|"hsp_hs.h"|"classic/hsp_hs.h"|g
s|"pan.h"|"classic/pan.h"|g
s|"rfcomm.h"|"classic/rfcomm.h"|g
s|"sdp.h"|"classic/sdp_server.h"|g
s|"sdp_client_rfcomm.h"|"classic/sdp_client_rfcomm.h"|g
s|"sdp_parser.h"|"classic/sdp_client.h"|g
s|#include "sdp_client.h"|#include "classic/sdp_client.h"|g
s|#include "sdp_query_util.h"|// sdp_query_util does not exist anymore|g
s|#include <btstack/sdp_util.h>|#include "classic/sdp_util.h"\n#include "classic/spp_server.h"|g
s|<btstack/hal_led.h>|"hal_led.h"|g
s|<btstack/hci_cmds.h>|"hci_cmd.h"|g
s|<btstack/run_loop.h>|"btstack_run_loop.h"|g
s|<btstack/utils.h>|"btstack_util.h"|g

# type changes
s|le_command_status_t|uint8_t|g
s|le_service_t|gatt_client_service_t|g
s|le_characteristic_t|gatt_client_characteristic_t|g
s|le_characteristic_descriptor_t|gatt_client_characteristic_descriptor_t|g

# File renames
s|ancs_client_lib|ancs_client|g
s|att\.c|att_db\.c|g
s|btstack-config.h|btstack_config.h|g
s|debug.h|btstack_debug.h|g
s|hci_cmds\.c|hci_cmd\.c|g
s|linked_list|btstack_linked_list|g
s|memory_pool|btstack_memory_pool|g
s|remote_device_db_memory|btstack_link_key_db_memory|g
s|run_loop\.c|btstack_run_loop\.c|g
s|sdp\.c|sdp_server\.c|g
s|sdp_parser.[c|o]||g
s|sdp_query_util.[c|o]||g
s|timer_source_t|btstack_timer_source_t|g
s|utils|btstack_util|g

# Makefile hacks: fix path to src/ble
s|/ble|/src/ble|g

## might not work, due to very precise matching rules, or BTSTACK_ROOT not being used

# Makefile hacks: add VPATH to src/classic
s|VPATH += ${BTSTACK_ROOT}/src$|VPATH += ${BTSTACK_ROOT}/src\nVPATH += ${BTSTACK_ROOT}/src/classic|g

# Disable ancs_client BTstack examples
s|${CC} $(filter-out ancs_client.h,$^) ${CFLAGS} ${LDFLAGS} -o $@|echo ANCS Client target disabled by converstion script|g

