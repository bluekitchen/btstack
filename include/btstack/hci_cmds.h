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
 *  hci_cmds.h
 *
 *  Created by Matthias Ringwald on 7/23/09.
 */

#ifndef __HCI_CMDS_H
#define __HCI_CMDS_H

#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif
    
/**
 * packet types - used in BTstack and over the H4 UART interface
 */
#define HCI_COMMAND_DATA_PACKET 0x01
#define HCI_ACL_DATA_PACKET     0x02
#define HCI_SCO_DATA_PACKET     0x03
#define HCI_EVENT_PACKET          0x04

// extension for client/server communication
#define DAEMON_EVENT_PACKET     0x05
    
// L2CAP data
#define L2CAP_DATA_PACKET       0x06

// RFCOMM data
#define RFCOMM_DATA_PACKET      0x07

// Attribute protocol data
#define ATT_DATA_PACKET         0x08

// Security Manager protocol data
#define SM_DATA_PACKET          0x09

// SDP query result
// format: type (8), record_id (16), attribute_id (16), attribute_length (16), attribute_value (max 1k)
#define SDP_CLIENT_PACKET       0x0a

// BNEP data
#define BNEP_DATA_PACKET        0x0b

// Unicast Connectionless Data
#define UCD_DATA_PACKET         0x0c
 
// debug log messages
#define LOG_MESSAGE_PACKET      0xfc

    
// Fixed PSM numbers
#define PSM_SDP    0x01
#define PSM_RFCOMM 0x03
#define PSM_HID_CONTROL 0x11
#define PSM_HID_INTERRUPT 0x13
#define PSM_BNEP   0x0F

// Events from host controller to host

/**
 * @format 1
 * @param status
 */
#define HCI_EVENT_INQUIRY_COMPLETE                         0x01
// no format yet, can contain multiple results

/** 
 * @format 1B11132
 * @param num_responses
 * @param bd_addr
 * @param page_scan_repetition_mode
 * @param reserved1
 * @param reserved2
 * @param class_of_device
 * @param clock_offset
 */
#define HCI_EVENT_INQUIRY_RESULT                           0x02

/**
 * @format 12B11
 * @param status
 * @param connection_handle
 * @param bd_addr
 * @param link_type
 * @param encryption_enabled
 */
#define HCI_EVENT_CONNECTION_COMPLETE                      0x03
/**
 * @format B31
 * @param bd_addr
 * @param class_of_device
 * @param link_type
 */
#define HCI_EVENT_CONNECTION_REQUEST                       0x04
/**
 * @format 121
 * @param status
 * @param connection_handle
 * @param reason 
 */
#define HCI_EVENT_DISCONNECTION_COMPLETE                   0x05
/**
 * @format 12
 * @param status
 * @param connection_handle
 */
#define HCI_EVENT_AUTHENTICATION_COMPLETE_EVENT            0x06
/**
 * @format 1BN
 * @param status
 * @param bd_addr
 * @param remote_name
 */
#define HCI_EVENT_REMOTE_NAME_REQUEST_COMPLETE             0x07
/**
 * @format 121
 * @param status
 * @param connection_handle
 * @param encryption_enabled 
 */
#define HCI_EVENT_ENCRYPTION_CHANGE                        0x08
/**
 * @format 12
 * @param status
 * @param connection_handle
 */
#define HCI_EVENT_CHANGE_CONNECTION_LINK_KEY_COMPLETE      0x09
/**
 * @format 121
 * @param status
 * @param connection_handle
 * @param key_flag 
 */
#define HCI_EVENT_MASTER_LINK_KEY_COMPLETE                 0x0A
#define HCI_EVENT_READ_REMOTE_SUPPORTED_FEATURES_COMPLETE  0x0B
#define HCI_EVENT_READ_REMOTE_VERSION_INFORMATION_COMPLETE 0x0C
#define HCI_EVENT_QOS_SETUP_COMPLETE                       0x0D

/**
 * @format 12R
 * @param num_hci_command_packets
 * @param command_opcode
 * @param return_parameters
 */
#define HCI_EVENT_COMMAND_COMPLETE                         0x0E
/**
 * @format 112
 * @param status
 * @param num_hci_command_packets
 * @param command_opcode
 */
#define HCI_EVENT_COMMAND_STATUS                                   0x0F

