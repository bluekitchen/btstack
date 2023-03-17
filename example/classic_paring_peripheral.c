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

#define BTSTACK_FILE__ "classic_paring_peripheral.c"

/*
 * classic_paring_peripheral.c
 */

// *****************************************************************************
/* EXAMPLE_START(classic_paring_peripheral):
 * 
 * @text Note: It initializes an ACL link to the peer and raises the security level.
 */
// *****************************************************************************

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
 
#include "btstack.h"

// prototypes
static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static btstack_packet_callback_registration_t hci_event_callback_registration;

static bool secure_simple_paring = true;
static io_capability_t io_capability = IO_CAPABILITY_DISPLAY_ONLY;
static gap_security_level_t security_level = LEVEL_0;
static bool dump_hci_event = false;

#define VALUE_TO_STR_IF_MATCH(value, macro) if((value)==(macro)){return(#macro);}

char *hci_event_type_to_str(uint8_t value) {
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_NOP);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_INQUIRY_COMPLETE);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_INQUIRY_RESULT);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_CONNECTION_COMPLETE);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_CONNECTION_REQUEST);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_DISCONNECTION_COMPLETE);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_AUTHENTICATION_COMPLETE);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_REMOTE_NAME_REQUEST_COMPLETE);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_ENCRYPTION_CHANGE);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_CHANGE_CONNECTION_LINK_KEY_COMPLETE);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_MASTER_LINK_KEY_COMPLETE);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_READ_REMOTE_SUPPORTED_FEATURES_COMPLETE);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_READ_REMOTE_VERSION_INFORMATION_COMPLETE);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_QOS_SETUP_COMPLETE);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_COMMAND_COMPLETE);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_COMMAND_STATUS);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_HARDWARE_ERROR);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_FLUSH_OCCURRED);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_ROLE_CHANGE);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_NUMBER_OF_COMPLETED_PACKETS);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_MODE_CHANGE);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_RETURN_LINK_KEYS);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_PIN_CODE_REQUEST);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_LINK_KEY_REQUEST);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_LINK_KEY_NOTIFICATION);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_LOOPBACK_COMMAND);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_DATA_BUFFER_OVERFLOW);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_MAX_SLOTS_CHANGED);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_READ_CLOCK_OFFSET_COMPLETE);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_CONNECTION_PACKET_TYPE_CHANGED);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_QOS_VIOLATION);
    // 0x1f not defined
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_PAGE_SCAN_REPETITION_MODE_CHANGE);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_FLOW_SPECIFICATION_COMPLETE);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_INQUIRY_RESULT_WITH_RSSI);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_READ_REMOTE_EXTENDED_FEATURES_COMPLETE);
    // 0x24..0x2b not defined
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_SYNCHRONOUS_CONNECTION_COMPLETE);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_SYNCHRONOUS_CONNECTION_CHANGED);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_SNIFF_SUBRATING);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_EXTENDED_INQUIRY_RESPONSE);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_ENCRYPTION_KEY_REFRESH_COMPLETE);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_IO_CAPABILITY_REQUEST);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_IO_CAPABILITY_RESPONSE);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_USER_CONFIRMATION_REQUEST);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_USER_PASSKEY_REQUEST);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_REMOTE_OOB_DATA_REQUEST);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_SIMPLE_PAIRING_COMPLETE);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_LINK_SUPERVISION_TIMEOUT_CHANGED);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_ENHANCED_FLUSH_COMPLETE);
    // 0x03a not defined
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_USER_PASSKEY_NOTIFICATION);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_KEYPRESS_NOTIFICATION);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_REMOTE_HOST_SUPPORTED_FEATURES);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_LE_META);
    // 0x3f..0x47 not defined
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_NUMBER_OF_COMPLETED_DATA_BLOCKS);
    // 0x49..0x58 not defined
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_ENCRYPTION_CHANGE_V2);
    // last used HCI_EVENT in 5.3 is 0x59
    // stat with 0x60
    VALUE_TO_STR_IF_MATCH(value, BTSTACK_EVENT_STATE);
    VALUE_TO_STR_IF_MATCH(value, BTSTACK_EVENT_NR_CONNECTIONS_CHANGED);
    VALUE_TO_STR_IF_MATCH(value, BTSTACK_EVENT_POWERON_FAILED);
    VALUE_TO_STR_IF_MATCH(value, DAEMON_EVENT_VERSION);
    VALUE_TO_STR_IF_MATCH(value, DAEMON_EVENT_SYSTEM_BLUETOOTH_ENABLED);
    VALUE_TO_STR_IF_MATCH(value, DAEMON_EVENT_REMOTE_NAME_CACHED);
    VALUE_TO_STR_IF_MATCH(value, BTSTACK_EVENT_SCAN_MODE_CHANGED);
    VALUE_TO_STR_IF_MATCH(value, DAEMON_EVENT_CONNECTION_OPENED);
    VALUE_TO_STR_IF_MATCH(value, DAEMON_EVENT_CONNECTION_CLOSED);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_TRANSPORT_SLEEP_MODE);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_TRANSPORT_USB_INFO);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_BIS_CAN_SEND_NOW);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_CIS_CAN_SEND_NOW);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_TRANSPORT_READY);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_TRANSPORT_PACKET_SENT);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_SCO_CAN_SEND_NOW);
    VALUE_TO_STR_IF_MATCH(value, L2CAP_EVENT_CHANNEL_OPENED);
    VALUE_TO_STR_IF_MATCH(value, L2CAP_EVENT_CHANNEL_CLOSED);
    VALUE_TO_STR_IF_MATCH(value, L2CAP_EVENT_INCOMING_CONNECTION);
    VALUE_TO_STR_IF_MATCH(value, L2CAP_EVENT_TIMEOUT_CHECK);
    VALUE_TO_STR_IF_MATCH(value, DAEMON_EVENT_L2CAP_CREDITS);
    VALUE_TO_STR_IF_MATCH(value, DAEMON_EVENT_L2CAP_SERVICE_REGISTERED);
    VALUE_TO_STR_IF_MATCH(value, L2CAP_EVENT_CONNECTION_PARAMETER_UPDATE_REQUEST);
    VALUE_TO_STR_IF_MATCH(value, L2CAP_EVENT_CONNECTION_PARAMETER_UPDATE_RESPONSE);
    VALUE_TO_STR_IF_MATCH(value, L2CAP_EVENT_INFORMATION_RESPONSE);
    VALUE_TO_STR_IF_MATCH(value, L2CAP_EVENT_CAN_SEND_NOW);
    VALUE_TO_STR_IF_MATCH(value, L2CAP_EVENT_PACKET_SENT);
    VALUE_TO_STR_IF_MATCH(value, L2CAP_EVENT_ERTM_BUFFER_RELEASED);
    VALUE_TO_STR_IF_MATCH(value, L2CAP_EVENT_CBM_INCOMING_CONNECTION);
    VALUE_TO_STR_IF_MATCH(value, L2CAP_EVENT_CBM_CHANNEL_OPENED);
    VALUE_TO_STR_IF_MATCH(value, L2CAP_EVENT_TRIGGER_RUN);
    VALUE_TO_STR_IF_MATCH(value, L2CAP_EVENT_ECBM_INCOMING_CONNECTION);
    VALUE_TO_STR_IF_MATCH(value, RFCOMM_EVENT_CHANNEL_OPENED);
    VALUE_TO_STR_IF_MATCH(value, RFCOMM_EVENT_CHANNEL_CLOSED);
    VALUE_TO_STR_IF_MATCH(value, RFCOMM_EVENT_INCOMING_CONNECTION);
    VALUE_TO_STR_IF_MATCH(value, RFCOMM_EVENT_REMOTE_LINE_STATUS);
    //0x84 not defined
    VALUE_TO_STR_IF_MATCH(value, DAEMON_EVENT_RFCOMM_SERVICE_REGISTERED);
    VALUE_TO_STR_IF_MATCH(value, DAEMON_EVENT_RFCOMM_PERSISTENT_CHANNEL);
    VALUE_TO_STR_IF_MATCH(value, RFCOMM_EVENT_REMOTE_MODEM_STATUS);
    VALUE_TO_STR_IF_MATCH(value, RFCOMM_EVENT_PORT_CONFIGURATION);
    VALUE_TO_STR_IF_MATCH(value, RFCOMM_EVENT_CAN_SEND_NOW);
    VALUE_TO_STR_IF_MATCH(value, L2CAP_EVENT_ECBM_CHANNEL_OPENED);
    VALUE_TO_STR_IF_MATCH(value, L2CAP_EVENT_ECBM_RECONFIGURED);
    VALUE_TO_STR_IF_MATCH(value, L2CAP_EVENT_ECBM_RECONFIGURATION_COMPLETE);
    // 0x8d .. 0x8f not defined
    VALUE_TO_STR_IF_MATCH(value, DAEMON_EVENT_SDP_SERVICE_REGISTERED);
    VALUE_TO_STR_IF_MATCH(value, SDP_EVENT_QUERY_COMPLETE); 
    VALUE_TO_STR_IF_MATCH(value, SDP_EVENT_QUERY_RFCOMM_SERVICE);
    VALUE_TO_STR_IF_MATCH(value, SDP_EVENT_QUERY_ATTRIBUTE_BYTE);
    VALUE_TO_STR_IF_MATCH(value, SDP_EVENT_QUERY_ATTRIBUTE_VALUE);
    VALUE_TO_STR_IF_MATCH(value, SDP_EVENT_QUERY_SERVICE_RECORD_HANDLE);
    //0x96 .. 0x9f not defined
    VALUE_TO_STR_IF_MATCH(value, GATT_EVENT_QUERY_COMPLETE);
    VALUE_TO_STR_IF_MATCH(value, GATT_EVENT_SERVICE_QUERY_RESULT);
    VALUE_TO_STR_IF_MATCH(value, GATT_EVENT_CHARACTERISTIC_QUERY_RESULT);
    VALUE_TO_STR_IF_MATCH(value, GATT_EVENT_INCLUDED_SERVICE_QUERY_RESULT);
    VALUE_TO_STR_IF_MATCH(value, GATT_EVENT_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_RESULT);
    VALUE_TO_STR_IF_MATCH(value, GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT);
    VALUE_TO_STR_IF_MATCH(value, GATT_EVENT_LONG_CHARACTERISTIC_VALUE_QUERY_RESULT);
    VALUE_TO_STR_IF_MATCH(value, GATT_EVENT_NOTIFICATION);
    VALUE_TO_STR_IF_MATCH(value, GATT_EVENT_INDICATION);
    VALUE_TO_STR_IF_MATCH(value, GATT_EVENT_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT);
    VALUE_TO_STR_IF_MATCH(value, GATT_EVENT_LONG_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT);
    VALUE_TO_STR_IF_MATCH(value, GATT_EVENT_MTU);
    VALUE_TO_STR_IF_MATCH(value, GATT_EVENT_CAN_WRITE_WITHOUT_RESPONSE);
    //0xad .. 0xb2 not defined
    VALUE_TO_STR_IF_MATCH(value, ATT_EVENT_CONNECTED);
    VALUE_TO_STR_IF_MATCH(value, ATT_EVENT_DISCONNECTED);
    VALUE_TO_STR_IF_MATCH(value, ATT_EVENT_MTU_EXCHANGE_COMPLETE);
    VALUE_TO_STR_IF_MATCH(value, ATT_EVENT_HANDLE_VALUE_INDICATION_COMPLETE);
    VALUE_TO_STR_IF_MATCH(value, ATT_EVENT_CAN_SEND_NOW);
    //0xb8 .. 0xbf not defined
    VALUE_TO_STR_IF_MATCH(value, BNEP_EVENT_SERVICE_REGISTERED);
    VALUE_TO_STR_IF_MATCH(value, BNEP_EVENT_CHANNEL_OPENED);
    VALUE_TO_STR_IF_MATCH(value, BNEP_EVENT_CHANNEL_CLOSED);
    VALUE_TO_STR_IF_MATCH(value, BNEP_EVENT_CHANNEL_TIMEOUT);    
    VALUE_TO_STR_IF_MATCH(value, BNEP_EVENT_CAN_SEND_NOW);
    VALUE_TO_STR_IF_MATCH(value, SM_EVENT_JUST_WORKS_REQUEST);
    VALUE_TO_STR_IF_MATCH(value, SM_EVENT_PASSKEY_DISPLAY_NUMBER);
    VALUE_TO_STR_IF_MATCH(value, SM_EVENT_PASSKEY_DISPLAY_CANCEL);
    VALUE_TO_STR_IF_MATCH(value, SM_EVENT_PASSKEY_INPUT_NUMBER);
    VALUE_TO_STR_IF_MATCH(value, SM_EVENT_NUMERIC_COMPARISON_REQUEST);
    VALUE_TO_STR_IF_MATCH(value, SM_EVENT_IDENTITY_RESOLVING_STARTED);
    VALUE_TO_STR_IF_MATCH(value, SM_EVENT_IDENTITY_RESOLVING_FAILED);
    VALUE_TO_STR_IF_MATCH(value, SM_EVENT_IDENTITY_RESOLVING_SUCCEEDED);
    VALUE_TO_STR_IF_MATCH(value, SM_EVENT_AUTHORIZATION_REQUEST);
    VALUE_TO_STR_IF_MATCH(value, SM_EVENT_AUTHORIZATION_RESULT);
    VALUE_TO_STR_IF_MATCH(value, SM_EVENT_KEYPRESS_NOTIFICATION);
    VALUE_TO_STR_IF_MATCH(value, SM_EVENT_IDENTITY_CREATED);
    VALUE_TO_STR_IF_MATCH(value, SM_EVENT_PAIRING_STARTED);
    VALUE_TO_STR_IF_MATCH(value, SM_EVENT_PAIRING_COMPLETE);
    VALUE_TO_STR_IF_MATCH(value, SM_EVENT_REENCRYPTION_STARTED);
    VALUE_TO_STR_IF_MATCH(value, SM_EVENT_REENCRYPTION_COMPLETE);
    VALUE_TO_STR_IF_MATCH(value, GAP_EVENT_SECURITY_LEVEL);
    VALUE_TO_STR_IF_MATCH(value, GAP_EVENT_DEDICATED_BONDING_COMPLETED);
    VALUE_TO_STR_IF_MATCH(value, GAP_EVENT_ADVERTISING_REPORT);
    VALUE_TO_STR_IF_MATCH(value, GAP_EVENT_EXTENDED_ADVERTISING_REPORT);
    VALUE_TO_STR_IF_MATCH(value, GAP_EVENT_INQUIRY_RESULT);
    VALUE_TO_STR_IF_MATCH(value, GAP_EVENT_INQUIRY_COMPLETE);
    VALUE_TO_STR_IF_MATCH(value, GAP_EVENT_RSSI_MEASUREMENT);
    VALUE_TO_STR_IF_MATCH(value, GAP_EVENT_LOCAL_OOB_DATA);
    VALUE_TO_STR_IF_MATCH(value, GAP_EVENT_PAIRING_STARTED);
    VALUE_TO_STR_IF_MATCH(value, GAP_EVENT_PAIRING_COMPLETE);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_META_GAP);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_HSP_META);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_HFP_META);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_ANCS_META);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_AVDTP_META);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_AVRCP_META);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_GOEP_META);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_PBAP_META);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_HID_META);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_A2DP_META);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_HIDS_META);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_GATTSERVICE_META);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_BIP_META);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_MAP_META);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_MESH_META);
    //0xf6 .. 0xfe not defined
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_VENDOR_SPECIFIC);

    static char undefined_event_type[] = "Undefined HCI_EVENT_TYPE 0x00";
    sprintf(undefined_event_type, "Undefined HCI_EVENT_TYPE 0x%02x", value);
    return undefined_event_type;
}

