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
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
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

extern const hci_cmd_t gap_disconnect_cmd;
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