/**
 * @format 121
 * @param hardware_code
 */
#define HCI_EVENT_HARDWARE_ERROR                           0x10

#define HCI_EVENT_FLUSH_OCCURED                            0x11
#define HCI_EVENT_ROLE_CHANGE                              0x12
#define HCI_EVENT_NUMBER_OF_COMPLETED_PACKETS              0x13
#define HCI_EVENT_MODE_CHANGE_EVENT                        0x14
#define HCI_EVENT_RETURN_LINK_KEYS                         0x15
#define HCI_EVENT_PIN_CODE_REQUEST                         0x16
#define HCI_EVENT_LINK_KEY_REQUEST                         0x17
#define HCI_EVENT_LINK_KEY_NOTIFICATION                    0x18
#define HCI_EVENT_DATA_BUFFER_OVERFLOW                     0x1A
#define HCI_EVENT_MAX_SLOTS_CHANGED                        0x1B
#define HCI_EVENT_READ_CLOCK_OFFSET_COMPLETE               0x1C
#define HCI_EVENT_PACKET_TYPE_CHANGED                      0x1D

/** 
 * @format 1B11321
 * @param num_responses
 * @param bd_addr
 * @param page_scan_repetition_mode
 * @param reserved
 * @param class_of_device
 * @param clock_offset
 * @param rssi
 */
#define HCI_EVENT_INQUIRY_RESULT_WITH_RSSI                 0x22

/**
 * @format 1HB111221
 * @param status
 * @param handle
 * @param bd_addr
 * @param link_type
 * @param transmission_interval
 * @param retransmission_interval
 * @param rx_packet_length
 * @param tx_packet_length
 * @param air_mode
 */
#define HCI_EVENT_SYNCHRONOUS_CONNECTION_COMPLETE          0x2C

// TODO: serialize extended_inquiry_response and provide parser
/** 
 * @format 1B11321
 * @param num_responses
 * @param bd_addr
 * @param page_scan_repetition_mode
 * @param reserved
 * @param class_of_device
 * @param clock_offset
 * @param rssi
 */
#define HCI_EVENT_EXTENDED_INQUIRY_RESPONSE                0x2F

#define HCI_EVENT_IO_CAPABILITY_REQUEST                    0x31
#define HCI_EVENT_IO_CAPABILITY_RESPONSE                   0x32
#define HCI_EVENT_USER_CONFIRMATION_REQUEST                0x33
#define HCI_EVENT_USER_PASSKEY_REQUEST                     0x34
#define HCI_EVENT_REMOTE_OOB_DATA_REQUEST                  0x35
#define HCI_EVENT_SIMPLE_PAIRING_COMPLETE                  0x36

#define HCI_EVENT_LE_META                                  0x3E

/** 
 * @format 11211B2221
 * @param subevent_code
 * @param status
 * @param connection_handle
 * @param role
 * @param peer_address_type
 * @param peer_address
 * @param conn_interval
 * @param conn_latency
 * @param supervision_timeout
 * @param master_clock_accuracy
 */
#define HCI_SUBEVENT_LE_CONNECTION_COMPLETE                0x01
#define HCI_SUBEVENT_LE_ADVERTISING_REPORT                 0x02
#define HCI_SUBEVENT_LE_CONNECTION_UPDATE_COMPLETE         0x03
#define HCI_SUBEVENT_LE_READ_REMOTE_USED_FEATURES_COMPLETE 0x04
#define HCI_SUBEVENT_LE_LONG_TERM_KEY_REQUEST              0x05
    
// last used HCI_EVENT in 2.1 is 0x3d
// last used HCI_EVENT in 4.1 is 0x57

/**
 * @format 1
 * @param state
 */
#define BTSTACK_EVENT_STATE                                0x60

// data: event(8), len(8), nr hci connections
#define BTSTACK_EVENT_NR_CONNECTIONS_CHANGED               0x61

/**
 * @format 
 */
#define BTSTACK_EVENT_POWERON_FAILED                       0x62

/**
 * @format 112
 * @param major
 * @param minor
 @ @param revision
 */
#define BTSTACK_EVENT_VERSION                              0x63

// data: system bluetooth on/off (bool)
#define BTSTACK_EVENT_SYSTEM_BLUETOOTH_ENABLED             0x64