char *bt_error_code_to_str(uint8_t value) {
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_SUCCESS);
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_UNKNOWN_HCI_COMMAND);
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER);
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_HARDWARE_FAILURE);
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_PAGE_TIMEOUT);
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_AUTHENTICATION_FAILURE);
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_PIN_OR_KEY_MISSING);
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_MEMORY_CAPACITY_EXCEEDED);
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_CONNECTION_TIMEOUT);
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_CONNECTION_LIMIT_EXCEEDED);
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_SYNCHRONOUS_CONNECTION_LIMIT_TO_A_DEVICE_EXCEEDED);
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_ACL_CONNECTION_ALREADY_EXISTS);
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_COMMAND_DISALLOWED);
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_CONNECTION_REJECTED_DUE_TO_LIMITED_RESOURCES);
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_CONNECTION_REJECTED_DUE_TO_SECURITY_REASONS); 
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_CONNECTION_REJECTED_DUE_TO_UNACCEPTABLE_BD_ADDR); 
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_CONNECTION_ACCEPT_TIMEOUT_EXCEEDED);
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE); 
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS); 
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_REMOTE_USER_TERMINATED_CONNECTION);
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_REMOTE_DEVICE_TERMINATED_CONNECTION_DUE_TO_LOW_RESOURCES);
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_REMOTE_DEVICE_TERMINATED_CONNECTION_DUE_TO_POWER_OFF);
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_CONNECTION_TERMINATED_BY_LOCAL_HOST);
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_REPEATED_ATTEMPTS);
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_PAIRING_NOT_ALLOWED);
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_UNKNOWN_LMP_PDU);
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_UNSUPPORTED_REMOTE_FEATURE_UNSUPPORTED_LMP_FEATURE);
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_SCO_OFFSET_REJECTED);
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_SCO_INTERVAL_REJECTED);
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_SCO_AIR_MODE_REJECTED);
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_INVALID_LMP_PARAMETERS_INVALID_LL_PARAMETERS);
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_UNSPECIFIED_ERROR);
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_UNSUPPORTED_LMP_PARAMETER_VALUE_UNSUPPORTED_LL_PARAMETER_VALUE);
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_ROLE_CHANGE_NOT_ALLOWED);
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_LMP_RESPONSE_TIMEOUT_LL_RESPONSE_TIMEOUT);
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_LMP_ERROR_TRANSACTION_COLLISION);
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_LMP_PDU_NOT_ALLOWED);
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_ENCRYPTION_MODE_NOT_ACCEPTABLE);
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_LINK_KEY_CANNOT_BE_CHANGED);
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_REQUESTED_QOS_NOT_SUPPORTED);
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_INSTANT_PASSED);
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_PAIRING_WITH_UNIT_KEY_NOT_SUPPORTED);
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_DIFFERENT_TRANSACTION_COLLISION);
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_RESERVED);
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_QOS_UNACCEPTABLE_PARAMETER);
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_QOS_REJECTED);
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_CHANNEL_CLASSIFICATION_NOT_SUPPORTED);
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_INSUFFICIENT_SECURITY);
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE);
    // ERROR_CODE_RESERVED
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_ROLE_SWITCH_PENDING);
    // ERROR_CODE_RESERVED
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_RESERVED_SLOT_VIOLATION);
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_ROLE_SWITCH_FAILED);
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_EXTENDED_INQUIRY_RESPONSE_TOO_LARGE);
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_SECURE_SIMPLE_PAIRING_NOT_SUPPORTED_BY_HOST);
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_HOST_BUSY_PAIRING);
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_CONNECTION_REJECTED_DUE_TO_NO_SUITABLE_CHANNEL_FOUND);
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_CONTROLLER_BUSY);
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_UNACCEPTABLE_CONNECTION_PARAMETERS);
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_DIRECTED_ADVERTISING_TIMEOUT);
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_CONNECTION_TERMINATED_DUE_TO_MIC_FAILURE);
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_CONNECTION_FAILED_TO_BE_ESTABLISHED);
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_MAC_CONNECTION_FAILED);
    VALUE_TO_STR_IF_MATCH(value, ERROR_CODE_COARSE_CLOCK_ADJUSTMENT_REJECTED_BUT_WILL_TRY_TO_ADJUST_USING_CLOCK_DRAGGING);

    // BTstack defined ERRORS, mapped into BLuetooth status code range
    VALUE_TO_STR_IF_MATCH(value, BTSTACK_CONNECTION_TO_BTDAEMON_FAILED);
    VALUE_TO_STR_IF_MATCH(value, BTSTACK_ACTIVATION_FAILED_SYSTEM_BLUETOOTH);
    VALUE_TO_STR_IF_MATCH(value, BTSTACK_ACTIVATION_POWERON_FAILED);
    VALUE_TO_STR_IF_MATCH(value, BTSTACK_ACTIVATION_FAILED_UNKNOWN);
    VALUE_TO_STR_IF_MATCH(value, BTSTACK_NOT_ACTIVATED);
    VALUE_TO_STR_IF_MATCH(value, BTSTACK_BUSY);
    VALUE_TO_STR_IF_MATCH(value, BTSTACK_MEMORY_ALLOC_FAILED);
    VALUE_TO_STR_IF_MATCH(value, BTSTACK_ACL_BUFFERS_FULL);

    // l2cap errors - enumeration by the command that created them
    VALUE_TO_STR_IF_MATCH(value, L2CAP_COMMAND_REJECT_REASON_COMMAND_NOT_UNDERSTOOD);
    VALUE_TO_STR_IF_MATCH(value, L2CAP_COMMAND_REJECT_REASON_SIGNALING_MTU_EXCEEDED);
    VALUE_TO_STR_IF_MATCH(value, L2CAP_COMMAND_REJECT_REASON_INVALID_CID_IN_REQUEST);

    VALUE_TO_STR_IF_MATCH(value, L2CAP_CONNECTION_RESPONSE_RESULT_SUCCESSFUL);
    VALUE_TO_STR_IF_MATCH(value, L2CAP_CONNECTION_RESPONSE_RESULT_PENDING);
    VALUE_TO_STR_IF_MATCH(value, L2CAP_CONNECTION_RESPONSE_RESULT_REFUSED_PSM);
    VALUE_TO_STR_IF_MATCH(value, L2CAP_CONNECTION_RESPONSE_RESULT_REFUSED_SECURITY);
    VALUE_TO_STR_IF_MATCH(value, L2CAP_CONNECTION_RESPONSE_RESULT_REFUSED_RESOURCES);
    VALUE_TO_STR_IF_MATCH(value, L2CAP_CONNECTION_RESPONSE_RESULT_ERTM_NOT_SUPPORTED);

    // should be L2CAP_CONNECTION_RTX_TIMEOUT
    VALUE_TO_STR_IF_MATCH(value, L2CAP_CONNECTION_RESPONSE_RESULT_RTX_TIMEOUT);
    VALUE_TO_STR_IF_MATCH(value, L2CAP_CONNECTION_BASEBAND_DISCONNECT);
    VALUE_TO_STR_IF_MATCH(value, L2CAP_SERVICE_ALREADY_REGISTERED);
    VALUE_TO_STR_IF_MATCH(value, L2CAP_DATA_LEN_EXCEEDS_REMOTE_MTU);
    VALUE_TO_STR_IF_MATCH(value, L2CAP_SERVICE_DOES_NOT_EXIST);
    VALUE_TO_STR_IF_MATCH(value, L2CAP_LOCAL_CID_DOES_NOT_EXIST);
    VALUE_TO_STR_IF_MATCH(value, L2CAP_CONNECTION_RESPONSE_UNKNOWN_ERROR);

    VALUE_TO_STR_IF_MATCH(value, RFCOMM_MULTIPLEXER_STOPPED);
    VALUE_TO_STR_IF_MATCH(value, RFCOMM_CHANNEL_ALREADY_REGISTERED);
    VALUE_TO_STR_IF_MATCH(value, RFCOMM_NO_OUTGOING_CREDITS);
    VALUE_TO_STR_IF_MATCH(value, RFCOMM_AGGREGATE_FLOW_OFF);
    VALUE_TO_STR_IF_MATCH(value, RFCOMM_DATA_LEN_EXCEEDS_MTU);

    VALUE_TO_STR_IF_MATCH(value, HFP_REMOTE_REJECTS_AUDIO_CONNECTION);

    VALUE_TO_STR_IF_MATCH(value, SDP_HANDLE_ALREADY_REGISTERED);
    VALUE_TO_STR_IF_MATCH(value, SDP_QUERY_INCOMPLETE);
    VALUE_TO_STR_IF_MATCH(value, SDP_SERVICE_NOT_FOUND);
    VALUE_TO_STR_IF_MATCH(value, SDP_HANDLE_INVALID);
    VALUE_TO_STR_IF_MATCH(value, SDP_QUERY_BUSY);

    VALUE_TO_STR_IF_MATCH(value, ATT_HANDLE_VALUE_INDICATION_IN_PROGRESS); 
    VALUE_TO_STR_IF_MATCH(value, ATT_HANDLE_VALUE_INDICATION_TIMEOUT);
    VALUE_TO_STR_IF_MATCH(value, ATT_HANDLE_VALUE_INDICATION_DISCONNECT);

    VALUE_TO_STR_IF_MATCH(value, GATT_CLIENT_NOT_CONNECTED);
    VALUE_TO_STR_IF_MATCH(value, GATT_CLIENT_BUSY);
    VALUE_TO_STR_IF_MATCH(value, GATT_CLIENT_IN_WRONG_STATE);
    VALUE_TO_STR_IF_MATCH(value, GATT_CLIENT_DIFFERENT_CONTEXT_FOR_ADDRESS_ALREADY_EXISTS);
    VALUE_TO_STR_IF_MATCH(value, GATT_CLIENT_VALUE_TOO_LONG);
    VALUE_TO_STR_IF_MATCH(value, GATT_CLIENT_CHARACTERISTIC_NOTIFICATION_NOT_SUPPORTED);
    VALUE_TO_STR_IF_MATCH(value, GATT_CLIENT_CHARACTERISTIC_INDICATION_NOT_SUPPORTED); 

    VALUE_TO_STR_IF_MATCH(value, BNEP_SERVICE_ALREADY_REGISTERED);
    VALUE_TO_STR_IF_MATCH(value, BNEP_CHANNEL_NOT_CONNECTED);
    VALUE_TO_STR_IF_MATCH(value, BNEP_DATA_LEN_EXCEEDS_MTU);

    // OBEX ERRORS
    VALUE_TO_STR_IF_MATCH(value, OBEX_UNKNOWN_ERROR);
    VALUE_TO_STR_IF_MATCH(value, OBEX_CONNECT_FAILED);
    VALUE_TO_STR_IF_MATCH(value, OBEX_DISCONNECTED);
    VALUE_TO_STR_IF_MATCH(value, OBEX_NOT_FOUND);
    VALUE_TO_STR_IF_MATCH(value, OBEX_NOT_ACCEPTABLE);
    VALUE_TO_STR_IF_MATCH(value, OBEX_ABORTED);

    VALUE_TO_STR_IF_MATCH(value, MESH_ERROR_APPKEY_INDEX_INVALID);

    static char undefined_error_code[] = "Undefined BT_ERROR_CODE 0x00";
    sprintf(undefined_error_code, "Undefined BT_ERROR_CODE 0x%02x", value);
    return undefined_error_code;
}

