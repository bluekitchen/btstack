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

#define BTSTACK_FILE__ "daemon_cmds.c"

/*
 *  hci_cmd.c
 *
 *  Created by Matthias Ringwald on 7/23/09.
 */

#include "daemon_cmds.h"
#include "hci.h"
 
// calculate combined ogf/ocf value
#define OPCODE(ogf, ocf) (ocf | ogf << 10)

// BTstack commands
const hci_cmd_t btstack_get_state = {
    DAEMON_OPCODE_BTSTACK_GET_STATE, ""
};

/**
 * @param power_mode (0 = off, 1 = on)
 */
const hci_cmd_t btstack_set_power_mode = {
    DAEMON_OPCODE_BTSTACK_SET_POWER_MODE, "1"
};

/**
 * @param acl_capture_mode (0 = off, 1 = on)
 */
const hci_cmd_t btstack_set_acl_capture_mode = {
    DAEMON_OPCODE_BTSTACK_SET_ACL_CAPTURE_MODE, "1"
};

const hci_cmd_t btstack_get_version = {
    DAEMON_OPCODE_BTSTACK_GET_VERSION, ""
};

const hci_cmd_t btstack_get_system_bluetooth_enabled = {
    DAEMON_OPCODE_BTSTACK_GET_SYSTEM_BLUETOOTH_ENABLED, ""
};

/**
 * @param bluetooth_enabled_flag (0 = off, 1 = on, only used by btstack config)
 */
const hci_cmd_t btstack_set_system_bluetooth_enabled = {
    DAEMON_OPCODE_BTSTACK_SET_SYSTEM_BLUETOOTH_ENABLED, "1"
};

/**
 * @param discoverable_flag (0 = off, 1 = on)
 */
const hci_cmd_t btstack_set_discoverable = {
    DAEMON_OPCODE_BTSTACK_SET_DISCOVERABLE, "1"
};

/**
 * @param bluetooth_enabled_flag (0 = off, 1 = on, only used by btstack config)
 */
const hci_cmd_t btstack_set_bluetooth_enabled = {
    DAEMON_OPCODE_BTSTACK_SET_BLUETOOTH_ENABLED, "1"
};

/**
 * @param bd_addr (48)
 * @param psm (16)
 */
const hci_cmd_t l2cap_create_channel_cmd = {
    DAEMON_OPCODE_L2CAP_CREATE_CHANNEL, "B2"
};

/**
 * @param bd_addr (48)
 * @param psm (16)
 * @param mtu (16)
 */
const hci_cmd_t l2cap_create_channel_mtu_cmd = {
    DAEMON_OPCODE_L2CAP_CREATE_CHANNEL_MTU, "B22"
// @param bd_addr(48), psm (16), mtu (16)
};

/**
 * @param channel (16)
 */
const hci_cmd_t l2cap_disconnect_cmd = {
    DAEMON_OPCODE_L2CAP_DISCONNECT, "2"
};

/**
 * @param psm (16)
 * @param mtu (16)
 */
const hci_cmd_t l2cap_register_service_cmd = {
    DAEMON_OPCODE_L2CAP_REGISTER_SERVICE, "22"
};

/**
 * @param psm (16)
 */
const hci_cmd_t l2cap_unregister_service_cmd = {
    DAEMON_OPCODE_L2CAP_UNREGISTER_SERVICE, "2"
};

/**
 * @param source_cid (16)
 */
const hci_cmd_t l2cap_accept_connection_cmd = {
    DAEMON_OPCODE_L2CAP_ACCEPT_CONNECTION, "2"
};

/**
 * @param source_cid (16)
 * @param reason (deprecated)
 */
const hci_cmd_t l2cap_decline_connection_cmd = {
    DAEMON_OPCODE_L2CAP_DECLINE_CONNECTION, "21"
};

/**
 * @param l2cap_cid
 */
const hci_cmd_t l2cap_request_can_send_now_cmd = {
    DAEMON_OPCODE_L2CAP_REQUEST_CAN_SEND_NOW, "2"
};

/**
 * @param service_record
 */
const hci_cmd_t sdp_register_service_record_cmd = {
    DAEMON_OPCODE_SDP_REGISTER_SERVICE_RECORD, "S"
};

/**
 * @param service_record_handle
 */