// data: event (8), len(8), status (8) == 0, address (48), name (1984 bits = 248 bytes)
#define BTSTACK_EVENT_REMOTE_NAME_CACHED                   0x65

// data: discoverable enabled (bool)
#define BTSTACK_EVENT_DISCOVERABLE_ENABLED                 0x66

// Daemon Events used internally

// data: event(8)
#define DAEMON_EVENT_CONNECTION_OPENED                     0x68

// data: event(8)
#define DAEMON_EVENT_CONNECTION_CLOSED                     0x69

// data: event(8), nr_connections(8)
#define DAEMON_NR_CONNECTIONS_CHANGED                      0x6A

// data: event(8)
#define DAEMON_EVENT_NEW_RFCOMM_CREDITS                    0x6B

// data: event(8)
#define DAEMON_EVENT_HCI_PACKET_SENT                       0x6C

// L2CAP EVENTS
    
// data: event (8), len(8), status (8), address(48), handle (16), psm (16), local_cid(16), remote_cid (16), local_mtu(16), remote_mtu(16), flush_timeout(16)
#define L2CAP_EVENT_CHANNEL_OPENED                         0x70

// data: event (8), len(8), channel (16)
#define L2CAP_EVENT_CHANNEL_CLOSED                         0x71

// data: event (8), len(8), address(48), handle (16), psm (16), local_cid(16), remote_cid (16) 
#define L2CAP_EVENT_INCOMING_CONNECTION                    0x72

// data: event(8), len(8), handle(16)
#define L2CAP_EVENT_TIMEOUT_CHECK                          0x73

// data: event(8), len(8), local_cid(16), credits(8)
#define L2CAP_EVENT_CREDITS                                0x74

// data: event(8), len(8), status (8), psm (16)
#define L2CAP_EVENT_SERVICE_REGISTERED                     0x75

// data: event(8), len(8), handle(16), interval min(16), interval max(16), latency(16), timeout multiplier(16)
#define L2CAP_EVENT_CONNECTION_PARAMETER_UPDATE_REQUEST    0x76

// data: event(8), len(8), handle(16), result (16) (0 == ok, 1 == fail)
#define L2CAP_EVENT_CONNECTION_PARAMETER_UPDATE_RESPONSE   0x77

// RFCOMM EVENTS
/**
 * @format 1B2122
 * @param status
 * @param bd_addr
 * @param con_handle
 * @param server_channel
 * @param rfcomm_cid
 * @param max_frame_size
 */
#define RFCOMM_EVENT_OPEN_CHANNEL_COMPLETE                 0x80

/**
 * @format 2
 * @param rfcomm_cid
 */
#define RFCOMM_EVENT_CHANNEL_CLOSED                        0x81

/**
 * @format B12
 * @param bd_addr
 * @param server_channel
 * @param rfcomm_cid
 */
#define RFCOMM_EVENT_INCOMING_CONNECTION                   0x82

/**
 * @format 21
 * @param rfcomm_cid
 * @param line_status
 */
#define RFCOMM_EVENT_REMOTE_LINE_STATUS                    0x83
    
/**
 * @format 21
 * @param rfcomm_cid
 * @param credits
 */
#define RFCOMM_EVENT_CREDITS                               0x84

/**
 * @format 11
 * @param status
 * @param channel_id
 */
#define RFCOMM_EVENT_SERVICE_REGISTERED                    0x85
    
/**
 * @format 11
 * @param status
 * @param server_channel_id
 */
#define RFCOMM_EVENT_PERSISTENT_CHANNEL                    0x86
    
// data: event (8), len(8), rfcomm_cid (16), modem status (8)

/**
 * @format 21
 * @param rfcomm_cid
 * @param modem_status
 */
#define RFCOMM_EVENT_REMOTE_MODEM_STATUS                   0x87

// data: event (8), len(8), rfcomm_cid (16), rpn_data_t (67)
 /**
  * TODO: format for variable data
  * @param rfcomm_cid
  * @param rpn_data
  */
#define RFCOMM_EVENT_PORT_CONFIGURATION                    0x88

    
// data: event(8), len(8), status(8), service_record_handle(32)
 /**
  * @format 14
  * @param status
  * @param service_record_handle
  */
#define SDP_SERVICE_REGISTERED                             0x90

// data: event(8), len(8), status(8)
/**
 * @format 1
 * @param status
 */
