/*
 * Copyright (C) 2015 BlueKitchen GmbH
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
 * btstack-defines.h
 *
 * BTstack definitions, events, and error codes */

#ifndef BTSTACK_DEFINES_H
#define BTSTACK_DEFINES_H

#include <stdint.h>
#include "btstack_linked_list.h" 


// UNUSED macro
#ifndef UNUSED
#define UNUSED(x) (void)(sizeof(x))
#endif

// TYPES

// packet handler
typedef void (*btstack_packet_handler_t) (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

// packet callback supporting multiple registrations
typedef struct {
    btstack_linked_item_t    item;
    btstack_packet_handler_t callback;
} btstack_packet_callback_registration_t;

// context callback supporting multiple registrations
typedef struct {
  btstack_linked_item_t * item;
  void (*callback)(void * context);
  void * context;
} btstack_context_callback_registration_t;

/**
 * @brief 128 bit key used with AES128 in Security Manager
 */
typedef uint8_t sm_key_t[16];

// DEFINES

// hci con handles (12 bit): 0x0000..0x0fff
#define HCI_CON_HANDLE_INVALID 0xffff


#define DAEMON_EVENT_PACKET     0x05
    
// L2CAP data
#define L2CAP_DATA_PACKET       0x06

// RFCOMM data
#define RFCOMM_DATA_PACKET      0x07

// Attribute protocol data
#define ATT_DATA_PACKET         0x08

// Security Manager protocol data
#define SM_DATA_PACKET          0x09

// SDP query result - only used by daemon
// format: type (8), record_id (16), attribute_id (16), attribute_length (16), attribute_value (max 1k)
#define SDP_CLIENT_PACKET       0x0a

// BNEP data
#define BNEP_DATA_PACKET        0x0b

// Unicast Connectionless Data
#define UCD_DATA_PACKET         0x0c

// GOEP data
#define GOEP_DATA_PACKET        0x0d

// PBAP data
#define PBAP_DATA_PACKET        0x0e

// AVRCP browsing data
#define AVRCP_BROWSING_DATA_PACKET     0x0f

// MAP data
#define MAP_DATA_PACKET        0x10

// Mesh Provisioning PDU
#define PROVISIONING_DATA_PACKET 0x11

// Mesh Proxy PDU
#define MESH_PROXY_DATA_PACKET   0x11

// Mesh Network PDU
#define MESH_NETWORK_PACKET      0x12

// Mesh Network PDU
#define MESH_BEACON_PACKET       0x13

// debug log messages
#define LOG_MESSAGE_PACKET      0xfc


// DAEMON COMMANDS

#define OGF_BTSTACK 0x3d

// cmds for BTstack 
// get state: @returns HCI_STATE
#define BTSTACK_GET_STATE                                  0x01

// set power mode: param HCI_POWER_MODE
#define BTSTACK_SET_POWER_MODE                             0x02

// set capture mode: param on
#define BTSTACK_SET_ACL_CAPTURE_MODE                       0x03

// get BTstack version
#define BTSTACK_GET_VERSION                                0x04

// get system Bluetooth state
#define BTSTACK_GET_SYSTEM_BLUETOOTH_ENABLED               0x05

// set system Bluetooth state
#define BTSTACK_SET_SYSTEM_BLUETOOTH_ENABLED               0x06

// enable inquiry scan for this client
#define BTSTACK_SET_DISCOVERABLE                           0x07

// set global Bluetooth state
#define BTSTACK_SET_BLUETOOTH_ENABLED                      0x08

// create l2cap channel: param bd_addr(48), psm (16)
#define L2CAP_CREATE_CHANNEL                               0x20

// disconnect l2cap disconnect, param channel(16), reason(8)
#define L2CAP_DISCONNECT                                   0x21

// register l2cap service: param psm(16), mtu (16)
#define L2CAP_REGISTER_SERVICE                             0x22

// unregister l2cap disconnect, param psm(16)
#define L2CAP_UNREGISTER_SERVICE                           0x23

// accept connection param bd_addr(48), dest cid (16)
#define L2CAP_ACCEPT_CONNECTION                            0x24

// decline l2cap disconnect,param bd_addr(48), dest cid (16), reason(8)
#define L2CAP_DECLINE_CONNECTION                           0x25

// create l2cap channel: param bd_addr(48), psm (16), mtu (16)
#define L2CAP_CREATE_CHANNEL_MTU                           0x26

// register SDP Service Record: service record (size)
#define SDP_REGISTER_SERVICE_RECORD                        0x30

// unregister SDP Service Record
#define SDP_UNREGISTER_SERVICE_RECORD                      0x31

// Get remote RFCOMM services
#define SDP_CLIENT_QUERY_RFCOMM_SERVICES                   0x32

// Get remote SDP services
#define SDP_CLIENT_QUERY_SERVICES                          0x33

// RFCOMM "HCI" Commands
#define RFCOMM_CREATE_CHANNEL       0x40
#define RFCOMM_DISCONNECT     0x41
#define RFCOMM_REGISTER_SERVICE     0x42
#define RFCOMM_UNREGISTER_SERVICE   0x43
#define RFCOMM_ACCEPT_CONNECTION    0x44
#define RFCOMM_DECLINE_CONNECTION   0x45
#define RFCOMM_PERSISTENT_CHANNEL   0x46
#define RFCOMM_CREATE_CHANNEL_WITH_CREDITS   0x47
#define RFCOMM_REGISTER_SERVICE_WITH_CREDITS 0x48
#define RFCOMM_GRANT_CREDITS                 0x49
    
// GAP Classic 0x50
#define GAP_DISCONNECT              0x50

// GAP LE      0x60  
#define GAP_LE_SCAN_START           0x60
#define GAP_LE_SCAN_STOP            0x61
#define GAP_LE_CONNECT              0x62
#define GAP_LE_CONNECT_CANCEL       0x63
#define GAP_LE_SET_SCAN_PARAMETERS  0x64

// GATT (Client) 0x70
#define GATT_DISCOVER_ALL_PRIMARY_SERVICES                       0x70
#define GATT_DISCOVER_PRIMARY_SERVICES_BY_UUID16                 0x71
#define GATT_DISCOVER_PRIMARY_SERVICES_BY_UUID128                0x72
#define GATT_FIND_INCLUDED_SERVICES_FOR_SERVICE                  0x73
#define GATT_DISCOVER_CHARACTERISTICS_FOR_SERVICE                0x74
#define GATT_DISCOVER_CHARACTERISTICS_FOR_SERVICE_BY_UUID128     0x75
#define GATT_DISCOVER_CHARACTERISTIC_DESCRIPTORS                 0x76
#define GATT_READ_VALUE_OF_CHARACTERISTIC                        0x77
#define GATT_READ_LONG_VALUE_OF_CHARACTERISTIC                   0x78
#define GATT_WRITE_VALUE_OF_CHARACTERISTIC_WITHOUT_RESPONSE      0x79
#define GATT_WRITE_VALUE_OF_CHARACTERISTIC                       0x7A
#define GATT_WRITE_LONG_VALUE_OF_CHARACTERISTIC                  0x7B
#define GATT_RELIABLE_WRITE_LONG_VALUE_OF_CHARACTERISTIC         0x7C
#define GATT_READ_CHARACTERISTIC_DESCRIPTOR                      0X7D
#define GATT_READ_LONG_CHARACTERISTIC_DESCRIPTOR                 0X7E
#define GATT_WRITE_CHARACTERISTIC_DESCRIPTOR                     0X7F
#define GATT_WRITE_LONG_CHARACTERISTIC_DESCRIPTOR                0X80
#define GATT_WRITE_CLIENT_CHARACTERISTIC_CONFIGURATION           0X81
#define GATT_GET_MTU                                             0x82

// SM 0x90
#define SM_SET_AUTHENTICATION_REQUIREMENTS 0x90
#define SM_SET_IO_CAPABILITIES             0x92
#define SM_BONDING_DECLINE                 0x93
#define SM_JUST_WORKS_CONFIRM              0x94
#define SM_NUMERIC_COMPARISON_CONFIRM      0x95
#define SM_PASSKEY_INPUT                   0x96

// ATT

// ..
// Internal properties reuse some GATT Characteristic Properties fields
#define ATT_DB_VERSION                                     0x01

// EVENTS

// Events from host controller to host

/**
 * @format 1
 * @param status
 */
#define HCI_EVENT_INQUIRY_COMPLETE                         0x01

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
#define HCI_EVENT_AUTHENTICATION_COMPLETE                 0x06

// HCI_EVENT_AUTHENTICATION_COMPLETE_EVENT is deprecated, use HCI_EVENT_AUTHENTICATION_COMPLETE instead
#define HCI_EVENT_AUTHENTICATION_COMPLETE_EVENT HCI_EVENT_AUTHENTICATION_COMPLETE

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

/**
 * @format 12122
 * @param status
 * @param connection_handle
 * @param version
 * @param manufacturer_name
 * @param subversion
 */
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
#define HCI_EVENT_COMMAND_STATUS                           0x0F

/**
 * @format 1
 * @param hardware_code
 */
#define HCI_EVENT_HARDWARE_ERROR                           0x10

#define HCI_EVENT_FLUSH_OCCURRED                           0x11

/**
 * @format 1B1
 * @param status
 * @param bd_addr
 * @param role
 */
#define HCI_EVENT_ROLE_CHANGE                              0x12

// TODO: number_of_handles 1, connection_handle[H*i], hc_num_of_completed_packets[2*i]
#define HCI_EVENT_NUMBER_OF_COMPLETED_PACKETS              0x13

/**
 * @format 1H12
 * @param status
 * @param handle
 * @param mode
 * @param interval
 */
#define HCI_EVENT_MODE_CHANGE                              0x14

// TODO: num_keys, bd_addr[B*i], link_key[16 octets * i]
#define HCI_EVENT_RETURN_LINK_KEYS                         0x15

/**
 * @format B
 * @param bd_addr
 */
#define HCI_EVENT_PIN_CODE_REQUEST                         0x16

/**
 * @format B
 * @param bd_addr
 */
#define HCI_EVENT_LINK_KEY_REQUEST                         0x17

// TODO: bd_addr B, link_key 16octets, key_type 1
#define HCI_EVENT_LINK_KEY_NOTIFICATION                    0x18

/**
 * @format 1
 * @param link_type
 */
#define HCI_EVENT_DATA_BUFFER_OVERFLOW                     0x1A

/**
 * @format H1
 * @param handle
 * @param lmp_max_slots
 */
#define HCI_EVENT_MAX_SLOTS_CHANGED                        0x1B

/**
 * @format 1H2
 * @param status
 * @param handle
 * @param clock_offset
 */
#define HCI_EVENT_READ_CLOCK_OFFSET_COMPLETE               0x1C

/**
 * @format 1H2
 * @param status
 * @param handle
 * @param packet_types
 * @pnote packet_type is in plural to avoid clash with Java binding Packet.getPacketType()
 */
#define HCI_EVENT_CONNECTION_PACKET_TYPE_CHANGED           0x1D

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

 /** 
  * @format 1H
  * @param status
  * @param handle
  */
#define HCI_EVENT_ENCRYPTION_KEY_REFRESH_COMPLETE          0x30

#define HCI_EVENT_IO_CAPABILITY_REQUEST                    0x31
#define HCI_EVENT_IO_CAPABILITY_RESPONSE                   0x32

/**
 * @format B4
 * @param bd_addr
 * @param numeric_value
 */
#define HCI_EVENT_USER_CONFIRMATION_REQUEST                0x33

/**
 * @format B
 * @param bd_addr
 */
#define HCI_EVENT_USER_PASSKEY_REQUEST                     0x34

/**
 * @format B
 * @param bd_addr
 */
#define HCI_EVENT_REMOTE_OOB_DATA_REQUEST                  0x35

/**
 * @format 1B
 * @param status
 * @param bd_addr
 */
#define HCI_EVENT_SIMPLE_PAIRING_COMPLETE                  0x36

#define HCI_EVENT_LE_META                                  0x3E

// last used HCI_EVENT in 2.1 is 0x3d
// last used HCI_EVENT in 4.1 is 0x57

#define HCI_EVENT_VENDOR_SPECIFIC                          0xFF

/** 
 * @format 11H11B2221
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

// array of advertisements, not handled by event accessor generator
#define HCI_SUBEVENT_LE_ADVERTISING_REPORT                 0x02

/**
 * @format 11H222
 * @param subevent_code
 * @param status
 * @param connection_handle
 * @param conn_interval
 * @param conn_latency
 * @param supervision_timeout
 */
 #define HCI_SUBEVENT_LE_CONNECTION_UPDATE_COMPLETE         0x03

/**
 * @format 1HD2
 * @param subevent_code
 * @param connection_handle
 * @param random_number
 * @param encryption_diversifier
 */
#define HCI_SUBEVENT_LE_READ_REMOTE_USED_FEATURES_COMPLETE 0x04

/**
 * @format 1HD2
 * @param subevent_code
 * @param connection_handle
 * @param random_number
 * @param encryption_diversifier
 */
#define HCI_SUBEVENT_LE_LONG_TERM_KEY_REQUEST              0x05

/**
 * @format 1H2222
 * @param subevent_code
 * @param connection_handle
 * @param interval_min
 * @param interval_max
 * @param latency
 * @param timeout
 */
#define HCI_SUBEVENT_LE_REMOTE_CONNECTION_PARAMETER_REQUEST 0x06

/**
 * @format 1H2222
 * @param subevent_code
 * @param connection_handle
 * @param max_tx_octets
 * @param max_tx_time
 * @param max_rx_octets
 * @param max_rx_time
 */
#define HCI_SUBEVENT_LE_DATA_LENGTH_CHANGE 0x07

/**
 * @format 11QQ
 * @param subevent_code
 * @param status
 * @param dhkey_x x coordinate of P256 public key
 * @param dhkey_y y coordinate of P256 public key
 */
#define HCI_SUBEVENT_LE_READ_LOCAL_P256_PUBLIC_KEY_COMPLETE 0x08
 /**
 * @format 11Q
 * @param subevent_code
 * @param status
 * @param dhkey Diffie-Hellman key
 */
#define HCI_SUBEVENT_LE_GENERATE_DHKEY_COMPLETE            0x09

/**
 * @format 11H11BBB2221
 * @param subevent_code
 * @param status
 * @param connection_handle
 * @param role
 * @param peer_address_type
 * @param perr_addresss
 * @param local_resolvable_private_addres
 * @param peer_resolvable_private_addres
 * @param conn_interval
 * @param conn_latency
 * @param supervision_timeout
 * @param master_clock_accuracy
 */
#define HCI_SUBEVENT_LE_ENHANCED_CONNECTION_COMPLETE       0x0A

// array of advertisements, not handled by event accessor generator
#define HCI_SUBEVENT_LE_DIRECT_ADVERTISING_REPORT          0x0B


/**
 * @format 1
 * @param state
 */
#define BTSTACK_EVENT_STATE                                0x60

/**
 * @format 1
 * @param number_connections
 */
#define BTSTACK_EVENT_NR_CONNECTIONS_CHANGED               0x61

/**
 * @format 
 */
#define BTSTACK_EVENT_POWERON_FAILED                       0x62

/**
 * @format 1
 * @param discoverable
 */
#define BTSTACK_EVENT_DISCOVERABLE_ENABLED                 0x66

// Daemon Events

/**
 * @format 112
 * @param major
 * @param minor
 @ @param revision
 */
#define DAEMON_EVENT_VERSION                               0x63

// data: system bluetooth on/off (bool)
/**
 * @format 1
 * param system_bluetooth_enabled
 */
#define DAEMON_EVENT_SYSTEM_BLUETOOTH_ENABLED              0x64

// data: event (8), len(8), status (8) == 0, address (48), name (1984 bits = 248 bytes)

/* 
 * @format 1BT
 * @param status == 0 to match read_remote_name_request
 * @param address
 * @param name
 */
#define DAEMON_EVENT_REMOTE_NAME_CACHED                    0x65

// internal - data: event(8)
#define DAEMON_EVENT_CONNECTION_OPENED                     0x67

// internal - data: event(8)
#define DAEMON_EVENT_CONNECTION_CLOSED                     0x68

// data: event(8), len(8), local_cid(16), credits(8)
#define DAEMON_EVENT_L2CAP_CREDITS                         0x74

/**
 * @format 12
 * @param status
 * @param psm
 */
#define DAEMON_EVENT_L2CAP_SERVICE_REGISTERED              0x75

/**
 * @format 21
 * @param rfcomm_cid
 * @param credits
 */
#define DAEMON_EVENT_RFCOMM_CREDITS                        0x84

/**
 * @format 11
 * @param status
 * @param channel_id
 */
#define DAEMON_EVENT_RFCOMM_SERVICE_REGISTERED             0x85

/**
 * @format 11
 * @param status
 * @param server_channel_id
 */
#define DAEMON_EVENT_RFCOMM_PERSISTENT_CHANNEL             0x86

/**
  * @format 14
  * @param status
  * @param service_record_handle
  */
#define DAEMON_EVENT_SDP_SERVICE_REGISTERED                0x90



// additional HCI events

/**
 * @brief Indicates HCI transport enters/exits Sleep mode
 * @format 1
 * @param active
 */
#define HCI_EVENT_TRANSPORT_SLEEP_MODE                     0x69

/**
 * @brief Outgoing packet 
 */
#define HCI_EVENT_TRANSPORT_PACKET_SENT                    0x6E

/**
 * @format B
 * @param handle
 */
#define HCI_EVENT_SCO_CAN_SEND_NOW                         0x6F


// L2CAP EVENTS
    
/**
 * @format 1BH222222111
 * @param status
 * @param address
 * @param handle
 * @param psm
 * @param local_cid
 * @param remote_cid
 * @param local_mtu
 * @param remote_mtu
 * @param flush_timeout
 * @param incoming
 * @param mode
 * @param fcs
 */
#define L2CAP_EVENT_CHANNEL_OPENED                         0x70

/*
 * @format 2
 * @param local_cid
 */
#define L2CAP_EVENT_CHANNEL_CLOSED                         0x71

/**
 * @format BH222
 * @param address
 * @param handle
 * @param psm
 * @param local_cid
 * @param remote_cid
 */
#define L2CAP_EVENT_INCOMING_CONNECTION                    0x72

// ??
// data: event(8), len(8), handle(16)
#define L2CAP_EVENT_TIMEOUT_CHECK                          0x73

/**
 * @format H2222
 * @param handle
 * @param interval_min
 * @param interval_max
 * @param latencey
 * @param timeout_multiplier
 */
#define L2CAP_EVENT_CONNECTION_PARAMETER_UPDATE_REQUEST    0x76

// data: event(8), len(8), handle(16), result (16) (0 == ok, 1 == fail)
 /** 
  * @format H2
  * @param handle
  * @param result
  */
#define L2CAP_EVENT_CONNECTION_PARAMETER_UPDATE_RESPONSE   0x77

/**
 * @format 2
 * @param local_cid
 */
#define L2CAP_EVENT_CAN_SEND_NOW                           0x78

// LE Data Channels

/**
 * @format 1BH2222
 * @param address_type
 * @param address
 * @param handle
 * @param psm
 * @param local_cid
 * @param remote_cid
 * @param remote_mtu
 */
#define L2CAP_EVENT_LE_INCOMING_CONNECTION                 0x79

/**
 * @format 11BH122222
 * @param status
 * @param address_type
 * @param address
 * @param handle
 * @param incoming
 * @param psm
 * @param local_cid
 * @param remote_cid
 * @param local_mtu
 * @param remote_mtu
 */
#define L2CAP_EVENT_LE_CHANNEL_OPENED                      0x7a

/*
 * @format 2
 * @param local_cid
 */
#define L2CAP_EVENT_LE_CHANNEL_CLOSED                      0x7b

/*
 * @format 2
 * @param local_cid
 */
#define L2CAP_EVENT_LE_CAN_SEND_NOW                        0x7c

/*
 * @format 2
 * @param local_cid
 */
#define L2CAP_EVENT_LE_PACKET_SENT                         0x7d

/*
 * @format 2
 * @param local_cid
 */
#define L2CAP_EVENT_ERTM_BUFFER_RELEASED                   0x7e


// RFCOMM EVENTS

/**
 * @format 1B21221
 * @param status
 * @param bd_addr
 * @param con_handle
 * @param server_channel
 * @param rfcomm_cid
 * @param max_frame_size
 * @param incoming
 */
#define RFCOMM_EVENT_CHANNEL_OPENED                        0x80

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
 * @param modem_status
 */
#define RFCOMM_EVENT_REMOTE_MODEM_STATUS                   0x87

 /**
  * TODO: format for variable data 2?
  * param rfcomm_cid
  * param rpn_data
  */
#define RFCOMM_EVENT_PORT_CONFIGURATION                    0x88

/**
 * @format 2
 * @param rfcomm_cid
 */
#define RFCOMM_EVENT_CAN_SEND_NOW                          0x89


/**
 * @format 1
 * @param status
 */
#define SDP_EVENT_QUERY_COMPLETE                                 0x91 

/**
 * @format 1T
 * @param rfcomm_channel
 * @param name
 */
#define SDP_EVENT_QUERY_RFCOMM_SERVICE                           0x92

/**
 * @format 22221
 * @param record_id
 * @param attribute_id
 * @param attribute_length
 * @param data_offset
 * @param data
 */
#define SDP_EVENT_QUERY_ATTRIBUTE_BYTE                           0x93

/**
 * @format 22LV
 * @param record_id
 * @param attribute_id
 * @param attribute_length
 * @param attribute_value
 */
#define SDP_EVENT_QUERY_ATTRIBUTE_VALUE                          0x94

/**
 * @format 224
 * @param total_count
 * @param record_index
 * @param record_handle
 * @note Not provided by daemon, only used for internal testing
 */
#define SDP_EVENT_QUERY_SERVICE_RECORD_HANDLE                    0x95

/**
 * @format H1
 * @param handle
 * @param att_status  see ATT errors in bluetooth.h  
 */
#define GATT_EVENT_QUERY_COMPLETE                                0xA0

/**
 * @format HX
 * @param handle
 * @param service
 */
#define GATT_EVENT_SERVICE_QUERY_RESULT                          0xA1

/**
 * @format HY
 * @param handle
 * @param characteristic
 */
#define GATT_EVENT_CHARACTERISTIC_QUERY_RESULT                   0xA2

/**
 * @format H2X
 * @param handle
 * @param include_handle
 * @param service
 */
#define GATT_EVENT_INCLUDED_SERVICE_QUERY_RESULT                 0xA3

/**
 * @format HZ
 * @param handle
 * @param characteristic_descriptor
 */
#define GATT_EVENT_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_RESULT   0xA4

/**
 * @format H2LV
 * @param handle
 * @param value_handle
 * @param value_length
 * @param value
 */
#define GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT             0xA5

/**
 * @format H22LV
 * @param handle
 * @param value_handle
 * @param value_offset
 * @param value_length
 * @param value
 */
#define GATT_EVENT_LONG_CHARACTERISTIC_VALUE_QUERY_RESULT        0xA6

/**
 * @format H2LV
 * @param handle
 * @param value_handle
 * @param value_length
 * @param value
 */
#define GATT_EVENT_NOTIFICATION                                  0xA7

/**
 * @format H2LV
 * @param handle
 * @param value_handle
 * @param value_length
 * @param value
 */
#define GATT_EVENT_INDICATION                                    0xA8

/**
 * @format H2LV
 * @param handle
 * @param descriptor_handle
 * @param descriptor_length
 * @param descriptor
 */
#define GATT_EVENT_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT        0xA9

/**
 * @format H2LV
 * @param handle
 * @param descriptor_offset
 * @param descriptor_length
 * @param descriptor
 */
#define GATT_EVENT_LONG_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT   0xAA

/** 
 * @format H2
 * @param handle
 * @param MTU
 */    
#define GATT_EVENT_MTU                                           0xAB

/**
 * @format H
 * @param handle
 */
#define GATT_EVENT_CAN_WRITE_WITHOUT_RESPONSE                    0xAC

/** 
 * @format 1BH
 * @param address_type
 * @param address
 * @param handle
 */    
#define ATT_EVENT_CONNECTED                                      0xB3

/** 
 * @format H
 * @param handle
 */    
#define ATT_EVENT_DISCONNECTED                                   0xB4

/** 
 * @format H2
 * @param handle
 * @param MTU
 */    
#define ATT_EVENT_MTU_EXCHANGE_COMPLETE                          0xB5

 /**
  * @format 1H2
  * @param status
  * @param conn_handle
  * @param attribute_handle
  */
#define ATT_EVENT_HANDLE_VALUE_INDICATION_COMPLETE               0xB6

/**
 * @format
 */
#define ATT_EVENT_CAN_SEND_NOW                                   0xB7

// TODO: daemon only event

/**
 * @format 12
 * @param status
 * @param service_uuid
 */
 #define BNEP_EVENT_SERVICE_REGISTERED                      0xC0

/**
 * @format 12222BH
 * @param status
 * @param bnep_cid
 * @param source_uuid
 * @param destination_uuid
 * @param mtu
 * @param remote_address
 * @param con_handle
 */
 #define BNEP_EVENT_CHANNEL_OPENED                   0xC1

/**
 * @format 222B
 * @param bnep_cid
 * @param source_uuid
 * @param destination_uuid
 * @param remote_address
 */
 #define BNEP_EVENT_CHANNEL_CLOSED                          0xC2

/**
 * @format 222B1
 * @param bnep_cid
 * @param source_uuid
 * @param destination_uuid
 * @param remote_address
 * @param channel_state
 */
#define BNEP_EVENT_CHANNEL_TIMEOUT                         0xC3    
    
/**
 * @format 222B
 * @param bnep_cid
 * @param source_uuid
 * @param destination_uuid
 * @param remote_address
 */
 #define BNEP_EVENT_CAN_SEND_NOW                           0xC4

 /**
  * @format H1B
  * @param handle
  * @param addr_type
  * @param address
  */
#define SM_EVENT_JUST_WORKS_REQUEST                              0xD0

 /**
  * @format H1B
  * @param handle
  * @param addr_type
  * @param address
  */
#define SM_EVENT_JUST_WORKS_CANCEL                               0xD1 

 /**
  * @format H1B4
  * @param handle
  * @param addr_type
  * @param address
  * @param passkey
  */
#define SM_EVENT_PASSKEY_DISPLAY_NUMBER                          0xD2

 /**
  * @format H1B
  * @param handle
  * @param addr_type
  * @param address
  */
#define SM_EVENT_PASSKEY_DISPLAY_CANCEL                          0xD3

 /**
  * @format H1B
  * @param handle
  * @param addr_type
  * @param address
  */
#define SM_EVENT_PASSKEY_INPUT_NUMBER                            0xD4

 /**
  * @format H1B
  * @param handle
  * @param addr_type
  * @param address
  */
#define SM_EVENT_PASSKEY_INPUT_CANCEL                            0xD5

 /**
  * @format H1B4
  * @param handle
  * @param addr_type
  * @param address
  * @param passkey
  */
#define SM_EVENT_NUMERIC_COMPARISON_REQUEST                      0xD6

 /**
  * @format H1B
  * @param handle
  * @param addr_type
  * @param address
  */
#define SM_EVENT_NUMERIC_COMPARISON_CANCEL                       0xD7

 /**
  * @format H1B
  * @param handle
  * @param addr_type
  * @param address
  */
#define SM_EVENT_IDENTITY_RESOLVING_STARTED                      0xD8

 /**
  * @format H1B
  * @param handle
  * @param addr_type
  * @param address
  */
#define SM_EVENT_IDENTITY_RESOLVING_FAILED                       0xD9

 /**
  * @brief Identify resolving succeeded
  *
  * @format H1B1B2
  * @param handle
  * @param addr_type
  * @param address
  * @param identity_addr_type
  * @param identity_address
  * @param index
  *
  */
#define SM_EVENT_IDENTITY_RESOLVING_SUCCEEDED                    0xDA

 /**
  * @format H1B
  * @param handle
  * @param addr_type
  * @param address
  */
#define SM_EVENT_AUTHORIZATION_REQUEST                           0xDB

 /**
  * @format H1B1
  * @param handle
  * @param addr_type
  * @param address
  * @param authorization_result
  */
#define SM_EVENT_AUTHORIZATION_RESULT                            0xDC

 /**
  * @format H1
  * @param handle
  * @param action see SM_KEYPRESS_*
  */
#define SM_EVENT_KEYPRESS_NOTIFICATION                           0xDD

 /**
  * @brief Emitted during pairing to inform app about address used as identity
  *
  * @format H1B1B2
  * @param handle
  * @param addr_type
  * @param address
  * @param identity_addr_type
  * @param identity_address
  * @param index
  */
#define SM_EVENT_IDENTITY_CREATED                                0xDE

 /**
  * @brief Emitted to inform app that pairing is complete. Possible status values:
  *        ERROR_CODE_SUCCESS                            -> pairing success
  *        ERROR_CODE_CONNECTION_TIMEOUT                 -> timeout
  *        ERROR_CODE_REMOTE_USER_TERMINATED_CONNECTION  -> disconnect
  *        ERROR_CODE_AUTHENTICATION_FAILURE             -> SM protocol error, see reason field with SM_REASON_* from bluetooth.h
  *
  * @format H1B11
  * @param handle
  * @param addr_type
  * @param address
  * @param status
  * @param reason if status == ERROR_CODE_AUTHENTICATION_FAILURE
  */
#define SM_EVENT_PAIRING_COMPLETE                                0xDF


// GAP

/**
 * @format H1
 * @param handle
 * @param security_level
 */
#define GAP_EVENT_SECURITY_LEVEL                                 0xE0

/**
 * @format 1B
 * @param status
 * @param address
 */
#define GAP_EVENT_DEDICATED_BONDING_COMPLETED                    0xE1

/**
 * @format 11B1JV
 * @param advertising_event_type
 * @param address_type
 * @param address
 * @param rssi
 * @param data_length
 * @param data
 */
#define GAP_EVENT_ADVERTISING_REPORT                          0xE2

 /**
 * @format B132111JV
 * @param bd_addr
 * @param page_scan_repetition_mode
 * @param class_of_device
 * @param clock_offset
 * @param rssi_available
 * @param rssi
 * @param name_available
 * @param name_len
 * @param name
 */
#define GAP_EVENT_INQUIRY_RESULT                              0xE3

/**
 * @format 1
 * @param status
 */
#define GAP_EVENT_INQUIRY_COMPLETE                            0xE4

/**
 * @format H1
 * @param con_handle
 * @param rssi (signed integer -127..127)
 * @note Classic: rssi is in dB relative to Golden Receive Power Range
 * @note LE: rssi is absolute dBm
 */
#define GAP_EVENT_RSSI_MEASUREMENT                            0xE5

// Meta Events, see below for sub events
#define HCI_EVENT_HSP_META                                 0xE8
#define HCI_EVENT_HFP_META                                 0xE9
#define HCI_EVENT_ANCS_META                                0xEA
#define HCI_EVENT_AVDTP_META                               0xEB
#define HCI_EVENT_AVRCP_META                               0xEC
#define HCI_EVENT_GOEP_META                                0xED
#define HCI_EVENT_PBAP_META                                0xEE
#define HCI_EVENT_HID_META                                 0xEF
#define HCI_EVENT_A2DP_META                                0xF0
#define HCI_EVENT_HIDS_META                                0xF1
#define HCI_EVENT_GATTSERVICE_META                         0xF2
#define HCI_EVENT_BIP_META                                 0xF3
#define HCI_EVENT_MAP_META                                 0xF4
#define HCI_EVENT_MESH_META                                0xF5

// Potential other meta groups
// #define HCI_EVENT_BNEP_META                                0xxx
// #define HCI_EVENT_GAP_META                                 0xxx
// #define HCI_EVENT_GATT_META                                0xxx
// #define HCI_EVENT_PAN_META                                 0xxx
// #define HCI_EVENT_SDP_META                                 0xxx
// #define HCI_EVENT_SM_META                                  0xxx


/** HSP Subevent */

/**
 * @format 11
 * @param subevent_code
 * @param status 0 == OK
 */
#define HSP_SUBEVENT_RFCOMM_CONNECTION_COMPLETE             0x01

/**
 * @format 11
 * @param subevent_code
 * @param status 0 == OK
 */
#define HSP_SUBEVENT_RFCOMM_DISCONNECTION_COMPLETE           0x02


/**
 * @format 11H
 * @param subevent_code
 * @param status 0 == OK
 * @param handle
 */
#define HSP_SUBEVENT_AUDIO_CONNECTION_COMPLETE             0x03

/**
 * @format 11
 * @param subevent_code
 * @param status 0 == OK
 */
#define HSP_SUBEVENT_AUDIO_DISCONNECTION_COMPLETE          0x04

/**
 * @format 1
 * @param subevent_code
 */
#define HSP_SUBEVENT_RING                                  0x05

/**
 * @format 11
 * @param subevent_code
 * @param gain Valid range: [0,15]
 */
#define HSP_SUBEVENT_MICROPHONE_GAIN_CHANGED               0x06

/**
 * @format 11
 * @param subevent_code
 * @param gain Valid range: [0,15]
 */
#define HSP_SUBEVENT_SPEAKER_GAIN_CHANGED                  0x07

/**
 * @format 1JV
 * @param subevent_code
 * @param value_length
 * @param value
 */
#define HSP_SUBEVENT_HS_COMMAND                            0x08

/**
 * @format 1JV
 * @param subevent_code
 * @param value_length
 * @param value
 */
#define HSP_SUBEVENT_AG_INDICATION                         0x09


/** HFP Subevent */

/**
 * @format 11HB
 * @param subevent_code
 * @param status 0 == OK
 * @param con_handle
 * @param bd_addr
 */
#define HFP_SUBEVENT_SERVICE_LEVEL_CONNECTION_ESTABLISHED  0x01

/**
 * @format 1
 * @param subevent_code
 */
#define HFP_SUBEVENT_SERVICE_LEVEL_CONNECTION_RELEASED     0x02

/**
 * @format 11HB1
 * @param subevent_code
 * @param status 0 == OK
 * @param handle
 * @param bd_addr
 * @param negotiated_codec
 */
#define HFP_SUBEVENT_AUDIO_CONNECTION_ESTABLISHED          0x03

/**
 * @format 1
 * @param subevent_code
 */
#define HFP_SUBEVENT_AUDIO_CONNECTION_RELEASED             0x04

/**
 * @format 11
 * @param subevent_code
 * @param status 0 == OK
 */
#define HFP_SUBEVENT_COMPLETE                              0x05

/**
 * @format 11111111T
 * @param subevent_code
 * @param indicator_index
 * @param indicator_status
 * @param indicator_min_range
 * @param indicator_max_range
 * @param indicator_mandatory
 * @param indicator_enabled
 * @param indicator_status_changed
 * @param indicator_name
 */
#define HFP_SUBEVENT_AG_INDICATOR_STATUS_CHANGED           0x06

/**
 * @format 111T
 * @param subevent_code
 * @param network_operator_mode
 * @param network_operator_format
 * @param network_operator_name
 */
#define HFP_SUBEVENT_NETWORK_OPERATOR_CHANGED              0x07

/**
 * @format 11
 * @param subevent_code
 * @param error
 */
#define HFP_SUBEVENT_EXTENDED_AUDIO_GATEWAY_ERROR             0x08

/**
 * @format 1
 * @param subevent_code
 */
#define HFP_SUBEVENT_START_RINGINIG                           0x0A

/**
 * @format 1
 * @param subevent_code
 */
#define HFP_SUBEVENT_STOP_RINGINIG                            0x0B

/**
 * @format 1
 * @param subevent_code
 */
#define HFP_SUBEVENT_CALL_TERMINATED                          0x0C

/**
 * @format 1T
 * @param subevent_code
 * @param number
 */
#define HFP_SUBEVENT_PLACE_CALL_WITH_NUMBER                   0x0D

/**
 * @format 1
 * @param subevent_code
 */
#define HFP_SUBEVENT_ATTACH_NUMBER_TO_VOICE_TAG               0x0E

/**
 * @format 1T
 * @param subevent_code
 * @param number
 */
#define HFP_SUBEVENT_NUMBER_FOR_VOICE_TAG                     0x0F

/**
 * @format 1T
 * @param subevent_code
 * @param dtmf code
 */
#define HFP_SUBEVENT_TRANSMIT_DTMF_CODES                      0x10

/**
 * @format 1
 * @param subevent_code
 */
 #define HFP_SUBEVENT_CALL_ANSWERED                            0x11

/**
 * @format 1
 * @param subevent_code
 */
#define HFP_SUBEVENT_CONFERENCE_CALL                          0x12

/**
 * @format 1
 * @param subevent_code
 */
#define HFP_SUBEVENT_RING                                     0x13

/**
 * @format 111
 * @param subevent_code
 * @param status
 * @param gain
 */
 #define HFP_SUBEVENT_SPEAKER_VOLUME                           0x14

/**
 * @format 111
 * @param subevent_code
 * @param status
 * @param gain
 */
#define HFP_SUBEVENT_MICROPHONE_VOLUME                        0x15

/**
 * @format 11T
 * @param subevent_code
 * @param type
 * @param number
 */
#define HFP_SUBEVENT_CALL_WAITING_NOTIFICATION                0x16

/**
 * @format 11T
 * @param subevent_code
 * @param type
 * @param number
 */
#define HFP_SUBEVENT_CALLING_LINE_IDENTIFICATION_NOTIFICATION 0x17

/**
 * @format 1111111T
 * @param subevent_code
 * @param clcc_idx
 * @param clcc_dir
 * @param clcc_status
 * @param clcc_mode
 * @param clcc_mpty
 * @param bnip_type
 * @param bnip_number
 */
#define HFP_SUBEVENT_ENHANCED_CALL_STATUS                     0x18

/**
 * @format 111T
 * @param subevent_code
 * @param status
 * @param bnip_type
 * @param bnip_number
 */
 #define HFP_SUBEVENT_SUBSCRIBER_NUMBER_INFORMATION            0x19

/**
 * @format 1T
 * @param subevent_code
 * @param value
 */
#define HFP_SUBEVENT_RESPONSE_AND_HOLD_STATUS                 0x1A

// ANCS Client

/**
 * @format 1H
 * @param subevent_code
 * @param handle
 */ 
#define ANCS_SUBEVENT_CLIENT_CONNECTED                              0xF0

/**
 * @format 1H2T
 * @param subevent_code
 * @param handle
 * @param attribute_id
 * @param text
 */ 
#define ANCS_SUBEVENT_CLIENT_NOTIFICATION                           0xF1

/**
 * @format 1H
 * @param subevent_code
 * @param handle
 */ 
#define ANCS_SUBEVENT_CLIENT_DISCONNECTED                           0xF2


/** AVDTP Subevent */

/**
 * @format 1211
 * @param subevent_code
 * @param avdtp_cid
 * @param local_seid
 * @param signal_identifier
 */
#define AVDTP_SUBEVENT_SIGNALING_ACCEPT                     0x01

/**
 * @format 1211
 * @param subevent_code
 * @param avdtp_cid
 * @param local_seid
 * @param signal_identifier 
 */
#define AVDTP_SUBEVENT_SIGNALING_REJECT                     0x02

/**
 * @format 1211
 * @param subevent_code
 * @param avdtp_cid
 * @param local_seid
 * @param signal_identifier
 */
#define AVDTP_SUBEVENT_SIGNALING_GENERAL_REJECT             0x03

/**
 * @format 12B1
 * @param subevent_code
 * @param avdtp_cid
 * @param bd_addr
 * @param status 0 == OK
 */
#define AVDTP_SUBEVENT_SIGNALING_CONNECTION_ESTABLISHED     0x04

/**
 * @format 12
 * @param subevent_code
 * @param avdtp_cid
 */
#define AVDTP_SUBEVENT_SIGNALING_CONNECTION_RELEASED        0x05

/**
 * @format 121111
 * @param subevent_code
 * @param avdtp_cid
 * @param remote_seid        0x01 – 0x3E
 * @param in_use      0-not in use, 1-in use
 * @param media_type  0-audio, 1-video, 2-multimedia
 * @param sep_type    0-source, 1-sink
 */
#define AVDTP_SUBEVENT_SIGNALING_SEP_FOUND                  0x06

/**
 * @format 121111111111
 * @param subevent_code
 * @param avdtp_cid
 * @param local_seid
 * @param remote_seid
 * @param media_type
 * @param sampling_frequency_bitmap
 * @param channel_mode_bitmap
 * @param block_length_bitmap
 * @param subbands_bitmap
 * @param allocation_method_bitmap
 * @param min_bitpool_value
 * @param max_bitpool_value
 */
#define AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_SBC_CAPABILITY          0x07

/**
 * @format 121112LV
 * @param subevent_code
 * @param avdtp_cid
 * @param local_seid
 * @param remote_seid
 * @param media_type
 * @param media_codec_type
 * @param media_codec_information_len
 * @param media_codec_information
 */
#define AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_OTHER_CAPABILITY        0x08


/**
 * @format 1211
 * @param subevent_code
 * @param avdtp_cid
 * @param local_seid
 * @param remote_seid
 */
#define AVDTP_SUBEVENT_SIGNALING_MEDIA_TRANSPORT_CAPABILITY        0x09


/**
 * @format 1211
 * @param subevent_code
 * @param avdtp_cid
 * @param local_seid
 * @param remote_seid
 */
#define AVDTP_SUBEVENT_SIGNALING_REPORTING_CAPABILITY        0x0A


/**
 * @format 1211111
 * @param subevent_code
 * @param avdtp_cid
 * @param local_seid
 * @param remote_seid
 * @param recovery_type
 * @param maximum_recovery_window_size
 * @param maximum_number_media_packets
 */
#define AVDTP_SUBEVENT_SIGNALING_RECOVERY_CAPABILITY        0x0B


/**
 * @format 12112LV
 * @param subevent_code
 * @param avdtp_cid
 * @param local_seid
 * @param remote_seid
 * @param cp_type
 * @param cp_type_value_len
 * @param cp_type_value
 */
#define AVDTP_SUBEVENT_SIGNALING_CONTENT_PROTECTION_CAPABILITY        0x0C


/**
 * @format 121111111111
 * @param subevent_code
 * @param avdtp_cid
 * @param local_seid
 * @param remote_seid
 * @param fragmentation
 * @param transport_identifiers_num
 * @param transport_session_identifier_1
 * @param transport_session_identifier_2
 * @param transport_session_identifier_3
 * @param tcid_1
 * @param tcid_2
 * @param tcid_3
 */
#define AVDTP_SUBEVENT_SIGNALING_MULTIPLEXING_CAPABILITY        0x0D


/**
 * @format 1211
 * @param subevent_code
 * @param avdtp_cid
 * @param local_seid
 * @param remote_seid
 */
#define AVDTP_SUBEVENT_SIGNALING_DELAY_REPORTING_CAPABILITY        0x0E


/**
 * @format 1211111
 * @param subevent_code
 * @param avdtp_cid
 * @param local_seid
 * @param remote_seid
 * @param back_ch
 * @param media
 * @param recovery
 */
#define AVDTP_SUBEVENT_SIGNALING_HEADER_COMPRESSION_CAPABILITY        0x0F


/**
 * @format 12111121111111
 * @param subevent_code
 * @param avdtp_cid
 * @param local_seid
 * @param remote_seid
 * @param reconfigure
 * @param media_type
 * @param sampling_frequency
 * @param channel_mode
 * @param num_channels
 * @param block_length
 * @param subbands
 * @param allocation_method
 * @param min_bitpool_value
 * @param max_bitpool_value
 */
#define AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_SBC_CONFIGURATION        0x10

/**
 * @format 1211112LV
 * @param subevent_code
 * @param avdtp_cid
 * @param local_seid
 * @param remote_seid
 * @param reconfigure
 * @param media_type
 * @param media_codec_type
 * @param media_codec_information_len
 * @param media_codec_information
 */
#define AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_OTHER_CONFIGURATION        0x11

/**
 * @format 12B111
 * @param subevent_code
 * @param avdtp_cid
 * @param bd_addr
 * @param local_seid
 * @param remote_seid
 * @param status 0 == OK
 */
#define AVDTP_SUBEVENT_STREAMING_CONNECTION_ESTABLISHED     0x12

/**
 * @format 121
 * @param subevent_code
 * @param avdtp_cid
 * @param local_seid
 */
#define AVDTP_SUBEVENT_STREAMING_CONNECTION_RELEASED        0x13

/**
 * @format 1212
 * @param subevent_code
 * @param avdtp_cid
 * @param local_seid
 * @param sequence_number
 */
#define AVDTP_SUBEVENT_STREAMING_CAN_SEND_MEDIA_PACKET_NOW   0x14

/**
 * @format 1211
 * @param subevent_code
 * @param avdtp_cid
 * @param local_seid
 * @param remote_seid
 */
#define AVDTP_SUBEVENT_SIGNALING_CAPABILITIES_DONE           0x15


/**
 * @format 12
 * @param subevent_code
 * @param avdtp_cid
 */
#define AVDTP_SUBEVENT_SIGNALING_SEP_DICOVERY_DONE           0x16

/**
 * @format 1212
 * @param subevent_code
 * @param avdtp_cid
 * @param local_seid
 * @param delay_100us
 */
#define AVDTP_SUBEVENT_SIGNALING_DELAY_REPORT               0x17

/** A2DP Subevent */
/* Stream goes through following states:
 * - OPEN         - indicated with A2DP_SUBEVENT_STREAM_ESTABLISHED event 
 * - START        - indicated with A2DP_SUBEVENT_STREAM_STARTED event
 * - SUSPEND      - indicated with A2DP_SUBEVENT_STREAM_SUSPENDED event
 * - ABORT/STOP   - indicated with A2DP_SUBEVENT_STREAM_RELEASED event

 OPEN state will be followed by ABORT/STOP. Stream is ready but media transfer is not started. 
 START can come only after the stream is OPENED, and indicates that media transfer is started. 
 SUSPEND is optional, it pauses the stream.
 */

/**
 * @format 121            Sent only by A2DP source.
 * @param subevent_code
 * @param a2dp_cid
 * @param local_seid
 */
#define A2DP_SUBEVENT_STREAMING_CAN_SEND_MEDIA_PACKET_NOW         0x01

/**
 * @format 12111121111111
 * @param subevent_code
 * @param a2dp_cid
 * @param int_seid
 * @param acp_seid
 * @param reconfigure
 * @param media_type
 * @param sampling_frequency
 * @param channel_mode
 * @param num_channels
 * @param block_length
 * @param subbands
 * @param allocation_method
 * @param min_bitpool_value
 * @param max_bitpool_value
 */
#define A2DP_SUBEVENT_SIGNALING_MEDIA_CODEC_SBC_CONFIGURATION      0x02

/**
 * @format 1211112LV
 * @param subevent_code
 * @param a2dp_cid
 * @param int_seid
 * @param acp_seid
 * @param reconfigure
 * @param media_type
 * @param media_codec_type
 * @param media_codec_information_len
 * @param media_codec_information
 */
#define A2DP_SUBEVENT_SIGNALING_MEDIA_CODEC_OTHER_CONFIGURATION    0x03

/**
 * @format 12B111          Stream is opened byt not started.
 * @param subevent_code 
 * @param a2dp_cid
 * @param bd_addr
 * @param local_seid
 * @param remote_seid
 * @param status
 */
#define A2DP_SUBEVENT_STREAM_ESTABLISHED                           0x04

/**
 * @format 121            Indicates that media transfer is started.
 * @param subevent_code
 * @param a2dp_cid
 * @param local_seid
 */
#define A2DP_SUBEVENT_STREAM_STARTED                               0x05

/**
 * @format 121           Stream is paused.
 * @param subevent_code
 * @param a2dp_cid
 * @param local_seid
 */
#define A2DP_SUBEVENT_STREAM_SUSPENDED                              0x06

/**
 * @format 121           Stream is stoped or aborted.
 * @param subevent_code
 * @param a2dp_cid
 * @param local_seid
 */
#define A2DP_SUBEVENT_STREAM_STOPPED                                0x07

/**
 * @format 121            Stream is released.
 * @param subevent_code
 * @param a2dp_cid
 * @param local_seid
 */
#define A2DP_SUBEVENT_STREAM_RELEASED                               0x08

/**
 * @format 1211
 * @param subevent_code
 * @param a2dp_cid
 * @param local_seid
 * @param signal_identifier
 */
#define A2DP_SUBEVENT_COMMAND_ACCEPTED                              0x09

/**
 * @format 1211
 * @param subevent_code
 * @param a2dp_cid
 * @param local_seid
 * @param signal_identifier 
 */
#define A2DP_SUBEVENT_COMMAND_REJECTED                              0x0A

/**
 * @format 12B1
 * @param subevent_code
 * @param a2dp_cid
 * @param bd_addr
 * @param status 0 == OK
 */
#define A2DP_SUBEVENT_SIGNALING_CONNECTION_ESTABLISHED              0x0B

/**
 * @format 12            Signaling channel is released.
 * @param subevent_code
 * @param a2dp_cid
 */
#define A2DP_SUBEVENT_SIGNALING_CONNECTION_RELEASED                  0x0C

/**
 * @format 1211          Stream was reconfigured
 * @param subevent_code
 * @param a2dp_cid
 * @param local_seid
 * @param status
 */
#define A2DP_SUBEVENT_STREAM_RECONFIGURED                            0x0D

/**
 * @format 1211
 * @param subevent_code
 * @param avdtp_cid
 * @param local_seid
 * @param remote_seid
 */
#define A2DP_SUBEVENT_SIGNALING_DELAY_REPORTING_CAPABILITY           0x0E


/**
 * @format 1212
 * @param subevent_code
 * @param avdtp_cid
 * @param local_seid
 * @param delay_100us
 */
#define A2DP_SUBEVENT_SIGNALING_DELAY_REPORT                         0x0F

/**
 * @format 1211
 * @param subevent_code
 * @param avdtp_cid
 * @param local_seid
 * @param remote_seid
 */
#define A2DP_SUBEVENT_SIGNALING_CAPABILITIES_DONE                    0x10

/** AVRCP Subevent */

/**
 * @format 11B2
 * @param subevent_code
 * @param status 0 == OK
 * @param bd_addr
 * @param avrcp_cid
 */
#define AVRCP_SUBEVENT_CONNECTION_ESTABLISHED                           0x01

/**
 * @format 12
 * @param subevent_code
 * @param avrcp_cid
 */
#define AVRCP_SUBEVENT_CONNECTION_RELEASED                              0x02

/**
 * @format 12111
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 * @param repeat_mode
 * @param shuffle_mode
 */
#define AVRCP_SUBEVENT_SHUFFLE_AND_REPEAT_MODE                          0x03

/**
 * @format 121441
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 * @param song_length
 * @param song_position
 * @param play_status
 */
 #define AVRCP_SUBEVENT_PLAY_STATUS                                     0x04

/**
 * @format 1211
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 * @param play_status
 */
#define AVRCP_SUBEVENT_NOTIFICATION_PLAYBACK_STATUS_CHANGED             0x05

/**
 * @format 121
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 */
#define AVRCP_SUBEVENT_NOTIFICATION_TRACK_CHANGED                       0x06
  
/**
 * @format 121
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 */
#define AVRCP_SUBEVENT_NOTIFICATION_NOW_PLAYING_CONTENT_CHANGED          0x07

/**
 * @format 121
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 */
#define AVRCP_SUBEVENT_NOTIFICATION_AVAILABLE_PLAYERS_CHANGED            0x08

/**
 * @format 1211
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 * @param absolute_volume
 */
#define AVRCP_SUBEVENT_NOTIFICATION_VOLUME_CHANGED                       0x09

/**
 * @format 1211
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 * @param absolute_volume
 */
#define AVRCP_SUBEVENT_SET_ABSOLUTE_VOLUME_RESPONSE                      0x0A

/**
 * @format 1211
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 * @param notification_id
 */
#define AVRCP_SUBEVENT_ENABLE_NOTIFICATION_COMPLETE                       0x0B

/**
 * @format 1211
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 * @param operation_id
 */
#define AVRCP_SUBEVENT_OPERATION_START                                    0x0C

/**
 * @format 1211
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 * @param operation_id
 */
#define AVRCP_SUBEVENT_OPERATION_COMPLETE                                 0x0D

/**
 * @format 121
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 */
#define AVRCP_SUBEVENT_PLAYER_APPLICATION_VALUE_RESPONSE                   0x0E

/**
 * @format 12
 * @param subevent_code
 * @param avrcp_cid
 */
#define AVRCP_SUBEVENT_COMPANY_IDS_QUERY                                    0x0F

/**
 * @format 12
 * @param subevent_code
 * @param avrcp_cid
 */
#define AVRCP_SUBEVENT_EVENT_IDS_QUERY                                      0x10

/**
 * @format 12
 * @param subevent_code
 * @param avrcp_cid
 */
#define AVRCP_SUBEVENT_PLAY_STATUS_QUERY                                    0x11

/**
 * @format 12111
 * @param subevent_code
 * @param avrcp_cid
 * @param operation_id
 * @param operands_length
 * @param operand
 */
#define AVRCP_SUBEVENT_OPERATION                                            0x12 

/**
 * @format 1211
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 * @param track
 */
#define AVRCP_SUBEVENT_NOW_PLAYING_TRACK_INFO                                 0x13

/**
 * @format 1211
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 * @param total_tracks
 */
#define AVRCP_SUBEVENT_NOW_PLAYING_TOTAL_TRACKS_INFO                          0x14

/**
 * @format 1214
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 * @param song_length in ms
 */
#define AVRCP_SUBEVENT_NOW_PLAYING_SONG_LENGTH_MS_INFO                        0x15

/**
 * @format 121JV
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 * @param value_len
 * @param value
 */
#define AVRCP_SUBEVENT_NOW_PLAYING_TITLE_INFO                                 0x16

 /*
 * @format 121JV
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 * @param value_len
 * @param value
 */
#define AVRCP_SUBEVENT_NOW_PLAYING_ARTIST_INFO                                0x17

 /*
 * @format 121JV
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 * @param value_len
 * @param value
 */
#define AVRCP_SUBEVENT_NOW_PLAYING_ALBUM_INFO                                 0x18

 /*
 * @format 121JV
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 * @param value_len
 * @param value
 */
#define AVRCP_SUBEVENT_NOW_PLAYING_GENRE_INFO                                 0x19

/*
 * @format 1211
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 * @param status
 */
#define AVRCP_SUBEVENT_NOW_PLAYING_INFO_DONE                                  0x1A

/**
 * @format 1B2
 * @param subevent_code
 * @param bd_addr
 * @param browsing_cid
 */
#define AVRCP_SUBEVENT_INCOMING_BROWSING_CONNECTION                          0x1B

/**
 * @format 11B2
 * @param subevent_code
 * @param status 0 == OK
 * @param bd_addr
 * @param browsing_cid
 */
#define AVRCP_SUBEVENT_BROWSING_CONNECTION_ESTABLISHED                        0x1C

/**
 * @format 12
 * @param subevent_code
 * @param browsing_cid
 */
#define AVRCP_SUBEVENT_BROWSING_CONNECTION_RELEASED                           0x1D

/**
 * @format 12211
 * @param subevent_code
 * @param browsing_cid
 * @param uid_counter
 * @param browsing_status
 * @param bluetooth_status
 */
#define AVRCP_SUBEVENT_BROWSING_DONE                                          0x1E

/**
 * @format 1214
 * @param subevent_code
 * @param browsing_cid
 * @param scope
 * @param attr_bitmap
 */
#define AVRCP_SUBEVENT_BROWSING_GET_FOLDER_ITEMS                              0x1F

/**
 * @format 121
 * @param subevent_code
 * @param browsing_cid
 * @param scope
 */
#define AVRCP_SUBEVENT_BROWSING_GET_TOTAL_NUM_ITEMS                           0x20

/**
 * @format 1214
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 * @param playback_position_ms
 */
#define AVRCP_SUBEVENT_NOTIFICATION_PLAYBACK_POS_CHANGED                      0x21


/**
 * @format 121BH1
 * @param subevent_code
 * @param goep_cid
 * @param status
 * @param bd_addr
 * @param con_handle
 * @param incoming
 */
#define GOEP_SUBEVENT_CONNECTION_OPENED                                    0x01

/**
 * @format 12
 * @param subevent_code
 * @param goep_cid
*/
#define GOEP_SUBEVENT_CONNECTION_CLOSED                                    0x02

/**
 * @format 12
 * @param subevent_code
 * @param goep_cid
*/
#define GOEP_SUBEVENT_CAN_SEND_NOW                                         0x03

/**
 * @format 121BH1
 * @param subevent_code
 * @param pbap_cid
 * @param status
 * @param bd_addr
 * @param con_handle
 * @param incoming
 */
#define PBAP_SUBEVENT_CONNECTION_OPENED                                    0x01

/**
 * @format 12
 * @param subevent_code
 * @param goep_cid
*/
#define PBAP_SUBEVENT_CONNECTION_CLOSED                                    0x02

/**
 * @format 121
 * @param subevent_code
 * @param goep_cid
 * @param status
 */
#define PBAP_SUBEVENT_OPERATION_COMPLETED                                  0x03

/**
 * @format 1212
 * @param subevent_code
 * @param goep_cid
 * @param status
 * @param phoneboook_size
 */
#define PBAP_SUBEVENT_PHONEBOOK_SIZE                                       0x04

/**
 * @format 1211
 * @param subevent_code
 * @param goep_cid
 * @param user_id_required
 * @param full_access 
 */
#define PBAP_SUBEVENT_AUTHENTICATION_REQUEST                               0x05

/**
 * @format 12JVJV
 * @param subevent_code
 * @param goep_cid
 * @param name_len
 * @param name 
 * @param handle_len
 * @param handle 
 */
#define PBAP_SUBEVENT_CARD_RESULT                                          0x06


// HID Meta Event Group

/**
 * @format 121BH1
 * @param subevent_code
 * @param hid_cid
 * @param status
 * @param bd_addr
 * @param con_handle
 * @param incoming
 */
#define HID_SUBEVENT_CONNECTION_OPENED                                     0x01

/**
 * @format 12
 * @param subevent_code
 * @param hid_cid
*/
#define HID_SUBEVENT_CONNECTION_CLOSED                                     0x02

/**
 * @format 12
 * @param subevent_code
 * @param hid_cid
*/
#define HID_SUBEVENT_CAN_SEND_NOW                                          0x03

/**
 * @format 12
 * @param subevent_code
 * @param con_handle
*/
#define HID_SUBEVENT_SUSPEND                                               0x04

/**
 * @format 12
 * @param subevent_code
 * @param con_handle
*/
#define HID_SUBEVENT_EXIT_SUSPEND                                          0x05


// HIDS Meta Event Group

/**
 * @format 12
 * @param subevent_code
 * @param con_handle
*/
#define HIDS_SUBEVENT_CAN_SEND_NOW                                          0x01

/**
 * @format 121
 * @param subevent_code
 * @param con_handle
 * @param protocol_mode
*/
#define HIDS_SUBEVENT_PROTOCOL_MODE                                         0x02

/**
 * @format 121
 * @param subevent_code
 * @param con_handle
 * @param enable
*/
#define HIDS_SUBEVENT_BOOT_MOUSE_INPUT_REPORT_ENABLE                        0x03

/**
 * @format 121
 * @param subevent_code
 * @param con_handle
 * @param enable
*/
#define HIDS_SUBEVENT_BOOT_KEYBOARD_INPUT_REPORT_ENABLE                     0x04

/**
 * @format 121
 * @param subevent_code
 * @param con_handle
 * @param enable
*/
#define HIDS_SUBEVENT_INPUT_REPORT_ENABLE                                   0x05

/**
 * @format 121
 * @param subevent_code
 * @param con_handle
 * @param enable
*/
#define HIDS_SUBEVENT_OUTPUT_REPORT_ENABLE                                  0x06

/**
 * @format 121
 * @param subevent_code
 * @param con_handle
 * @param enable
*/
#define HIDS_SUBEVENT_FEATURE_REPORT_ENABLE                                 0x07

/**
 * @format 12
 * @param subevent_code
 * @param con_handle
*/
#define HIDS_SUBEVENT_SUSPEND                                               0x08

/**
 * @format 12
 * @param subevent_code
 * @param con_handle
*/
#define HIDS_SUBEVENT_EXIT_SUSPEND                                          0x09

/**
 * @format 1211
 * @param subevent_code
 * @param con_handle
 * @param measurement_type  // 0 - force magnitude, 1 - torque magnitude, see cycling_power_sensor_measurement_context_t
 * @param is_enhanced
*/
#define GATTSERVICE_SUBEVENT_CYCLING_POWER_START_CALIBRATION               0x01

/**
 * @format 12
 * @param subevent_code
 * @param con_handle
*/
#define GATTSERVICE_SUBEVENT_CYCLING_POWER_BROADCAST_START                 0x02

/**
 * @format 12
 * @param subevent_code
 * @param con_handle
*/
#define GATTSERVICE_SUBEVENT_CYCLING_POWER_BROADCAST_STOP                  0x03


// MAP Meta Event Group

/**
 * @format 121BH1
 * @param subevent_code
 * @param map_cid
 * @param status
 * @param bd_addr
 * @param con_handle
 * @param incoming
 */
#define MAP_SUBEVENT_CONNECTION_OPENED                                    0x01

/**
 * @format 12
 * @param subevent_code
 * @param map_cid
*/
#define MAP_SUBEVENT_CONNECTION_CLOSED                                    0x02

/**
 * @format 121
 * @param subevent_code
 * @param map_cid
 * @param status
 */
#define MAP_SUBEVENT_OPERATION_COMPLETED                                  0x03


/**
 * @format 12LV
 * @param subevent_code
 * @param map_cid
 * @param name_len
 * @param name
 */
#define MAP_SUBEVENT_FOLDER_LISTING_ITEM                                  0x04

/**
 * @format 12D
 * @param subevent_code
 * @param map_cid
 * @param handle

 */
#define MAP_SUBEVENT_MESSAGE_LISTING_ITEM                                 0x05

/**
 * @format 12
 * @param subevent_code
 * @param map_cid
 */
#define MAP_SUBEVENT_PARSING_DONE                                         0x06


// MESH Meta Event Group

/**
 * @format 1
 * @param subevent_code
 */
#define MESH_SUBEVENT_CAN_SEND_NOW                                          0x01

/**
 * @format 11
 * @param subevent_code
 * @param status
 */
#define MESH_SUBEVENT_PB_TRANSPORT_PDU_SENT                                          0x02

/**
 * @format 1121
 * @param subevent_code
 * @param status
 * @param pb_transport_cid
 * @param pb_type
 */
#define MESH_SUBEVENT_PB_TRANSPORT_LINK_OPEN                                         0x03

/**
 * @format 112
 * @param subevent_code
 * @param pb_transport_cid
 * @param reason
 */
#define MESH_SUBEVENT_PB_TRANSPORT_LINK_CLOSED                                       0x04

/**
 * @format 121
 * @param subevent_code
 * @param pb_transport_cid
 * @param attention_time in seconds
 */
#define MESH_SUBEVENT_PB_PROV_ATTENTION_TIMER                                        0x10

/**
 * Device Role
 * @format 12
 * @param subevent_code
 * @param pb_transport_cid
 */
#define MESH_SUBEVENT_PB_PROV_START_EMIT_PUBLIC_KEY_OOB                              0x11

/**
 * Device Role
 * @format 12
 * @param subevent_code
 * @param pb_transport_cid
 */
#define MESH_SUBEVENT_PB_PROV_STOP_EMIT_PUBLIC_KEY_OOB                               0x12

/**
 * Device Role
 * @format 12
 * @param subevent_code
 * @param pb_transport_cid
 */
#define MESH_SUBEVENT_PB_PROV_INPUT_OOB_REQUEST                                      0x13

/**
 * Device Role
 * @format 124
 * @param subevent_code
 * @param pb_transport_cid
 * @param output_oob number
 */
#define MESH_SUBEVENT_PB_PROV_START_EMIT_OUTPUT_OOB                                  0x15

/**
 * Device Role
 * @format 12
 * @param subevent_code
 * @param pb_transport_cid
 */
#define MESH_SUBEVENT_PB_PROV_STOP_EMIT_OUTPUT_OOB                                   0x16

/**
 * Provisioner Role
 * @format 12
 * @param subevent_code
 * @param pb_transport_cid
 */
#define MESH_SUBEVENT_PB_PROV_START_RECEIVE_PUBLIC_KEY_OOB                           0x17

/**
 * Provisioner Role
 * @format 12
 * @param subevent_code
 * @param pb_transport_cid
 */
#define MESH_SUBEVENT_PB_PROV_STOP_RECEIVE_PUBLIC_KEY_OOB                            0x18

/**
 * Provisioner Role
 * @format 12
 * @param subevent_code
 * @param pb_transport_cid
 */
#define MESH_SUBEVENT_PB_PROV_OUTPUT_OOB_REQUEST                                     0x19

/**
 * Provisioner Role
 * @format 124
 * @param subevent_code
 * @param pb_transport_cid
 * @param output_oob number
 */
#define MESH_SUBEVENT_PB_PROV_START_EMIT_INPUT_OOB                                   0x1a

/**
 * Provisioner Role
 * @format 12
 * @param subevent_code
 * @param pb_transport_cid
 */
#define MESH_SUBEVENT_PB_PROV_STOP_EMIT_INPUT_OOB                                    0x1b

/**
 * Provisioner Role
 * @format 1212111212
 * @param subevent_code
 * @param pb_transport_cid
 * @param num_elements
 * @param algorithms
 * @param public_key
 * @param static_oob_type
 * @param output_oob_size
 * @param output_oob_action
 * @param input_oob_size
 * @param input_oob_action
 */
#define MESH_SUBEVENT_PB_PROV_CAPABILITIES                                           0x1c

/**
 * @format 12
 * @param subevent_code
 * @param pb_transport_cid
 */
#define MESH_SUBEVENT_PB_PROV_COMPLETE                                               0x1d

/**
 * @format 11
 * @param subevent_code
 * @param attention_time in seconds
 */
#define MESH_SUBEVENT_ATTENTION_TIMER                                                0x1e

/**
 * @format 11H
 * @param subevent_code
 * @param status
 * @param con_handle
 */
#define MESH_SUBEVENT_PROXY_CONNECTED                                                0x20

/**
 * @format 1H
 * @param subevent_code
 * @param con_handle
 */
#define MESH_SUBEVENT_PROXY_PDU_SENT                                                 0x21

/**
 * @format 1H
 * @param subevent_code
 * @param con_handle
 */
#define MESH_SUBEVENT_PROXY_DISCONNECTED                                             0x22

/**
 * @format 1H
 * @param subevent_code
 * @param con_handle
 */
#define MESH_SUBEVENT_MESSAGE_SENT                                                   0x23

/**
 * @format 114411
 * @param subevent_code
 * @param element_index
 * @param model_identifier
 * @param state_identifier
 * @param reason
 * @param value
 */
#define MESH_SUBEVENT_STATE_UPDATE_BOOL                                              0x24

/**
 * @format 114412
 * @param subevent_code
 * @param element_index
 * @param model_identifier
 * @param state_identifier
 * @param reason
 * @param value
 */
#define MESH_SUBEVENT_STATE_UPDATE_INT16                                              0x25

// Mesh Client Events
/**
 * @format 11442
 * @param subevent_code
 * @param element_index
 * @param model_identifier
 * @param opcode
 * @param dest
 */
#define MESH_SUBEVENT_MESSAGE_NOT_ACKNOWLEDGED                                        0x30

/**
 * @format 114114
 * @param subevent_code
 * @param element_index
 * @param model_identifier
 * @param present_value
 * @param target_value       // optinal, if value > 0, than remaining_time_ms must be read
 * @param remaining_time_ms  
 */
#define MESH_SUBEVENT_GENERIC_ON_OFF_STATUS                                           0x31

/**
 * @format 114224
 * @param subevent_code
 * @param element_index
 * @param model_identifier
 * @param present_value
 * @param target_value       // optinal, if value > 0, than remaining_time_ms must be read
 * @param remaining_time_ms  
 */
#define MESH_SUBEVENT_GENERIC_LEVEL_STATUS                                           0x32


/**
 * @format 112
 * @param subevent_code
 * @param element_index
 * @param company_id
 */
#define MESH_SUBEVENT_HEALTH_CLEAR_REGISTERED_FAULTS                                 0x33

/**
 * @format 11422221
 * @param subevent_code
 * @param element_index
 * @param model_identifier
 * @param dest
 * @param netkey_index
 * @param appkey_index
 * @param company_id
 * @param test_id
 */
#define MESH_SUBEVENT_HEALTH_PERFORM_TEST                                            0x34

/**
 * @format 111
 * @param subevent_code
 * @param element_index
 * @param fast_period_divisor
 */
#define MESH_SUBEVENT_HEALTH_FAST_PERIOD_DIVISOR_CHANGED                             0x35

/**
 * @format 11
 * @param subevent_code
 * @param element_index
 */
#define MESH_SUBEVENT_HEALTH_ATTENTION_TIMER_CHANGED                                 0x36

#endif