const hci_cmd_t sdp_unregister_service_record_cmd = {
    DAEMON_OPCODE_SDP_UNREGISTER_SERVICE_RECORD, "4"
};

/**
 * @param bd_addr
 * @param service_search_pattern
 */
const hci_cmd_t sdp_client_query_rfcomm_services_cmd = {
    DAEMON_OPCODE_SDP_CLIENT_QUERY_RFCOMM_SERVICES, "BS"
};

/**
 * @param bd_addr
 * @param service_search_pattern
 * @param attribute_ID_list
 */
const hci_cmd_t sdp_client_query_services_cmd = {
    DAEMON_OPCODE_SDP_CLIENT_QUERY_SERVICES, "BSS"
};

/**
 * @param bd_addr
 * @param server_channel
 */
const hci_cmd_t rfcomm_create_channel_cmd = {
    DAEMON_OPCODE_RFCOMM_CREATE_CHANNEL, "B1"
};

/**
 * @param bd_addr
 * @param server_channel
 * @param mtu
 * @param credits
 */
const hci_cmd_t rfcomm_create_channel_with_initial_credits_cmd = {
    DAEMON_OPCODE_RFCOMM_CREATE_CHANNEL_WITH_INITIAL_CREDITS, "B121"
};

/**
 * @param rfcomm_cid
 * @param credits
 */
const hci_cmd_t rfcomm_grants_credits_cmd = {
    DAEMON_OPCODE_RFCOMM_GRANTS_CREDITS, "21"
};

/**
 * @param rfcomm_cid
 * @param reason
 */
const hci_cmd_t rfcomm_disconnect_cmd = {
    DAEMON_OPCODE_RFCOMM_DISCONNECT, "21"
};

/**
 * @param server_channel
 * @param mtu
 */
const hci_cmd_t rfcomm_register_service_cmd = {
    DAEMON_OPCODE_RFCOMM_REGISTER_SERVICE, "12"
};

/**
 * @param server_channel
 * @param mtu
 * @param initial_credits
 */
const hci_cmd_t rfcomm_register_service_with_initial_credits_cmd = {
    DAEMON_OPCODE_RFCOMM_REGISTER_SERVICE_WITH_INITIAL_CREDITS, "121"
};

/**
 * @param service_channel
 */
const hci_cmd_t rfcomm_unregister_service_cmd = {
    DAEMON_OPCODE_RFCOMM_UNREGISTER_SERVICE, "2"
};

/**
 * @param source_cid
 */
const hci_cmd_t rfcomm_accept_connection_cmd = {
    DAEMON_OPCODE_RFCOMM_ACCEPT_CONNECTION, "2"
};


/**
 * @param source_cid
 * @param reason
 */
const hci_cmd_t rfcomm_decline_connection_cmd = {
    DAEMON_OPCODE_RFCOMM_DECLINE_CONNECTION, "21"
};

// request persistent rfcomm channel number for named service
/**
 * @param named_service
 */
const hci_cmd_t rfcomm_persistent_channel_for_service_cmd = {
    DAEMON_OPCODE_RFCOMM_PERSISTENT_CHANNEL_FOR_SERVICE, "N"
};

/**
 * @param rfcomm_cid
 */
const hci_cmd_t rfcomm_request_can_send_now_cmd = {
    DAEMON_OPCODE_RFCOMM_REQUEST_CAN_SEND_NOW, "2"
};

/**
 * @param handle
 */
const hci_cmd_t gap_disconnect_cmd = {
    DAEMON_OPCODE_GAP_DISCONNECT, "H"
};

/**
 * @param duration in 1.28s units
 */
const hci_cmd_t gap_inquiry_start_cmd = {
    DAEMON_OPCODE_GAP_INQUIRY_START, "1"
};

/**
 */
const hci_cmd_t gap_inquiry_stop_cmd = {
    DAEMON_OPCODE_GAP_INQUIRY_STOP, ""
};

/**
* @param addr
* @param page_scan_repetition_mode
* @param clock_offset only used when bit 15 is set - pass 0 if not known
 */
const hci_cmd_t gap_remote_name_request_cmd = {
    DAEMON_OPCODE_GAP_REMOTE_NAME_REQUEST, "B12"
};

/**
* @param addr
 */
const hci_cmd_t gap_drop_link_key_cmd = {
    DAEMON_OPCODE_GAP_DROP_LINK_KEY_FOR_BD_ADDR, "B"
};