#define SDP_QUERY_COMPLETE                                 0x91 

// data: event(8), len(8), rfcomm channel(8), name(var)
/**
 * @format 1T
 * @param rfcomm_channel
 * @param name
 * @brief SDP_QUERY_RFCOMM_SERVICE 0x92
 */
#define SDP_QUERY_RFCOMM_SERVICE                           0x92

// data: event(8), len(8), record nr(16), attribute id(16), attribute value(var)
/**
 * TODO: format for variable data
 * @param record_nr
 * @param attribute_id
 * @param attribute_value
 */
#define SDP_QUERY_ATTRIBUTE_VALUE                          0x93

// not provided by daemon, only used for internal testing
#define SDP_QUERY_SERVICE_RECORD_HANDLE                    0x94

/**
 * @format H1
 * @param handle
 * @param status
 */
#define GATT_QUERY_COMPLETE                                0xA0

/**
 * @format HX
 * @param handle
 * @param service
 */
#define GATT_SERVICE_QUERY_RESULT                          0xA1

/**
 * @format HY
 * @param handle
 * @param characteristic
 */
#define GATT_CHARACTERISTIC_QUERY_RESULT                   0xA2

/**
 * @format HX
 * @param handle
 * @param service
 */
#define GATT_INCLUDED_SERVICE_QUERY_RESULT                 0xA3

/**
 * @format HZ
 * @param handle
 * @param characteristic_descriptor
 */
#define GATT_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_RESULT   0xA4

/**
 * @format H2LV
 * @param handle
 * @param value_handle
 * @param value_length
 * @param value
 */
#define GATT_CHARACTERISTIC_VALUE_QUERY_RESULT             0xA5

/**
 * @format H2LV
 * @param handle
 * @param value_handle
 * @param value_length
 * @param value
 */
#define GATT_LONG_CHARACTERISTIC_VALUE_QUERY_RESULT        0xA6

/**
 * @format H2LV
 * @param handle
 * @param value_handle
 * @param value_length
 * @param value
 */
#define GATT_NOTIFICATION                                  0xA7

/**
 * @format H2LV
 * @param handle
 * @param value_handle
 * @param value_length
 * @param value
 */
#define GATT_INDICATION                                    0xA8

#define GATT_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT        0xA9
#define GATT_LONG_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT   0xAA
    
// data: event(8), len(8), status (8), hci_handle (16), attribute_handle (16)
#define ATT_HANDLE_VALUE_INDICATION_COMPLETE                 0xB6


// data: event(8), len(8), status (8), bnep service uuid (16) 
#define BNEP_EVENT_SERVICE_REGISTERED                      0xC0

// data: event(8), len(8), status (8), bnep source uuid (16), bnep destination uuid (16), mtu (16), remote_address (48) 
#define BNEP_EVENT_OPEN_CHANNEL_COMPLETE                   0xC1

// data: event(8), len(8), status (8), bnep source uuid (16), bnep destination uuid (16), mtu (16), remote_address (48) 
#define BNEP_EVENT_INCOMING_CONNECTION                     0xC2

// data: event(8), len(8), bnep source uuid (16), bnep destination uuid (16), remote_address (48) 
#define BNEP_EVENT_CHANNEL_CLOSED                          0xC3

// data: event(8), len(8), bnep source uuid (16), bnep destination uuid (16), remote_address (48), channel state (8)
#define BNEP_EVENT_CHANNEL_TIMEOUT                         0xC4    
    
// data: event(8), len(8)
#define BNEP_EVENT_READY_TO_SEND                           0xC5

// data: event(8), address_type(8), address (48), [number(32)]
#define SM_JUST_WORKS_REQUEST                              0xD0
#define SM_JUST_WORKS_CANCEL                               0xD1 
#define SM_PASSKEY_DISPLAY_NUMBER                          0xD2
#define SM_PASSKEY_DISPLAY_CANCEL                          0xD3
#define SM_PASSKEY_INPUT_NUMBER                            0xD4
#define SM_PASSKEY_INPUT_CANCEL                            0xD5
#define SM_IDENTITY_RESOLVING_STARTED                      0xD6
#define SM_IDENTITY_RESOLVING_FAILED                       0xD7
#define SM_IDENTITY_RESOLVING_SUCCEEDED                    0xD8
#define SM_AUTHORIZATION_REQUEST                           0xD9
#define SM_AUTHORIZATION_RESULT                            0xDA