char *packet_type_to_str(uint8_t value) {
    VALUE_TO_STR_IF_MATCH(value, HCI_COMMAND_DATA_PACKET);
    VALUE_TO_STR_IF_MATCH(value, HCI_ACL_DATA_PACKET);
    VALUE_TO_STR_IF_MATCH(value, HCI_SCO_DATA_PACKET);
    VALUE_TO_STR_IF_MATCH(value, HCI_EVENT_PACKET);

    // FIXME: Don't know what 5 stands for
    //VALUE_TO_STR_IF_MATCH(value, HCI_ISO_DATA_PACKET);
    VALUE_TO_STR_IF_MATCH(value, DAEMON_EVENT_PACKET);
    // L2CAP data
    VALUE_TO_STR_IF_MATCH(value, L2CAP_DATA_PACKET);
    // RFCOMM data
    VALUE_TO_STR_IF_MATCH(value, RFCOMM_DATA_PACKET);
    // Attribute protocol data
    VALUE_TO_STR_IF_MATCH(value, ATT_DATA_PACKET);
    // Security Manager protocol data
    VALUE_TO_STR_IF_MATCH(value, SM_DATA_PACKET);
    // SDP query result - only used by daemon
    // format: type (8), record_id (16), attribute_id (16), attribute_length (16), attribute_value (max 1k)
    VALUE_TO_STR_IF_MATCH(value, SDP_CLIENT_PACKET);
    // BNEP data
    VALUE_TO_STR_IF_MATCH(value, BNEP_DATA_PACKET);
    // Unicast Connectionless Data
    VALUE_TO_STR_IF_MATCH(value, UCD_DATA_PACKET);
    // GOEP data
    VALUE_TO_STR_IF_MATCH(value, GOEP_DATA_PACKET);
    // PBAP data
    VALUE_TO_STR_IF_MATCH(value, PBAP_DATA_PACKET);
    // AVRCP browsing data
    VALUE_TO_STR_IF_MATCH(value, AVRCP_BROWSING_DATA_PACKET);
    // MAP data
    VALUE_TO_STR_IF_MATCH(value, MAP_DATA_PACKET);
    // Mesh Provisioning PDU
    VALUE_TO_STR_IF_MATCH(value, PROVISIONING_DATA_PACKET);
    // Mesh Proxy PDU
    VALUE_TO_STR_IF_MATCH(value, MESH_PROXY_DATA_PACKET);
    // Mesh Network PDU
    VALUE_TO_STR_IF_MATCH(value, MESH_NETWORK_PACKET);
    // Mesh Network PDU
    VALUE_TO_STR_IF_MATCH(value, MESH_BEACON_PACKET);
    // debug log messages
    VALUE_TO_STR_IF_MATCH(value, LOG_MESSAGE_PACKET);

    static char undefined_packet_type[] = "Undefined PACKET_TYPE 0x00";
    sprintf(undefined_packet_type, "Undefined PACKET_TYPE 0x%02x", value);
    return undefined_packet_type;
}