/**
 */
const hci_cmd_t gap_delete_all_link_keys_cmd = {
    DAEMON_OPCODE_GAP_DELETE_ALL_LINK_KEYS, ""
};

/**
 * @param bd_addr
 * @param pin_length
 * @param pin (c-string)
 */
const hci_cmd_t gap_pin_code_response_cmd = {
    DAEMON_OPCODE_GAP_PIN_CODE_RESPONSE, "B1P"
};

/**
 * @param bd_addr
 */
const hci_cmd_t gap_pin_code_negative_cmd = {
    DAEMON_OPCODE_GAP_PIN_CODE_NEGATIVE, "B"
};


/**
 */
const hci_cmd_t gap_le_scan_start = {
    DAEMON_OPCODE_GAP_LE_SCAN_START, ""
};

/**
 */
const hci_cmd_t gap_le_scan_stop = {
    DAEMON_OPCODE_GAP_LE_SCAN_STOP, ""
};

/**
 * @param scan_type
 * @param scan_interval
 * @param scan_window
 */
const hci_cmd_t gap_le_set_scan_parameters = {
    DAEMON_OPCODE_GAP_LE_SET_SCAN_PARAMETERS, "122"
};

/**
 * @param peer_address_type
 * @param peer_address
 */
const hci_cmd_t gap_le_connect_cmd = {
    DAEMON_OPCODE_GAP_LE_CONNECT, "1B"
};

/**
 * @param peer_address_type
 * @param peer_address
 */
const hci_cmd_t gap_le_connect_cancel_cmd = {
    DAEMON_OPCODE_GAP_LE_CONNECT_CANCEL, ""
};

/**
 * @param handle
 */
const hci_cmd_t gatt_discover_primary_services_cmd = {
    DAEMON_OPCODE_GATT_DISCOVER_PRIMARY_SERVICES, "H"
};

/**
 * @param handle
 * @param uuid16
 */
const hci_cmd_t gatt_discover_primary_services_by_uuid16_cmd = {
    DAEMON_OPCODE_GATT_DISCOVER_PRIMARY_SERVICES_BY_UUID16, "H2"
};

/**
 * @param handle
 * @param uuid128
 */
const hci_cmd_t gatt_discover_primary_services_by_uuid128_cmd = {
    DAEMON_OPCODE_GATT_DISCOVER_PRIMARY_SERVICES_BY_UUID128, "HU"
};

/**
 * @param handle
 * @param service
 */
const hci_cmd_t gatt_find_included_services_for_service_cmd = {
    DAEMON_OPCODE_GATT_FIND_INCLUDED_SERVICES_FOR_SERVICE, "HX"
};

/**
 * @param handle
 * @param service
 */
const hci_cmd_t gatt_discover_characteristics_for_service_cmd = {
    DAEMON_OPCODE_GATT_DISCOVER_CHARACTERISTICS_FOR_SERVICE, "HX"
};

/**
 * @param handle
 * @param service
 * @param uuid128
 */
const hci_cmd_t gatt_discover_characteristics_for_service_by_uuid128_cmd = {
    DAEMON_OPCODE_GATT_DISCOVER_CHARACTERISTICS_FOR_SERVICE_BY_UUID128, "HXU"
};

/**
 * @param handle
 * @param characteristic
 */
const hci_cmd_t gatt_discover_characteristic_descriptors_cmd = {
    DAEMON_OPCODE_GATT_DISCOVER_CHARACTERISTIC_DESCRIPTORS, "HY"
};

/**
 * @param handle
 * @param characteristic
 */
const hci_cmd_t gatt_read_value_of_characteristic_cmd = {
    DAEMON_OPCODE_GATT_READ_VALUE_OF_CHARACTERISTIC, "HY"
};

/**
 * @param handle
 * @param characteristic
 */
const hci_cmd_t gatt_read_long_value_of_characteristic_cmd = {
    DAEMON_OPCODE_GATT_READ_LONG_VALUE_OF_CHARACTERISTIC, "HY"
};

/**
 * @param handle
 * @param characteristic
 * @param data_length
 * @param data
 */
const hci_cmd_t gatt_write_value_of_characteristic_without_response_cmd = {
    DAEMON_OPCODE_GATT_WRITE_VALUE_OF_CHARACTERISTIC_WITHOUT_RESPONSE, "HYLV"
};