// GAP

// data: event(8), len(8), hci_handle (16), security_level (8)
#define GAP_SECURITY_LEVEL                                 0xE0

// data: event(8), len(8), status (8), bd_addr(48)
#define GAP_DEDICATED_BONDING_COMPLETED                    0xE1

/**
 * @format 11B1JV
 * @param advertising_event_type
 * @param address_type
 * @param address
 * @param rssi
 * @param data_length
 * @param data
 */
#define GAP_LE_ADVERTISING_REPORT                          0xE2

#define HCI_EVENT_HSP_META                                 0xE8

#define HSP_SUBEVENT_AUDIO_CONNECTION_COMPLETE             0x01
#define HSP_SUBEVENT_AUDIO_DISCONNECTION_COMPLETE          0x02
#define HSP_SUBEVENT_MICROPHONE_GAIN_CHANGED               0x03
#define HSP_SUBEVENT_SPEAKER_GAIN_CHANGED                  0x04
#define HSP_SUBEVENT_HS_COMMAND                            0x05
#define HSP_SUBEVENT_AG_INDICATION                         0x06
#define HSP_SUBEVENT_ERROR                                 0x07
#define HSP_SUBEVENT_RING                                  0x08


// ANCS Client
#define ANCS_CLIENT_CONNECTED                              0xF0
#define ANCS_CLIENT_NOTIFICATION                           0xF1
#define ANCS_CLIENT_DISCONNECTED                           0xF2



// #define HCI_EVENT_HFP_META                                 0xxx
// #define HCI_EVENT_GATT_META                                0xxx
// #define HCI_EVENT_SDP_META                                 0xxx
// #define HCI_EVENT_ANCS_META                                0xxx
// #define HCI_EVENT_SM_META                                  0xxx
// #define HCI_EVENT_GAP_META                                 0xxx
// #define HCI_EVENT_BNEP_META                                0xxx
// #define HCI_EVENT_PAN_META                                 0xxx


#define HCI_EVENT_VENDOR_SPECIFIC                          0xFF

//
// Error Codes
//

// from Bluetooth Core Specification
#define ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER           0x02
#define ERROR_CODE_AUTHENTICATION_FAILURE				   0x05
#define ERROR_CODE_COMMAND_DISALLOWED                      0x0C
#define ERROR_CODE_PAIRING_NOT_ALLOWED                     0x18
#define ERROR_CODE_INSUFFICIENT_SECURITY                   0x2F
 
// last error code in 2.1 is 0x38 - we start with 0x50 for BTstack errors
#define BTSTACK_CONNECTION_TO_BTDAEMON_FAILED              0x50
#define BTSTACK_ACTIVATION_FAILED_SYSTEM_BLUETOOTH         0x51
#define BTSTACK_ACTIVATION_POWERON_FAILED                  0x52
#define BTSTACK_ACTIVATION_FAILED_UNKNOWN                  0x53
#define BTSTACK_NOT_ACTIVATED                              0x54
#define BTSTACK_BUSY                                       0x55
#define BTSTACK_MEMORY_ALLOC_FAILED                        0x56
#define BTSTACK_ACL_BUFFERS_FULL                           0x57

// l2cap errors - enumeration by the command that created them
#define L2CAP_COMMAND_REJECT_REASON_COMMAND_NOT_UNDERSTOOD 0x60
#define L2CAP_COMMAND_REJECT_REASON_SIGNALING_MTU_EXCEEDED 0x61
#define L2CAP_COMMAND_REJECT_REASON_INVALID_CID_IN_REQUEST 0x62

#define L2CAP_CONNECTION_RESPONSE_RESULT_SUCCESSFUL        0x63
#define L2CAP_CONNECTION_RESPONSE_RESULT_PENDING           0x64
#define L2CAP_CONNECTION_RESPONSE_RESULT_REFUSED_PSM       0x65
#define L2CAP_CONNECTION_RESPONSE_RESULT_REFUSED_SECURITY  0x66
#define L2CAP_CONNECTION_RESPONSE_RESULT_REFUSED_RESOURCES 0x67
#define L2CAP_CONNECTION_RESPONSE_RESULT_RTX_TIMEOUT       0x68