/* 
 * @section Gerenal Packet Handler
 * 
 * @text Handles startup (BTSTACK_EVENT_STATE), inquiry, pairing
 */
static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    bd_addr_t peer_addr;

	switch (packet_type) {
		case HCI_EVENT_PACKET:
			switch (hci_event_packet_get_type(packet)) {
                case BTSTACK_EVENT_STATE:
                    if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING) return;
                    dump_hci_event = true;
                    break;

                case HCI_EVENT_CONNECTION_COMPLETE:
                    if (ERROR_CODE_SUCCESS == hci_event_connection_complete_get_status(packet)) {
                        printf("ACL link 0x%x established.\n", hci_event_connection_complete_get_connection_handle(packet));
                    }
                    else {
                        printf("ACL link failed %s\n", bt_error_code_to_str(hci_event_connection_complete_get_status(packet)));
                    }
                    break;

                case GAP_EVENT_PAIRING_STARTED:
                    gap_event_pairing_started_get_bd_addr(packet, peer_addr);
                    printf("Paring started from %s on link 0x%x, ssp %d, initiator %d\n",
                        bd_addr_to_str(peer_addr), gap_event_pairing_started_get_con_handle(packet), \
                        gap_event_pairing_started_get_ssp(packet), \
                        gap_event_pairing_started_get_initiator(packet));
                    break;

                case HCI_EVENT_IO_CAPABILITY_RESPONSE:
                    printf("Peer io_capability %d oob_data_present %d authentication_requirements %d\n", \
                        hci_event_io_capability_response_get_io_capability(packet), \
                        hci_event_io_capability_response_get_oob_data_present(packet), \
                        hci_event_io_capability_response_get_authentication_requirements(packet));
                    break;

                case GAP_EVENT_PAIRING_COMPLETE:
                    gap_event_pairing_complete_get_bd_addr(packet, peer_addr);
                    if (ERROR_CODE_SUCCESS == gap_event_pairing_complete_get_status(packet)) {
                        printf("Paired to %s\n", bd_addr_to_str(peer_addr));
                    }
                    else {
                        printf("Error %s when paring with %s\n", \
                            bt_error_code_to_str(gap_event_pairing_complete_get_status(packet)), bd_addr_to_str(peer_addr));
                    }
                    break;

                case GAP_EVENT_SECURITY_LEVEL:
                    printf("Link handle 0x%x security level %d\n", \
                        gap_event_security_level_get_handle(packet), \
                        gap_event_security_level_get_security_level(packet));
                    break;

                case HCI_EVENT_PIN_CODE_REQUEST:
                    // inform about pin code request
                    printf("LMP Paring - pin code request - using '0000'\n");
                    hci_event_pin_code_request_get_bd_addr(packet, peer_addr);
                    gap_pin_code_response(peer_addr, "0000");
                    break;

                case HCI_EVENT_USER_CONFIRMATION_REQUEST:
                    // inform about user confirmation request
                    printf("SSP User Confirmation Request with numeric value '%06"PRIu32"'\n", little_endian_read_32(packet, 8));
                    printf("SSP User Confirmation Auto accept\n");
                    break;

                default:
                    if (dump_hci_event) {
                        printf("%s\n", hci_event_type_to_str(hci_event_packet_get_type(packet)));
                    }
                    break;
			}
            break;

        default:
            printf("%s\n", packet_type_to_str(packet_type));
            break;
	}
}