/**
 * @param handle
 * @param characteristic
 * @param data_length
 * @param data
 */
const hci_cmd_t gatt_write_value_of_characteristic_cmd = {
    DAEMON_OPCODE_GATT_WRITE_VALUE_OF_CHARACTERISTIC, "HYLV"
};

/**
 * @param handle
 * @param characteristic
 * @param data_length
 * @param data
 */
const hci_cmd_t gatt_write_long_value_of_characteristic_cmd = {
    DAEMON_OPCODE_GATT_WRITE_LONG_VALUE_OF_CHARACTERISTIC, "HYLV"
};

/**
 * @param handle
 * @param characteristic
 * @param data_length
 * @param data
 */
const hci_cmd_t gatt_reliable_write_long_value_of_characteristic_cmd = {
    DAEMON_OPCODE_GATT_RELIABLE_WRITE_LONG_VALUE_OF_CHARACTERISTIC, "HYLV"
};

/**
 * @param handle
 * @param descriptor
 */
const hci_cmd_t gatt_read_characteristic_descriptor_cmd = {
    DAEMON_OPCODE_GATT_READ_CHARACTERISTIC_DESCRIPTOR, "HZ"
};

/**
 * @param handle
 * @param descriptor
 */
const hci_cmd_t gatt_read_long_characteristic_descriptor_cmd = {
    DAEMON_OPCODE_GATT_READ_LONG_CHARACTERISTIC_DESCRIPTOR, "HZ"
};

/**
 * @param handle
 * @param descriptor
 * @param data_length
 * @param data
 */
const hci_cmd_t gatt_write_characteristic_descriptor_cmd = {
    DAEMON_OPCODE_GATT_WRITE_CHARACTERISTIC_DESCRIPTOR, "HZLV"
};

/**
 * @param handle
 * @param descriptor
 * @param data_length
 * @param data
 */
const hci_cmd_t gatt_write_long_characteristic_descriptor_cmd = {
    DAEMON_OPCODE_GATT_WRITE_LONG_CHARACTERISTIC_DESCRIPTOR, "HZLV"
};

/**
 * @param handle
 * @param characteristic
 * @param configuration
 */
const hci_cmd_t gatt_write_client_characteristic_configuration_cmd = {
    DAEMON_OPCODE_GATT_WRITE_CLIENT_CHARACTERISTIC_CONFIGURATION, "HY2"
};

/**
 * @param handle
 */
const hci_cmd_t gatt_get_mtu = {
    DAEMON_OPCODE_GATT_GET_MTU, "H"
};

/**
 * @brief Sets the requested authentication requirements, bonding yes/no, MITM yes/no, SC yes/no, keypress yes/no
 * @param auth_req OR combination of SM_AUTHREQ_ flags
 */
const hci_cmd_t sm_set_authentication_requirements_cmd = {
    DAEMON_OPCODE_SM_SET_AUTHENTICATION_REQUIREMENTS, "1"
};

/**
 * @brief Sets the available IO Capabilities
 * @param io_capabilities
 */
const hci_cmd_t sm_set_io_capabilities_cmd = {
    DAEMON_OPCODE_SM_SET_IO_CAPABILITIES, "1"
};

/**
 * @brief Decline bonding triggered by event before
 * @param con_handle
 */
const hci_cmd_t sm_bonding_decline_cmd = {
    DAEMON_OPCODE_SM_BONDING_DECLINE, "H"
};

/**
 * @brief Confirm Just Works bonding 
 * @param con_handle
 */
const hci_cmd_t sm_just_works_confirm_cmd = {
    DAEMON_OPCODE_SM_JUST_WORKS_CONFIRM, "H"
};

/**
 * @brief Confirm value from SM_EVENT_NUMERIC_COMPARISON_REQUEST for Numeric Comparison bonding 
 * @param con_handle
 */
const hci_cmd_t sm_numeric_comparison_confirm_cmd = {
    DAEMON_OPCODE_SM_NUMERIC_COMPARISON_CONFIRM, "H"
};

/**
 * @brief Reports passkey input by user
 * @param con_handle
 * @param passkey in [0..999999]
 */
const hci_cmd_t sm_passkey_input_cmd = {
    DAEMON_OPCODE_SM_PASSKEY_INPUT, "H4"
};