#define L2CAP_SERVICE_ALREADY_REGISTERED                   0x69
#define L2CAP_DATA_LEN_EXCEEDS_REMOTE_MTU                  0x6A
    
#define RFCOMM_MULTIPLEXER_STOPPED                         0x70
#define RFCOMM_CHANNEL_ALREADY_REGISTERED                  0x71
#define RFCOMM_NO_OUTGOING_CREDITS                         0x72
#define RFCOMM_AGGREGATE_FLOW_OFF                          0x73
#define RFCOMM_DATA_LEN_EXCEEDS_MTU                        0x74

#define SDP_HANDLE_ALREADY_REGISTERED                      0x80
#define SDP_QUERY_INCOMPLETE                               0x81
#define SDP_SERVICE_NOT_FOUND                              0x82
 
#define ATT_HANDLE_VALUE_INDICATION_IN_PORGRESS            0x90 
#define ATT_HANDLE_VALUE_INDICATION_TIMEOUT                0x91

#define GATT_CLIENT_NOT_CONNECTED                          0x93
#define GATT_CLIENT_BUSY                                   0x94

#define BNEP_SERVICE_ALREADY_REGISTERED                    0xA0
#define BNEP_CHANNEL_NOT_CONNECTED                         0xA1
#define BNEP_DATA_LEN_EXCEEDS_MTU                          0xA2

typedef enum {
    BLE_PERIPHERAL_OK = 0xA0,
    BLE_PERIPHERAL_IN_WRONG_STATE,
    BLE_PERIPHERAL_DIFFERENT_CONTEXT_FOR_ADDRESS_ALREADY_EXISTS,
    BLE_PERIPHERAL_NOT_CONNECTED,
    BLE_VALUE_TOO_LONG,
    BLE_PERIPHERAL_BUSY,
    BLE_CHARACTERISTIC_NOTIFICATION_NOT_SUPPORTED,
    BLE_CHARACTERISTIC_INDICATION_NOT_SUPPORTED
} le_command_status_t;

/**
 * Default INQ Mode
 */
#define HCI_INQUIRY_LAP 0x9E8B33L  // 0x9E8B33: General/Unlimited Inquiry Access Code (GIAC)

/**
 * SSP IO Capabilities - if capability is set, BTstack answers IO Capability Requests
 */
#define SSP_IO_CAPABILITY_DISPLAY_ONLY   0
#define SSP_IO_CAPABILITY_DISPLAY_YES_NO 1
#define SSP_IO_CAPABILITY_KEYBOARD_ONLY  2
#define SSP_IO_CAPABILITY_NO_INPUT_NO_OUTPUT 3
#define SSP_IO_CAPABILITY_UNKNOWN 0xff

/**
 * SSP Authentication Requirements, see IO Capability Request Reply Commmand 
 */

// Numeric comparison with automatic accept allowed.
#define SSP_IO_AUTHREQ_MITM_PROTECTION_NOT_REQUIRED_NO_BONDING 0x00

// Use IO Capabilities to deter- mine authentication procedure
#define SSP_IO_AUTHREQ_MITM_PROTECTION_REQUIRED_NO_BONDING 0x01

// Numeric compar- ison with automatic accept allowed.
#define SSP_IO_AUTHREQ_MITM_PROTECTION_NOT_REQUIRED_DEDICATED_BONDING 0x02

// Use IO Capabilities to determine authentication procedure
#define SSP_IO_AUTHREQ_MITM_PROTECTION_REQUIRED_DEDICATED_BONDING 0x03

// Numeric Compari- son with automatic accept allowed.
#define SSP_IO_AUTHREQ_MITM_PROTECTION_NOT_REQUIRED_GENERAL_BONDING 0x04

// . Use IO capabilities to determine authentication procedure.
#define SSP_IO_AUTHREQ_MITM_PROTECTION_REQUIRED_GENERAL_BONDING 0x05

/**
 * Address types
 * @note: BTstack uses a custom addr type to refer to classic ACL and SCO devices
 */
 typedef enum {
    BD_ADDR_TYPE_LE_PUBLIC = 0,
    BD_ADDR_TYPE_LE_RANDOM = 1,
  BD_ADDR_TYPE_SCO       = 0xfe,
    BD_ADDR_TYPE_CLASSIC   = 0xff,
    BD_ADDR_TYPE_UNKNOWN   = 0xfe
 } bd_addr_type_t;

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


