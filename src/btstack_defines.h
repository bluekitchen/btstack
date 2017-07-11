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

#ifndef __BTSTACK_DEFINES_H
#define __BTSTACK_DEFINES_H

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
 
// debug log messages
#define LOG_MESSAGE_PACKET      0xfc


// ERRORS
// last error code in 2.1 is 0x38 - we start with 0x50 for BTstack errors

/* ENUM_START: BTSTACK_ERROR_CODE */
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
#define L2CAP_CONNECTION_RESPONSE_RESULT_ERTM_NOT_SUPPORTD 0x68
#define L2CAP_CONNECTION_RESPONSE_RESULT_RTX_TIMEOUT       0x69
#define L2CAP_SERVICE_ALREADY_REGISTERED                   0x6A
#define L2CAP_DATA_LEN_EXCEEDS_REMOTE_MTU                  0x6B
#define L2CAP_SERVICE_DOES_NOT_EXIST                       0x6C
#define L2CAP_LOCAL_CID_DOES_NOT_EXIST                     0x6D
    
#define RFCOMM_MULTIPLEXER_STOPPED                         0x70
#define RFCOMM_CHANNEL_ALREADY_REGISTERED                  0x71
#define RFCOMM_NO_OUTGOING_CREDITS                         0x72
#define RFCOMM_AGGREGATE_FLOW_OFF                          0x73
#define RFCOMM_DATA_LEN_EXCEEDS_MTU                        0x74

#define SDP_HANDLE_ALREADY_REGISTERED                      0x80
#define SDP_QUERY_INCOMPLETE                               0x81
#define SDP_SERVICE_NOT_FOUND                              0x82
#define SDP_HANDLE_INVALID                                 0x83
#define SDP_QUERY_BUSY                                     0x84

#define ATT_HANDLE_VALUE_INDICATION_IN_PROGRESS            0x90 
#define ATT_HANDLE_VALUE_INDICATION_TIMEOUT                0x91

#define GATT_CLIENT_NOT_CONNECTED                          0x93
#define GATT_CLIENT_BUSY                                   0x94
#define GATT_CLIENT_IN_WRONG_STATE                         0x95
#define GATT_CLIENT_DIFFERENT_CONTEXT_FOR_ADDRESS_ALREADY_EXISTS 0x96
#define GATT_CLIENT_VALUE_TOO_LONG                         0x97
#define GATT_CLIENT_CHARACTERISTIC_NOTIFICATION_NOT_SUPPORTED 0x98
#define GATT_CLIENT_CHARACTERISTIC_INDICATION_NOT_SUPPORTED   0x99 

#define BNEP_SERVICE_ALREADY_REGISTERED                    0xA0
#define BNEP_CHANNEL_NOT_CONNECTED                         0xA1
#define BNEP_DATA_LEN_EXCEEDS_MTU                          0xA2

// OBEX ERRORS
#define OBEX_UNKNOWN_ERROR                                 0xB0
#define OBEX_CONNECT_FAILED                                0xB1
#define OBEX_DISCONNECTED                                  0xB2
#define OBEX_NOT_FOUND                                     0xB3

#define AVDTP_SEID_DOES_NOT_EXIST                          0xC0
#define AVDTP_CONNECTION_DOES_NOT_EXIST                    0xC1
#define AVDTP_CONNECTION_IN_WRONG_STATE                    0xC2
#define AVDTP_STREAM_ENDPOINT_IN_WRONG_STATE               0xC3
#define AVDTP_MEDIA_CONNECTION_DOES_NOT_EXIST              0xC4

/* ENUM_END */

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


// EVENTS

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
 * @format 1BH2222221
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
 * @param status
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
 * @format 12222B
 * @param status
 * @param bnep_cid
 * @param source_uuid
 * @param destination_uuid
 * @param mtu
 * @param remote_address
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
  * @param index_internal
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
  * @format H1B1B
  * @param handle
  * @param addr_type
  * @param address
  * @param identity_addr_type
  * @param identity_address
  */