/*
 * @section Main Application Setup
 *
 * @text As with the packet handlers, the combined app setup contains the code from the individual example setups.
 */

#ifdef HAVE_BTSTACK_STDIN
static void usage(const char *name){
	fprintf(stderr, "\nUsage: %s [-h|--help]\n"
                    "\t[-s|--ssp <0|1>]\n"
                    "\t[-i|--io_capability <0-3>]\n"
                    "\t[-l|--level <0-4>]\n", name);

    fprintf(stderr, "\n\t-h --help\n"
                    "\tPrint this usage and exit.\n"
                    
                    "\n\t-s --ssp\n"
                    "\tEnable(1)/disable(0) Secure Simple Paring. Use enable(1) if not specified.\n"
                    "\tIf SSP is disabled, LMP paring(PIN-code based) is used.\n"
                    "\tSSP should always been enabled unless working with devices which doesn't support Bluetooth 2.1.\n"

                    "\n\t-i --io_capability\n"
                    "\tSet local io_capability when using SSP. Use DISPLAY_ONLY(0) if not specified.\n"
                    "\t\t0 -- DISPLAY_ONLY\n"
                    "\t\t1 -- DISPLAY_YES_NO\n"
                    "\t\t2 -- KEYBOARD_ONLY\n"
                    "\t\t3 -- NO_INPUT_NO_OUTPUT\n"

                    "\n\t-l --level\n"
                    "\tSepcify the security level once a connection is established. Use 0 if not specified.\n");
}
#endif