// HCI Commands - see hci_cmds.c for info on parameters
extern const hci_cmd_t btstack_get_state;
extern const hci_cmd_t btstack_set_power_mode;
extern const hci_cmd_t btstack_set_acl_capture_mode;
extern const hci_cmd_t btstack_get_version;
extern const hci_cmd_t btstack_get_system_bluetooth_enabled;
extern const hci_cmd_t btstack_set_system_bluetooth_enabled;
extern const hci_cmd_t btstack_set_discoverable;
extern const hci_cmd_t btstack_set_bluetooth_enabled;    // only used by btstack config
    
extern const hci_cmd_t hci_accept_connection_request;
extern const hci_cmd_t hci_accept_synchronous_connection_command;
extern const hci_cmd_t hci_authentication_requested;
extern const hci_cmd_t hci_change_connection_link_key;
extern const hci_cmd_t hci_change_connection_packet_type;
extern const hci_cmd_t hci_create_connection;
extern const hci_cmd_t hci_create_connection_cancel;
extern const hci_cmd_t hci_delete_stored_link_key;
extern const hci_cmd_t hci_enhanced_setup_synchronous_connection_command;
extern const hci_cmd_t hci_enhanced_accept_synchronous_connection_command;
extern const hci_cmd_t hci_disconnect;
extern const hci_cmd_t hci_host_buffer_size;
extern const hci_cmd_t hci_inquiry;
extern const hci_cmd_t hci_io_capability_request_reply;
extern const hci_cmd_t hci_io_capability_request_negative_reply;
extern const hci_cmd_t hci_inquiry_cancel;
extern const hci_cmd_t hci_link_key_request_negative_reply;
extern const hci_cmd_t hci_link_key_request_reply;
extern const hci_cmd_t hci_pin_code_request_reply;
extern const hci_cmd_t hci_pin_code_request_negative_reply;
extern const hci_cmd_t hci_qos_setup;
extern const hci_cmd_t hci_read_bd_addr;
extern const hci_cmd_t hci_read_buffer_size;
extern const hci_cmd_t hci_read_le_host_supported;
extern const hci_cmd_t hci_read_link_policy_settings;
extern const hci_cmd_t hci_read_link_supervision_timeout;
extern const hci_cmd_t hci_read_local_supported_features;
extern const hci_cmd_t hci_read_num_broadcast_retransmissions;
extern const hci_cmd_t hci_read_remote_supported_features_command;
extern const hci_cmd_t hci_read_rssi;
extern const hci_cmd_t hci_reject_connection_request;
extern const hci_cmd_t hci_remote_name_request;
extern const hci_cmd_t hci_remote_name_request_cancel;
extern const hci_cmd_t hci_remote_oob_data_request_negative_reply;
extern const hci_cmd_t hci_reset;
extern const hci_cmd_t hci_role_discovery;
extern const hci_cmd_t hci_set_event_mask;
extern const hci_cmd_t hci_set_connection_encryption;
extern const hci_cmd_t hci_setup_synchronous_connection_command;
extern const hci_cmd_t hci_sniff_mode;
extern const hci_cmd_t hci_switch_role_command;
extern const hci_cmd_t hci_user_confirmation_request_negative_reply;
extern const hci_cmd_t hci_user_confirmation_request_reply;
extern const hci_cmd_t hci_user_passkey_request_negative_reply;
extern const hci_cmd_t hci_user_passkey_request_reply;
extern const hci_cmd_t hci_write_authentication_enable;
extern const hci_cmd_t hci_write_class_of_device;
extern const hci_cmd_t hci_write_extended_inquiry_response;
extern const hci_cmd_t hci_write_inquiry_mode;
extern const hci_cmd_t hci_write_le_host_supported;
extern const hci_cmd_t hci_write_link_policy_settings;
extern const hci_cmd_t hci_write_link_supervision_timeout;
extern const hci_cmd_t hci_write_local_name;
extern const hci_cmd_t hci_write_num_broadcast_retransmissions;
extern const hci_cmd_t hci_write_page_timeout;
extern const hci_cmd_t hci_write_scan_enable;
extern const hci_cmd_t hci_write_simple_pairing_mode;
extern const hci_cmd_t hci_write_synchronous_flow_control_enable;

