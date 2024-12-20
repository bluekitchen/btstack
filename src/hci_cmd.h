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

/**
 *  HCI Command Builder and official commands
 *
 */

#ifndef HCI_CMDS_H
#define HCI_CMDS_H

#include "bluetooth.h"
#include "btstack_defines.h"

#include <stdint.h>
#include <stdarg.h>

#if defined __cplusplus
extern "C" {
#endif

// calculate hci opcode for ogf/ocf value
#define HCI_OPCODE(ogf, ocf) ((ocf) | ((ogf) << 10))

/**
 *  Hardware state of Bluetooth controller 
 */
typedef enum {
    HCI_POWER_OFF = 0,
    HCI_POWER_ON,
    HCI_POWER_SLEEP
} HCI_POWER_MODE;

/**
 * State of BTstack 
 */
typedef enum {
    HCI_STATE_OFF = 0,
    HCI_STATE_INITIALIZING,
    HCI_STATE_WORKING,
    HCI_STATE_HALTING,
    HCI_STATE_SLEEPING,
    HCI_STATE_FALLING_ASLEEP
} HCI_STATE;

/** 
 * compact HCI Command packet description
 */
 typedef struct {
    uint16_t    opcode;
    const char *format;
} hci_cmd_t;

typedef enum {

    // Link Control
    HCI_OPCODE_HCI_INQUIRY = HCI_OPCODE (OGF_LINK_CONTROL, 0x01),
    HCI_OPCODE_HCI_INQUIRY_CANCEL = HCI_OPCODE (OGF_LINK_CONTROL, 0x02),
    HCI_OPCODE_HCI_PERIODIC_INQUIRY_MODE = HCI_OPCODE(OGF_LINK_CONTROL, 0x03),
    HCI_OPCODE_HCI_EXIT_PERIODIC_INQUIRY_MODE = HCI_OPCODE(OGF_LINK_CONTROL, 0x04),
    HCI_OPCODE_HCI_CREATE_CONNECTION = HCI_OPCODE (OGF_LINK_CONTROL, 0x05),
    HCI_OPCODE_HCI_DISCONNECT = HCI_OPCODE (OGF_LINK_CONTROL, 0x06),
    HCI_OPCODE_HCI_CREATE_CONNECTION_CANCEL = HCI_OPCODE (OGF_LINK_CONTROL, 0x08),
    HCI_OPCODE_HCI_ACCEPT_CONNECTION_REQUEST = HCI_OPCODE (OGF_LINK_CONTROL, 0x09),
    HCI_OPCODE_HCI_REJECT_CONNECTION_REQUEST = HCI_OPCODE (OGF_LINK_CONTROL, 0x0a),
    HCI_OPCODE_HCI_LINK_KEY_REQUEST_REPLY = HCI_OPCODE (OGF_LINK_CONTROL, 0x0b),
    HCI_OPCODE_HCI_LINK_KEY_REQUEST_NEGATIVE_REPLY = HCI_OPCODE (OGF_LINK_CONTROL, 0x0c),
    HCI_OPCODE_HCI_PIN_CODE_REQUEST_REPLY = HCI_OPCODE (OGF_LINK_CONTROL, 0x0d),
    HCI_OPCODE_HCI_PIN_CODE_REQUEST_NEGATIVE_REPLY = HCI_OPCODE (OGF_LINK_CONTROL, 0x0e),
    HCI_OPCODE_HCI_CHANGE_CONNECTION_PACKET_TYPE = HCI_OPCODE (OGF_LINK_CONTROL, 0x0f),
    HCI_OPCODE_HCI_AUTHENTICATION_REQUESTED = HCI_OPCODE (OGF_LINK_CONTROL, 0x11),
    HCI_OPCODE_HCI_SET_CONNECTION_ENCRYPTION = HCI_OPCODE (OGF_LINK_CONTROL, 0x13),
    HCI_OPCODE_HCI_CHANGE_CONNECTION_LINK_KEY = HCI_OPCODE (OGF_LINK_CONTROL, 0x15),
    HCI_OPCODE_HCI_REMOTE_NAME_REQUEST = HCI_OPCODE (OGF_LINK_CONTROL, 0x19),
    HCI_OPCODE_HCI_REMOTE_NAME_REQUEST_CANCEL = HCI_OPCODE (OGF_LINK_CONTROL, 0x1A),
    HCI_OPCODE_HCI_READ_REMOTE_SUPPORTED_FEATURES_COMMAND = HCI_OPCODE (OGF_LINK_CONTROL, 0x1B),
    HCI_OPCODE_HCI_READ_REMOTE_EXTENDED_FEATURES_COMMAND = HCI_OPCODE (OGF_LINK_CONTROL, 0x1C),
    HCI_OPCODE_HCI_READ_REMOTE_VERSION_INFORMATION = HCI_OPCODE (OGF_LINK_CONTROL, 0x1D),
    HCI_OPCODE_HCI_READ_CLOCK_OFFSET = HCI_OPCODE (OGF_LINK_CONTROL, 0x1F),
    HCI_OPCODE_HCI_SETUP_SYNCHRONOUS_CONNECTION = HCI_OPCODE (OGF_LINK_CONTROL, 0x0028),
    HCI_OPCODE_HCI_ACCEPT_SYNCHRONOUS_CONNECTION = HCI_OPCODE (OGF_LINK_CONTROL, 0x0029),
    HCI_OPCODE_HCI_IO_CAPABILITY_REQUEST_REPLY = HCI_OPCODE (OGF_LINK_CONTROL, 0x2b),
    HCI_OPCODE_HCI_USER_CONFIRMATION_REQUEST_REPLY = HCI_OPCODE (OGF_LINK_CONTROL, 0x2c),
    HCI_OPCODE_HCI_USER_CONFIRMATION_REQUEST_NEGATIVE_REPLY = HCI_OPCODE (OGF_LINK_CONTROL, 0x2d),
    HCI_OPCODE_HCI_USER_PASSKEY_REQUEST_REPLY = HCI_OPCODE (OGF_LINK_CONTROL, 0x2e),
    HCI_OPCODE_HCI_USER_PASSKEY_REQUEST_NEGATIVE_REPLY = HCI_OPCODE (OGF_LINK_CONTROL, 0x2f),
    HCI_OPCODE_HCI_REMOTE_OOB_DATA_REQUEST_REPLY = HCI_OPCODE (OGF_LINK_CONTROL, 0x30),
    HCI_OPCODE_HCI_REMOTE_OOB_DATA_REQUEST_NEGATIVE_REPLY = HCI_OPCODE (OGF_LINK_CONTROL, 0x33),
    HCI_OPCODE_HCI_IO_CAPABILITY_REQUEST_NEGATIVE_REPLY = HCI_OPCODE (OGF_LINK_CONTROL, 0x34),
    HCI_OPCODE_HCI_ENHANCED_SETUP_SYNCHRONOUS_CONNECTION = HCI_OPCODE (OGF_LINK_CONTROL, 0x3d),
    HCI_OPCODE_HCI_ENHANCED_ACCEPT_SYNCHRONOUS_CONNECTION = HCI_OPCODE (OGF_LINK_CONTROL, 0x3e),
    HCI_OPCODE_HCI_REMOTE_OOB_EXTENDED_DATA_REQUEST_REPLY = HCI_OPCODE (OGF_LINK_CONTROL, 0x45),

    // Link Policy
    HCI_OPCODE_HCI_HOLD_MODE = HCI_OPCODE (OGF_LINK_POLICY, 0x01),
    HCI_OPCODE_HCI_SNIFF_MODE = HCI_OPCODE (OGF_LINK_POLICY, 0x03),
    HCI_OPCODE_HCI_EXIT_SNIFF_MODE = HCI_OPCODE (OGF_LINK_POLICY, 0x04),
    HCI_OPCODE_HCI_PARK_STATE = HCI_OPCODE (OGF_LINK_POLICY, 0x05),
    HCI_OPCODE_HCI_EXIT_PARK_STATE = HCI_OPCODE (OGF_LINK_POLICY, 0x06),
    HCI_OPCODE_HCI_QOS_SETUP = HCI_OPCODE (OGF_LINK_POLICY, 0x07),
    HCI_OPCODE_HCI_ROLE_DISCOVERY = HCI_OPCODE (OGF_LINK_POLICY, 0x09),
    HCI_OPCODE_HCI_SWITCH_ROLE_COMMAND = HCI_OPCODE (OGF_LINK_POLICY, 0x0b),
    HCI_OPCODE_HCI_READ_LINK_POLICY_SETTINGS = HCI_OPCODE (OGF_LINK_POLICY, 0x0c),
    HCI_OPCODE_HCI_WRITE_LINK_POLICY_SETTINGS = HCI_OPCODE (OGF_LINK_POLICY, 0x0d),
    HCI_OPCODE_HCI_WRITE_DEFAULT_LINK_POLICY_SETTING = HCI_OPCODE (OGF_LINK_POLICY, 0x0F),
    HCI_OPCODE_HCI_FLOW_SPECIFICATION = HCI_OPCODE(OGF_LINK_POLICY, 0x10),
    HCI_OPCODE_HCI_SNIFF_SUBRATING = HCI_OPCODE (OGF_LINK_POLICY, 0x11),

    // Controller Baseband
    HCI_OPCODE_HCI_SET_EVENT_MASK = HCI_OPCODE (OGF_CONTROLLER_BASEBAND, 0x01),
    HCI_OPCODE_HCI_RESET = HCI_OPCODE (OGF_CONTROLLER_BASEBAND, 0x03),
    HCI_OPCODE_HCI_FLUSH = HCI_OPCODE (OGF_CONTROLLER_BASEBAND, 0x08),
    HCI_OPCODE_HCI_READ_PIN_TYPE = HCI_OPCODE (OGF_CONTROLLER_BASEBAND, 0x09),
    HCI_OPCODE_HCI_WRITE_PIN_TYPE = HCI_OPCODE (OGF_CONTROLLER_BASEBAND, 0x0A),
    HCI_OPCODE_HCI_DELETE_STORED_LINK_KEY = HCI_OPCODE (OGF_CONTROLLER_BASEBAND, 0x12),
    HCI_OPCODE_HCI_WRITE_LOCAL_NAME = HCI_OPCODE (OGF_CONTROLLER_BASEBAND, 0x13),
    HCI_OPCODE_HCI_READ_LOCAL_NAME = HCI_OPCODE (OGF_CONTROLLER_BASEBAND, 0x14),
    HCI_OPCODE_HCI_READ_PAGE_TIMEOUT = HCI_OPCODE (OGF_CONTROLLER_BASEBAND, 0x17),
    HCI_OPCODE_HCI_WRITE_PAGE_TIMEOUT = HCI_OPCODE (OGF_CONTROLLER_BASEBAND, 0x18),
    HCI_OPCODE_HCI_WRITE_SCAN_ENABLE = HCI_OPCODE (OGF_CONTROLLER_BASEBAND, 0x1A),
    HCI_OPCODE_HCI_READ_PAGE_SCAN_ACTIVITY = HCI_OPCODE (OGF_CONTROLLER_BASEBAND, 0x1B),
    HCI_OPCODE_HCI_WRITE_PAGE_SCAN_ACTIVITY = HCI_OPCODE (OGF_CONTROLLER_BASEBAND, 0x1C),
    HCI_OPCODE_HCI_READ_INQUIRY_SCAN_ACTIVITY = HCI_OPCODE (OGF_CONTROLLER_BASEBAND, 0x1D),
    HCI_OPCODE_HCI_WRITE_INQUIRY_SCAN_ACTIVITY = HCI_OPCODE (OGF_CONTROLLER_BASEBAND, 0x1E),
    HCI_OPCODE_HCI_WRITE_AUTHENTICATION_ENABLE = HCI_OPCODE (OGF_CONTROLLER_BASEBAND, 0x20),
    HCI_OPCODE_HCI_WRITE_CLASS_OF_DEVICE = HCI_OPCODE (OGF_CONTROLLER_BASEBAND, 0x24),
    HCI_OPCODE_HCI_WRITE_AUTOMATIC_FLUSH_TIMEOUT = HCI_OPCODE(OGF_CONTROLLER_BASEBAND, 0x28),
    HCI_OPCODE_HCI_READ_NUM_BROADCAST_RETRANSMISSIONS = HCI_OPCODE (OGF_CONTROLLER_BASEBAND, 0x29),
    HCI_OPCODE_HCI_WRITE_NUM_BROADCAST_RETRANSMISSIONS = HCI_OPCODE (OGF_CONTROLLER_BASEBAND, 0x2a),
    HCI_OPCODE_HCI_READ_TRANSMIT_POWER_LEVEL = HCI_OPCODE (OGF_CONTROLLER_BASEBAND, 0x2D),
    HCI_OPCODE_HCI_WRITE_SYNCHRONOUS_FLOW_CONTROL_ENABLE = HCI_OPCODE (OGF_CONTROLLER_BASEBAND, 0x2f),
    HCI_OPCODE_HCI_SET_CONTROLLER_TO_HOST_FLOW_CONTROL = HCI_OPCODE (OGF_CONTROLLER_BASEBAND, 0x31),
    HCI_OPCODE_HCI_HOST_BUFFER_SIZE = HCI_OPCODE (OGF_CONTROLLER_BASEBAND, 0x33),
    HCI_OPCODE_HCI_HOST_NUMBER_OF_COMPLETED_PACKETS = HCI_OPCODE (OGF_CONTROLLER_BASEBAND, 0x35),
    HCI_OPCODE_HCI_READ_LINK_SUPERVISION_TIMEOUT = HCI_OPCODE (OGF_CONTROLLER_BASEBAND, 0x36),
    HCI_OPCODE_HCI_WRITE_LINK_SUPERVISION_TIMEOUT = HCI_OPCODE (OGF_CONTROLLER_BASEBAND, 0x37),
    HCI_OPCODE_HCI_WRITE_CURRENT_IAC_LAP_TWO_IACS = HCI_OPCODE (OGF_CONTROLLER_BASEBAND, 0x3A),
    HCI_OPCODE_HCI_READ_INQUIRY_SCAN_TYPE = HCI_OPCODE (OGF_CONTROLLER_BASEBAND, 0x42),
    HCI_OPCODE_HCI_WRITE_INQUIRY_SCAN_TYPE = HCI_OPCODE (OGF_CONTROLLER_BASEBAND, 0x43),
    HCI_OPCODE_HCI_READ_INQUIRY_MODE = HCI_OPCODE (OGF_CONTROLLER_BASEBAND, 0x44),
    HCI_OPCODE_HCI_WRITE_INQUIRY_MODE = HCI_OPCODE (OGF_CONTROLLER_BASEBAND, 0x45),
    HCI_OPCODE_HCI_READ_PAGE_SCAN_TYPE = HCI_OPCODE (OGF_CONTROLLER_BASEBAND, 0x46),
    HCI_OPCODE_HCI_WRITE_PAGE_SCAN_TYPE = HCI_OPCODE (OGF_CONTROLLER_BASEBAND, 0x47),
    HCI_OPCODE_HCI_WRITE_EXTENDED_INQUIRY_RESPONSE = HCI_OPCODE (OGF_CONTROLLER_BASEBAND, 0x52),
    HCI_OPCODE_HCI_WRITE_SIMPLE_PAIRING_MODE = HCI_OPCODE (OGF_CONTROLLER_BASEBAND, 0x56),
    HCI_OPCODE_HCI_READ_LOCAL_OOB_DATA = HCI_OPCODE (OGF_CONTROLLER_BASEBAND, 0x57),
    HCI_OPCODE_HCI_READ_INQUIRY_RESPONSE_TRANSMIT_POWER_LEVEL = HCI_OPCODE (OGF_CONTROLLER_BASEBAND, 0x58),
    HCI_OPCODE_HCI_WRITE_INQUIRY_TRANSMIT_POWER_LEVEL = HCI_OPCODE (OGF_CONTROLLER_BASEBAND, 0x59),
    HCI_OPCODE_HCI_WRITE_DEFAULT_ERRONEOUS_DATA_REPORTING = HCI_OPCODE (OGF_CONTROLLER_BASEBAND, 0x5B),
    HCI_OPCODE_HCI_SET_EVENT_MASK_2 = HCI_OPCODE (OGF_CONTROLLER_BASEBAND, 0x63),
    HCI_OPCODE_HCI_READ_LE_HOST_SUPPORTED = HCI_OPCODE (OGF_CONTROLLER_BASEBAND, 0x6c),
    HCI_OPCODE_HCI_WRITE_LE_HOST_SUPPORTED = HCI_OPCODE (OGF_CONTROLLER_BASEBAND, 0x6d),
    HCI_OPCODE_HCI_WRITE_SECURE_CONNECTIONS_HOST_SUPPORT = HCI_OPCODE (OGF_CONTROLLER_BASEBAND, 0x7a),
    HCI_OPCODE_HCI_READ_LOCAL_EXTENDED_OOB_DATA = HCI_OPCODE (OGF_CONTROLLER_BASEBAND, 0x7d),
    HCI_OPCODE_HCI_READ_EXTENDED_PAGE_TIMEOUT = HCI_OPCODE (OGF_CONTROLLER_BASEBAND, 0x7e),
    HCI_OPCODE_HCI_WRITE_EXTENDED_PAGE_TIMEOUT = HCI_OPCODE (OGF_CONTROLLER_BASEBAND, 0x7f),
    HCI_OPCODE_HCI_READ_EXTENDED_INQUIRY_LENGTH = HCI_OPCODE (OGF_CONTROLLER_BASEBAND, 0x80),
    HCI_OPCODE_HCI_WRITE_EXTENDED_INQUIRY_LENGTH = HCI_OPCODE (OGF_CONTROLLER_BASEBAND, 0x81),
    HCI_OPCODE_HCI_SET_ECOSYSTEM_BASE_INTERVAL = HCI_OPCODE (OGF_CONTROLLER_BASEBAND, 0x82),
    HCI_OPCODE_HCI_CONFIGURE_DATA_PATH = HCI_OPCODE (OGF_CONTROLLER_BASEBAND, 0x83),
    HCI_OPCODE_HCI_SET_MIN_ENCRYPTION_KEY_SIZE = HCI_OPCODE (OGF_CONTROLLER_BASEBAND, 0x84),

    // Testing
    HCI_OPCODE_HCI_READ_LOOPBACK_MODE = HCI_OPCODE (OGF_TESTING, 0x01),
    HCI_OPCODE_HCI_WRITE_LOOPBACK_MODE = HCI_OPCODE (OGF_TESTING, 0x02),
    HCI_OPCODE_HCI_ENABLE_DEVICE_UNDER_TEST_MODE = HCI_OPCODE (OGF_TESTING, 0x03),
    HCI_OPCODE_HCI_WRITE_SIMPLE_PAIRING_DEBUG_MODE = HCI_OPCODE (OGF_TESTING, 0x04),
    HCI_OPCODE_HCI_WRITE_SECURE_CONNECTIONS_TEST_MODE = HCI_OPCODE (OGF_TESTING, 0x0a),

    // Information Parameters
    HCI_OPCODE_HCI_READ_LOCAL_VERSION_INFORMATION = HCI_OPCODE (OGF_INFORMATIONAL_PARAMETERS, 0x01),
    HCI_OPCODE_HCI_READ_LOCAL_SUPPORTED_COMMANDS = HCI_OPCODE (OGF_INFORMATIONAL_PARAMETERS, 0x02),
    HCI_OPCODE_HCI_READ_LOCAL_SUPPORTED_FEATURES = HCI_OPCODE (OGF_INFORMATIONAL_PARAMETERS, 0x03),
    HCI_OPCODE_HCI_READ_BUFFER_SIZE = HCI_OPCODE (OGF_INFORMATIONAL_PARAMETERS, 0x05),
    HCI_OPCODE_HCI_READ_BD_ADDR = HCI_OPCODE (OGF_INFORMATIONAL_PARAMETERS, 0x09),

    // Status Parameters
    HCI_OPCODE_HCI_READ_FAILED_CONTACT_COUNTER = HCI_OPCODE (OGF_STATUS_PARAMETERS, 0x01),
    HCI_OPCODE_HCI_RESET_FAILED_CONTACT_COUNTER = HCI_OPCODE (OGF_STATUS_PARAMETERS, 0x02),
    HCI_OPCODE_HCI_READ_LINK_QUALITY = HCI_OPCODE (OGF_STATUS_PARAMETERS, 0x03),
    HCI_OPCODE_HCI_READ_RSSI = HCI_OPCODE (OGF_STATUS_PARAMETERS, 0x05),
    HCI_OPCODE_HCI_READ_CLOCK = HCI_OPCODE (OGF_STATUS_PARAMETERS, 0x07),
    HCI_OPCODE_HCI_READ_ENCRYPTION_KEY_SIZE = HCI_OPCODE (OGF_STATUS_PARAMETERS, 0x08),

    // LE Controller
    HCI_OPCODE_HCI_LE_SET_EVENT_MASK = HCI_OPCODE (OGF_LE_CONTROLLER, 0x01),
    HCI_OPCODE_HCI_LE_READ_BUFFER_SIZE = HCI_OPCODE (OGF_LE_CONTROLLER, 0x02),
    HCI_OPCODE_HCI_LE_READ_LOCAL_SUPPORTED_FEATURES = HCI_OPCODE (OGF_LE_CONTROLLER, 0x03),
    HCI_OPCODE_HCI_LE_SET_RANDOM_ADDRESS = HCI_OPCODE (OGF_LE_CONTROLLER, 0x05),
    HCI_OPCODE_HCI_LE_SET_ADVERTISING_PARAMETERS = HCI_OPCODE (OGF_LE_CONTROLLER, 0x06),
    HCI_OPCODE_HCI_LE_READ_ADVERTISING_CHANNEL_TX_POWER = HCI_OPCODE (OGF_LE_CONTROLLER, 0x07),
    HCI_OPCODE_HCI_LE_SET_ADVERTISING_DATA = HCI_OPCODE (OGF_LE_CONTROLLER, 0x08),
    HCI_OPCODE_HCI_LE_SET_SCAN_RESPONSE_DATA = HCI_OPCODE (OGF_LE_CONTROLLER, 0x09),
    HCI_OPCODE_HCI_LE_SET_ADVERTISE_ENABLE = HCI_OPCODE (OGF_LE_CONTROLLER, 0x0a),
    HCI_OPCODE_HCI_LE_SET_SCAN_PARAMETERS = HCI_OPCODE (OGF_LE_CONTROLLER, 0x0b),
    HCI_OPCODE_HCI_LE_SET_SCAN_ENABLE = HCI_OPCODE (OGF_LE_CONTROLLER, 0x0c),
    HCI_OPCODE_HCI_LE_CREATE_CONNECTION = HCI_OPCODE (OGF_LE_CONTROLLER, 0x0d),
    HCI_OPCODE_HCI_LE_CREATE_CONNECTION_CANCEL = HCI_OPCODE (OGF_LE_CONTROLLER, 0x0e),
    HCI_OPCODE_HCI_LE_READ_WHITE_LIST_SIZE = HCI_OPCODE (OGF_LE_CONTROLLER, 0x0f),
    HCI_OPCODE_HCI_LE_CLEAR_WHITE_LIST = HCI_OPCODE (OGF_LE_CONTROLLER, 0x10),
    HCI_OPCODE_HCI_LE_ADD_DEVICE_TO_WHITE_LIST = HCI_OPCODE (OGF_LE_CONTROLLER, 0x11),
    HCI_OPCODE_HCI_LE_REMOVE_DEVICE_FROM_WHITE_LIST = HCI_OPCODE (OGF_LE_CONTROLLER, 0x12),
    HCI_OPCODE_HCI_LE_CONNECTION_UPDATE = HCI_OPCODE (OGF_LE_CONTROLLER, 0x13),
    HCI_OPCODE_HCI_LE_SET_HOST_CHANNEL_CLASSIFICATION = HCI_OPCODE (OGF_LE_CONTROLLER, 0x14),
    HCI_OPCODE_HCI_LE_READ_CHANNEL_MAP = HCI_OPCODE (OGF_LE_CONTROLLER, 0x15),
    HCI_OPCODE_HCI_LE_READ_REMOTE_USED_FEATURES = HCI_OPCODE (OGF_LE_CONTROLLER, 0x16),
    HCI_OPCODE_HCI_LE_ENCRYPT = HCI_OPCODE (OGF_LE_CONTROLLER, 0x17),
    HCI_OPCODE_HCI_LE_RAND = HCI_OPCODE (OGF_LE_CONTROLLER, 0x18),
    HCI_OPCODE_HCI_LE_START_ENCRYPTION = HCI_OPCODE (OGF_LE_CONTROLLER, 0x19),
    HCI_OPCODE_HCI_LE_LONG_TERM_KEY_REQUEST_REPLY = HCI_OPCODE (OGF_LE_CONTROLLER, 0x1a),
    HCI_OPCODE_HCI_LE_LONG_TERM_KEY_NEGATIVE_REPLY = HCI_OPCODE (OGF_LE_CONTROLLER, 0x1b),
    HCI_OPCODE_HCI_LE_READ_SUPPORTED_STATES = HCI_OPCODE (OGF_LE_CONTROLLER, 0x1c),
    HCI_OPCODE_HCI_LE_RECEIVER_TEST = HCI_OPCODE (OGF_LE_CONTROLLER, 0x1d),
    HCI_OPCODE_HCI_LE_TRANSMITTER_TEST = HCI_OPCODE (OGF_LE_CONTROLLER, 0x1e),
    HCI_OPCODE_HCI_LE_TEST_END = HCI_OPCODE (OGF_LE_CONTROLLER, 0x1f),
    HCI_OPCODE_HCI_LE_REMOTE_CONNECTION_PARAMETER_REQUEST_REPLY = HCI_OPCODE (OGF_LE_CONTROLLER, 0x20),
    HCI_OPCODE_HCI_LE_REMOTE_CONNECTION_PARAMETER_REQUEST_NEGATIVE_REPLY = HCI_OPCODE (OGF_LE_CONTROLLER, 0x21),
    HCI_OPCODE_HCI_LE_SET_DATA_LENGTH = HCI_OPCODE (OGF_LE_CONTROLLER, 0x22),
    HCI_OPCODE_HCI_LE_READ_SUGGESTED_DEFAULT_DATA_LENGTH = HCI_OPCODE (OGF_LE_CONTROLLER, 0x23),
    HCI_OPCODE_HCI_LE_WRITE_SUGGESTED_DEFAULT_DATA_LENGTH = HCI_OPCODE (OGF_LE_CONTROLLER, 0x24),
    HCI_OPCODE_HCI_LE_READ_LOCAL_P256_PUBLIC_KEY = HCI_OPCODE (OGF_LE_CONTROLLER, 0x25),
    HCI_OPCODE_HCI_LE_GENERATE_DHKEY = HCI_OPCODE (OGF_LE_CONTROLLER, 0x26),
    HCI_OPCODE_HCI_LE_ADD_DEVICE_TO_RESOLVING_LIST = HCI_OPCODE (OGF_LE_CONTROLLER, 0x27),
    HCI_OPCODE_HCI_LE_REMOVE_DEVICE_FROM_RESOLVING_LIST = HCI_OPCODE (OGF_LE_CONTROLLER, 0x28),
    HCI_OPCODE_HCI_LE_CLEAR_RESOLVING_LIST = HCI_OPCODE (OGF_LE_CONTROLLER, 0x29),
    HCI_OPCODE_HCI_LE_READ_RESOLVING_LIST_SIZE = HCI_OPCODE (OGF_LE_CONTROLLER, 0x2A),
    HCI_OPCODE_HCI_LE_READ_PEER_RESOLVABLE_ADDRESS = HCI_OPCODE (OGF_LE_CONTROLLER, 0x2B),
    HCI_OPCODE_HCI_LE_READ_LOCAL_RESOLVABLE_ADDRESS = HCI_OPCODE (OGF_LE_CONTROLLER, 0x2C),
    HCI_OPCODE_HCI_LE_SET_ADDRESS_RESOLUTION_ENABLED = HCI_OPCODE (OGF_LE_CONTROLLER, 0x2D),
    HCI_OPCODE_HCI_LE_SET_RESOLVABLE_PRIVATE_ADDRESS_TIMEOUT = HCI_OPCODE (OGF_LE_CONTROLLER, 0x2E),
    HCI_OPCODE_HCI_LE_READ_MAXIMUM_DATA_LENGTH = HCI_OPCODE (OGF_LE_CONTROLLER, 0x2F),
    HCI_OPCODE_HCI_LE_READ_PHY = HCI_OPCODE (OGF_LE_CONTROLLER, 0x30),
    HCI_OPCODE_HCI_LE_SET_DEFAULT_PHY = HCI_OPCODE (OGF_LE_CONTROLLER, 0x31),
    HCI_OPCODE_HCI_LE_SET_PHY = HCI_OPCODE (OGF_LE_CONTROLLER, 0x32),
    HCI_OPCODE_HCI_LE_RECEIVER_TEST_V2 = HCI_OPCODE (OGF_LE_CONTROLLER, 0x33),
    HCI_OPCODE_HCI_LE_TRANSMITTER_TEST_V2 = HCI_OPCODE (OGF_LE_CONTROLLER, 0x34),
    HCI_OPCODE_HCI_LE_SET_ADVERTISING_SET_RANDOM_ADDRESS = HCI_OPCODE (OGF_LE_CONTROLLER, 0x35),
    HCI_OPCODE_HCI_LE_SET_EXTENDED_ADVERTISING_PARAMETERS = HCI_OPCODE (OGF_LE_CONTROLLER, 0x36),
    HCI_OPCODE_HCI_LE_SET_EXTENDED_ADVERTISING_DATA = HCI_OPCODE (OGF_LE_CONTROLLER, 0x37),
    HCI_OPCODE_HCI_LE_SET_EXTENDED_SCAN_RESPONSE_DATA = HCI_OPCODE (OGF_LE_CONTROLLER, 0x38),
    HCI_OPCODE_HCI_LE_SET_EXTENDED_ADVERTISING_ENABLE = HCI_OPCODE (OGF_LE_CONTROLLER, 0x39),
    HCI_OPCODE_HCI_LE_READ_MAXIMUM_ADVERTISING_DATA_LENGTH = HCI_OPCODE (OGF_LE_CONTROLLER, 0x3a),
    HCI_OPCODE_HCI_LE_READ_NUMBER_OF_SUPPORTED_ADVERTISING_SETS = HCI_OPCODE (OGF_LE_CONTROLLER, 0x3b),
    HCI_OPCODE_HCI_LE_REMOVE_ADVERTISING_SET = HCI_OPCODE (OGF_LE_CONTROLLER, 0x3c),
    HCI_OPCODE_HCI_LE_CLEAR_ADVERTISING_SETS = HCI_OPCODE (OGF_LE_CONTROLLER, 0x3d),
    HCI_OPCODE_HCI_LE_SET_PERIODIC_ADVERTISING_PARAMETERS = HCI_OPCODE (OGF_LE_CONTROLLER, 0x3e),
    HCI_OPCODE_HCI_LE_SET_PERIODIC_ADVERTISING_DATA = HCI_OPCODE (OGF_LE_CONTROLLER, 0x3f),
    HCI_OPCODE_HCI_LE_SET_PERIODIC_ADVERTISING_ENABLE = HCI_OPCODE (OGF_LE_CONTROLLER, 0x40),
    HCI_OPCODE_HCI_LE_SET_EXTENDED_SCAN_PARAMETERS = HCI_OPCODE (OGF_LE_CONTROLLER, 0x41),
    HCI_OPCODE_HCI_LE_SET_EXTENDED_SCAN_ENABLE = HCI_OPCODE (OGF_LE_CONTROLLER, 0x42),
    HCI_OPCODE_HCI_LE_EXTENDED_CREATE_CONNECTION = HCI_OPCODE (OGF_LE_CONTROLLER, 0x43),
    HCI_OPCODE_HCI_LE_PERIODIC_ADVERTISING_CREATE_SYNC = HCI_OPCODE (OGF_LE_CONTROLLER, 0x44),
    HCI_OPCODE_HCI_LE_PERIODIC_ADVERTISING_CREATE_SYNC_CANCEL = HCI_OPCODE (OGF_LE_CONTROLLER, 0x45),
    HCI_OPCODE_HCI_LE_PERIODIC_ADVERTISING_TERMINATE_SYNC = HCI_OPCODE (OGF_LE_CONTROLLER, 0x46),
    HCI_OPCODE_HCI_LE_ADD_DEVICE_TO_PERIODIC_ADVERTISER_LIST = HCI_OPCODE (OGF_LE_CONTROLLER, 0x47),
    HCI_OPCODE_HCI_LE_REMOVE_DEVICE_FROM_PERIODIC_ADVERTISER_LIST = HCI_OPCODE (OGF_LE_CONTROLLER, 0x48),
    HCI_OPCODE_HCI_LE_CLEAR_PERIODIC_ADVERTISER_LIST = HCI_OPCODE (OGF_LE_CONTROLLER, 0x49),
    HCI_OPCODE_HCI_LE_READ_PERIODIC_ADVERTISER_LIST_SIZE = HCI_OPCODE (OGF_LE_CONTROLLER, 0x4a),
    HCI_OPCODE_HCI_LE_READ_TRANSMIT_POWER = HCI_OPCODE (OGF_LE_CONTROLLER, 0x4b),
    HCI_OPCODE_HCI_LE_READ_RF_PATH_COMPENSATION = HCI_OPCODE (OGF_LE_CONTROLLER, 0x4c),
    HCI_OPCODE_HCI_LE_WRITE_RF_PATH_COMPENSATION = HCI_OPCODE (OGF_LE_CONTROLLER, 0x4d),
    HCI_OPCODE_HCI_LE_SET_PRIVACY_MODE = HCI_OPCODE (OGF_LE_CONTROLLER, 0x4e),
    HCI_OPCODE_HCI_LE_RECEIVER_TEST_V3 = HCI_OPCODE (OGF_LE_CONTROLLER, 0x4f),
    HCI_OPCODE_HCI_LE_TRANSMITTER_TEST_V3 = HCI_OPCODE (OGF_LE_CONTROLLER, 0x50),
    HCI_OPCODE_HCI_LE_SET_CONNECTIONLESS_CTE_TRANSMIT_PARAMETERS = HCI_OPCODE (OGF_LE_CONTROLLER, 0x51),
    HCI_OPCODE_HCI_LE_SET_CONNECTIONLESS_CTE_TRANSMIT_ENABLE = HCI_OPCODE (OGF_LE_CONTROLLER, 0x52),
    HCI_OPCODE_HCI_LE_SET_CONNECTIONLESS_IQ_SAMPLING_ENABLE = HCI_OPCODE (OGF_LE_CONTROLLER, 0x53),
    HCI_OPCODE_HCI_LE_SET_CONNECTION_CTE_RECEIVE_PARAMETERS = HCI_OPCODE (OGF_LE_CONTROLLER, 0x54),
    HCI_OPCODE_HCI_LE_SET_CONNECTION_CTE_TRANSMIT_PARAMETERS = HCI_OPCODE (OGF_LE_CONTROLLER, 0x55),
    HCI_OPCODE_HCI_LE_CONNECTION_CTE_REQUEST_ENABLE = HCI_OPCODE (OGF_LE_CONTROLLER, 0x56),
    HCI_OPCODE_HCI_LE_CONNECTION_CTE_RESPONSE_ENABLE = HCI_OPCODE (OGF_LE_CONTROLLER, 0x57),
    HCI_OPCODE_HCI_LE_READ_ANTENNA_INFORMATION = HCI_OPCODE (OGF_LE_CONTROLLER, 0x58),
    HCI_OPCODE_HCI_LE_SET_PERIODIC_ADVERTISING_RECEIVE_ENABLE = HCI_OPCODE (OGF_LE_CONTROLLER, 0x59),
    HCI_OPCODE_HCI_LE_PERIODIC_ADVERTISING_SYNC_TRANSFER = HCI_OPCODE (OGF_LE_CONTROLLER, 0x5a),
    HCI_OPCODE_HCI_LE_PERIODIC_ADVERTISING_SET_INFO_TRANSFER = HCI_OPCODE (OGF_LE_CONTROLLER, 0x5b),
    HCI_OPCODE_HCI_LE_SET_PERIODIC_ADVERTISING_SYNC_TRANSFER_PARAMETERS = HCI_OPCODE (OGF_LE_CONTROLLER, 0x5c),
    HCI_OPCODE_HCI_LE_SET_DEFAULT_PERIODIC_ADVERTISING_SYNC_TRANSFER_PARAMETERS = HCI_OPCODE (OGF_LE_CONTROLLER, 0x5d),
    HCI_OPCODE_HCI_LE_GENERATE_DHKEY_V2 = HCI_OPCODE (OGF_LE_CONTROLLER, 0x5e),
    HCI_OPCODE_HCI_LE_MODIFY_SLEEP_CLOCK_ACCURACY = HCI_OPCODE (OGF_LE_CONTROLLER, 0x5f),
    HCI_OPCODE_HCI_LE_READ_BUFFER_SIZE_V2 = HCI_OPCODE (OGF_LE_CONTROLLER, 0x60),
    HCI_OPCODE_HCI_LE_READ_ISO_TX_SYNC = HCI_OPCODE (OGF_LE_CONTROLLER, 0x61),
    HCI_OPCODE_HCI_LE_SET_CIG_PARAMETERS = HCI_OPCODE (OGF_LE_CONTROLLER, 0x62),
    HCI_OPCODE_HCI_LE_SET_CIG_PARAMETERS_TEST = HCI_OPCODE (OGF_LE_CONTROLLER, 0x63),
    HCI_OPCODE_HCI_LE_CREATE_CIS = HCI_OPCODE (OGF_LE_CONTROLLER, 0x64),
    HCI_OPCODE_HCI_LE_REMOVE_CIG = HCI_OPCODE (OGF_LE_CONTROLLER, 0x65),
    HCI_OPCODE_HCI_LE_ACCEPT_CIS_REQUEST = HCI_OPCODE (OGF_LE_CONTROLLER, 0x66),
    HCI_OPCODE_HCI_LE_REJECT_CIS_REQUEST = HCI_OPCODE (OGF_LE_CONTROLLER, 0x67),
    HCI_OPCODE_HCI_LE_CREATE_BIG = HCI_OPCODE (OGF_LE_CONTROLLER, 0x68),
    HCI_OPCODE_HCI_LE_CREATE_BIG_TEST = HCI_OPCODE (OGF_LE_CONTROLLER, 0x69),
    HCI_OPCODE_HCI_LE_TERMINATE_BIG = HCI_OPCODE (OGF_LE_CONTROLLER, 0x6a),
    HCI_OPCODE_HCI_LE_BIG_CREATE_SYNC = HCI_OPCODE (OGF_LE_CONTROLLER, 0x6b),
    HCI_OPCODE_HCI_LE_BIG_TERMINATE_SYNC = HCI_OPCODE (OGF_LE_CONTROLLER, 0x6c),
    HCI_OPCODE_HCI_LE_REQUEST_PEER_SCA = HCI_OPCODE (OGF_LE_CONTROLLER, 0x6d),
    HCI_OPCODE_HCI_LE_SETUP_ISO_DATA_PATH = HCI_OPCODE (OGF_LE_CONTROLLER, 0x6e),
    HCI_OPCODE_HCI_LE_REMOVE_ISO_DATA_PATH = HCI_OPCODE (OGF_LE_CONTROLLER, 0x6f),
    HCI_OPCODE_HCI_LE_ISO_TRANSMIT_TEST = HCI_OPCODE (OGF_LE_CONTROLLER, 0x70),
    HCI_OPCODE_HCI_LE_ISO_RECEIVE_TEST = HCI_OPCODE (OGF_LE_CONTROLLER, 0x71),
    HCI_OPCODE_HCI_LE_ISO_READ_TEST_COUNTERS = HCI_OPCODE (OGF_LE_CONTROLLER, 0x72),
    HCI_OPCODE_HCI_LE_ISO_TEST_END = HCI_OPCODE (OGF_LE_CONTROLLER, 0x73),
    HCI_OPCODE_HCI_LE_SET_HOST_FEATURE = HCI_OPCODE (OGF_LE_CONTROLLER, 0x74),
    HCI_OPCODE_HCI_LE_READ_ISO_LINK_QUALITY = HCI_OPCODE (OGF_LE_CONTROLLER, 0x75),
    HCI_OPCODE_HCI_LE_ENHANCED_READ_TRANSMIT_POWER_LEVEL = HCI_OPCODE (OGF_LE_CONTROLLER, 0x76),
    HCI_OPCODE_HCI_LE_READ_REMOTE_TRANSMIT_POWER_LEVEL = HCI_OPCODE (OGF_LE_CONTROLLER, 0x77),
    HCI_OPCODE_HCI_LE_SET_PATH_LOSS_REPORTING_PARAMETERS = HCI_OPCODE (OGF_LE_CONTROLLER, 0x78),
    HCI_OPCODE_HCI_LE_SET_PATH_LOSS_REPORTING_ENABLE = HCI_OPCODE (OGF_LE_CONTROLLER, 0x79),
    HCI_OPCODE_HCI_LE_SET_TRANSMIT_POWER_REPORTING_ENABLE = HCI_OPCODE (OGF_LE_CONTROLLER, 0x7a),
    HCI_OPCODE_HCI_LE_TRANSMITTER_TEST_V4 = HCI_OPCODE (OGF_LE_CONTROLLER, 0x7B),
    HCI_OPCODE_HCI_LE_SET_DATA_RELATED_ADDRESS_CHANGES = HCI_OPCODE (OGF_LE_CONTROLLER, 0x7C),
    HCI_OPCODE_HCI_LE_SET_DEFAULT_SUBRATE= HCI_OPCODE (OGF_LE_CONTROLLER, 0x7D),
    HCI_OPCODE_HCI_LE_SUBRATE_REQUEST = HCI_OPCODE (OGF_LE_CONTROLLER, 0x7E),

    // Broadcom/Cypress/Infineon/Synaptics
    HCI_OPCODE_HCI_BCM_WRITE_SCO_PCM_INT = HCI_OPCODE (0x3f, 0x1c),
    HCI_OPCODE_HCI_BCM_WRITE_PCM_DATA_FORMAT_PARAM = HCI_OPCODE (0x3f, 0x1e),
    HCI_OPCODE_HCI_BCM_SET_SLEEP_MODE = HCI_OPCODE (0x3f, 0x27),
    HCI_OPCODE_HCI_BCM_WRITE_I2SPCM_INTERFACE_PARAM = HCI_OPCODE (0x3f, 0x6d),
    HCI_OPCODE_HCI_BCM_ENABLE_WBS = HCI_OPCODE(0x3f, 0x7e),
    HCI_OPCODE_HCI_BCM_PCM2_SETUP = HCI_OPCODE (0x3f, 0xae),
    HCI_OPCODE_HCI_BCM_WRITE_TX_POWER_TABLE = HCI_OPCODE (0x3f, 0x1C9),
    HCI_OPCODE_HCI_BCM_SET_TX_PWR = HCI_OPCODE (0x3f, 0x1A5),

    // Texas Instruments
    HCI_OPCODE_HCI_TI_VS_CONFIGURE_DDIP = 0xFD55,

    // Realtek
    HCI_OPCODE_HCI_RTK_CONFIGURE_SCO_ROUTING = HCI_OPCODE (0x3f, 0x93),
    HCI_OPCODE_HCI_RTK_READ_CARD_INFO = 0xFC61,

    // Zephyr
    HCI_OPCODE_HCI_ZEPHYR_READ_STATIC_ADDRESS = 0xFC09,

    // Marvell/NXP
    HCI_OPCODE_HCI_NXP_WRITE_PCM_I2S_SETTINGS      = 0xFC07,
    HCI_OPCODE_HCI_NXP_SET_SCO_DATA_PATH	       = 0xFC1D,
    HCI_OPCODE_HCI_NXP_SET_BDADDR		           = 0xFC22,
    HCI_OPCODE_HCI_NXP_WRITE_PCM_I2S_SYNC_SETTINGS = 0xFC28,
    HCI_OPCODE_HCI_NXP_WRITE_PCM_LINK_SETTINGS     = 0xFC29,
    HCI_OPCODE_HCI_NXP_SET_WBS_CONNECTION          = 0xFC73,
    HCI_OPCODE_HCI_NXP_HOST_PCM_I2S_AUDIO_CONFIG   = 0xFC6F,
    HCI_OPCODE_HCI_NXP_HOST_PCM_I2S_CONTROL_ENABLE = 0xFC70,

} hci_opcode_t;

// HCI Commands - see hci_cmd.c for info on parameters
extern const hci_cmd_t hci_accept_connection_request;
extern const hci_cmd_t hci_accept_synchronous_connection;
extern const hci_cmd_t hci_authentication_requested;
extern const hci_cmd_t hci_change_connection_link_key;
extern const hci_cmd_t hci_change_connection_packet_type;
extern const hci_cmd_t hci_configure_data_path;
extern const hci_cmd_t hci_create_connection;
extern const hci_cmd_t hci_create_connection_cancel;
extern const hci_cmd_t hci_delete_stored_link_key;
extern const hci_cmd_t hci_disconnect;
extern const hci_cmd_t hci_enable_device_under_test_mode;
extern const hci_cmd_t hci_enhanced_accept_synchronous_connection;
extern const hci_cmd_t hci_enhanced_setup_synchronous_connection;
extern const hci_cmd_t hci_exit_park_state;
extern const hci_cmd_t hci_exit_periodic_inquiry_mode;
extern const hci_cmd_t hci_exit_sniff_mode;
extern const hci_cmd_t hci_flow_specification;
extern const hci_cmd_t hci_flush;
extern const hci_cmd_t hci_hold_mode;
extern const hci_cmd_t hci_host_buffer_size;
extern const hci_cmd_t hci_inquiry;
extern const hci_cmd_t hci_inquiry_cancel;
extern const hci_cmd_t hci_io_capability_request_negative_reply;
extern const hci_cmd_t hci_io_capability_request_reply;
extern const hci_cmd_t hci_link_key_request_negative_reply;
extern const hci_cmd_t hci_link_key_request_reply;
extern const hci_cmd_t hci_park_state;
extern const hci_cmd_t hci_periodic_inquiry_mode;
extern const hci_cmd_t hci_pin_code_request_negative_reply;
extern const hci_cmd_t hci_pin_code_request_reply;
extern const hci_cmd_t hci_qos_setup;
extern const hci_cmd_t hci_read_bd_addr;
extern const hci_cmd_t hci_read_buffer_size;
extern const hci_cmd_t hci_read_clock_offset;
extern const hci_cmd_t hci_read_encryption_key_size;
extern const hci_cmd_t hci_read_extended_inquiry_length;
extern const hci_cmd_t hci_read_extended_page_timeout;
extern const hci_cmd_t hci_read_inquiry_scan_activity;
extern const hci_cmd_t hci_read_le_host_supported;
extern const hci_cmd_t hci_read_link_policy_settings;
extern const hci_cmd_t hci_read_link_supervision_timeout;
extern const hci_cmd_t hci_read_local_extended_oob_data;
extern const hci_cmd_t hci_read_local_name;
extern const hci_cmd_t hci_read_local_oob_data;
extern const hci_cmd_t hci_read_inquiry_response_transmit_power_level;
extern const hci_cmd_t hci_write_inquiry_transmit_power_level;
extern const hci_cmd_t hci_read_clock;
extern const hci_cmd_t hci_read_failed_contact_counter;
extern const hci_cmd_t hci_read_link_quality;
extern const hci_cmd_t hci_read_local_supported_commands;
extern const hci_cmd_t hci_read_local_supported_features;
extern const hci_cmd_t hci_read_local_version_information;
extern const hci_cmd_t hci_read_loopback_mode;
extern const hci_cmd_t hci_read_num_broadcast_retransmissions;
extern const hci_cmd_t hci_read_page_scan_activity;
extern const hci_cmd_t hci_read_page_timeout;
extern const hci_cmd_t hci_read_pin_type;
extern const hci_cmd_t hci_read_remote_extended_features_command;
extern const hci_cmd_t hci_read_remote_supported_features_command;
extern const hci_cmd_t hci_read_remote_version_information;
extern const hci_cmd_t hci_read_rssi;
extern const hci_cmd_t hci_read_transmit_power_level;
extern const hci_cmd_t hci_reject_connection_request;
extern const hci_cmd_t hci_remote_name_request;
extern const hci_cmd_t hci_remote_name_request_cancel;
extern const hci_cmd_t hci_remote_oob_data_request_negative_reply;
extern const hci_cmd_t hci_remote_oob_data_request_reply;
extern const hci_cmd_t hci_remote_oob_extended_data_request_reply;
extern const hci_cmd_t hci_reset;
extern const hci_cmd_t hci_reset_failed_contact_counter;
extern const hci_cmd_t hci_role_discovery;
extern const hci_cmd_t hci_set_connection_encryption;
extern const hci_cmd_t hci_set_controller_to_host_flow_control;
extern const hci_cmd_t hci_set_ecosystem_base_interval;
extern const hci_cmd_t hci_set_event_mask;
extern const hci_cmd_t hci_set_event_mask_2;
extern const hci_cmd_t hci_set_min_encryption_key_size;
extern const hci_cmd_t hci_setup_synchronous_connection;
extern const hci_cmd_t hci_sniff_mode;
extern const hci_cmd_t hci_sniff_subrating;
extern const hci_cmd_t hci_switch_role_command;
extern const hci_cmd_t hci_user_confirmation_request_negative_reply;
extern const hci_cmd_t hci_user_confirmation_request_reply;
extern const hci_cmd_t hci_user_passkey_request_negative_reply;
extern const hci_cmd_t hci_user_passkey_request_reply;
extern const hci_cmd_t hci_write_authentication_enable;
extern const hci_cmd_t hci_write_automatic_flush_timeout;
extern const hci_cmd_t hci_write_class_of_device;
extern const hci_cmd_t hci_write_current_iac_lap_two_iacs;
extern const hci_cmd_t hci_write_default_erroneous_data_reporting;
extern const hci_cmd_t hci_write_default_link_policy_setting;
extern const hci_cmd_t hci_write_extended_inquiry_length;
extern const hci_cmd_t hci_write_extended_inquiry_response;
extern const hci_cmd_t hci_write_extended_page_timeout;
extern const hci_cmd_t hci_write_inquiry_mode;
extern const hci_cmd_t hci_write_inquiry_scan_activity;
extern const hci_cmd_t hci_write_inquiry_scan_type;
extern const hci_cmd_t hci_write_le_host_supported;
extern const hci_cmd_t hci_write_link_policy_settings;
extern const hci_cmd_t hci_write_link_supervision_timeout;
extern const hci_cmd_t hci_write_local_name;
extern const hci_cmd_t hci_write_loopback_mode;
extern const hci_cmd_t hci_write_num_broadcast_retransmissions;
extern const hci_cmd_t hci_write_page_scan_activity;
extern const hci_cmd_t hci_write_page_scan_type;
extern const hci_cmd_t hci_write_page_timeout;
extern const hci_cmd_t hci_write_pin_type;
extern const hci_cmd_t hci_write_scan_enable;
extern const hci_cmd_t hci_write_secure_connections_host_support;
extern const hci_cmd_t hci_write_secure_connections_test_mode;
extern const hci_cmd_t hci_write_simple_pairing_debug_mode;
extern const hci_cmd_t hci_write_simple_pairing_mode;
extern const hci_cmd_t hci_write_synchronous_flow_control_enable;

extern const hci_cmd_t hci_le_accept_cis_request;
extern const hci_cmd_t hci_le_add_device_to_periodic_advertiser_list;
extern const hci_cmd_t hci_le_add_device_to_resolving_list;
extern const hci_cmd_t hci_le_add_device_to_white_list;
extern const hci_cmd_t hci_le_big_create_sync;
extern const hci_cmd_t hci_le_big_terminate_sync;
extern const hci_cmd_t hci_le_clear_advertising_sets;
extern const hci_cmd_t hci_le_clear_periodic_advertiser_list;
extern const hci_cmd_t hci_le_clear_resolving_list;
extern const hci_cmd_t hci_le_clear_white_list;
extern const hci_cmd_t hci_le_connection_cte_request_enable;
extern const hci_cmd_t hci_le_connection_cte_response_enable;
extern const hci_cmd_t hci_le_connection_update;
extern const hci_cmd_t hci_le_create_big;
extern const hci_cmd_t hci_le_create_big_test;
extern const hci_cmd_t hci_le_create_cis;
extern const hci_cmd_t hci_le_create_connection;
extern const hci_cmd_t hci_le_create_connection_cancel;
extern const hci_cmd_t hci_le_encrypt;
extern const hci_cmd_t hci_le_enhanced_read_transmit_power_level;
extern const hci_cmd_t hci_le_extended_create_connection;
extern const hci_cmd_t hci_le_generate_dhkey;
extern const hci_cmd_t hci_le_generate_dhkey_v2;
extern const hci_cmd_t hci_le_iso_read_test_counters;
extern const hci_cmd_t hci_le_iso_receive_test;
extern const hci_cmd_t hci_le_iso_test_end;
extern const hci_cmd_t hci_le_iso_transmit_test;
extern const hci_cmd_t hci_le_long_term_key_negative_reply;
extern const hci_cmd_t hci_le_long_term_key_request_reply;
extern const hci_cmd_t hci_le_modify_sleep_clock_accuracy;
extern const hci_cmd_t hci_le_periodic_advertising_create_sync;
extern const hci_cmd_t hci_le_periodic_advertising_create_sync_cancel;
extern const hci_cmd_t hci_le_periodic_advertising_set_info_transfer;
extern const hci_cmd_t hci_le_periodic_advertising_sync_transfer;
extern const hci_cmd_t hci_le_periodic_advertising_terminate_sync;
extern const hci_cmd_t hci_le_rand;
extern const hci_cmd_t hci_le_read_advertising_channel_tx_power;
extern const hci_cmd_t hci_le_read_antenna_information;
extern const hci_cmd_t hci_le_read_buffer_size;
extern const hci_cmd_t hci_le_read_buffer_size_v2;
extern const hci_cmd_t hci_le_read_channel_map;
extern const hci_cmd_t hci_le_read_iso_link_quality;
extern const hci_cmd_t hci_le_read_iso_tx_sync;
extern const hci_cmd_t hci_le_read_local_p256_public_key;
extern const hci_cmd_t hci_le_read_local_resolvable_address;
extern const hci_cmd_t hci_le_read_local_supported_features;
extern const hci_cmd_t hci_le_read_maximum_advertising_data_length;
extern const hci_cmd_t hci_le_read_maximum_data_length;
extern const hci_cmd_t hci_le_read_number_of_supported_advertising_sets;
extern const hci_cmd_t hci_le_read_peer_resolvable_address;
extern const hci_cmd_t hci_le_read_periodic_advertiser_list_size;
extern const hci_cmd_t hci_le_read_phy;
extern const hci_cmd_t hci_le_read_remote_transmit_power_level;
extern const hci_cmd_t hci_le_read_remote_used_features;
extern const hci_cmd_t hci_le_read_resolving_list_size;
extern const hci_cmd_t hci_le_read_rf_path_compensation;
extern const hci_cmd_t hci_le_read_suggested_default_data_length;
extern const hci_cmd_t hci_le_read_supported_states;
extern const hci_cmd_t hci_le_read_transmit_power;
extern const hci_cmd_t hci_le_read_white_list_size;
extern const hci_cmd_t hci_le_receiver_test;
extern const hci_cmd_t hci_le_receiver_test_v2;
extern const hci_cmd_t hci_le_receiver_test_v3;
extern const hci_cmd_t hci_le_reject_cis_request;
extern const hci_cmd_t hci_le_remote_connection_parameter_request_negative_reply;
extern const hci_cmd_t hci_le_remote_connection_parameter_request_reply;
extern const hci_cmd_t hci_le_remove_advertising_set;
extern const hci_cmd_t hci_le_remove_cig;
extern const hci_cmd_t hci_le_remove_device_from_periodic_advertiser_list;
extern const hci_cmd_t hci_le_remove_device_from_resolving_list;
extern const hci_cmd_t hci_le_remove_device_from_white_list;
extern const hci_cmd_t hci_le_remove_iso_data_path;
extern const hci_cmd_t hci_le_request_peer_sca;
extern const hci_cmd_t hci_le_set_address_resolution_enabled;
extern const hci_cmd_t hci_le_set_advertise_enable;
extern const hci_cmd_t hci_le_set_advertising_data;
extern const hci_cmd_t hci_le_set_advertising_parameters;
extern const hci_cmd_t hci_le_set_advertising_set_random_address;
extern const hci_cmd_t hci_le_set_cig_parameters;
extern const hci_cmd_t hci_le_set_cig_parameters_test;
extern const hci_cmd_t hci_le_set_connection_cte_receive_parameters;
extern const hci_cmd_t hci_le_set_connection_cte_transmit_parameters;
extern const hci_cmd_t hci_le_set_connectionless_cte_transmit_enable;
extern const hci_cmd_t hci_le_set_connectionless_cte_transmit_parameters;
extern const hci_cmd_t hci_le_set_connectionless_iq_sampling_enable;
extern const hci_cmd_t hci_le_set_data_length;
extern const hci_cmd_t hci_le_set_data_related_address_changes;
extern const hci_cmd_t hci_le_set_default_periodic_advertising_sync_transfer_parameters;
extern const hci_cmd_t hci_le_set_default_phy;
extern const hci_cmd_t hci_le_set_default_subrate;
extern const hci_cmd_t hci_le_set_event_mask;
extern const hci_cmd_t hci_le_set_extended_advertising_data;
extern const hci_cmd_t hci_le_set_extended_advertising_enable;
extern const hci_cmd_t hci_le_set_extended_advertising_parameters;
extern const hci_cmd_t hci_le_set_extended_scan_enable;
extern const hci_cmd_t hci_le_set_extended_scan_parameters;
extern const hci_cmd_t hci_le_set_extended_scan_response_data;
extern const hci_cmd_t hci_le_set_host_channel_classification;
extern const hci_cmd_t hci_le_set_host_feature;
extern const hci_cmd_t hci_le_set_path_loss_reporting_enable;
extern const hci_cmd_t hci_le_set_path_loss_reporting_parameters;
extern const hci_cmd_t hci_le_set_periodic_advertising_data;
extern const hci_cmd_t hci_le_set_periodic_advertising_enable;
extern const hci_cmd_t hci_le_set_periodic_advertising_parameters;
extern const hci_cmd_t hci_le_set_periodic_advertising_receive_enable;
extern const hci_cmd_t hci_le_set_periodic_advertising_sync_transfer_parameters;
extern const hci_cmd_t hci_le_set_phy;
extern const hci_cmd_t hci_le_set_privacy_mode;
extern const hci_cmd_t hci_le_set_random_address;
extern const hci_cmd_t hci_le_set_resolvable_private_address_timeout;
extern const hci_cmd_t hci_le_set_scan_enable;
extern const hci_cmd_t hci_le_set_scan_parameters;
extern const hci_cmd_t hci_le_set_scan_response_data;
extern const hci_cmd_t hci_le_set_transmit_power_reporting_enable;
extern const hci_cmd_t hci_le_setup_iso_data_path;
extern const hci_cmd_t hci_le_start_encryption;
extern const hci_cmd_t hci_le_subrate_request;
extern const hci_cmd_t hci_le_terminate_big;
extern const hci_cmd_t hci_le_test_end;
extern const hci_cmd_t hci_le_transmitter_test;
extern const hci_cmd_t hci_le_transmitter_test_v2;
extern const hci_cmd_t hci_le_transmitter_test_v3;
extern const hci_cmd_t hci_le_transmitter_test_v4;
extern const hci_cmd_t hci_le_write_rf_path_compensation;
extern const hci_cmd_t hci_le_write_suggested_default_data_length;
extern const hci_cmd_t hci_opcode_hci_le_read_buffer_size_v2;


// Broadcom / Cypress specific HCI commands
extern const hci_cmd_t hci_bcm_enable_wbs;
extern const hci_cmd_t hci_bcm_pcm2_setup;
extern const hci_cmd_t hci_bcm_write_pcm_data_format_param;
extern const hci_cmd_t hci_bcm_set_sleep_mode;
extern const hci_cmd_t hci_bcm_write_i2spcm_interface_param;
extern const hci_cmd_t hci_bcm_write_sco_pcm_int;
extern const hci_cmd_t hci_bcm_write_tx_power_table;
extern const hci_cmd_t hci_bcm_set_tx_pwr;

// TI specific HCI commands
extern const hci_cmd_t hci_ti_avrp_enable;
extern const hci_cmd_t hci_ti_configure_ddip;
extern const hci_cmd_t hci_ti_drpb_enable_rf_calibration;
extern const hci_cmd_t hci_ti_drpb_tester_con_rx;
extern const hci_cmd_t hci_ti_drpb_tester_con_tx;
extern const hci_cmd_t hci_ti_drpb_tester_packet_tx_rx;
extern const hci_cmd_t hci_ti_wbs_associate;
extern const hci_cmd_t hci_ti_wbs_disassociate;
extern const hci_cmd_t hci_ti_write_codec_config;
extern const hci_cmd_t hci_ti_write_hardware_register;


// Realtek specific HCI commands
extern const hci_cmd_t hci_rtk_configure_sco_routing;
extern const hci_cmd_t hci_rtk_read_card_info;

// Marvell/NXP specific HCI commands
extern const hci_cmd_t hci_nxp_set_sco_data_path;
extern const hci_cmd_t hci_nxp_write_pcm_i2s_settings;
extern const hci_cmd_t hci_nxp_write_pcm_i2s_sync_settings;
extern const hci_cmd_t hci_nxp_write_pcm_link_settings;
extern const hci_cmd_t hci_nxp_set_wbs_connection;
extern const hci_cmd_t hci_nxp_host_pcm_i2s_audio_config;
extern const hci_cmd_t hci_nxp_host_pcm_i2s_control_enable;

/**
 * construct HCI Command based on template
 * @param hci_cmd_buffer for command
 * @param cmd describing command opcode and format
 * @param argptr for command arguments
 *
 * Format:
 *   1,2,3,4: one to four byte value
 *   H: HCI connection handle
 *   B: Bluetooth Baseband Address (BD_ADDR)
 *   D: 8 byte data block
 *   E: Extended Inquiry Result
 *   N: Name up to 248 chars, \0 terminated
 *   P: 16 byte Pairing code
 *   A: 31 bytes advertising data
 *   S: Service Record (Data Element Sequence)
 * @returns size of command
 */

uint16_t hci_cmd_create_from_template(uint8_t *hci_cmd_buffer, const hci_cmd_t *cmd, va_list argptr);

/**
 * construct HCI Command based on template
 * Same as hci_cmd_create_from_template but with variable arguments
 *
 * @param hci_cmd_buffer for command
 * @param cmd describing command opcode and format
 * @returns size of command
 */
uint16_t hci_cmd_create_from_template_with_vargs(uint8_t * hci_cmd_buffer, const hci_cmd_t * cmd, ...);

#if defined __cplusplus
}
#endif

#endif // HCI_CMDS_H