/* LISTING_START(MainConfiguration): Init L2CAP and starts to inquiry or connect */
int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]) {
    #ifdef HAVE_BTSTACK_STDIN
    int arg = 1;
    while (arg < argc) {
        if (!strcmp(argv[arg], "-h") || !strcmp(argv[arg], "--help")) {
            usage(argv[0]);
            hci_power_control(HCI_POWER_OFF);
            exit(0);
        }
        else if(!strcmp(argv[arg], "-s") || !strcmp(argv[arg], "--ssp")) {
            arg++;
            secure_simple_paring = atoi(argv[arg]);
            arg++;
        }
        else if(!strcmp(argv[arg], "-i") || !strcmp(argv[arg], "--io_capability")) {
            arg++;
            io_capability = atoi(argv[arg]);
            arg++;
        }
        else if(!strcmp(argv[arg], "-l") || !strcmp(argv[arg], "--level")) {
            arg++;
            security_level = atoi(argv[arg]);
            arg++;
        }
        else {
            printf("Unknown argument %s omitted.\n", argv[arg++]);
        }
	}
    #endif

    printf("Secure simple paring is %s\n", secure_simple_paring ? "enabled" : "disabled");
    if (secure_simple_paring) {
        printf("Local io_capability is %d\n", io_capability);
    }
    printf("Security level for completed link is %d\n", security_level);

    l2cap_init();

    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // init legacy pin code
    gap_discoverable_control(true);
    gap_connectable_control(true);
    gap_set_bondable_mode(true);
    
    gap_ssp_set_enable(secure_simple_paring);
    if (secure_simple_paring) {
        gap_ssp_set_io_capability(io_capability);
    }

    // turn on!
	hci_power_control(HCI_POWER_ON);
	    
    return 0;
}
/* LISTING_END */
/* EXAMPLE_END */