extern const hci_cmd_t hci_le_add_device_to_whitelist;
extern const hci_cmd_t hci_le_clear_white_list;
extern const hci_cmd_t hci_le_connection_update;
extern const hci_cmd_t hci_le_create_connection;
extern const hci_cmd_t hci_le_create_connection_cancel;
extern const hci_cmd_t hci_le_encrypt;
extern const hci_cmd_t hci_le_long_term_key_negative_reply;
extern const hci_cmd_t hci_le_long_term_key_request_reply;
extern const hci_cmd_t hci_le_rand;
extern const hci_cmd_t hci_le_read_advertising_channel_tx_power;
extern const hci_cmd_t hci_le_read_buffer_size ;
extern const hci_cmd_t hci_le_read_channel_map;
extern const hci_cmd_t hci_le_read_remote_used_features;
extern const hci_cmd_t hci_le_read_supported_features;
extern const hci_cmd_t hci_le_read_supported_states;
extern const hci_cmd_t hci_le_read_white_list_size;
extern const hci_cmd_t hci_le_receiver_test;
extern const hci_cmd_t hci_le_remove_device_from_whitelist;
extern const hci_cmd_t hci_le_set_advertise_enable;
extern const hci_cmd_t hci_le_set_advertising_data;
extern const hci_cmd_t hci_le_set_advertising_parameters;
extern const hci_cmd_t hci_le_set_event_mask;
extern const hci_cmd_t hci_le_set_host_channel_classification;
extern const hci_cmd_t hci_le_set_random_address;
extern const hci_cmd_t hci_le_set_scan_enable;
extern const hci_cmd_t hci_le_set_scan_parameters;
extern const hci_cmd_t hci_le_set_scan_response_data;
extern const hci_cmd_t hci_le_start_encryption;
extern const hci_cmd_t hci_le_test_end;
extern const hci_cmd_t hci_le_transmitter_test;
    
extern const hci_cmd_t l2cap_accept_connection;
extern const hci_cmd_t l2cap_create_channel;
extern const hci_cmd_t l2cap_create_channel_mtu;
extern const hci_cmd_t l2cap_decline_connection;
extern const hci_cmd_t l2cap_disconnect;
extern const hci_cmd_t l2cap_register_service;
extern const hci_cmd_t l2cap_unregister_service;

extern const hci_cmd_t sdp_register_service_record;
extern const hci_cmd_t sdp_unregister_service_record;
extern const hci_cmd_t sdp_client_query_rfcomm_services;
extern const hci_cmd_t sdp_client_query_services;


// accept connection @param bd_addr(48), rfcomm_cid (16)
extern const hci_cmd_t rfcomm_accept_connection;
// create rfcomm channel: @param bd_addr(48), channel (8)
extern const hci_cmd_t rfcomm_create_channel;
// create rfcomm channel: @param bd_addr(48), channel (8), mtu (16), credits (8)
extern const hci_cmd_t rfcomm_create_channel_with_initial_credits;
// decline rfcomm disconnect,@param bd_addr(48), rfcomm cid (16), reason(8)
extern const hci_cmd_t rfcomm_decline_connection;
// disconnect rfcomm disconnect, @param rfcomm_cid(8), reason(8)
extern const hci_cmd_t rfcomm_disconnect;
// register rfcomm service: @param channel(8), mtu (16)
extern const hci_cmd_t rfcomm_register_service;
// register rfcomm service: @param channel(8), mtu (16), initial credits (8)
extern const hci_cmd_t rfcomm_register_service_with_initial_credits;
// unregister rfcomm service, @param service_channel(16)
extern const hci_cmd_t rfcomm_unregister_service;
// request persisten rfcomm channel for service name: serive name (char*) 
extern const hci_cmd_t rfcomm_persistent_channel_for_service;
    
extern const hci_cmd_t gap_disconnect_cmd;
extern const hci_cmd_t gap_le_scan_start;
extern const hci_cmd_t gap_le_scan_stop;
extern const hci_cmd_t gap_le_set_scan_parameters;
extern const hci_cmd_t gap_le_connect_cmd;
extern const hci_cmd_t gap_le_connect_cancel_cmd;
extern const hci_cmd_t gatt_discover_primary_services_cmd;

#if defined __cplusplus
}
#endif

#endif // __HCI_CMDS_H