#define SM_EVENT_IDENTITY_CREATED                                0xDE

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
 * @format 111T
 * @param subevent_code
 * @param indicator_index
 * @param indicator_status
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
 * @format 111111T
 * @param subevent_code
 * @param clcc_idx
 * @param clcc_dir
 * @param clcc_status
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
#define AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_SBC_CONFIGURATION        0x09

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
#define AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_OTHER_CONFIGURATION        0x0A

/**
 * @format 12111
 * @param subevent_code
 * @param avdtp_cid
 * @param local_seid
 * @param remote_seid
 * @param status 0 == OK
 */
#define AVDTP_SUBEVENT_STREAMING_CONNECTION_ESTABLISHED     0x0B

/**
 * @format 121
 * @param subevent_code
 * @param avdtp_cid
 * @param local_seid
 */
#define AVDTP_SUBEVENT_STREAMING_CONNECTION_RELEASED        0x0C

/**
 * @format 1212
 * @param subevent_code
 * @param avdtp_cid
 * @param local_seid
 * @param sequence_number
 */
#define AVDTP_SUBEVENT_STREAMING_CAN_SEND_MEDIA_PACKET_NOW   0x0D


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
 * @format 12111          Stream is opened byt not started.
 * @param subevent_code 
 * @param a2dp_cid
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
 * @format 121            Stream is released.
 * @param subevent_code
 * @param a2dp_cid
 * @param local_seid
 */
#define A2DP_SUBEVENT_STREAM_RELEASED                               0x07

/**
 * @format 1211
 * @param subevent_code
 * @param a2dp_cid
 * @param local_seid
 * @param signal_identifier
 */
#define A2DP_SUBEVENT_COMMAND_ACCEPTED                              0x08

/**
 * @format 1211
 * @param subevent_code
 * @param a2dp_cid
 * @param local_seid
 * @param signal_identifier 
 */
#define A2DP_SUBEVENT_COMMAND_REJECTED                              0x09

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
 * @format 121114JVJVJVJV
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 * @param track
 * @param total_tracks
 * @param song_length in ms
 * @param title_len
 * @param title
 * @param artist_len
 * @param artist
 * @param album_len
 * @param album
 * @param genre_len
 * @param genre
 */
#define AVRCP_SUBEVENT_NOW_PLAYING_INFO                                 0x03

/**
 * @format 12111
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 * @param repeat_mode
 * @param shuffle_mode
 */
#define AVRCP_SUBEVENT_SHUFFLE_AND_REPEAT_MODE                          0x04

/**
 * @format 121441
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 * @param song_length
 * @param song_position
 * @param play_status
 */
 #define AVRCP_SUBEVENT_PLAY_STATUS                                     0x05

/**
 * @format 1211
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 * @param play_status
 */
#define AVRCP_SUBEVENT_NOTIFICATION_PLAYBACK_STATUS_CHANGED             0x06

/**
 * @format 121
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 */
#define AVRCP_SUBEVENT_NOTIFICATION_TRACK_CHANGED                       0x07
  
/**
 * @format 121
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 */
#define AVRCP_SUBEVENT_NOTIFICATION_NOW_PLAYING_CONTENT_CHANGED          0x08

/**
 * @format 121
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 */
#define AVRCP_SUBEVENT_NOTIFICATION_AVAILABLE_PLAYERS_CHANGED            0x09

/**
 * @format 1211
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 * @param absolute_volume
 */
#define AVRCP_SUBEVENT_NOTIFICATION_VOLUME_CHANGED                       0x0A

/**
 * @format 1211
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 * @param absolute_volume
 */
#define AVRCP_SUBEVENT_SET_ABSOLUTE_VOLUME_RESPONSE                      0x0B

/**
 * @format 1211
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 * @param notification_id
 */
#define AVRCP_SUBEVENT_ENABLE_NOTIFICATION_COMPLETE                       0x0C

/**
 * @format 1211
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 * @param operation_id
 */
#define AVRCP_SUBEVENT_OPERATION_START                                    0x0D

/**
 * @format 1211
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 * @param operation_id
 */
#define AVRCP_SUBEVENT_OPERATION_COMPLETE                                 0x0E

/**
 * @format 121
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 */
#define AVRCP_SUBEVENT_PLAYER_APPLICATION_VALUE_RESPONSE                   0x0F

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

#endif
