/*
 * Copyright (C) 2014 BlueKitchen GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL BLUEKITCHEN
 * GMBH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at 
 * contact@bluekitchen-gmbh.com
 *
 */

/*
 *  daemon_cmds.h
 */

#ifndef DAEMON_CMDS_H
#define DAEMON_CMDS_H

#include <stdint.h>

#include "bluetooth.h"
#include "hci_cmd.h"

#if defined __cplusplus
extern "C" {
#endif

// calculate daemon (hci) opcode for ocf value
#define DAEMON_OPCODE(ocf) ((ocf) | ((OGF_BTSTACK) << 10))

typedef enum {
    DAEMON_OPCODE_BTSTACK_GET_STATE = DAEMON_OPCODE(BTSTACK_GET_STATE),
    DAEMON_OPCODE_BTSTACK_SET_POWER_MODE = DAEMON_OPCODE(BTSTACK_SET_POWER_MODE),
    DAEMON_OPCODE_BTSTACK_SET_ACL_CAPTURE_MODE = DAEMON_OPCODE(BTSTACK_SET_ACL_CAPTURE_MODE),
    DAEMON_OPCODE_BTSTACK_GET_VERSION = DAEMON_OPCODE(BTSTACK_GET_VERSION),
    DAEMON_OPCODE_BTSTACK_GET_SYSTEM_BLUETOOTH_ENABLED = DAEMON_OPCODE(BTSTACK_GET_SYSTEM_BLUETOOTH_ENABLED),
    DAEMON_OPCODE_BTSTACK_SET_SYSTEM_BLUETOOTH_ENABLED = DAEMON_OPCODE(BTSTACK_SET_SYSTEM_BLUETOOTH_ENABLED),
    DAEMON_OPCODE_BTSTACK_SET_DISCOVERABLE = DAEMON_OPCODE(BTSTACK_SET_DISCOVERABLE),
    DAEMON_OPCODE_BTSTACK_SET_BLUETOOTH_ENABLED = DAEMON_OPCODE(BTSTACK_SET_BLUETOOTH_ENABLED),
    DAEMON_OPCODE_L2CAP_CREATE_CHANNEL = DAEMON_OPCODE(L2CAP_CREATE_CHANNEL),
    DAEMON_OPCODE_L2CAP_CREATE_CHANNEL_MTU = DAEMON_OPCODE(L2CAP_CREATE_CHANNEL_MTU),
    DAEMON_OPCODE_L2CAP_DISCONNECT = DAEMON_OPCODE(L2CAP_DISCONNECT),
    DAEMON_OPCODE_L2CAP_REGISTER_SERVICE = DAEMON_OPCODE(L2CAP_REGISTER_SERVICE),
    DAEMON_OPCODE_L2CAP_UNREGISTER_SERVICE = DAEMON_OPCODE(L2CAP_UNREGISTER_SERVICE),
    DAEMON_OPCODE_L2CAP_ACCEPT_CONNECTION = DAEMON_OPCODE(L2CAP_ACCEPT_CONNECTION),
    DAEMON_OPCODE_L2CAP_DECLINE_CONNECTION = DAEMON_OPCODE(L2CAP_DECLINE_CONNECTION),
    DAEMON_OPCODE_L2CAP_REQUEST_CAN_SEND_NOW = DAEMON_OPCODE(L2CAP_REQUEST_CAN_SEND_NOW),
    DAEMON_OPCODE_SDP_REGISTER_SERVICE_RECORD = DAEMON_OPCODE(SDP_REGISTER_SERVICE_RECORD),
    DAEMON_OPCODE_SDP_UNREGISTER_SERVICE_RECORD = DAEMON_OPCODE(SDP_UNREGISTER_SERVICE_RECORD),
    DAEMON_OPCODE_SDP_CLIENT_QUERY_RFCOMM_SERVICES = DAEMON_OPCODE(SDP_CLIENT_QUERY_RFCOMM_SERVICES),
    DAEMON_OPCODE_SDP_CLIENT_QUERY_SERVICES = DAEMON_OPCODE(SDP_CLIENT_QUERY_SERVICES),
    DAEMON_OPCODE_RFCOMM_CREATE_CHANNEL = DAEMON_OPCODE(RFCOMM_CREATE_CHANNEL),
    DAEMON_OPCODE_RFCOMM_CREATE_CHANNEL_WITH_INITIAL_CREDITS = DAEMON_OPCODE(RFCOMM_CREATE_CHANNEL_WITH_CREDITS),
    DAEMON_OPCODE_RFCOMM_GRANTS_CREDITS = DAEMON_OPCODE(RFCOMM_GRANT_CREDITS),
    DAEMON_OPCODE_RFCOMM_DISCONNECT = DAEMON_OPCODE(RFCOMM_DISCONNECT),
    DAEMON_OPCODE_RFCOMM_REGISTER_SERVICE = DAEMON_OPCODE(RFCOMM_REGISTER_SERVICE),
    DAEMON_OPCODE_RFCOMM_REGISTER_SERVICE_WITH_INITIAL_CREDITS = DAEMON_OPCODE(RFCOMM_REGISTER_SERVICE_WITH_CREDITS),
    DAEMON_OPCODE_RFCOMM_UNREGISTER_SERVICE = DAEMON_OPCODE(RFCOMM_UNREGISTER_SERVICE),
    DAEMON_OPCODE_RFCOMM_ACCEPT_CONNECTION = DAEMON_OPCODE(RFCOMM_ACCEPT_CONNECTION),
    DAEMON_OPCODE_RFCOMM_DECLINE_CONNECTION = DAEMON_OPCODE(RFCOMM_DECLINE_CONNECTION),
    DAEMON_OPCODE_RFCOMM_PERSISTENT_CHANNEL_FOR_SERVICE = DAEMON_OPCODE(RFCOMM_PERSISTENT_CHANNEL),
    DAEMON_OPCODE_RFCOMM_REQUEST_CAN_SEND_NOW = DAEMON_OPCODE(RFCOMM_REQUEST_CAN_SEND_NOW),
    DAEMON_OPCODE_GAP_DISCONNECT = DAEMON_OPCODE(GAP_DISCONNECT),
    DAEMON_OPCODE_GAP_INQUIRY_START = DAEMON_OPCODE(GAP_INQUIRY_START),
    DAEMON_OPCODE_GAP_INQUIRY_STOP = DAEMON_OPCODE(GAP_INQUIRY_STOP),
    DAEMON_OPCODE_GAP_REMOTE_NAME_REQUEST = DAEMON_OPCODE(GAP_REMOTE_NAME_REQUEST),
    DAEMON_OPCODE_GAP_DROP_LINK_KEY_FOR_BD_ADDR = DAEMON_OPCODE(GAP_DROP_LINK_KEY_FOR_BD_ADDR),
    DAEMON_OPCODE_GAP_DELETE_ALL_LINK_KEYS = DAEMON_OPCODE(GAP_DELETE_ALL_LINK_KEYS),
    DAEMON_OPCODE_GAP_PIN_CODE_RESPONSE = DAEMON_OPCODE(GAP_PIN_CODE_RESPONSE),
    DAEMON_OPCODE_GAP_PIN_CODE_NEGATIVE = DAEMON_OPCODE(GAP_PIN_CODE_NEGATIVE),
    DAEMON_OPCODE_GAP_LE_SCAN_START = DAEMON_OPCODE(GAP_LE_SCAN_START),
    DAEMON_OPCODE_GAP_LE_SCAN_STOP = DAEMON_OPCODE(GAP_LE_SCAN_STOP),
    DAEMON_OPCODE_GAP_LE_SET_SCAN_PARAMETERS = DAEMON_OPCODE(GAP_LE_SET_SCAN_PARAMETERS),
    DAEMON_OPCODE_GAP_LE_CONNECT = DAEMON_OPCODE(GAP_LE_CONNECT),
    DAEMON_OPCODE_GAP_LE_CONNECT_CANCEL = DAEMON_OPCODE(GAP_LE_CONNECT_CANCEL),
    DAEMON_OPCODE_GATT_DISCOVER_PRIMARY_SERVICES = DAEMON_OPCODE(GATT_DISCOVER_ALL_PRIMARY_SERVICES),
    DAEMON_OPCODE_GATT_DISCOVER_PRIMARY_SERVICES_BY_UUID16 = DAEMON_OPCODE(GATT_DISCOVER_PRIMARY_SERVICES_BY_UUID16),
    DAEMON_OPCODE_GATT_DISCOVER_PRIMARY_SERVICES_BY_UUID128 = DAEMON_OPCODE(GATT_DISCOVER_PRIMARY_SERVICES_BY_UUID128),
    DAEMON_OPCODE_GATT_FIND_INCLUDED_SERVICES_FOR_SERVICE = DAEMON_OPCODE(GATT_FIND_INCLUDED_SERVICES_FOR_SERVICE),
    DAEMON_OPCODE_GATT_DISCOVER_CHARACTERISTICS_FOR_SERVICE = DAEMON_OPCODE(GATT_DISCOVER_CHARACTERISTICS_FOR_SERVICE),
    DAEMON_OPCODE_GATT_DISCOVER_CHARACTERISTICS_FOR_SERVICE_BY_UUID128 = DAEMON_OPCODE(GATT_DISCOVER_CHARACTERISTICS_FOR_SERVICE_BY_UUID128),
    DAEMON_OPCODE_GATT_DISCOVER_CHARACTERISTIC_DESCRIPTORS = DAEMON_OPCODE(GATT_DISCOVER_CHARACTERISTIC_DESCRIPTORS),
    DAEMON_OPCODE_GATT_READ_VALUE_OF_CHARACTERISTIC = DAEMON_OPCODE(GATT_READ_VALUE_OF_CHARACTERISTIC),
    DAEMON_OPCODE_GATT_READ_LONG_VALUE_OF_CHARACTERISTIC = DAEMON_OPCODE(GATT_READ_LONG_VALUE_OF_CHARACTERISTIC),
    DAEMON_OPCODE_GATT_WRITE_VALUE_OF_CHARACTERISTIC_WITHOUT_RESPONSE = DAEMON_OPCODE(GATT_WRITE_VALUE_OF_CHARACTERISTIC_WITHOUT_RESPONSE),
    DAEMON_OPCODE_GATT_WRITE_VALUE_OF_CHARACTERISTIC = DAEMON_OPCODE(GATT_WRITE_VALUE_OF_CHARACTERISTIC),
    DAEMON_OPCODE_GATT_WRITE_LONG_VALUE_OF_CHARACTERISTIC = DAEMON_OPCODE(GATT_WRITE_LONG_VALUE_OF_CHARACTERISTIC),
    DAEMON_OPCODE_GATT_RELIABLE_WRITE_LONG_VALUE_OF_CHARACTERISTIC = DAEMON_OPCODE(GATT_RELIABLE_WRITE_LONG_VALUE_OF_CHARACTERISTIC),
    DAEMON_OPCODE_GATT_READ_CHARACTERISTIC_DESCRIPTOR = DAEMON_OPCODE(GATT_READ_CHARACTERISTIC_DESCRIPTOR),
    DAEMON_OPCODE_GATT_READ_LONG_CHARACTERISTIC_DESCRIPTOR = DAEMON_OPCODE(GATT_READ_LONG_CHARACTERISTIC_DESCRIPTOR),
    DAEMON_OPCODE_GATT_WRITE_CHARACTERISTIC_DESCRIPTOR = DAEMON_OPCODE(GATT_WRITE_CHARACTERISTIC_DESCRIPTOR),
    DAEMON_OPCODE_GATT_WRITE_LONG_CHARACTERISTIC_DESCRIPTOR = DAEMON_OPCODE(GATT_WRITE_LONG_CHARACTERISTIC_DESCRIPTOR),
    DAEMON_OPCODE_GATT_WRITE_CLIENT_CHARACTERISTIC_CONFIGURATION = DAEMON_OPCODE(GATT_WRITE_CLIENT_CHARACTERISTIC_CONFIGURATION),
    DAEMON_OPCODE_GATT_GET_MTU = DAEMON_OPCODE(GATT_GET_MTU),
    DAEMON_OPCODE_SM_SET_AUTHENTICATION_REQUIREMENTS = DAEMON_OPCODE(SM_SET_AUTHENTICATION_REQUIREMENTS),
    DAEMON_OPCODE_SM_SET_IO_CAPABILITIES = DAEMON_OPCODE(SM_SET_IO_CAPABILITIES),
    DAEMON_OPCODE_SM_BONDING_DECLINE = DAEMON_OPCODE(SM_BONDING_DECLINE),
    DAEMON_OPCODE_SM_JUST_WORKS_CONFIRM = DAEMON_OPCODE(SM_JUST_WORKS_CONFIRM),
    DAEMON_OPCODE_SM_NUMERIC_COMPARISON_CONFIRM = DAEMON_OPCODE(SM_NUMERIC_COMPARISON_CONFIRM),
    DAEMON_OPCODE_SM_PASSKEY_INPUT = DAEMON_OPCODE(SM_PASSKEY_INPUT),
} daemon_opcode_t;

extern const hci_cmd_t btstack_get_state;
extern const hci_cmd_t btstack_set_power_mode;
extern const hci_cmd_t btstack_set_acl_capture_mode;
extern const hci_cmd_t btstack_get_version;
extern const hci_cmd_t btstack_get_system_bluetooth_enabled;
extern const hci_cmd_t btstack_set_system_bluetooth_enabled;
extern const hci_cmd_t btstack_set_discoverable;
extern const hci_cmd_t btstack_set_bluetooth_enabled;    // only used by btstack config

extern const hci_cmd_t l2cap_accept_connection_cmd;
extern const hci_cmd_t l2cap_create_channel_cmd;
extern const hci_cmd_t l2cap_create_channel_mtu_cmd;
extern const hci_cmd_t l2cap_decline_connection_cmd;
extern const hci_cmd_t l2cap_disconnect_cmd;
extern const hci_cmd_t l2cap_register_service_cmd;
extern const hci_cmd_t l2cap_unregister_service_cmd;
extern const hci_cmd_t l2cap_request_can_send_now_cmd;

extern const hci_cmd_t sdp_register_service_record_cmd;
extern const hci_cmd_t sdp_unregister_service_record_cmd;
extern const hci_cmd_t sdp_client_query_rfcomm_services_cmd;
extern const hci_cmd_t sdp_client_query_services_cmd;

// accept connection @param bd_addr(48), rfcomm_cid (16)
extern const hci_cmd_t rfcomm_accept_connection_cmd;
// create rfcomm channel: @param bd_addr(48), channel (8)
extern const hci_cmd_t rfcomm_create_channel_cmd;
// create rfcomm channel: @param bd_addr(48), channel (8), mtu (16), credits (8)
extern const hci_cmd_t rfcomm_create_channel_with_initial_credits_cmd;
// decline rfcomm disconnect,@param bd_addr(48), rfcomm cid (16), reason(8)
extern const hci_cmd_t rfcomm_decline_connection_cmd;
// disconnect rfcomm disconnect, @param rfcomm_cid(8), reason(8)
extern const hci_cmd_t rfcomm_disconnect_cmd;
// register rfcomm service: @param channel(8), mtu (16)
extern const hci_cmd_t rfcomm_register_service_cmd;
// register rfcomm service: @param channel(8), mtu (16), initial credits (8)
extern const hci_cmd_t rfcomm_register_service_with_initial_credits_cmd;
// unregister rfcomm service, @param service_channel(16)
extern const hci_cmd_t rfcomm_unregister_service_cmd;
// request persisten rfcomm channel for service name: serive name (char*) 
extern const hci_cmd_t rfcomm_persistent_channel_for_service_cmd;
extern const hci_cmd_t rfcomm_grants_credits_cmd;
extern const hci_cmd_t rfcomm_request_can_send_now_cmd;

extern const hci_cmd_t gap_delete_all_link_keys_cmd;
extern const hci_cmd_t gap_disconnect_cmd;
extern const hci_cmd_t gap_drop_link_key_for_bd_addr_cmd;
extern const hci_cmd_t gap_inquiry_start_cmd;
extern const hci_cmd_t gap_inquiry_stop_cmd;
extern const hci_cmd_t gap_remote_name_request_cmd;
extern const hci_cmd_t gap_pin_code_response_cmd;
extern const hci_cmd_t gap_pin_code_response_cmd;

extern const hci_cmd_t gap_le_scan_start;
extern const hci_cmd_t gap_le_scan_stop;
extern const hci_cmd_t gap_le_set_scan_parameters;
extern const hci_cmd_t gap_le_connect_cmd;
extern const hci_cmd_t gap_le_connect_cancel_cmd;
extern const hci_cmd_t gatt_discover_primary_services_cmd;

extern const hci_cmd_t gatt_discover_primary_services_by_uuid16_cmd;
extern const hci_cmd_t gatt_discover_primary_services_by_uuid128_cmd;
extern const hci_cmd_t gatt_find_included_services_for_service_cmd;
extern const hci_cmd_t gatt_discover_characteristics_for_service_cmd;
extern const hci_cmd_t gatt_discover_characteristics_for_service_by_uuid128_cmd;
extern const hci_cmd_t gatt_discover_characteristic_descriptors_cmd;
extern const hci_cmd_t gatt_read_value_of_characteristic_cmd;
extern const hci_cmd_t gatt_read_long_value_of_characteristic_cmd;
extern const hci_cmd_t gatt_write_value_of_characteristic_without_response_cmd;
extern const hci_cmd_t gatt_write_value_of_characteristic_cmd;
extern const hci_cmd_t gatt_write_long_value_of_characteristic_cmd;
extern const hci_cmd_t gatt_reliable_write_long_value_of_characteristic_cmd;
extern const hci_cmd_t gatt_read_characteristic_descriptor_cmd;
extern const hci_cmd_t gatt_read_long_characteristic_descriptor_cmd;
extern const hci_cmd_t gatt_write_characteristic_descriptor_cmd;
extern const hci_cmd_t gatt_write_long_characteristic_descriptor_cmd;
extern const hci_cmd_t gatt_write_client_characteristic_configuration_cmd;
extern const hci_cmd_t gatt_get_mtu;

extern const hci_cmd_t sm_set_authentication_requirements_cmd;
extern const hci_cmd_t sm_set_io_capabilities_cmd;
extern const hci_cmd_t sm_bonding_decline_cmd;
extern const hci_cmd_t sm_just_works_confirm_cmd;
extern const hci_cmd_t sm_numeric_comparison_confirm_cmd;
extern const hci_cmd_t sm_passkey_input_cmd;

#if defined __cplusplus
}
#endif

#endif // DAEMON_CMDS_H
