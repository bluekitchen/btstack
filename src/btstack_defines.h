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
 *
 * BTstack definitions, events, and error codes 
 *
 */

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
#define HCI_CON_HANDLE_INVALID  0xffffu


#define DAEMON_EVENT_PACKET     0x05u
    
// L2CAP data
#define L2CAP_DATA_PACKET       0x06u

// RFCOMM data
#define RFCOMM_DATA_PACKET      0x07u

// Attribute protocol data
#define ATT_DATA_PACKET         0x08u

// Security Manager protocol data
#define SM_DATA_PACKET          0x09u

// SDP query result - only used by daemon
// format: type (8), record_id (16), attribute_id (16), attribute_length (16), attribute_value (max 1k)
#define SDP_CLIENT_PACKET       0x0au

// BNEP data
#define BNEP_DATA_PACKET        0x0bu

// Unicast Connectionless Data
#define UCD_DATA_PACKET         0x0cu

// GOEP data
#define GOEP_DATA_PACKET        0x0du

// PBAP data
#define PBAP_DATA_PACKET        0x0eu

// AVRCP browsing data
#define AVRCP_BROWSING_DATA_PACKET     0x0fu

// MAP data
#define MAP_DATA_PACKET        0x10u

// Mesh Provisioning PDU
#define PROVISIONING_DATA_PACKET 0x11u

// Mesh Proxy PDU
#define MESH_PROXY_DATA_PACKET   0x11u

// Mesh Network PDU
#define MESH_NETWORK_PACKET      0x12u

// Mesh Network PDU
#define MESH_BEACON_PACKET       0x13u

// debug log messages
#define LOG_MESSAGE_PACKET      0xfcu


// DAEMON COMMANDS

#define OGF_BTSTACK 0x3du

// cmds for BTstack 
// get state: @return HCI_STATE
#define BTSTACK_GET_STATE                                  0x01u

// set power mode: param HCI_POWER_MODE
#define BTSTACK_SET_POWER_MODE                             0x02u

// set capture mode: param on
#define BTSTACK_SET_ACL_CAPTURE_MODE                       0x03u

// get BTstack version
#define BTSTACK_GET_VERSION                                0x04u

// get system Bluetooth state
#define BTSTACK_GET_SYSTEM_BLUETOOTH_ENABLED               0x05u

// set system Bluetooth state
#define BTSTACK_SET_SYSTEM_BLUETOOTH_ENABLED               0x06u

// enable inquiry scan for this client
#define BTSTACK_SET_DISCOVERABLE                           0x07u

// set global Bluetooth state
#define BTSTACK_SET_BLUETOOTH_ENABLED                      0x08u

// create l2cap channel: param bd_addr(48), psm (16)
#define L2CAP_CREATE_CHANNEL                               0x20u

// disconnect l2cap disconnect, param channel(16), reason(8)
#define L2CAP_DISCONNECT                                   0x21u

// register l2cap service: param psm(16), mtu (16)
#define L2CAP_REGISTER_SERVICE                             0x22u

// unregister l2cap disconnect, param psm(16)
#define L2CAP_UNREGISTER_SERVICE                           0x23u

// accept connection param bd_addr(48), dest cid (16)
#define L2CAP_ACCEPT_CONNECTION                            0x24u

// decline l2cap disconnect,param bd_addr(48), dest cid (16), reason(8)
#define L2CAP_DECLINE_CONNECTION                           0x25u

// create l2cap channel: param bd_addr(48), psm (16), mtu (16)
#define L2CAP_CREATE_CHANNEL_MTU                           0x26u

// request can send now event: l2cap_cid
#define L2CAP_REQUEST_CAN_SEND_NOW                         0x27u

// register SDP Service Record: service record (size)
#define SDP_REGISTER_SERVICE_RECORD                        0x30u

// unregister SDP Service Record
#define SDP_UNREGISTER_SERVICE_RECORD                      0x31u

// Get remote RFCOMM services
#define SDP_CLIENT_QUERY_RFCOMM_SERVICES                   0x32u

// Get remote SDP services
#define SDP_CLIENT_QUERY_SERVICES                          0x33u

// RFCOMM "HCI" Commands
#define RFCOMM_CREATE_CHANNEL                              0x40u
#define RFCOMM_DISCONNECT                                  0x41u
#define RFCOMM_REGISTER_SERVICE                            0x42u
#define RFCOMM_UNREGISTER_SERVICE                          0x43u
#define RFCOMM_ACCEPT_CONNECTION                           0x44u
#define RFCOMM_DECLINE_CONNECTION                          0x45u
#define RFCOMM_CREATE_CHANNEL_WITH_CREDITS                 0x47u
#define RFCOMM_PERSISTENT_CHANNEL                          0x46u
#define RFCOMM_REGISTER_SERVICE_WITH_CREDITS               0x48u
#define RFCOMM_GRANT_CREDITS                               0x49u
// request can send now event: rfcomm_cid
#define RFCOMM_REQUEST_CAN_SEND_NOW                        0x4Au

// GAP Classic 0x50u
#define GAP_DISCONNECT                0x50u
#define GAP_INQUIRY_START             0x51u
#define GAP_INQUIRY_STOP              0x52u
#define GAP_REMOTE_NAME_REQUEST       0x53u
#define GAP_DROP_LINK_KEY_FOR_BD_ADDR 0x54u
#define GAP_DELETE_ALL_LINK_KEYS      0x55u
#define GAP_PIN_CODE_RESPONSE         0x56u
#define GAP_PIN_CODE_NEGATIVE         0x57u

// GAP LE      0x60u
#define GAP_LE_SCAN_START           0x60u
#define GAP_LE_SCAN_STOP            0x61u
#define GAP_LE_CONNECT              0x62u
#define GAP_LE_CONNECT_CANCEL       0x63u
#define GAP_LE_SET_SCAN_PARAMETERS  0x64u

// GATT (Client) 0x70u
#define GATT_DISCOVER_ALL_PRIMARY_SERVICES                       0x70u
#define GATT_DISCOVER_PRIMARY_SERVICES_BY_UUID16                 0x71u
#define GATT_DISCOVER_PRIMARY_SERVICES_BY_UUID128                0x72u
#define GATT_FIND_INCLUDED_SERVICES_FOR_SERVICE                  0x73u
#define GATT_DISCOVER_CHARACTERISTICS_FOR_SERVICE                0x74u
#define GATT_DISCOVER_CHARACTERISTICS_FOR_SERVICE_BY_UUID128     0x75u
#define GATT_DISCOVER_CHARACTERISTIC_DESCRIPTORS                 0x76u
#define GATT_READ_VALUE_OF_CHARACTERISTIC                        0x77u
#define GATT_READ_LONG_VALUE_OF_CHARACTERISTIC                   0x78u
#define GATT_WRITE_VALUE_OF_CHARACTERISTIC_WITHOUT_RESPONSE      0x79u
#define GATT_WRITE_VALUE_OF_CHARACTERISTIC                       0x7Au
#define GATT_WRITE_LONG_VALUE_OF_CHARACTERISTIC                  0x7Bu
#define GATT_RELIABLE_WRITE_LONG_VALUE_OF_CHARACTERISTIC         0x7Cu
#define GATT_READ_CHARACTERISTIC_DESCRIPTOR                      0X7Du
#define GATT_READ_LONG_CHARACTERISTIC_DESCRIPTOR                 0X7Eu
#define GATT_WRITE_CHARACTERISTIC_DESCRIPTOR                     0X7Fu
#define GATT_WRITE_LONG_CHARACTERISTIC_DESCRIPTOR                0X80u
#define GATT_WRITE_CLIENT_CHARACTERISTIC_CONFIGURATION           0X81u
#define GATT_GET_MTU                                             0x82u

// SM 0x90u
#define SM_SET_AUTHENTICATION_REQUIREMENTS 0x90u
#define SM_SET_IO_CAPABILITIES             0x92u
#define SM_BONDING_DECLINE                 0x93u
#define SM_JUST_WORKS_CONFIRM              0x94u
#define SM_NUMERIC_COMPARISON_CONFIRM      0x95u
#define SM_PASSKEY_INPUT                   0x96u

// ATT

// ..
// Internal properties reuse some GATT Characteristic Properties fields
#define ATT_DB_VERSION                                     0x01u

// EVENTS

// Events from host controller to host

/**
 * @brief Custom NOP Event - used for internal testing
 */
#define HCI_EVENT_NOP                                      0x00u

/**
 * @format 1
 * @param status
 */
#define HCI_EVENT_INQUIRY_COMPLETE                         0x01u

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
#define HCI_EVENT_INQUIRY_RESULT                           0x02u

/**
 * @format 12B11
 * @param status
 * @param connection_handle
 * @param bd_addr
 * @param link_type
 * @param encryption_enabled
 */
#define HCI_EVENT_CONNECTION_COMPLETE                      0x03u
/**
 * @format B31
 * @param bd_addr
 * @param class_of_device
 * @param link_type
 */
#define HCI_EVENT_CONNECTION_REQUEST                       0x04u
/**
 * @format 121
 * @param status
 * @param connection_handle
 * @param reason 
 */
#define HCI_EVENT_DISCONNECTION_COMPLETE                   0x05u
/**
 * @format 12
 * @param status
 * @param connection_handle
 */
#define HCI_EVENT_AUTHENTICATION_COMPLETE                 0x06u

// HCI_EVENT_AUTHENTICATION_COMPLETE_EVENT is deprecated, use HCI_EVENT_AUTHENTICATION_COMPLETE instead
#define HCI_EVENT_AUTHENTICATION_COMPLETE_EVENT HCI_EVENT_AUTHENTICATION_COMPLETE

/**
 * @format 1BN
 * @param status
 * @param bd_addr
 * @param remote_name
 */
#define HCI_EVENT_REMOTE_NAME_REQUEST_COMPLETE             0x07u

/**
 * @format 121
 * @param status
 * @param connection_handle
 * @param encryption_enabled 
 */
#define HCI_EVENT_ENCRYPTION_CHANGE                        0x08u

/**
 * @format 12
 * @param status
 * @param connection_handle
 */
#define HCI_EVENT_CHANGE_CONNECTION_LINK_KEY_COMPLETE      0x09u

/**
 * @format 121
 * @param status
 * @param connection_handle
 * @param key_flag 
 */
#define HCI_EVENT_MASTER_LINK_KEY_COMPLETE                 0x0Au

#define HCI_EVENT_READ_REMOTE_SUPPORTED_FEATURES_COMPLETE  0x0Bu

/**
 * @format 12122
 * @param status
 * @param connection_handle
 * @param version
 * @param manufacturer_name
 * @param subversion
 */
#define HCI_EVENT_READ_REMOTE_VERSION_INFORMATION_COMPLETE 0x0Cu

#define HCI_EVENT_QOS_SETUP_COMPLETE                       0x0Du

/**
 * @format 12R
 * @param num_hci_command_packets
 * @param command_opcode
 * @param return_parameters
 */
#define HCI_EVENT_COMMAND_COMPLETE                         0x0Eu
/**
 * @format 112
 * @param status
 * @param num_hci_command_packets
 * @param command_opcode
 */
#define HCI_EVENT_COMMAND_STATUS                           0x0Fu

/**
 * @format 1
 * @param hardware_code
 */
#define HCI_EVENT_HARDWARE_ERROR                           0x10u

/**
 * @format H
 * @param handle
 */
#define HCI_EVENT_FLUSH_OCCURRED                           0x11u

/**
 * @format 1B1
 * @param status
 * @param bd_addr
 * @param role
 */
#define HCI_EVENT_ROLE_CHANGE                              0x12u

// TODO: number_of_handles 1, connection_handle[H*i], hc_num_of_completed_packets[2*i]
#define HCI_EVENT_NUMBER_OF_COMPLETED_PACKETS              0x13u

/**
 * @format 1H12
 * @param status
 * @param handle
 * @param mode
 * @param interval
 */
#define HCI_EVENT_MODE_CHANGE                              0x14u

// TODO: num_keys, bd_addr[B*i], link_key[16 octets * i]
#define HCI_EVENT_RETURN_LINK_KEYS                         0x15u

/**
 * @format B
 * @param bd_addr
 */
#define HCI_EVENT_PIN_CODE_REQUEST                         0x16u

/**
 * @format B
 * @param bd_addr
 */
#define HCI_EVENT_LINK_KEY_REQUEST                         0x17u

// TODO: bd_addr B, link_key 16octets, key_type 1
#define HCI_EVENT_LINK_KEY_NOTIFICATION                    0x18u

// event params contains HCI ccommand
#define HCI_EVENT_LOOPBACK_COMMAND                         0x19u

/**
 * @format 1
 * @param link_type
 */
#define HCI_EVENT_DATA_BUFFER_OVERFLOW                     0x1Au

/**
 * @format H1
 * @param handle
 * @param lmp_max_slots
 */
#define HCI_EVENT_MAX_SLOTS_CHANGED                        0x1Bu

/**
 * @format 1H2
 * @param status
 * @param handle
 * @param clock_offset
 */
#define HCI_EVENT_READ_CLOCK_OFFSET_COMPLETE               0x1Cu

/**
 * @format 1H2
 * @param status
 * @param handle
 * @param packet_types
 * @pnote packet_type is in plural to avoid clash with Java binding Packet.getPacketType()
 */
#define HCI_EVENT_CONNECTION_PACKET_TYPE_CHANGED           0x1Du

/**
 * @format H
 * @param handle
 */
#define HCI_EVENT_QOS_VIOLATION                            0x1Eu

// 0x1f not defined

/**
 * @format H1
 * @param handle
 * @param page_scan_repetition_mode
 */
#define HCI_EVENT_PAGE_SCAN_REPETITION_MODE_CHANGE         0x20u

/**
 * @format 1H1114444
 * @param status
 * @param handle
 * @param unused
 * @param flow_direction
 * @param service_type
 * @param token_rate
 * @param token_bucket_size
 * @param peak_bandwidth
 * @param access_latency
 *
 */
#define HCI_EVENT_FLOW_SPECIFICATION_COMPLETE              0x21u

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
#define HCI_EVENT_INQUIRY_RESULT_WITH_RSSI                 0x22u

#define HCI_EVENT_READ_REMOTE_EXTENDED_FEATURES_COMPLETE   0x23u

// 0x24..0x2b not defined

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
#define HCI_EVENT_SYNCHRONOUS_CONNECTION_COMPLETE          0x2Cu

/**
 * @format 1H1122
 * @param status
 * @param handle
 * @param transmission_interval
 * @param retransmission_interval
 * @param rx_packet_length
 * @param tx_packet_length
 */
#define HCI_EVENT_SYNCHRONOUS_CONNECTION_CHANGED          0x2Du

/**
 * @format 1H2222
 * @param status
 * @param handle
 * @param max_tx_latency
 * @param max_rx_latency
 * @param min_remote_timeout
 * @param min_local_timeout
 */
#define HCI_EVENT_SNIFF_SUBRATING                         0x2Eu

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
#define HCI_EVENT_EXTENDED_INQUIRY_RESPONSE                0x2Fu

 /** 
  * @format 1H
  * @param status
  * @param handle
  */
#define HCI_EVENT_ENCRYPTION_KEY_REFRESH_COMPLETE          0x30u

 /**
  * @format B
  * @param bd_addr
  */
#define HCI_EVENT_IO_CAPABILITY_REQUEST                    0x31u

/**
 * @format B111
 * @param bd_addr
 * @param io_capability
 * @param oob_data_present
 * @param authentication_requirements
 */
#define HCI_EVENT_IO_CAPABILITY_RESPONSE                   0x32u

/**
 * @format B4
 * @param bd_addr
 * @param numeric_value
 */
#define HCI_EVENT_USER_CONFIRMATION_REQUEST                0x33u

/**
 * @format B
 * @param bd_addr
 */
#define HCI_EVENT_USER_PASSKEY_REQUEST                     0x34u

/**
 * @format B
 * @param bd_addr
 */
#define HCI_EVENT_REMOTE_OOB_DATA_REQUEST                  0x35u

/**
 * @format 1B
 * @param status
 * @param bd_addr
 */
#define HCI_EVENT_SIMPLE_PAIRING_COMPLETE                  0x36u

/**
 * @format H2
 * @param handle
 * @param link_supervision_timeout
 */
#define HCI_EVENT_LINK_SUPERVISION_TIMEOUT_CHANGED         0x38u

/**
 * @format H
 * @param handle
 */
#define HCI_EVENT_ENHANCED_FLUSH_COMPLETE                  0x39u

// 0x03a not defined

/**
 * @format B4
 * @param bd_addr
 * @param numeric_value
 */
#define HCI_EVENT_USER_PASSKEY_NOTIFICATION                0x3Bu

/**
 * @format B1
 * @param bd_addr
 * @param notification_type
 */
#define HCI_EVENT_KEYPRESS_NOTIFICATION                    0x3Cu

#define HCI_EVENT_REMOTE_HOST_SUPPORTED_FEATURES           0x3Du

#define HCI_EVENT_LE_META                                  0x3Eu

// 0x3f..0x47 not defined

#define HCI_EVENT_NUMBER_OF_COMPLETED_DATA_BLOCKS          0x48u

// 0x49..0x58 not defined

/**
 * @format 1211
 * @param status
 * @param connection_handle
 * @param encryption_enabled
 * @param encryption_key_size
 */
#define HCI_EVENT_ENCRYPTION_CHANGE_V2                     0x59u

// last used HCI_EVENT in 5.3 is 0x59u

#define HCI_EVENT_VENDOR_SPECIFIC                          0xFFu

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
#define HCI_SUBEVENT_LE_CONNECTION_COMPLETE                0x01u

// array of advertisements, not handled by event accessor generator
#define HCI_SUBEVENT_LE_ADVERTISING_REPORT                 0x02u

/**
 * @format 11H222
 * @param subevent_code
 * @param status
 * @param connection_handle
 * @param conn_interval
 * @param conn_latency
 * @param supervision_timeout
 */
 #define HCI_SUBEVENT_LE_CONNECTION_UPDATE_COMPLETE         0x03u

/**
 * @format 1HD
 * @param subevent_code
 * @param connection_handle
 * @param le_features
 */
#define HCI_SUBEVENT_LE_READ_REMOTE_FEATURES_COMPLETE 0x04u

/**
 * @format 1HD2
 * @param subevent_code
 * @param connection_handle
 * @param random_number
 * @param encryption_diversifier
 */
#define HCI_SUBEVENT_LE_LONG_TERM_KEY_REQUEST              0x05u

/**
 * @format 1H2222
 * @param subevent_code
 * @param connection_handle
 * @param interval_min
 * @param interval_max
 * @param latency
 * @param timeout
 */
#define HCI_SUBEVENT_LE_REMOTE_CONNECTION_PARAMETER_REQUEST 0x06u

/**
 * @format 1H2222
 * @param subevent_code
 * @param connection_handle
 * @param max_tx_octets
 * @param max_tx_time
 * @param max_rx_octets
 * @param max_rx_time
 */
#define HCI_SUBEVENT_LE_DATA_LENGTH_CHANGE 0x07u

/**
 * @format 11QQ
 * @param subevent_code
 * @param status
 * @param dhkey_x x coordinate of P256 public key
 * @param dhkey_y y coordinate of P256 public key
 */
#define HCI_SUBEVENT_LE_READ_LOCAL_P256_PUBLIC_KEY_COMPLETE 0x08u

 /**
 * @format 11Q
 * @param subevent_code
 * @param status
 * @param dhkey Diffie-Hellman key
 */
#define HCI_SUBEVENT_LE_GENERATE_DHKEY_COMPLETE            0x09u

/**
 * @format 11H11BBB2221
 * @param subevent_code
 * @param status
 * @param connection_handle
 * @param role
 * @param peer_address_type
 * @param peer_addresss
 * @param local_resolvable_private_addres
 * @param peer_resolvable_private_addres
 * @param conn_interval
 * @param conn_latency
 * @param supervision_timeout
 * @param master_clock_accuracy
 */
#define HCI_SUBEVENT_LE_ENHANCED_CONNECTION_COMPLETE       0x0Au

// array of advertisements, not handled by event accessor generator
#define HCI_SUBEVENT_LE_DIRECT_ADVERTISING_REPORT          0x0Bu

/**
 * @format 11H1
 * @param subevent_code
 * @param status
 * @param connection_handle
 * @param tx_phy
 */
#define HCI_SUBEVENT_LE_PHY_UPDATE_COMPLETE                0x0Cu

// array of advertisements, not handled by event accessor generator
#define HCI_SUBEVENT_LE_EXTENDED_ADVERTISING_REPORT        0x0Du

/**
 * @format 11H11B121
 * @param subevent_code
 * @param status
 * @param sync_handle
 * @param advertising_sid
 * @param advertiser_address_type
 * @param advertiser_address
 * @param advertiser_phy
 * @param periodic_advertising_interval
 * @param advertiser_clock_accuracy
 */
#define HCI_SUBEVENT_LE_PERIODIC_ADVERTISING_SYNC_ESTABLISHMENT 0x0Eu

/**
 * @format 1H1111JV
 * @param subevent_code
 * @param sync_handle
 * @param tx_power
 * @param rssi
 * @param cte_type
 * @param data_status
 * @param data_length
 * @param data
*/
#define HCI_SUBEVENT_LE_PERIODIC_ADVERTISING_REPORT             0x0Fu

/**
 * @format 1H
 * @param subevent_code
 * @param sync_handle
 */
#define HCI_SUBEVENT_LE_PERIODIC_ADVERTISING_SYNC_LOST          0x10u

/**
 * @format 1
 * @param subevent_code
 */
#define HCI_SUBEVENT_LE_SCAN_TIMEOUT                            0x11u

/**
 * @format 111H1
 * @param subevent_code
 * @param status
 * @param advertising_handle
 * @param connection_handle
 * @param num_completed_exteneded_advertising_events
 */
#define HCI_SUBEVENT_LE_ADVERTISING_SET_TERMINATED              0x12u

/**
 * @format 111B
 * @param subevent_code
 * @param advertising_handle
 * @param scanner_address_type
 * @param scanner_address
 */
#define HCI_SUBEVENT_LE_SCAN_REQUEST_RECEIVED                   0x13u

/**
 * @format 1H1
 * @param subevent_code
 * @param connection_handle
 * @param channel_selection_algorithm
 */
#define HCI_SUBEVENT_LE_CHANNEL_SELECTION_ALGORITHM             0x14u

// array of advertisements, not handled by event accessor generator
#define HCI_SUBEVENT_LE_CONNECTIONLESS_IQ_REPORT                0x15u

// array of advertisements, not handled by event accessor generator
#define HCI_SUBEVENT_LE_CONNECTION_IQ_REPORT                    0x16u

/**
 * @format 11H
 * @param subevent_code
 * @param status
 * @param connection_handle
 */
#define HCI_SUBEVENT_LE_LE_CTE_REQUEST_FAILED                   0x17u

/**
 * @format 11H2H11B121
 * @param subevent_code
 * @param status
 * @param connection_handle
 * @param service_data
 * @param sync_handle
 * @param advertising_sid
 * @param advertiser_address_type
 * @param advertiser_address
 * @param advertiser_phy
 * @param periodic_advertising_interval
 * @param advertiser_clock_accuracy
 */
#define HCI_SUBEVENT_LE_PERIODIC_ADVERTISING_SYNC_TRANSFER_RECEIVED 0x18u

/**
 * @format 11H33331111111222
 * @param subevent_code
 * @param status
 * @param connection_handle
 * @param cig_sync_delay
 * @param cis_sync_delay
 * @param transport_latency_c_to_p
 * @param transport_latency_p_to_c
 * @param phy_c_to_p
 * @param phy_p_to_c
 * @param nse
 * @param bn_c_to_p
 * @param bn_p_to_c
 * @param ft_c_to_p
 * @param ft_p_to_c
 * @param max_pdu_c_to_p
 * @param max_pdu_p_to_c
 * @param iso_interval
 */
#define HCI_SUBEVENT_LE_CIS_ESTABLISHED                          0x19u

/**
 * @format 1HH11
 * @param subevent_code
 * @param acl_connection_handle
 * @param cis_connection_handle
 * @param cig_id
 * @param cis_id
 */
#define HCI_SUBEVENT_LE_CIS_REQUEST                            0x1au

// array of advertisements, not handled by event accessor generator
#define HCI_SUBEVENT_LE_CREATE_BIG_COMPLETE                     0x1Bu

/**
 * @format 111
 * @param subevent_code
 * @param big_handle
 * @param reason
 */
#define HCI_SUBEVENT_LE_TERMINATE_BIG_COMPLETE                   0x1Cu

// array of advertisements, not handled by event accessor generator
#define HCI_SUBEVENT_LE_BIG_SYNC_ESTABLISHED                     0x1Du

/**
 * @format 111
 * @param subevent_code
 * @param big_handle
 * @param reason
 */
#define HCI_SUBEVENT_LE_BIG_SYNC_LOST                            0x1Eu

/**
 * @format 11H1
 * @param subevent_code
 * @param status
 * @param connection_handle
 * @param peer_clock_accuracy
 */
#define HCI_SUBEVENT_LE_REQUEST_PEER_SCA_COMPLETE                0x1Fu

/**
 * @format 11H11111
 * @param subevent_code
 * @param status
 * @param connection_handle
 * @param reason
 * @param phy
 * @param tx_power_level
 * @param tx_power_level_flag
 * @param delta
 */
#define HCI_SUBEVENT_LE_TRANSMIT_POWER_REPORTING                 0x21u

/**
 * @format 1H112111232111
 * @param subevent_code
 * @param sync_handle
 * @param num_bis
 * @param nse
 * @param iso_interval
 * @param bn
 * @param pto
 * @param irc
 * @param max_pdu
 * @param sdu_interval
 * @param max_sdu
 * @param phy
 * @param framing
 * @param encryption
 */
#define HCI_SUBEVENT_LE_BIGINFO_ADVERTISING_REPORT                0x22u

/**
 * @format 11H2222
 * @param subevent_code
 * @param status
 * @param connection_handle
 * @param subrate_factor
 * @param peripheral_latency
 * @param continuation_number
 * @param supervision_timeout
 */
#define HCI_SUBEVENT_LE_SUBRATE_CHANGE                            0x23u

/**
 * @format 1
 * @param state
 */
#define BTSTACK_EVENT_STATE                                0x60u

/**
 * @format 1
 * @param number_connections
 */
#define BTSTACK_EVENT_NR_CONNECTIONS_CHANGED               0x61u

/**
 * @format 
 */
#define BTSTACK_EVENT_POWERON_FAILED                       0x62u

/**
 * @format 11
 * @param discoverable
 * @param connectable
 */
#define BTSTACK_EVENT_SCAN_MODE_CHANGED                    0x66u

// Daemon Events

/**
 * @format 112
 * @param major
 * @param minor
 @ @param revision
 */
#define DAEMON_EVENT_VERSION                               0x63u

// data: system bluetooth on/off (bool)
/**
 * @format 1
 * param system_bluetooth_enabled
 */
#define DAEMON_EVENT_SYSTEM_BLUETOOTH_ENABLED              0x64u

// data: event (8), len(8), status (8) == 0, address (48), name (1984 bits = 248 bytes)

/* 
 * @format 1BT
 * @param status == 0 to match read_remote_name_request
 * @param address
 * @param name
 */
#define DAEMON_EVENT_REMOTE_NAME_CACHED                    0x65u

// internal - data: event(8)
#define DAEMON_EVENT_CONNECTION_OPENED                     0x67u

// internal - data: event(8)
#define DAEMON_EVENT_CONNECTION_CLOSED                     0x68u

// data: event(8), len(8), local_cid(16), credits(8)
#define DAEMON_EVENT_L2CAP_CREDITS                         0x74u

/**
 * @format 12
 * @param status
 * @param psm
 */
#define DAEMON_EVENT_L2CAP_SERVICE_REGISTERED              0x75u

/**
 * @format 11
 * @param status
 * @param channel_id
 */
#define DAEMON_EVENT_RFCOMM_SERVICE_REGISTERED             0x85u

/**
 * @format 11
 * @param status
 * @param server_channel_id
 */
#define DAEMON_EVENT_RFCOMM_PERSISTENT_CHANNEL             0x86u

/**
  * @format 14
  * @param status
  * @param service_record_handle
  */
#define DAEMON_EVENT_SDP_SERVICE_REGISTERED                0x90u



// additional HCI events

/**
 * @brief Indicates HCI transport enters/exits Sleep mode
 * @format 1
 * @param active
 */
#define HCI_EVENT_TRANSPORT_SLEEP_MODE                     0x69u

/**
 * @brief Transport USB Bluetooth Controller info
 * @format 22JV
 * @param vendor_id
 * @param product_id
 * @param path_len
 * @param path
 */
#define HCI_EVENT_TRANSPORT_USB_INFO                       0x6Au

/**
 * @brief Transport ready 
 */
#define HCI_EVENT_TRANSPORT_READY                          0x6Du

/**
 * @brief Outgoing packet 
 */
#define HCI_EVENT_TRANSPORT_PACKET_SENT                    0x6Eu

/**
 * @format 11H
 * @param big_handle
 * @param bis_index
 * @param con_handle
 */
#define HCI_EVENT_BIS_CAN_SEND_NOW                         0x6Bu

/**
 * @format H
 * @param cis_con_handle
 */
#define HCI_EVENT_CIS_CAN_SEND_NOW                         0x6Cu

/**
 * @format
 */
#define HCI_EVENT_SCO_CAN_SEND_NOW                         0x6Fu


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
#define L2CAP_EVENT_CHANNEL_OPENED                         0x70u

/*
 * @format 2
 * @param local_cid
 */
#define L2CAP_EVENT_CHANNEL_CLOSED                         0x71u

/**
 * @format BH222
 * @param address
 * @param handle
 * @param psm
 * @param local_cid
 * @param remote_cid
 */
#define L2CAP_EVENT_INCOMING_CONNECTION                    0x72u

// ??
// data: event(8), len(8), handle(16)
#define L2CAP_EVENT_TIMEOUT_CHECK                          0x73u

/**
 * @format H2222
 * @param handle
 * @param interval_min
 * @param interval_max
 * @param latency
 * @param timeout_multiplier
 */
#define L2CAP_EVENT_CONNECTION_PARAMETER_UPDATE_REQUEST    0x76u

// data: event(8), len(8), handle(16), result (16) (0 == ok, 1 == fail)
 /** 
  * @format H2
  * @param handle
  * @param result
  */
#define L2CAP_EVENT_CONNECTION_PARAMETER_UPDATE_RESPONSE   0x77u

 /*
 * @format H22
 * @param con_handle
 * @param extended_feature_mask
 * @param fixed_channels_supported
 */
#define L2CAP_EVENT_INFORMATION_RESPONSE                   0x78u

/**
 * @format 2
 * @param local_cid
 */
#define L2CAP_EVENT_CAN_SEND_NOW                           0x79u

/*
 * @format 2
 * @param local_cid
 */
#define L2CAP_EVENT_PACKET_SENT                            0x7au

/*
 * @format 2
 * @param local_cid
 */
#define L2CAP_EVENT_ERTM_BUFFER_RELEASED                   0x7bu

// L2CAP Channel in LE Credit-based Flow-Control Mode (CBM)

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
#define L2CAP_EVENT_CBM_INCOMING_CONNECTION                 0x7cu

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
#define L2CAP_EVENT_CBM_CHANNEL_OPENED                      0x7du

/*
 * @format
 */
#define L2CAP_EVENT_TRIGGER_RUN                             0x7eu

/**
 * @format 1BH212
 * @param address_type
 * @param address
 * @param handle
 * @param psm
 * @param num_channels
 * @param local_cid first new cid
 */
#define L2CAP_EVENT_ECBM_INCOMING_CONNECTION               0x7fu

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
#define L2CAP_EVENT_ECBM_CHANNEL_OPENED              0x8au

/*
 * @format 222
 * @param remote_cid
 * @param mtu
 * @param mps
 */
#define L2CAP_EVENT_ECBM_RECONFIGURED                0x8bu

/*
 * @format 22
 * @param local_cid
 * @param reconfigure_result
 */
#define L2CAP_EVENT_ECBM_RECONFIGURATION_COMPLETE    0x8cu


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
#define RFCOMM_EVENT_CHANNEL_OPENED                        0x80u

/**
 * @format 2
 * @param rfcomm_cid
 */
#define RFCOMM_EVENT_CHANNEL_CLOSED                        0x81u

/**
 * @format B12H
 * @param bd_addr
 * @param server_channel
 * @param rfcomm_cid
 * @param con_handle

 */
#define RFCOMM_EVENT_INCOMING_CONNECTION                   0x82u

/**
 * @format 21
 * @param rfcomm_cid
 * @param line_status
 */
#define RFCOMM_EVENT_REMOTE_LINE_STATUS                    0x83u
        
/**
 * @format 21
 * @param rfcomm_cid
 * @param modem_status
 */
#define RFCOMM_EVENT_REMOTE_MODEM_STATUS                   0x87u

/**
 * note: port configuration not parsed by stack, getters provided by rfcomm.h
 * param rfcomm_cid
 * param remote - 0 for local port, 1 for remote port
 * param baud_rate
 * param data_bits
 * param stop_bits
 * param parity
 * param flow_control
 * param xon
 * param xoff
 */
#define RFCOMM_EVENT_PORT_CONFIGURATION                    0x88u

/**
 * @format 2
 * @param rfcomm_cid
 */
#define RFCOMM_EVENT_CAN_SEND_NOW                          0x89u


/**
 * @format 1
 * @param status
 */
#define SDP_EVENT_QUERY_COMPLETE                                 0x91u 

/**
 * @format 1T
 * @param rfcomm_channel
 * @param name
 */
#define SDP_EVENT_QUERY_RFCOMM_SERVICE                           0x92u

/**
 * @format 22221
 * @param record_id
 * @param attribute_id
 * @param attribute_length
 * @param data_offset
 * @param data
 */
#define SDP_EVENT_QUERY_ATTRIBUTE_BYTE                           0x93u

/**
 * @format 22LV
 * @param record_id
 * @param attribute_id
 * @param attribute_length
 * @param attribute_value
 */
#define SDP_EVENT_QUERY_ATTRIBUTE_VALUE                          0x94u

/**
 * @format 224
 * @param total_count
 * @param record_index
 * @param record_handle
 * @note Not provided by daemon, only used for internal testing
 */
#define SDP_EVENT_QUERY_SERVICE_RECORD_HANDLE                    0x95u

/**
 * @format H1
 * @param handle
 * @param att_status  see ATT errors in bluetooth.h  
 */
#define GATT_EVENT_QUERY_COMPLETE                                0xA0u

/**
 * @format HX
 * @param handle
 * @param service
 */
#define GATT_EVENT_SERVICE_QUERY_RESULT                          0xA1u

/**
 * @format HY
 * @param handle
 * @param characteristic
 */
#define GATT_EVENT_CHARACTERISTIC_QUERY_RESULT                   0xA2u

/**
 * @format H2X
 * @param handle
 * @param include_handle
 * @param service
 */
#define GATT_EVENT_INCLUDED_SERVICE_QUERY_RESULT                 0xA3u

/**
 * @format HZ
 * @param handle
 * @param characteristic_descriptor
 */
#define GATT_EVENT_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_RESULT   0xA4u

/**
 * @format H2LV
 * @param handle
 * @param value_handle
 * @param value_length
 * @param value
 */
#define GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT             0xA5u

/**
 * @format H22LV
 * @param handle
 * @param value_handle
 * @param value_offset
 * @param value_length
 * @param value
 */
#define GATT_EVENT_LONG_CHARACTERISTIC_VALUE_QUERY_RESULT        0xA6u

/**
 * @format H2LV
 * @param handle
 * @param value_handle
 * @param value_length
 * @param value
 */
#define GATT_EVENT_NOTIFICATION                                  0xA7u

/**
 * @format H2LV
 * @param handle
 * @param value_handle
 * @param value_length
 * @param value
 */
#define GATT_EVENT_INDICATION                                    0xA8u

/**
 * @format H2LV
 * @param handle
 * @param descriptor_handle
 * @param descriptor_length
 * @param descriptor
 */
#define GATT_EVENT_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT        0xA9u

/**
 * @format H2LV
 * @param handle
 * @param descriptor_offset
 * @param descriptor_length
 * @param descriptor
 */
#define GATT_EVENT_LONG_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT   0xAAu

/** 
 * @format H2
 * @param handle
 * @param MTU
 */    
#define GATT_EVENT_MTU                                           0xABu

/**
 * @format H
 * @param handle
 */
#define GATT_EVENT_CAN_WRITE_WITHOUT_RESPONSE                    0xACu


/** 
 * @format 1BH
 * @param address_type
 * @param address
 * @param handle
 */    
#define ATT_EVENT_CONNECTED                                      0xB3u

/** 
 * @format H
 * @param handle
 */    
#define ATT_EVENT_DISCONNECTED                                   0xB4u

/** 
 * @format H2
 * @param handle
 * @param MTU
 */    
#define ATT_EVENT_MTU_EXCHANGE_COMPLETE                          0xB5u

 /**
  * @format 1H2
  * @param status
  * @param conn_handle
  * @param attribute_handle
  */
#define ATT_EVENT_HANDLE_VALUE_INDICATION_COMPLETE               0xB6u

/**
 * @format
 */
#define ATT_EVENT_CAN_SEND_NOW                                   0xB7u

// TODO: daemon only event

/**
 * @format 12
 * @param status
 * @param service_uuid
 */
 #define BNEP_EVENT_SERVICE_REGISTERED                           0xC0u

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
 #define BNEP_EVENT_CHANNEL_OPENED                               0xC1u

/**
 * @format 222B
 * @param bnep_cid
 * @param source_uuid
 * @param destination_uuid
 * @param remote_address
 */
 #define BNEP_EVENT_CHANNEL_CLOSED                               0xC2u

/**
 * @format 222B1
 * @param bnep_cid
 * @param source_uuid
 * @param destination_uuid
 * @param remote_address
 * @param channel_state
 */
#define BNEP_EVENT_CHANNEL_TIMEOUT                               0xC3u    
    
/**
 * @format 222B
 * @param bnep_cid
 * @param source_uuid
 * @param destination_uuid
 * @param remote_address
 */
 #define BNEP_EVENT_CAN_SEND_NOW                                 0xC4u

 /**
  * @format H1B1
  * @param handle
  * @param addr_type
  * @param address
  * @param secure_connection - set to 1 if LE Secure Connection pairing will be used
  */
#define SM_EVENT_JUST_WORKS_REQUEST                              0xC8u

 /**
  * @format H1B14
  * @param handle
  * @param addr_type
  * @param address
  * @param secure_connection - set to 1 if LE Secure Connection pairing will be used
  * @param passkey
  */
#define SM_EVENT_PASSKEY_DISPLAY_NUMBER                          0xC9u

 /**
  * @format H1B
  * @param handle
  * @param addr_type
  * @param address
  */
#define SM_EVENT_PASSKEY_DISPLAY_CANCEL                          0xCAu

 /**
  * @format H1B1
  * @param handle
  * @param addr_type
  * @param address
  * @param secure_connection - set to 1 if LE Secure Connection pairing will be used
  */
#define SM_EVENT_PASSKEY_INPUT_NUMBER                            0xCBu

 /**
  * @format H1B14
  * @param handle
  * @param addr_type
  * @param address
  * @param secure_connection - set to 1 if LE Secure Connection pairing will be used
  * @param passkey
  */
#define SM_EVENT_NUMERIC_COMPARISON_REQUEST                      0xCCu

 /**
  * @format H1B
  * @param handle
  * @param addr_type
  * @param address
  */
#define SM_EVENT_IDENTITY_RESOLVING_STARTED                      0xCDu

 /**
  * @format H1B
  * @param handle
  * @param addr_type
  * @param address
  */
#define SM_EVENT_IDENTITY_RESOLVING_FAILED                       0xCEu

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
#define SM_EVENT_IDENTITY_RESOLVING_SUCCEEDED                    0xCFu

 /**
  * @format H1B
  * @param handle
  * @param addr_type
  * @param address
  */
#define SM_EVENT_AUTHORIZATION_REQUEST                           0xD0u

 /**
  * @format H1B1
  * @param handle
  * @param addr_type
  * @param address
  * @param authorization_result
  */
#define SM_EVENT_AUTHORIZATION_RESULT                            0xD1u

 /**
  * @format H1
  * @param handle
  * @param action see SM_KEYPRESS_*
  */
#define SM_EVENT_KEYPRESS_NOTIFICATION                           0xD2u

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
#define SM_EVENT_IDENTITY_CREATED                                0xD3u

/**
 * @brief Emitted to inform app that pairing has started.
 * @format H1B
 * @param handle
 * @param addr_type
 * @param address
 */
#define SM_EVENT_PAIRING_STARTED                                 0xD4u

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
#define SM_EVENT_PAIRING_COMPLETE                                0xD5u


/**
 * @brief Proactive Authentication for bonded devices started.
 * @format H1B
 * @param handle
 * @param addr_type
 * @param address
 */
#define SM_EVENT_REENCRYPTION_STARTED                            0xD6u

/**
 * @brief Proactive Authentication for bonded devices complete. Possible status values:
 *         ERROR_CODE_SUCCESS                           -> connection secure
 *         ERROR_CODE_CONNECTION_TIMEOUT                -> timeout
 *         ERROR_CODE_PIN_OR_KEY_MISSING                -> remote did not provide (as Peripheral) or use LTK (as Central)
 * @format H1B1
 * @param handle
 * @param addr_type
 * @param address
 * @param status
 */
#define SM_EVENT_REENCRYPTION_COMPLETE                           0xD7u

// GAP

/**
 * @format H1
 * @param handle
 * @param security_level
 */
#define GAP_EVENT_SECURITY_LEVEL                                 0xD8u

/**
 * @format 1B
 * @param status
 * @param address
 */
#define GAP_EVENT_DEDICATED_BONDING_COMPLETED                    0xD9u

/**
 * @format 11B1JV
 * @param advertising_event_type
 * @param address_type
 * @param address
 * @param rssi
 * @param data_length
 * @param data
 */
#define GAP_EVENT_ADVERTISING_REPORT                             0xDAu

/**
 * @format 21B1111121BJV
 * @param advertising_event_type
 * @param address_type
 * @param address
 * @param primary_phy
 * @param secondary_phy
 * @param advertising_sid
 * @param tx_power
 * @param rssi
 * @param periodic_advertising_interval
 * @param direct_address_type
 * @param direct_address
 * @param data_length
 * @param data
 */
#define GAP_EVENT_EXTENDED_ADVERTISING_REPORT                    0xDBu

 /**
 * @format B13211122221JV
 * @param bd_addr
 * @param page_scan_repetition_mode
 * @param class_of_device
 * @param clock_offset
 * @param rssi_available
 * @param rssi
 * @param device_id_available
 * @param device_id_vendor_id_source
 * @param device_id_vendor_id
 * @param device_id_product_id
 * @param device_id_version
 * @param name_available
 * @param name_len
 * @param name
 */
#define GAP_EVENT_INQUIRY_RESULT                                 0xDCu

/**
 * @format 1
 * @param status
 */
#define GAP_EVENT_INQUIRY_COMPLETE                               0xDDu

/**
 * @format H1
 * @param con_handle
 * @param rssi (signed integer -127..127)
 * @note Classic: rssi is in dB relative to Golden Receive Power Range
 * @note LE: rssi is absolute dBm
 */
#define GAP_EVENT_RSSI_MEASUREMENT                               0xDEu

/**
 * @format 1KKKK
 * @param oob_data_present 0 = none, 1 = p_192, 2 = p_256, 3 = both
 * @param c_192 Simple Pairing Hash C derived from P-192 public key
 * @param r_192 Simple Pairing Randomizer derived from P-192 public key
 * @param c_256 Simple Pairing Hash C derived from P-256 public key
 * @param r_256 Simple Pairing Randomizer derived from P-256 public key
 */
#define GAP_EVENT_LOCAL_OOB_DATA                                 0xDFu


/**
 * @format HB11
 * @param con_handle
 * @param bd_addr
 * @param ssp
 * @param initiator
 */
#define GAP_EVENT_PAIRING_STARTED                                0xE0u

/**
 * @format HB1
 * @param con_handle
 * @param bd_addr
 * @param status
 */
#define GAP_EVENT_PAIRING_COMPLETE                               0xE1u

// Meta Events, see below for sub events
#define HCI_EVENT_META_GAP                                       0xE7u
#define HCI_EVENT_HSP_META                                       0xE8u
#define HCI_EVENT_HFP_META                                       0xE9u
#define HCI_EVENT_ANCS_META                                      0xEAu
#define HCI_EVENT_AVDTP_META                                     0xEBu
#define HCI_EVENT_AVRCP_META                                     0xECu
#define HCI_EVENT_GOEP_META                                      0xEDu
#define HCI_EVENT_PBAP_META                                      0xEEu
#define HCI_EVENT_HID_META                                       0xEFu
#define HCI_EVENT_A2DP_META                                      0xF0u
#define HCI_EVENT_HIDS_META                                      0xF1u
#define HCI_EVENT_GATTSERVICE_META                               0xF2u
#define HCI_EVENT_BIP_META                                       0xF3u
#define HCI_EVENT_MAP_META                                       0xF4u
#define HCI_EVENT_MESH_META                                      0xF5u

// Potential other meta groups
// #define HCI_EVENT_BNEP_META                                0xxx
// #define HCI_EVENT_GAP_META                                 0xxx
// #define HCI_EVENT_GATT_META                                0xxx
// #define HCI_EVENT_PAN_META                                 0xxx
// #define HCI_EVENT_SDP_META                                 0xxx
// #define HCI_EVENT_SM_META                                  0xxx

/** GAP Subevent */


/**
 * @format 1111
 * @param subevent_code
 * @param advertisement_handle
 * @param status
 * @param selected_tx_power
 */
#define GAP_SUBEVENT_ADVERTISING_SET_INSTALLED                   0x00u

/**
 * @format 11
 * @param subevent_code
 * @param advertisement_handle
 */
#define GAP_SUBEVENT_ADVERTISING_SET_REMOVED                     0x01u

/**
 * @format 1111C
 * @param subevent_code
 * @param status
 * @param big_handle
 * @param num_bis
 * @param bis_con_handles
 */
#define GAP_SUBEVENT_BIG_CREATED                                 0x02u

/**
 * @format 11
 * @param subevent_code
 * @param big_handle
 */
#define GAP_SUBEVENT_BIG_TERMINATED                              0x03u

/**
 * @format 1111C
 * @param subevent_code
 * @param status
 * @param big_handle
 * @param num_bis
 * @param bis_con_handles
 */
#define GAP_SUBEVENT_BIG_SYNC_CREATED                            0x04u

/**
 * @format 11
 * @param subevent_code
 * @param big_handle
 */
#define GAP_SUBEVENT_BIG_SYNC_STOPPED                            0x05u

/**
 * @format 1111C
 * @param subevent_code
 * @param status
 * @param cig_id
 * @param num_cis
 * @param cis_con_handles
 */
#define GAP_SUBEVENT_CIG_CREATED                                 0x06u

/**
 * @format 111H
 * @param subevent_code
 * @param status
 * @param cig_id
 * @param cis_con_handle
 */
#define GAP_SUBEVENT_CIS_CREATED                                 0x07u

/** HSP Subevent */

/**
 * @format 1H1B
 * @param subevent_code
 * @param acl_handle
 * @param status 0 == OK
 * @param bd_addr
 */
#define HSP_SUBEVENT_RFCOMM_CONNECTION_COMPLETE             0x01u

/**
 * @format 1H
 * @param subevent_code
 * @param acl_handle
 */
#define HSP_SUBEVENT_RFCOMM_DISCONNECTION_COMPLETE           0x02u

/**
 * @format 1H1H
 * @param subevent_code
 * @param acl_handle
 * @param status 0 == OK
 * @param sco_handle
 */
#define HSP_SUBEVENT_AUDIO_CONNECTION_COMPLETE             0x03u

/**
 * @format 1HH
 * @param subevent_code
 * @param acl_handle
 * @param sco_handle
 */
#define HSP_SUBEVENT_AUDIO_DISCONNECTION_COMPLETE          0x04u

/**
 * @format 1H
 * @param subevent_code
 * @param acl_handle
 */
#define HSP_SUBEVENT_RING                                  0x05u

/**
 * @format 1H1
 * @param subevent_code
 * @param acl_handle
 * @param gain Valid range: [0,15]
 */
#define HSP_SUBEVENT_MICROPHONE_GAIN_CHANGED               0x06u

/**
 * @format 1H1
 * @param subevent_code
 * @param acl_handle
 * @param gain Valid range: [0,15]
 */
#define HSP_SUBEVENT_SPEAKER_GAIN_CHANGED                  0x07u

/**
 * @format 1HJV
 * @param subevent_code
 * @param acl_handle
 * @param value_length
 * @param value
 */
#define HSP_SUBEVENT_HS_COMMAND                            0x08u

/**
 * @format 1HJV
 * @param subevent_code
 * @param acl_handle
 * @param value_length
 * @param value
 */
#define HSP_SUBEVENT_AG_INDICATION                         0x09u

/**
 * @format 1H
 * @param subevent_code
 * @param acl_handle
 */
#define HSP_SUBEVENT_BUTTON_PRESSED                        0x0au

/** HFP Subevent */

/**
 * @format 1H1B
 * @param subevent_code
 * @param acl_handle
 * @param status 0 == OK
 * @param bd_addr
 */
#define HFP_SUBEVENT_SERVICE_LEVEL_CONNECTION_ESTABLISHED    0x01u

/**
 * @format 1H
 * @param subevent_code
 * @param acl_handle
 */
#define HFP_SUBEVENT_SERVICE_LEVEL_CONNECTION_RELEASED       0x02u

/**
 * @format 1H1HB1
 * @param subevent_code
 * @param acl_handle
 * @param status 0 == OK
 * @param sco_handle
 * @param bd_addr
 * @param negotiated_codec
 */
#define HFP_SUBEVENT_AUDIO_CONNECTION_ESTABLISHED            0x03u

/**
 * @format 1HH
 * @param subevent_code
 * @param acl_handle
 * @param sco_handle
 */
#define HFP_SUBEVENT_AUDIO_CONNECTION_RELEASED               0x04u

/**
 * @format 1H1
 * @param subevent_code
 * @param acl_handle
 * @param status 0 == OK
 */
#define HFP_SUBEVENT_COMPLETE                                0x05u

/**
 * @format 1H111T
 * @param subevent_code
 * @param acl_handle
 * @param indicator_index
 * @param indicator_min_range
 * @param indicator_max_range
 * @param indicator_name
 */
#define HFP_SUBEVENT_AG_INDICATOR_MAPPING                    0x06u

/**
 * @format 1H1111111T
 * @param subevent_code
 * @param acl_handle
 * @param indicator_index
 * @param indicator_status
 * @param indicator_min_range
 * @param indicator_max_range
 * @param indicator_mandatory
 * @param indicator_enabled
 * @param indicator_status_changed
 * @param indicator_name
 */
#define HFP_SUBEVENT_AG_INDICATOR_STATUS_CHANGED              0x07u

/**
 * @format 1H11T
 * @param subevent_code
 * @param acl_handle
 * @param network_operator_mode
 * @param network_operator_format
 * @param network_operator_name
 */
#define HFP_SUBEVENT_NETWORK_OPERATOR_CHANGED                 0x08u

/**
 * @format 1H1
 * @param subevent_code
 * @param acl_handle
 * @param error
 */
#define HFP_SUBEVENT_EXTENDED_AUDIO_GATEWAY_ERROR             0x09u

/**
 * @format 1H
 * @param subevent_code
 * @param acl_handle
 */
#define HFP_SUBEVENT_START_RINGING                            0x0Au

/**
 * @format 1H
 * @param subevent_code
 * @param acl_handle
 */
#define HFP_SUBEVENT_RING                                     0x0Bu

/**
 * @format 1H
 * @param subevent_code
 * @param acl_handle
 */
#define HFP_SUBEVENT_STOP_RINGING                             0x0Cu

/**
 * @format 1HT
 * @param subevent_code
 * @param acl_handle
 * @param number
 */
#define HFP_SUBEVENT_PLACE_CALL_WITH_NUMBER                   0x0Du

/**
 * @format 1H
 * @param subevent_code
 * @param acl_handle
 */
#define HFP_SUBEVENT_ATTACH_NUMBER_TO_VOICE_TAG               0x0Eu

/**
 * @format 1HT
 * @param subevent_code
 * @param acl_handle
 * @param number
 */
#define HFP_SUBEVENT_NUMBER_FOR_VOICE_TAG                     0x0Fu

/**
 * @format 1HT
 * @param subevent_code
 * @param acl_handle
 * @param dtmf code
 */
#define HFP_SUBEVENT_TRANSMIT_DTMF_CODES                      0x10u

/**
 * @format 1H
 * @param subevent_code
 * @param acl_handle
 */
#define HFP_SUBEVENT_CALL_ANSWERED                            0x11u

/**
 * @format 1H
 * @param subevent_code
 * @param acl_handle
 */
#define HFP_SUBEVENT_CALL_TERMINATED                          0x12u

/**
 * @format 1H
 * @param subevent_code
 * @param acl_handle
 */
#define HFP_SUBEVENT_CONFERENCE_CALL                          0x13u


/**
 * @format 1H1
 * @param subevent_code
 * @param acl_handle
 * @param gain
 */
#define HFP_SUBEVENT_SPEAKER_VOLUME                           0x14u

/**
 * @format 1H1
 * @param subevent_code
 * @param acl_handle
 * @param gain
 */
#define HFP_SUBEVENT_MICROPHONE_VOLUME                        0x15u

/**
 * @format 1H1JVJV
 * @param subevent_code
 * @param acl_handle
 * @param type
 * @param number_length
 * @param number
 * @param alpha_length
 * @param alpha
 */
#define HFP_SUBEVENT_CALL_WAITING_NOTIFICATION                0x16u

/**
 * @format 1H1JVJV
 * @param subevent_code
 * @param acl_handle
 * @param type
 * @param number_length
 * @param number
 * @param alpha_length
 * @param alpha
 */
#define HFP_SUBEVENT_CALLING_LINE_IDENTIFICATION_NOTIFICATION 0x17u

/**
 * @format 1H111111T
 * @param subevent_code
 * @param acl_handle
 * @param clcc_idx
 * @param clcc_dir
 * @param clcc_status
 * @param clcc_mode
 * @param clcc_mpty
 * @param bnip_type
 * @param bnip_number
 */
#define HFP_SUBEVENT_ENHANCED_CALL_STATUS                     0x18u

/**
 * @format 1H11T
 * @param subevent_code
 * @param acl_handle
 * @param status
 * @param bnip_type
 * @param bnip_number
 */
#define HFP_SUBEVENT_SUBSCRIBER_NUMBER_INFORMATION            0x19u

/**
 * @format 1HT
 * @param subevent_code
 * @param acl_handle
 * @param value
 */
#define HFP_SUBEVENT_RESPONSE_AND_HOLD_STATUS                 0x1Au

/**
 * @format 1HT
 * @param subevent_code
 * @param acl_handle
 * @param command
 */
#define HFP_SUBEVENT_AT_MESSAGE_SENT                          0x1Bu

/**
 * @format 1HT
 * @param subevent_code
 * @param acl_handle
 * @param command
 */
#define HFP_SUBEVENT_AT_MESSAGE_RECEIVED                      0x1Cu

/**
 * @format 1H1
 * @param subevent_code
 * @param acl_handle
 * @param status
 */
#define HFP_SUBEVENT_IN_BAND_RING_TONE                        0x1Du

/**
 * @format 1H11
 * @param subevent_code
 * @param acl_handle
 * @param status      0-success
 * @param enhanced    0-legacy, 1-enhanced
 */
#define HFP_SUBEVENT_VOICE_RECOGNITION_ACTIVATED                0x1Eu

/**
 * @format 1H1
 * @param subevent_code
 * @param acl_handle
 * @param status      0-success
 */
#define HFP_SUBEVENT_VOICE_RECOGNITION_DEACTIVATED               0x1Fu

/**
 * @format 1H1
 * @param subevent_code
 * @param acl_handle
 * @param status
 */
#define HFP_SUBEVENT_ENHANCED_VOICE_RECOGNITION_HF_READY_FOR_AUDIO  0x20u


/**
 * @format 1H1
 * @param subevent_code
 * @param acl_handle
 * @param status
 */
#define HFP_SUBEVENT_ENHANCED_VOICE_RECOGNITION_AG_READY_TO_ACCEPT_AUDIO_INPUT 0x21u

/**
 * @format 1H1
 * @param subevent_code
 * @param acl_handle
 * @param status
 */
#define HFP_SUBEVENT_ENHANCED_VOICE_RECOGNITION_AG_IS_STARTING_SOUND 0x22u

/**
 * @format 1H1
 * @param subevent_code
 * @param acl_handle
 * @param status
 */
#define HFP_SUBEVENT_ENHANCED_VOICE_RECOGNITION_AG_IS_PROCESSING_AUDIO_INPUT 0x23u

/**
 * @format 1H1
 * @param subevent_code
 * @param acl_handle
 * @param status
 */
#define HFP_SUBEVENT_ENHANCED_VOICE_RECOGNITION_AG_MESSAGE_SENT     0x24u


/**
 * @format 1H211LV
 * @param subevent_code
 * @param acl_handle
 * @param text_id
 * @param text_type
 * @param text_operation
 * @param text_length
 * @param text
 */
#define HFP_SUBEVENT_ENHANCED_VOICE_RECOGNITION_AG_MESSAGE           0x25u

/**
 * @format 1H1
 * @param subevent_code
 * @param acl_handle
 * @param status
 */
#define HFP_SUBEVENT_ECHO_CANCELING_AND_NOISE_REDUCTION_DEACTIVATE   0x26u

/**
 * @format 1H21
 * @param subevent_code
 * @param acl_handle
 * @param uuid
 * @param value
 */
#define HFP_SUBEVENT_HF_INDICATOR                                    0x27u

/**
 * @format 1H2T
 * @param subevent_code
 * @param acl_handle
 * @param command_id
 * @param command_string
 */
#define HFP_SUBEVENT_CUSTOM_AT_COMMAND                               0x28u


// ANCS Client

/**
 * @format 1H
 * @param subevent_code
 * @param handle
 */ 
#define ANCS_SUBEVENT_CLIENT_CONNECTED                              0xF0u

/**
 * @format 1H2T
 * @param subevent_code
 * @param handle
 * @param attribute_id
 * @param text
 */ 
#define ANCS_SUBEVENT_CLIENT_NOTIFICATION                           0xF1u

/**
 * @format 1H
 * @param subevent_code
 * @param handle
 */ 
#define ANCS_SUBEVENT_CLIENT_DISCONNECTED                           0xF2u


/** AVDTP Subevent */

/**
 * @format 12111
 * @param subevent_code
 * @param avdtp_cid
 * @param local_seid
 * @param is_initiator
 * @param signal_identifier
 */
#define AVDTP_SUBEVENT_SIGNALING_ACCEPT                     0x01u

/**
 * @format 12111
 * @param subevent_code
 * @param avdtp_cid
 * @param local_seid 
 * @param is_initiator
 * @param signal_identifier 
 */
#define AVDTP_SUBEVENT_SIGNALING_REJECT                     0x02u

/**
 * @format 12111
 * @param subevent_code
 * @param avdtp_cid
 * @param local_seid
 * @param is_initiator
 * @param signal_identifier
 */
#define AVDTP_SUBEVENT_SIGNALING_GENERAL_REJECT             0x03u

/**
 * @format 12B21
 * @param subevent_code
 * @param avdtp_cid
 * @param bd_addr
 * @param con_handle
 * @param status 0 == OK
 */
#define AVDTP_SUBEVENT_SIGNALING_CONNECTION_ESTABLISHED     0x04u

/**
 * @format 12
 * @param subevent_code
 * @param avdtp_cid
 */
#define AVDTP_SUBEVENT_SIGNALING_CONNECTION_RELEASED        0x05u

/**
 * @format 121111
 * @param subevent_code
 * @param avdtp_cid
 * @param remote_seid        0x01u – 0x3Eu
 * @param in_use      0-not in use, 1-in use
 * @param media_type  0-audio, 1-video, 2-multimedia
 * @param sep_type    0-source, 1-sink
 */
#define AVDTP_SUBEVENT_SIGNALING_SEP_FOUND                  0x06u

/**
 * @format 12111111111
 * @param subevent_code
 * @param avdtp_cid
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
#define AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_SBC_CAPABILITY          0x07u

/**
 * @format 12111111112
 * @param subevent_code
 * @param avdtp_cid
 * @param remote_seid
 * @param media_type
 * @param layer_bitmap
 * @param crc
 * @param channel_mode_bitmap
 * @param media_payload_format
 * @param sampling_frequency_bitmap
 * @param vbr
 * @param bit_rate_index_bitmap
 */
#define AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_MPEG_AUDIO_CAPABILITY   0x08u

/**
 * @format 121112131
 * @param subevent_code
 * @param avdtp_cid
 * @param remote_seid
 * @param media_type
 * @param object_type_bitmap
 * @param sampling_frequency_bitmap
 * @param channels_bitmap
 * @param bit_rate
 * @param vbr
 */
#define AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_MPEG_AAC_CAPABILITY     0x09u

/**
 * @format 1211111132
 * @param subevent_code
 * @param avdtp_cid
 * @param remote_seid
 * @param media_type
 * @param version
 * @param channel_mode_bitmap
 * @param sampling_frequency_bitmap
 * @param vbr
 * @param bit_rate_index_bitmap
 * @param maximum_sul
 */
#define AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_ATRAC_CAPABILITY        0x0Au

/**
 * @format 12112LV
 * @param subevent_code
 * @param avdtp_cid
 * @param remote_seid
 * @param media_type
 * @param media_codec_type
 * @param media_codec_information_len
 * @param media_codec_information
 */
#define AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_OTHER_CAPABILITY        0x0Bu


/**
 * @format 121
 * @param subevent_code
 * @param avdtp_cid
 * @param remote_seid
 */
#define AVDTP_SUBEVENT_SIGNALING_MEDIA_TRANSPORT_CAPABILITY         0x0Cu


/**
 * @format 121
 * @param subevent_code
 * @param avdtp_cid
 * @param remote_seid
 */
#define AVDTP_SUBEVENT_SIGNALING_REPORTING_CAPABILITY        0x0Du


/**
 * @format 121111
 * @param subevent_code
 * @param avdtp_cid
 * @param remote_seid
 * @param recovery_type
 * @param maximum_recovery_window_size
 * @param maximum_number_media_packets
 */
#define AVDTP_SUBEVENT_SIGNALING_RECOVERY_CAPABILITY        0x0Eu


/**
 * @format 1212LV
 * @param subevent_code
 * @param avdtp_cid
 * @param remote_seid
 * @param cp_type
 * @param cp_type_value_len
 * @param cp_type_value
 */
#define AVDTP_SUBEVENT_SIGNALING_CONTENT_PROTECTION_CAPABILITY        0x0Fu


/**
 * @format 12111111111
 * @param subevent_code
 * @param avdtp_cid
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
#define AVDTP_SUBEVENT_SIGNALING_MULTIPLEXING_CAPABILITY        0x10u


/**
 * @format 121
 * @param subevent_code
 * @param avdtp_cid
 * @param remote_seid
 */
#define AVDTP_SUBEVENT_SIGNALING_DELAY_REPORTING_CAPABILITY        0x11u


/**
 * @format 121111
 * @param subevent_code
 * @param avdtp_cid
 * @param remote_seid
 * @param back_ch
 * @param media
 * @param recovery
 */
#define AVDTP_SUBEVENT_SIGNALING_HEADER_COMPRESSION_CAPABILITY        0x12u

/**
 * @format 121
 * @param subevent_code
 * @param avdtp_cid
 * @param remote_seid
 */
#define AVDTP_SUBEVENT_SIGNALING_CAPABILITIES_DONE                    0x13u


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
#define AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_SBC_CONFIGURATION        0x14u

/**
 * @format 12111111111211
 * @param subevent_code
 * @param avdtp_cid
 * @param local_seid
 * @param remote_seid
 * @param reconfigure
 * @param media_type
 * @param layer
 * @param crc
 * @param channel_mode
 * @param num_channels
 * @param media_payload_format
 * @param sampling_frequency
 * @param vbr
 * @param bit_rate_index
 */
#define AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_MPEG_AUDIO_CONFIGURATION   0x15u

/**
 * @format 12111113131
 * @param subevent_code
 * @param avdtp_cid
 * @param local_seid
 * @param remote_seid
 * @param reconfigure
 * @param media_type
 * @param object_type
 * @param sampling_frequency
 * @param num_channels
 * @param bit_rate
 * @param vbr
 */
#define AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_MPEG_AAC_CONFIGURATION     0x16u

/**
 * @format 1211111112112
 * @param subevent_code
 * @param avdtp_cid
 * @param local_seid
 * @param remote_seid
 * @param reconfigure
 * @param media_type
 * @param version
 * @param channel_mode
 * @param num_channels
 * @param sampling_frequency
 * @param vbr
 * @param bit_rate_index
 * @param maximum_sul
 */
#define AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_ATRAC_CONFIGURATION        0x17u

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
#define AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_OTHER_CONFIGURATION        0x18u

/**
 * @format 12B111
 * @param subevent_code
 * @param avdtp_cid
 * @param bd_addr
 * @param local_seid
 * @param remote_seid
 * @param status 0 == OK
 */
#define AVDTP_SUBEVENT_STREAMING_CONNECTION_ESTABLISHED     0x19u

/**
 * @format 121
 * @param subevent_code
 * @param avdtp_cid
 * @param local_seid
 */
#define AVDTP_SUBEVENT_STREAMING_CONNECTION_RELEASED        0x1Au

/**
 * @format 1212
 * @param subevent_code
 * @param avdtp_cid
 * @param local_seid
 * @param sequence_number
 */
#define AVDTP_SUBEVENT_STREAMING_CAN_SEND_MEDIA_PACKET_NOW   0x1Bu


/**
 * @format 12
 * @param subevent_code
 * @param avdtp_cid
 */
#define AVDTP_SUBEVENT_SIGNALING_SEP_DICOVERY_DONE           0x1Cu

/**
 * @format 1212
 * @param subevent_code
 * @param avdtp_cid
 * @param local_seid
 * @param delay_100us
 */
#define AVDTP_SUBEVENT_SIGNALING_DELAY_REPORT               0x1Du


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
#define A2DP_SUBEVENT_STREAMING_CAN_SEND_MEDIA_PACKET_NOW         0x01u

/**
 * @format 12111121111111
 * @param subevent_code
 * @param a2dp_cid
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
#define A2DP_SUBEVENT_SIGNALING_MEDIA_CODEC_SBC_CONFIGURATION      0x02u

/**
 * @format 12111111111211
 * @param subevent_code
 * @param a2dp_cid
 * @param local_seid
 * @param remote_seid
 * @param reconfigure
 * @param media_type
 * @param layer
 * @param crc
 * @param channel_mode
 * @param num_channels
 * @param media_payload_format
 * @param sampling_frequency
 * @param vbr
 * @param bit_rate_index
 */
#define A2DP_SUBEVENT_SIGNALING_MEDIA_CODEC_MPEG_AUDIO_CONFIGURATION   0x03u

/**
 * @format 12111113131
 * @param subevent_code
 * @param a2dp_cid
 * @param local_seid
 * @param remote_seid
 * @param reconfigure
 * @param media_type
 * @param object_type
 * @param sampling_frequency
 * @param num_channels
 * @param bit_rate
 * @param vbr
 */
#define A2DP_SUBEVENT_SIGNALING_MEDIA_CODEC_MPEG_AAC_CONFIGURATION     0x04u

/**
 * @format 1211111112112
 * @param subevent_code
 * @param a2dp_cid
 * @param local_seid
 * @param remote_seid
 * @param reconfigure
 * @param media_type
 * @param version
 * @param channel_mode
 * @param num_channels
 * @param sampling_frequency
 * @param vbr
 * @param bit_rate_index
 * @param maximum_sul
 */
#define A2DP_SUBEVENT_SIGNALING_MEDIA_CODEC_ATRAC_CONFIGURATION        0x05u

/**
 * @format 1211112LV
 * @param subevent_code
 * @param a2dp_cid
 * @param local_seid
 * @param remote_seid
 * @param reconfigure
 * @param media_type
 * @param media_codec_type
 * @param media_codec_information_len
 * @param media_codec_information
 */
#define A2DP_SUBEVENT_SIGNALING_MEDIA_CODEC_OTHER_CONFIGURATION    0x06u

/**
 * @format 12B111          Stream is opened but not started.
 * @param subevent_code 
 * @param a2dp_cid
 * @param bd_addr
 * @param local_seid
 * @param remote_seid
 * @param status
 */
#define A2DP_SUBEVENT_STREAM_ESTABLISHED                           0x07u

/**
 * @format 121            If ENABLE_AVDTP_ACCEPTOR_EXPLICIT_START_STREAM_CONFIRMATION, user must explicitly accept stream start.
 * @param subevent_code
 * @param a2dp_cid
 * @param local_seid
 */
#define A2DP_SUBEVENT_START_STREAM_REQUESTED                       0x08u

/**
 * @format 121            Indicates that media transfer is started.
 * @param subevent_code
 * @param a2dp_cid
 * @param local_seid
 */
#define A2DP_SUBEVENT_STREAM_STARTED                               0x09u

/**
 * @format 121           Stream is paused.
 * @param subevent_code
 * @param a2dp_cid
 * @param local_seid
 */
#define A2DP_SUBEVENT_STREAM_SUSPENDED                              0x0Au

/**
 * @format 121           Stream is stopped or aborted.
 * @param subevent_code
 * @param a2dp_cid
 * @param local_seid
 */
#define A2DP_SUBEVENT_STREAM_STOPPED                                0x0Bu

/**
 * @format 121            Stream is released.
 * @param subevent_code
 * @param a2dp_cid
 * @param local_seid
 */
#define A2DP_SUBEVENT_STREAM_RELEASED                               0x0Cu

/**
 * @format 1211
 * @param subevent_code
 * @param a2dp_cid
 * @param local_seid
 * @param signal_identifier
 */
#define A2DP_SUBEVENT_COMMAND_ACCEPTED                              0x0Du

/**
 * @format 12111
 * @param subevent_code
 * @param a2dp_cid
 * @param local_seid
 * @param is_initiator
 * @param signal_identifier
 */
#define A2DP_SUBEVENT_COMMAND_REJECTED                              0x0Eu

/**
 * @format 12B21
 * @param subevent_code
 * @param a2dp_cid
 * @param bd_addr
 * @param con_handle
 * @param status 0 == OK
 */
#define A2DP_SUBEVENT_SIGNALING_CONNECTION_ESTABLISHED              0x0Fu

/**
 * @format 12            Signaling channel is released.
 * @param subevent_code
 * @param a2dp_cid
 */
#define A2DP_SUBEVENT_SIGNALING_CONNECTION_RELEASED                  0x10u

/**
 * @format 1211          Stream was reconfigured
 * @param subevent_code
 * @param a2dp_cid
 * @param local_seid
 * @param status
 */
#define A2DP_SUBEVENT_STREAM_RECONFIGURED                            0x12u

/**
 * @format 12111111111
 * @param subevent_code
 * @param a2dp_cid
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
#define A2DP_SUBEVENT_SIGNALING_MEDIA_CODEC_SBC_CAPABILITY          0x13u

/**
 * @format 12111111112
 * @param subevent_code
 * @param a2dp_cid
 * @param remote_seid
 * @param media_type
 * @param layer_bitmap
 * @param crc
 * @param channel_mode_bitmap
 * @param media_payload_format
 * @param sampling_frequency_bitmap
 * @param vbr
 * @param bit_rate_index_bitmap
 */
#define A2DP_SUBEVENT_SIGNALING_MEDIA_CODEC_MPEG_AUDIO_CAPABILITY   0x14u

/**
 * @format 121112131
 * @param subevent_code
 * @param a2dp_cid
 * @param remote_seid
 * @param media_type
 * @param object_type_bitmap
 * @param sampling_frequency_bitmap
 * @param channels_bitmap
 * @param bit_rate
 * @param vbr
 */
#define A2DP_SUBEVENT_SIGNALING_MEDIA_CODEC_MPEG_AAC_CAPABILITY     0x15u

/**
 * @format 1211111132
 * @param subevent_code
 * @param a2dp_cid
 * @param remote_seid
 * @param media_type
 * @param version
 * @param channel_mode_bitmap
 * @param sampling_frequency_bitmap
 * @param vbr
 * @param bit_rate_index_bitmap
 * @param maximum_sul
 */
#define A2DP_SUBEVENT_SIGNALING_MEDIA_CODEC_ATRAC_CAPABILITY        0x16u

/**
 * @format 12112LV
 * @param subevent_code
 * @param a2dp_cid
 * @param remote_seid
 * @param media_type
 * @param media_codec_type
 * @param media_codec_information_len
 * @param media_codec_information
 */
#define A2DP_SUBEVENT_SIGNALING_MEDIA_CODEC_OTHER_CAPABILITY        0x17u

/**
 * @format 121
 * @param subevent_code
 * @param a2dp_cid
 * @param remote_seid
 */
#define A2DP_SUBEVENT_SIGNALING_DELAY_REPORTING_CAPABILITY           0x18u


/**
 * @format 1212
 * @param subevent_code
 * @param a2dp_cid
 * @param local_seid
 * @param delay_100us
 */
#define A2DP_SUBEVENT_SIGNALING_DELAY_REPORT                         0x19u

/**
 * @format 121
 * @param subevent_code
 * @param a2dp_cid
 * @param remote_seid
 */
#define A2DP_SUBEVENT_SIGNALING_CAPABILITIES_DONE                    0x1Au

/**
 * @format 12
 * @param subevent_code
 * @param a2dp_cid
 */
#define A2DP_SUBEVENT_SIGNALING_CAPABILITIES_COMPLETE                0x1Bu


/** AVRCP Subevent */

/**
 * @format 1211
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 * @param play_status
 */
#define AVRCP_SUBEVENT_NOTIFICATION_PLAYBACK_STATUS_CHANGED                         0x01u

/**
 * @format 121
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 */
#define AVRCP_SUBEVENT_NOTIFICATION_TRACK_CHANGED                                   0x02u

/**
 * @format 121
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 */
#define AVRCP_SUBEVENT_NOTIFICATION_EVENT_TRACK_REACHED_END                         0x03u

/**
 * @format 121
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 */
#define AVRCP_SUBEVENT_NOTIFICATION_EVENT_TRACK_REACHED_START                       0x04u              

/**
 * @format 1214
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 * @param playback_position  If no track currently selected, then return 0xFFuFFFFFF in the INTERIM response.
 */
#define AVRCP_SUBEVENT_NOTIFICATION_EVENT_PLAYBACK_POS_CHANGED                      0x05u

/**
 * @format 1211
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 * @param battery_status  see avrcp_battery_status_t
 */
#define AVRCP_SUBEVENT_NOTIFICATION_EVENT_BATT_STATUS_CHANGED                       0x06u

/**
 * @format 1211
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 * @param system_status  see avrcp_system_status_t
 */
#define AVRCP_SUBEVENT_NOTIFICATION_EVENT_SYSTEM_STATUS_CHANGED                     0x07u


// Recquires 1 byte for num_attributes, followed by num_attributes tuples [attribute_id(1), value_id(1)], see avrcp_player_application_setting_attribute_id_t
#define AVRCP_SUBEVENT_NOTIFICATION_EVENT_PLAYER_APPLICATION_SETTING_CHANGED        0x08u

/**
 * @format 121
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 */
#define AVRCP_SUBEVENT_NOTIFICATION_NOW_PLAYING_CONTENT_CHANGED                     0x09u

/**
 * @format 121
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 */
#define AVRCP_SUBEVENT_NOTIFICATION_AVAILABLE_PLAYERS_CHANGED                       0x0Au

// AVRCP_SUBEVENT_NOTIFICATION_EVENT_ADDRESSED_PLAYER_CHANGED = 0x0bu,           -- The Addressed Player has been changed, see 6.9.2.

/**
 * @format 1212
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 * @param uid_counter of the currently browsed player
 */
#define AVRCP_SUBEVENT_NOTIFICATION_EVENT_UIDS_CHANGED                              0x0Cu

/**
 * @format 1211
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 * @param absolute_volume
 */
#define AVRCP_SUBEVENT_NOTIFICATION_VOLUME_CHANGED                                  0x0Du
         

/**
 * @format 1211
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 * @param absolute_volume
 */
#define AVRCP_SUBEVENT_SET_ABSOLUTE_VOLUME_RESPONSE                      0x10u

/**
 * @format 12111
 * @param subevent_code
 * @param avrcp_cid
 * @param status
 * @param enabled      1 enabled, 0 disabled
 * @param event_id
 */
#define AVRCP_SUBEVENT_NOTIFICATION_STATE                               0x11u

/**
 * @format 112B2
 * @param subevent_code
 * @param status 0 == OK
 * @param avrcp_cid
 * @param bd_addr
 * @param con_handle
 */
#define AVRCP_SUBEVENT_CONNECTION_ESTABLISHED                           0x12u

/**
 * @format 12
 * @param subevent_code
 * @param avrcp_cid
 */
#define AVRCP_SUBEVENT_CONNECTION_RELEASED                              0x13u

/**
 * @format 12111
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 * @param repeat_mode
 * @param shuffle_mode
 */
#define AVRCP_SUBEVENT_SHUFFLE_AND_REPEAT_MODE                          0x14u

/**
 * @format 121441
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 * @param song_length
 * @param song_position
 * @param play_status
 */
 #define AVRCP_SUBEVENT_PLAY_STATUS                                     0x15u

/**
 * @format 1211
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 * @param operation_id
 */
#define AVRCP_SUBEVENT_OPERATION_START                                    0x16u

/**
 * @format 1211
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 * @param operation_id
 */
#define AVRCP_SUBEVENT_OPERATION_COMPLETE                                 0x17u

/**
 * @format 121
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 */
#define AVRCP_SUBEVENT_PLAYER_APPLICATION_VALUE_RESPONSE                   0x18u

/**
 * @format 12
 * @param subevent_code
 * @param avrcp_cid
 */
#define AVRCP_SUBEVENT_PLAY_STATUS_QUERY                                    0x19u

/**
 * @format 121111
 * @param subevent_code
 * @param avrcp_cid
 * @param operation_id
 * @param button_pressed
 * @param operands_length
 * @param operand
 */
#define AVRCP_SUBEVENT_OPERATION                                            0x1Au 

/**
 * @format 1211
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 * @param track
 */
#define AVRCP_SUBEVENT_NOW_PLAYING_TRACK_INFO                               0x1Bu

/**
 * @format 1211
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 * @param total_tracks
 */
#define AVRCP_SUBEVENT_NOW_PLAYING_TOTAL_TRACKS_INFO                        0x1Cu

/**
 * @format 1214
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 * @param song_length in ms
 */
#define AVRCP_SUBEVENT_NOW_PLAYING_SONG_LENGTH_MS_INFO                      0x1Du

/**
 * @format 121JV
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 * @param value_len
 * @param value
 */
#define AVRCP_SUBEVENT_NOW_PLAYING_TITLE_INFO                                 0x1Eu

 /*
 * @format 121JV
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 * @param value_len
 * @param value
 */
#define AVRCP_SUBEVENT_NOW_PLAYING_ARTIST_INFO                                0x1Fu

 /*
 * @format 121JV
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 * @param value_len
 * @param value
 */
#define AVRCP_SUBEVENT_NOW_PLAYING_ALBUM_INFO                                 0x20u

 /*
 * @format 121JV
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 * @param value_len
 * @param value
 */
#define AVRCP_SUBEVENT_NOW_PLAYING_GENRE_INFO                                 0x21u

/*
 * @format 1211
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 * @param status
 */
#define AVRCP_SUBEVENT_NOW_PLAYING_INFO_DONE                                  0x22u

/**
 * @format 1214
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 * @param playback_position_ms
 */
#define AVRCP_SUBEVENT_NOTIFICATION_PLAYBACK_POS_CHANGED                      0x23u

/*
 * @format 12111
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 * @param status
 * @param event_id
 */
#define AVRCP_SUBEVENT_GET_CAPABILITY_EVENT_ID                                0x24u
/*
 * @format 1211
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 * @param status
 */
#define AVRCP_SUBEVENT_GET_CAPABILITY_EVENT_ID_DONE                           0x25u

/*
 * @format 12113
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 * @param status
 * @param company_id
 */
#define AVRCP_SUBEVENT_GET_CAPABILITY_COMPANY_ID                              0x26u
/*
 * @format 1211
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 * @param status
 */
#define AVRCP_SUBEVENT_GET_CAPABILITY_COMPANY_ID_DONE                         0x27u

/**
 * @format 1211LV
 * @param subevent_code
 * @param avrcp_cid
 * @param command_type
 * @param pdu_id
 * @param params_len
 * @param params
*/
#define AVRCP_SUBEVENT_CUSTOM_COMMAND_RESPONSE                               0x28u


/**
 * @format 1B2
 * @param subevent_code
 * @param bd_addr
 * @param browsing_cid
 */
#define AVRCP_SUBEVENT_INCOMING_BROWSING_CONNECTION                          0x30u

/**
 * @format 11B2
 * @param subevent_code
 * @param status 0 == OK
 * @param bd_addr
 * @param browsing_cid
 */
#define AVRCP_SUBEVENT_BROWSING_CONNECTION_ESTABLISHED                        0x31u

/**
 * @format 12
 * @param subevent_code
 * @param browsing_cid
 */
#define AVRCP_SUBEVENT_BROWSING_CONNECTION_RELEASED                           0x32u

/**
 * @format 12211
 * @param subevent_code
 * @param browsing_cid
 * @param uid_counter
 * @param browsing_status
 * @param bluetooth_status
 */
#define AVRCP_SUBEVENT_BROWSING_DONE                                          0x33u

/**
 * @format 1214
 * @param subevent_code
 * @param browsing_cid
 * @param scope
 * @param attr_bitmap
 */
#define AVRCP_SUBEVENT_BROWSING_GET_FOLDER_ITEMS                              0x34u

/**
 * @format 121
 * @param subevent_code
 * @param browsing_cid
 * @param scope
 */
#define AVRCP_SUBEVENT_BROWSING_GET_TOTAL_NUM_ITEMS                           0x35u

/**
 * @format 122
 * @param subevent_code
 * @param browsing_cid
 * @param player_id
 */
#define AVRCP_SUBEVENT_BROWSING_SET_BROWSED_PLAYER                            0x36u


/**
 * @format 12BH
 * @param subevent_code
 * @param goep_cid
 * @param address
 * @param handle
 */
#define GOEP_SUBEVENT_INCOMING_CONNECTION                                  0x01u

/**
 * @format 121BH1
 * @param subevent_code
 * @param goep_cid
 * @param status
 * @param bd_addr
 * @param con_handle
 * @param incoming
 */
#define GOEP_SUBEVENT_CONNECTION_OPENED                                    0x02u

/**
 * @format 12
 * @param subevent_code
 * @param goep_cid
*/
#define GOEP_SUBEVENT_CONNECTION_CLOSED                                    0x03u

/**
 * @format 12
 * @param subevent_code
 * @param goep_cid
*/
#define GOEP_SUBEVENT_CAN_SEND_NOW                                         0x04u

/**
 * @format 121BH1
 * @param subevent_code
 * @param pbap_cid
 * @param status
 * @param bd_addr
 * @param con_handle
 * @param incoming
 */
#define PBAP_SUBEVENT_CONNECTION_OPENED                                    0x01u

/**
 * @format 12
 * @param subevent_code
 * @param goep_cid
*/
#define PBAP_SUBEVENT_CONNECTION_CLOSED                                    0x02u

/**
 * @format 121
 * @param subevent_code
 * @param goep_cid
 * @param status
 */
#define PBAP_SUBEVENT_OPERATION_COMPLETED                                  0x03u

/**
 * @format 1212
 * @param subevent_code
 * @param goep_cid
 * @param status
 * @param phonebook_size
 */
#define PBAP_SUBEVENT_PHONEBOOK_SIZE                                       0x04u

/**
 * @format 1211
 * @param subevent_code
 * @param goep_cid
 * @param user_id_required
 * @param full_access 
 */
#define PBAP_SUBEVENT_AUTHENTICATION_REQUEST                               0x05u

/**
 * @format 12JVJV
 * @param subevent_code
 * @param goep_cid
 * @param name_len
 * @param name 
 * @param handle_len
 * @param handle 
 */
#define PBAP_SUBEVENT_CARD_RESULT                                          0x06u

/**
 * @format 121
 * @param subevent_code
 * @param goep_cid
 * @param phonebook
 */
#define PBAP_SUBEVENT_RESET_MISSED_CALLS                                   0x0Au

/**
 * @format 12411
 * @param subevent_code
 * @param goep_cid
 * @param vcard_selector
 * @param vcard_selector_operator
 * @param phonebook
 */
#define PBAP_SUBEVENT_QUERY_PHONEBOOK_SIZE                                 0x0Bu

/**
 * @format 1244122411
 * @param subevent_code
 * @param goep_cid
 * @param continuation - value provided by caller of pbap_server_send_pull_response
 * @param property_selector
 * @param format
 * @param max_list_count 0xffff for unlimited
 * @param list_start_offset
 * @param vcard_selector
 * @param vcard_selector_operator
 * @param phonebook
 */
#define PBAP_SUBEVENT_PULL_PHONEBOOK                                      0x0Cu

/**
 * @format 124122411JV1
 * @param subevent_code
 * @param goep_cid
 * @param continuation - value provided by caller of pbap_server_send_pull_response
 * @param order
 * @param max_list_count 0xffff for unlimited
 * @param list_start_offset
 * @param vcard_selector
 * @param vcard_selector_operator
 * @param search_property
 * @param search_value_len
 * @param search_value
 * @param phonebook
 */
#define PBAP_SUBEVENT_PULL_VCARD_LISTING                                   0x0Du

/**
 * @format 124411T
 * @param subevent_code
 * @param goep_cid
 * @param continuation - value provided by caller of pbap_server_send_pull_response
 * @param property_selector
 * @param format
 * @param phonebook
 * @param name
 */
#define PBAP_SUBEVENT_PULL_VCARD_ENTRY                                     0x0Eu


// HID Meta Event Group

/**
 * @format 12BH
 * @param subevent_code
 * @param hid_cid
 * @param address
 * @param handle
 */
#define HID_SUBEVENT_INCOMING_CONNECTION                                   0x01u

/**
 * @format 121BH1
 * @param subevent_code
 * @param hid_cid
 * @param status
 * @param bd_addr
 * @param con_handle
 * @param incoming
 */
#define HID_SUBEVENT_CONNECTION_OPENED                                     0x02u

/**
 * @format 12
 * @param subevent_code
 * @param hid_cid
*/
#define HID_SUBEVENT_CONNECTION_CLOSED                                     0x03u

/**
 * @format 12
 * @param subevent_code
 * @param hid_cid
*/
#define HID_SUBEVENT_CAN_SEND_NOW                                          0x04u

/**
 * @format 12
 * @param subevent_code
 * @param hid_cid
*/
#define HID_SUBEVENT_SUSPEND                                               0x05u

/**
 * @format 12
 * @param subevent_code
 * @param hid_cid
*/
#define HID_SUBEVENT_EXIT_SUSPEND                                          0x06u

/**
 * @format 12
 * @param subevent_code
 * @param hid_cid
*/
#define HID_SUBEVENT_VIRTUAL_CABLE_UNPLUG                                  0x07u

/** 
 * @format 121LV
 * @param subevent_code
 * @param hid_cid
 * @param handshake_status
 * @param report_len
 * @param report
*/
#define HID_SUBEVENT_GET_REPORT_RESPONSE                                   0x08u

/** 
 * @format 121
 * @param subevent_code
 * @param hid_cid
 * @param handshake_status
*/
#define HID_SUBEVENT_SET_REPORT_RESPONSE                                   0x09u

/** 
 * @format 1211
 * @param subevent_code
 * @param hid_cid
 * @param handshake_status
 * @param protocol_mode
*/
#define HID_SUBEVENT_GET_PROTOCOL_RESPONSE                                 0x0Au

/** 
 * @format 1211
 * @param subevent_code
 * @param hid_cid
 * @param handshake_status
 * @param protocol_mode
*/
#define HID_SUBEVENT_SET_PROTOCOL_RESPONSE                                 0x0Bu

/** 
 * @format 12LV
 * @param subevent_code
 * @param hid_cid
 * @param report_len
 * @param report
*/
#define HID_SUBEVENT_REPORT                                                0x0Cu

/**
 * @format 121
 * @param subevent_code
 * @param hid_cid
 * @param status
 */
#define HID_SUBEVENT_DESCRIPTOR_AVAILABLE                                  0x0Du

/**
 * @format 1222
 * @param subevent_code
 * @param hid_cid
 * @param host_max_latency
 * @param host_min_timeout
 */
#define HID_SUBEVENT_SNIFF_SUBRATING_PARAMS                                0x0Eu

// HIDS Meta Event Group

/**
 * @format 12
 * @param subevent_code
 * @param con_handle
*/
#define HIDS_SUBEVENT_CAN_SEND_NOW                                          0x01u

/**
 * @format 121
 * @param subevent_code
 * @param con_handle
 * @param protocol_mode
*/
#define HIDS_SUBEVENT_PROTOCOL_MODE                                         0x02u

/**
 * @format 121
 * @param subevent_code
 * @param con_handle
 * @param enable
*/
#define HIDS_SUBEVENT_BOOT_MOUSE_INPUT_REPORT_ENABLE                        0x03u

/**
 * @format 121
 * @param subevent_code
 * @param con_handle
 * @param enable
*/
#define HIDS_SUBEVENT_BOOT_KEYBOARD_INPUT_REPORT_ENABLE                     0x04u

/**
 * @format 121
 * @param subevent_code
 * @param con_handle
 * @param enable
*/
#define HIDS_SUBEVENT_INPUT_REPORT_ENABLE                                   0x05u

/**
 * @format 121
 * @param subevent_code
 * @param con_handle
 * @param enable
*/
#define HIDS_SUBEVENT_OUTPUT_REPORT_ENABLE                                  0x06u

/**
 * @format 121
 * @param subevent_code
 * @param con_handle
 * @param enable
*/
#define HIDS_SUBEVENT_FEATURE_REPORT_ENABLE                                 0x07u

/**
 * @format 12
 * @param subevent_code
 * @param con_handle
*/
#define HIDS_SUBEVENT_SUSPEND                                               0x08u

/**
 * @format 12
 * @param subevent_code
 * @param con_handle
*/
#define HIDS_SUBEVENT_EXIT_SUSPEND                                          0x09u

/**
 * @format 1211
 * @param subevent_code
 * @param con_handle
 * @param measurement_type 0 - force magnitude, 1 - torque magnitude, see cycling_power_sensor_measurement_context_t
 * @param is_enhanced
*/
#define GATTSERVICE_SUBEVENT_CYCLING_POWER_START_CALIBRATION               0x01u

/**
 * @format 12
 * @param subevent_code
 * @param con_handle
*/
#define GATTSERVICE_SUBEVENT_CYCLING_POWER_BROADCAST_START                 0x02u

/**
 * @format 12
 * @param subevent_code
 * @param con_handle
*/
#define GATTSERVICE_SUBEVENT_CYCLING_POWER_BROADCAST_STOP                  0x03u

/**
 * @format 12111
 * @param subevent_code
 * @param hids_cid
 * @param status
 * @param num_instances
 * @param poll_bitmap
*/
#define GATTSERVICE_SUBEVENT_BATTERY_SERVICE_CONNECTED                     0x04u

/**
 * @format 12111
 * @param subevent_code
 * @param hids_cid
 * @param sevice_index
 * @param att_status  see ATT errors in bluetooth.h  
 * @param level
*/
#define GATTSERVICE_SUBEVENT_BATTERY_SERVICE_LEVEL                         0x05u

/**
 * @format 1H1
 * @param subevent_code
 * @param con_handle
 * @param att_status
 */
#define GATTSERVICE_SUBEVENT_DEVICE_INFORMATION_DONE                       0x06u

/**
 * @format 1H1T
 * @param subevent_code
 * @param con_handle
 * @param att_status
 * @param value
 */
#define GATTSERVICE_SUBEVENT_DEVICE_INFORMATION_MANUFACTURER_NAME          0x07u

/**
 * @format 1H1T
 * @param subevent_code
 * @param con_handle
 * @param att_status
 * @param value
 */
#define GATTSERVICE_SUBEVENT_DEVICE_INFORMATION_MODEL_NUMBER               0x08u

/**
 * @format 1H1T
 * @param subevent_code
 * @param con_handle
 * @param att_status
 * @param value
 */
#define GATTSERVICE_SUBEVENT_DEVICE_INFORMATION_SERIAL_NUMBER              0x09u

/**
 * @format 1H1T
 * @param subevent_code
 * @param con_handle
 * @param att_status
 * @param value
 */
#define GATTSERVICE_SUBEVENT_DEVICE_INFORMATION_HARDWARE_REVISION          0x0Au

/**
 * @format 1H1T
 * @param subevent_code
 * @param con_handle
 * @param att_status
 * @param value
 */
#define GATTSERVICE_SUBEVENT_DEVICE_INFORMATION_FIRMWARE_REVISION          0x0Bu

/**
 * @format 1H1T
 * @param subevent_code
 * @param con_handle
 * @param att_status
 * @param value
 */
#define GATTSERVICE_SUBEVENT_DEVICE_INFORMATION_SOFTWARE_REVISION          0x0Cu

/**
 * @format 1H1413
 * @param subevent_code
 * @param con_handle
 * @param att_status
 * @param manufacturer_id_low
 * @param manufacturer_id_high
 * @param organizationally_unique_id
 */
#define GATTSERVICE_SUBEVENT_DEVICE_INFORMATION_SYSTEM_ID                  0x0Du

/**
 * @format 1H122
 * @param subevent_code
 * @param con_handle
 * @param att_status
 * @param value_a
 * @param value_b
 */
#define GATTSERVICE_SUBEVENT_DEVICE_INFORMATION_IEEE_REGULATORY_CERTIFICATION     0x0Eu

/**
 * @format 1H11222
 * @param subevent_code
 * @param con_handle
 * @param att_status
 * @param vendor_source_id
 * @param vendor_id
 * @param product_id
 * @param product_version
 */
#define GATTSERVICE_SUBEVENT_DEVICE_INFORMATION_PNP_ID                    0x0Fu

/**
 * @format 1H1
 * @param subevent_code
 * @param con_handle
 * @param att_status
 */
#define GATTSERVICE_SUBEVENT_SCAN_PARAMETERS_SERVICE_CONNECTED            0x10u

/**
 * @format 1H
 * @param subevent_code
 * @param con_handle
 */
#define GATTSERVICE_SUBEVENT_SPP_SERVICE_CONNECTED                        0x11u

/**
 * @format 1H
 * @param subevent_code
 * @param con_handle
 */
#define GATTSERVICE_SUBEVENT_SPP_SERVICE_DISCONNECTED                     0x12u

/**
 * @format 12111
 * @param subevent_code
 * @param hids_cid
 * @param status
 * @param protocol_mode
 * @param num_instances
*/
#define GATTSERVICE_SUBEVENT_HID_SERVICE_CONNECTED                        0x13u

/** 
 * @format 1211LV
 * @param subevent_code
 * @param hids_cid
 * @param service_index
 * @param report_id
 * @param report_len
 * @param report
*/
#define GATTSERVICE_SUBEVENT_HID_REPORT                                   0x14u

/**
 * @format 1212111
 * @param subevent_code
 * @param hids_cid
 * @param service_index
 * @param base_usb_hid_version      Version number of base USB HID Specification implemented by HID Device
 * @param country_code              Country HID Device hardware is localized for (not localized: 0x00)
 * @param remote_wake               Indicates whether HID Device is capable of sending a wake-signal to a HID Host
 * @param normally_connectable      Indicates whether HID Device will be advertising when bonded but not connected.
*/
#define GATTSERVICE_SUBEVENT_HID_INFORMATION                              0x15u

/**
 * @format 1211
 * @param subevent_code
 * @param hids_cid
 * @param service_index
 * @param protocol_mode    see hid_protocol_mode_t in btstack_hid.h
*/
#define GATTSERVICE_SUBEVENT_HID_PROTOCOL_MODE                            0x16u

/**
 * @format 121
 * @param subevent_code
 * @param hids_cid
 * @param configuration    
*/
#define GATTSERVICE_SUBEVENT_HID_SERVICE_REPORTS_NOTIFICATION             0x17u

/**
 * @format 1H22
 * @param subevent_code
 * @param con_handle
 * @param max_scan_interval
 * @param min_scan_window
 */
#define GATTSERVICE_SUBEVENT_SCAN_PARAMETERS_SERVICE_SCAN_INTERVAL_UPDATE 0x18u

// LE Audio

/**
 * @format 121
 * @param subevent_code
 * @param hids_cid
 * @param status
*/
#define GATTSERVICE_SUBEVENT_MICS_CONNECTED                               0x19u

/**
 * @format 1211
 * @param subevent_code
 * @param cid
 * @param status
 * @param state
*/
#define GATTSERVICE_SUBEVENT_REMOTE_MICS_MUTE                              0x1Au

/**
 * @format 1H1
 * @param subevent_code
 * @param con_handle
 * @param state
*/
#define GATTSERVICE_SUBEVENT_LOCAL_MICS_MUTE                                0x1Bu

/**
 * @format 1H11
 * @param subevent_code
 * @param con_handle
 * @param index
 * @param state
*/
#define GATTSERVICE_SUBEVENT_AICS_MUTE_MODE                                 0x1Cu

/**
 * @format 1H11
 * @param subevent_code
 * @param con_handle
 * @param index
 * @param state
*/
#define GATTSERVICE_SUBEVENT_AICS_GAIN_MODE                                 0x1Du

/**
 * @format 1H11
 * @param subevent_code
 * @param con_handle
 * @param index
 * @param gain_db
*/
#define GATTSERVICE_SUBEVENT_AICS_GAIN_CHANGED                              0x1Eu

/**
 * @format 1H1JV
 * @param subevent_code
 * @param con_handle
 * @param index
 * @param description_len
 * @param description 
*/
#define GATTSERVICE_SUBEVENT_AICS_AUDIO_INPUT_DESC_CHANGED                   0x20u

/**
 * @format 1H12
 * @param subevent_code
 * @param con_handle
 * @param index
 * @param volume_offset
*/
#define GATTSERVICE_SUBEVENT_VOCS_VOLUME_OFFSET                              0x21u

/**
 * @format 1H14
 * @param subevent_code
 * @param con_handle
 * @param index
 * @param audio_location
*/
#define GATTSERVICE_SUBEVENT_VOCS_AUDIO_LOCATION                             0x22u

/**
 * @format 1H1JV
 * @param subevent_code
 * @param con_handle
 * @param index
 * @param description_len
 * @param description 
*/
#define GATTSERVICE_SUBEVENT_VOCS_AUDIO_OUTPUT_DESC_CHANGED                   0x23u

/**
 * @format 1H111
 * @param subevent_code
 * @param con_handle
 * @param volume_setting
 * @param volume_change_step
 * @param mute 
*/
#define GATTSERVICE_SUBEVENT_VCS_VOLUME_STATE                                 0x24u

/**
 * @format 1H1
 * @param subevent_code
 * @param con_handle
 * @param flags
*/
#define GATTSERVICE_SUBEVENT_VCS_VOLUME_FLAGS                                 0x25u

/**
 * @format 1H
 * @param subevent_code
 * @param con_handle
*/
#define GATTSERVICE_SUBEVENT_BASS_REMOTE_SCAN_STOPPED                         0x26u

/**
 * @format 1H
 * @param subevent_code
 * @param con_handle
*/
#define GATTSERVICE_SUBEVENT_BASS_REMOTE_SCAN_STARTED                          0x27u

/**
 * @format 1H1K
 * @param subevent_code
 * @param con_handle
 * @param source_id
 * @param broadcast_code
*/
#define GATTSERVICE_SUBEVENT_BASS_BROADCAST_CODE                               0x28u

/**
 * @format 1H11
 * @param subevent_code
 * @param con_handle
 * @param source_id
 * @param pa_sync
 */
#define GATTSERVICE_SUBEVENT_BASS_SOURCE_ADDED                                 0x29u

/**
 * @format 1H11
 * @param subevent_code
 * @param con_handle
 * @param source_id
 * @param pa_sync
 */
#define GATTSERVICE_SUBEVENT_BASS_SOURCE_MODIFIED                              0x30u

/**
 * @format 1H11
 * @param subevent_code
 * @param con_handle
 * @param source_id
 * @param pa_sync
 */
#define GATTSERVICE_SUBEVENT_BASS_SOURCE_DELETED                               0x31u

/**
 * @format 1H111122111421
 * @param subevent_code
 * @param con_handle
 * @param ase_id
 * @param target_latency
 * @param target_phy
 * @param coding_format
 * @param company_id
 * @param vendor_specific_codec_id
 * @param specific_codec_configuration_mask
 * @param sampling_frequency_index
 * @param frame_duration_index
 * @param audio_channel_allocation_mask
 * @param octets_per_frame 
 * @param frame_blocks_per_sdu 
*/
#define GATTSERVICE_SUBEVENT_ASCS_CODEC_CONFIGURATION_REQUEST                  0x32u

/**
 * @format 1H1113112123
 * @param subevent_code
 * @param con_handle
 * @param ase_id
 * @param cig_id
 * @param cis_id
 * @param sdu_interval
 * @param framing
 * @param phy
 * @param max_sdu
 * @param retransmission_number
 * @param max_transport_latency
 * @param presentation_delay_us
*/
#define GATTSERVICE_SUBEVENT_ASCS_QOS_CONFIGURATION                           0x033u

/**
 * @format 1H1122JV3JV1JV2JV2JV
 * @param subevent_code
 * @param con_handle
 * @param ase_id
 * @param metadata_mask
 * @param preferred_audio_contexts_mask
 * @param streaming_audio_contexts_mask
 * @param program_info_length
 * @param program_info
 * @param language_code
 * @param ccids_num
 * @param ccids
 * @param parental_rating
 * @param program_info_uri_length
 * @param program_info_uri
 * @param extended_metadata_type
 * @param extended_metadata_value_length
 * @param extended_metadata_value
 * @param vendor_specific_metadata_type
 * @param vendor_specific_metadata_value_length
 * @param vendor_specific_metadata_value
*/
#define GATTSERVICE_SUBEVENT_ASCS_METADATA                                     0x34u

/**
 * @format 1H1
 * @param subevent_code
 * @param con_handle
 * @param ase_id
*/ 
#define GATTSERVICE_SUBEVENT_ASCS_CLIENT_START_READY                           0x35u

/**
 * @format 1H1
 * @param subevent_code
 * @param con_handle
 * @param ase_id
*/ 
#define GATTSERVICE_SUBEVENT_ASCS_CLIENT_DISABLING                             0x36u

/**
 * @format 1H1
 * @param subevent_code
 * @param con_handle
 * @param ase_id
*/ 
#define GATTSERVICE_SUBEVENT_ASCS_CLIENT_RELEASING                             0x37u

/**
 * @format 1H1
 * @param subevent_code
 * @param con_handle
 * @param ase_id
*/ 
#define GATTSERVICE_SUBEVENT_ASCS_CLIENT_STOP_READY                            0x38u

/**
 * @format 1H1
 * @param subevent_code
 * @param con_handle
 * @param ase_id
*/
#define GATTSERVICE_SUBEVENT_ASCS_CLIENT_RELEASED                              0x39u

/**
 * @format 1H41
 * @param subevent_code
 * @param con_handle
 * @param audio_locations
 * @param role              see le_audio_role_t
*/
#define GATTSERVICE_SUBEVENT_PACS_AUDIO_LOCATION_RECEIVED                      0x42u

/**
 * @format 1H21
 * @param subevent_code
 * @param con_handle
 * @param bass_cid
 * @param status
*/
#define GATTSERVICE_SUBEVENT_BASS_CONNECTED                                    0x43u

/**
 * @format 12
 * @param subevent_code
 * @param bass_cid
*/
#define GATTSERVICE_SUBEVENT_BASS_DISCONNECTED                                 0x44u

/**
 * @format 1211
 * @param subevent_code
 * @param bass_cid
 * @param status
 * @param opcode
*/
#define GATTSERVICE_SUBEVENT_BASS_SCAN_OPERATION_COMPLETE                       0x45u

/**
 * @format 1211B1311P1
 * @param subevent_code
 * @param bass_cid
 * @param source_id
 * @param source_address_type
 * @param source_address
 * @param source_adv_sid
 * @param broadcast_id
 * @param pa_sync_state
 * @param big_encryption
 * @param bad_code
 * @param subgroups_num
*/
#define GATTSERVICE_SUBEVENT_BASS_NOTIFY_RECEIVE_STATE_BASE                     0x46u

/**
 * @format 1214122JV3JV1JV2JV2JV
 * @param subevent_code
 * @param bass_cid
 * @param source_id
 * @param bis_sync_state
 * @param metadata_mask
 * @param preferred_audio_contexts_mask
 * @param streaming_audio_contexts_mask
 * @param program_info_length
 * @param program_info
 * @param language_code
 * @param ccids_num
 * @param ccids
 * @param parental_rating
 * @param program_info_uri_length
 * @param program_info_uri
 * @param extended_metadata_type
 * @param extended_metadata_value_length
 * @param extended_metadata_value
 * @param vendor_specific_metadata_type
 * @param vendor_specific_metadata_value_length
 * @param vendor_specific_metadata_value
*/
#define GATTSERVICE_SUBEVENT_BASS_NOTIFY_RECEIVE_STATE_SUBGROUP                 0x47u

/**
 * @format 121
 * @param subevent_code
 * @param bass_cid
 * @param source_id
*/
#define GATTSERVICE_SUBEVENT_BASS_NOTIFICATION_COMPLETE                         0x48u

/**
 * @format 12111
 * @param subevent_code
 * @param bass_cid
 * @param status
 * @param opcode
 * @param source_id
*/
#define GATTSERVICE_SUBEVENT_BASS_SOURCE_OPERATION_COMPLETE                     0x49u

/**
 * @format 1H21
 * @param subevent_code
 * @param con_handle
 * @param pacs_cid
 * @param status
*/
#define GATTSERVICE_SUBEVENT_PACS_CONNECTED                                      0x50u

/**
 * @format 121
 * @param subevent_code
 * @param pacs_cid
 * @param status
*/
#define GATTSERVICE_SUBEVENT_PACS_OPERATION_DONE                                 0x51u

/**
 * @format 1214
 * @param subevent_code
 * @param pacs_cid
 * @param le_audio_role
 * @param audio_location_mask
*/
#define GATTSERVICE_SUBEVENT_PACS_AUDIO_LOCATIONS                               0x52u

/**
 * @format 1222
 * @param subevent_code
 * @param pacs_cid
 * @param sink_mask
 * @param source_mask
*/
#define GATTSERVICE_SUBEVENT_PACS_AVAILABLE_AUDIO_CONTEXTS                       0x53u

/**
 * @format 1222
 * @param subevent_code
 * @param pacs_cid
 * @param sink_mask
 * @param source_mask
*/
#define GATTSERVICE_SUBEVENT_PACS_SUPPORTED_AUDIO_CONTEXTS                       0x54u

/**
 * @format 1211221211221222JV3JV1JV2JV2JV
 * @param subevent_code
 * @param pacs_cid
 * @param le_audio_role
 * @param coding_format
 * @param company_id
 * @param vendor_specific_codec_id
 * @param codec_capability_mask
 * @param supported_sampling_frequencies_mask
 * @param supported_frame_durations_mask
 * @param supported_audio_channel_counts_mask
 * @param supported_octets_per_frame_min_num 
 * @param supported_octets_per_frame_max_num 
 * @param supported_max_codec_frames_per_sdu 
 * @param metadata_mask
 * @param preferred_audio_contexts_mask
 * @param streaming_audio_contexts_mask
 * @param program_info_length
 * @param program_info
 * @param language_code
 * @param ccids_num
 * @param ccids
 * @param parental_rating
 * @param program_info_uri_length
 * @param program_info_uri
 * @param extended_metadata_type
 * @param extended_metadata_value_length
 * @param extended_metadata_value
 * @param vendor_specific_metadata_type
 * @param vendor_specific_metadata_value_length
 * @param vendor_specific_metadata_value
*/

#define GATTSERVICE_SUBEVENT_PACS_PACK_RECORD                                    0x55u

/**
 * @format 121
 * @param subevent_code
 * @param pacs_cid
 * @param le_audio_role
*/
#define GATTSERVICE_SUBEVENT_PACS_PACK_RECORD_DONE                               0x56u


/**
 * @format 1H21
 * @param subevent_code
 * @param con_handle
 * @param ascs_cid
 * @param status
*/
#define GATTSERVICE_SUBEVENT_ASCS_CONNECTED                                      0x57u

/**
 * @format 12
 * @param subevent_code
 * @param ascs_cid
*/
#define GATTSERVICE_SUBEVENT_ASCS_DISCONNECTED                                   0x58u

/**
 * @format 1H111123333122111421
 * @param subevent_code
 * @param con_handle
 * @param ase_id
 * @param framing
 * @param preferred_phy
 * @param preferred_retransmission_number
 * @param max_transport_latency
 * @param presentation_delay_min
 * @param presentation_delay_max
 * @param preferred_presentation_delay_min
 * @param preferred_presentation_delay_max
 * @param coding_format
 * @param company_id
 * @param vendor_specific_codec_id
 * @param specific_codec_configuration_mask
 * @param sampling_frequency_index
 * @param frame_duration_index
 * @param audio_channel_allocation_mask
 * @param octets_per_frame 
 * @param frame_blocks_per_sdu 
*/
#define GATTSERVICE_SUBEVENT_ASCS_CODEC_CONFIGURATION                             0x59u

/**
 * @format 12
 * @param subevent_code
 * @param pacs_cid
*/
#define GATTSERVICE_SUBEVENT_PACS_DISCONNECTED                                    0x5Au

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
#define MAP_SUBEVENT_CONNECTION_OPENED                                    0x01u

/**
 * @format 12
 * @param subevent_code
 * @param map_cid
*/
#define MAP_SUBEVENT_CONNECTION_CLOSED                                    0x02u

/**
 * @format 121
 * @param subevent_code
 * @param map_cid
 * @param status
 */
#define MAP_SUBEVENT_OPERATION_COMPLETED                                  0x03u


/**
 * @format 12LV
 * @param subevent_code
 * @param map_cid
 * @param name_len
 * @param name
 */
#define MAP_SUBEVENT_FOLDER_LISTING_ITEM                                  0x04u

/**
 * @format 12D
 * @param subevent_code
 * @param map_cid
 * @param handle
 */
#define MAP_SUBEVENT_MESSAGE_LISTING_ITEM                                 0x05u

/**
 * @format 12
 * @param subevent_code
 * @param map_cid
 */
#define MAP_SUBEVENT_PARSING_DONE                                         0x06u


// MESH Meta Event Group

/**
 * @format 1
 * @param subevent_code
 */
#define MESH_SUBEVENT_CAN_SEND_NOW                                          0x01u

/**
 * @format 11
 * @param subevent_code
 * @param status
 */
#define MESH_SUBEVENT_PB_TRANSPORT_PDU_SENT                                          0x02u

/**
 * @format 1121
 * @param subevent_code
 * @param status
 * @param pb_transport_cid
 * @param pb_type
 */
#define MESH_SUBEVENT_PB_TRANSPORT_LINK_OPEN                                         0x03u

/**
 * @format 112
 * @param subevent_code
 * @param pb_transport_cid
 * @param reason
 */
#define MESH_SUBEVENT_PB_TRANSPORT_LINK_CLOSED                                       0x04u

/**
 * @format 121
 * @param subevent_code
 * @param pb_transport_cid
 * @param attention_time in seconds
 */
#define MESH_SUBEVENT_PB_PROV_ATTENTION_TIMER                                        0x10u

/**
 * Device Role
 * @format 12
 * @param subevent_code
 * @param pb_transport_cid
 */
#define MESH_SUBEVENT_PB_PROV_START_EMIT_PUBLIC_KEY_OOB                              0x11u

/**
 * Device Role
 * @format 12
 * @param subevent_code
 * @param pb_transport_cid
 */
#define MESH_SUBEVENT_PB_PROV_STOP_EMIT_PUBLIC_KEY_OOB                               0x12u

/**
 * Device Role
 * @format 12
 * @param subevent_code
 * @param pb_transport_cid
 */
#define MESH_SUBEVENT_PB_PROV_INPUT_OOB_REQUEST                                      0x13u

/**
 * Device Role
 * @format 124
 * @param subevent_code
 * @param pb_transport_cid
 * @param output_oob number
 */
#define MESH_SUBEVENT_PB_PROV_START_EMIT_OUTPUT_OOB                                  0x15u

/**
 * Device Role
 * @format 12
 * @param subevent_code
 * @param pb_transport_cid
 */
#define MESH_SUBEVENT_PB_PROV_STOP_EMIT_OUTPUT_OOB                                   0x16u

/**
 * Provisioner Role
 * @format 12
 * @param subevent_code
 * @param pb_transport_cid
 */
#define MESH_SUBEVENT_PB_PROV_START_RECEIVE_PUBLIC_KEY_OOB                           0x17u

/**
 * Provisioner Role
 * @format 12
 * @param subevent_code
 * @param pb_transport_cid
 */
#define MESH_SUBEVENT_PB_PROV_STOP_RECEIVE_PUBLIC_KEY_OOB                            0x18u

/**
 * Provisioner Role
 * @format 12
 * @param subevent_code
 * @param pb_transport_cid
 */
#define MESH_SUBEVENT_PB_PROV_OUTPUT_OOB_REQUEST                                     0x19u

/**
 * Provisioner Role
 * @format 124
 * @param subevent_code
 * @param pb_transport_cid
 * @param output_oob number
 */
#define MESH_SUBEVENT_PB_PROV_START_EMIT_INPUT_OOB                                   0x1au

/**
 * Provisioner Role
 * @format 12
 * @param subevent_code
 * @param pb_transport_cid
 */
#define MESH_SUBEVENT_PB_PROV_STOP_EMIT_INPUT_OOB                                    0x1bu

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
#define MESH_SUBEVENT_PB_PROV_CAPABILITIES                                           0x1cu

/**
 * @format 12
 * @param subevent_code
 * @param pb_transport_cid
 */
#define MESH_SUBEVENT_PB_PROV_COMPLETE                                               0x1du

/**
 * @format 11
 * @param subevent_code
 * @param attention_time in seconds
 */
#define MESH_SUBEVENT_ATTENTION_TIMER                                                0x1eu

/**
 * @format 1H
 * @param subevent_code
 * @param con_handle
 */
#define MESH_SUBEVENT_PROXY_CONNECTED                                                0x20u

/**
 * @format 1H
 * @param subevent_code
 * @param con_handle
 */
#define MESH_SUBEVENT_PROXY_PDU_SENT                                                 0x21u

/**
 * @format 1H
 * @param subevent_code
 * @param con_handle
 */
#define MESH_SUBEVENT_PROXY_DISCONNECTED                                             0x22u

/**
 * @format 1H
 * @param subevent_code
 * @param con_handle
 */
#define MESH_SUBEVENT_MESSAGE_SENT                                                   0x23u

/**
 * @format 114411
 * @param subevent_code
 * @param element_index
 * @param model_identifier
 * @param state_identifier
 * @param reason
 * @param value
 */
#define MESH_SUBEVENT_STATE_UPDATE_BOOL                                              0x24u

/**
 * @format 114412
 * @param subevent_code
 * @param element_index
 * @param model_identifier
 * @param state_identifier
 * @param reason
 * @param value
 */
#define MESH_SUBEVENT_STATE_UPDATE_INT16                                              0x25u

// Mesh Client Events
/**
 * @format 11442
 * @param subevent_code
 * @param element_index
 * @param model_identifier
 * @param opcode
 * @param dest
 */
#define MESH_SUBEVENT_MESSAGE_NOT_ACKNOWLEDGED                                        0x30u

/**
 * @format 121114
 * @param subevent_code
 * @param dest
 * @param status
 * @param present_value
 * @param target_value       optional, if value > 0, than remaining_time_ms must be read
 * @param remaining_time_ms  
 */
#define MESH_SUBEVENT_GENERIC_ON_OFF                                                  0x31u

/**
 * @format 121224
 * @param subevent_code
 * @param dest
 * @param status
 * @param present_value
 * @param target_value       optional, if value > 0, than remaining_time_ms must be read
 * @param remaining_time_ms  
 */
#define MESH_SUBEVENT_GENERIC_LEVEL                                                   0x32u

/**
 * @format 1222211
 * @param subevent_code
 * @param dest
 * @param netkey_index
 * @param appkey_index
 * @param company_id
 * @param test_id
 * @param acknowledged
 */
#define MESH_SUBEVENT_HEALTH_PERFORM_TEST                                            0x33u

/**
 * @format 11
 * @param subevent_code
 * @param element_index
 */
#define MESH_SUBEVENT_HEALTH_ATTENTION_TIMER_CHANGED                                 0x34u

/**
 * @format 1211
 * @param subevent_code
 * @param dest
 * @param status
 * @param transition_time_gdtt  
 */
#define MESH_SUBEVENT_GENERIC_DEFAULT_TRANSITION_TIME                                0x35u

/**
 * @format 1211
 * @param subevent_code
 * @param dest
 * @param foundation_status
 * @param secure_network_beacon_state  
 */
#define MESH_SUBEVENT_CONFIGURATION_BEACON                                           0x36u

// Composition Data has variable of element descriptions, with two lists of model lists
// Use .. getters to access data
#define MESH_SUBEVENT_CONFIGURATION_COMPOSITION_DATA                                 0x37u

/**
 * @format 1211
 * @param subevent_code
 * @param dest
 * @param foundation_status
 * @param default_ttl  
 */
#define MESH_SUBEVENT_CONFIGURATION_DEFAULT_TTL                                      0x38u

/**
 * @format 1211
 * @param subevent_code
 * @param dest
 * @param foundation_status
 * @param gatt_proxy_state  
 */
#define MESH_SUBEVENT_CONFIGURATION_GATT_PROXY                                       0x39u

/**
 * @format 121111
 * @param subevent_code
 * @param dest
 * @param foundation_status
 * @param relay
 * @param retransmit_count                          the number of times that packet is transmitted for each packet that is relayed.
 * @param retransmit_interval_ms                    retransmission interval in ms
 */
#define MESH_SUBEVENT_CONFIGURATION_RELAY                                            0x40u


/**
 * @format 12122111114
 * @param subevent_code
 * @param dest                                      element_address
 * @param foundation_status
 * @param publish_address
 * @param appkey_index
 * @param credential_flag
 * @param publish_ttl
 * @param publish_period
 * @param publish_retransmit_count
 * @param publish_retransmit_interval_steps
 * @param model_identifier
 */
#define MESH_SUBEVENT_CONFIGURATION_MODEL_PUBLICATION                                0x41u

/**
 * @format 12124
 * @param subevent_code
 * @param dest
 * @param foundation_status
 * @param address  
 * @param model_identifier
 */
#define MESH_SUBEVENT_CONFIGURATION_MODEL_SUBSCRIPTION                               0x42u

/**
 * @format 1214112
 * @param subevent_code
 * @param dest
 * @param foundation_status
 * @param model_identifier
 * @param num_subscription_addresses
 * @param subscription_address_pos
 * @param subscription_address_item
 */
#define MESH_SUBEVENT_CONFIGURATION_MODEL_SUBSCRIPTION_LIST_ITEM                     0x43u   


/**
 * @format 121
 * @param subevent_code
 * @param dest
 * @param foundation_status
 */
#define MESH_SUBEVENT_CONFIGURATION_NETKEY_INDEX                                      0x44u

/**
 * @format 121112
 * @param subevent_code
 * @param dest
 * @param foundation_status
 * @param num_netkey_indexes
 * @param netkey_index_pos
 * @param netkey_index_item
 */
#define MESH_SUBEVENT_CONFIGURATION_NETKEY_INDEX_LIST_ITEM                             0x45u

/**
 * @format 12122
 * @param subevent_code
 * @param dest
 * @param foundation_status
 * @param netkey_index_item
 * @param appkey_index_item
 */
#define MESH_SUBEVENT_CONFIGURATION_APPKEY_INDEX                                       0x46u

/**
 * @format 12121122
 * @param subevent_code
 * @param dest
 * @param foundation_status
 * @param netkey_index
 * @param num_appkey_indexes
 * @param appkey_index_pos
 * @param netkey_index_item
 * @param appkey_index_item
 */
 #define MESH_SUBEVENT_CONFIGURATION_APPKEY_INDEX_LIST_ITEM                            0x47u

/**
 * @format 12121
 * @param subevent_code
 * @param dest
 * @param foundation_status
 * @param netkey_index_item
 * @param identity_status
 */
#define MESH_SUBEVENT_CONFIGURATION_NODE_IDENTITY                                      0x48u

/**
 * @format 12124
 * @param subevent_code
 * @param dest
 * @param foundation_status
 * @param appkey_index
 * @param model_identifier
 */
#define MESH_SUBEVENT_CONFIGURATION_MODEL_APP                                           0x49u

/**
 * @format 1214112
 * @param subevent_code
 * @param dest
 * @param foundation_status
 * @param model_id
 * @param num_appkey_indexes
 * @param appkey_index_pos
 * @param appkey_index_item
 */
#define MESH_SUBEVENT_CONFIGURATION_MODEL_APP_LIST_ITEM                                 0x50u

/**
 * @format 121
 * @param subevent_code
 * @param dest
 * @param foundation_status
 */
#define MESH_SUBEVENT_CONFIGURATION_NODE_RESET                                          0x51u

/**
 * @format 1211
 * @param subevent_code
 * @param dest
 * @param foundation_status
 * @param friend_state
 */
#define MESH_SUBEVENT_CONFIGURATION_FRIEND                                              0x52u

/**
 * @format 12121
 * @param subevent_code
 * @param dest
 * @param foundation_status
 * @param netkey_index
 * @param phase
 */
#define MESH_SUBEVENT_CONFIGURATION_KEY_REFRESH_PHASE                                   0x53u

/**
 * @format 121222122
 * @param subevent_code
 * @param dest
 * @param foundation_status
 * @param heartbeat_destination
 * @param count_S
 * @param period_S
 * @param ttl
 * @param features
 * @param netkey_index
 */
#define MESH_SUBEVENT_CONFIGURATION_HEARTBEAT_PUBLICATION                               0x54u

/**
 * @format 121222211
 * @param subevent_code
 * @param dest
 * @param foundation_status
 * @param heartbeat_destination
 * @param heartbeat_source
 * @param count_S
 * @param period_S
 * @param min_hops
 * @param max_hops
 */
#define MESH_SUBEVENT_CONFIGURATION_HEARTBEAT_SUBSCRIPTION                              0x55u

/**
 * @format 12123
 * @param subevent_code
 * @param dest
 * @param foundation_status
 * @param lpn_address
 * @param poll_timeout
 */
#define MESH_SUBEVENT_CONFIGURATION_LOW_POWER_NODE_POLL_TIMEOUT                         0x56u

/**
 * @format 12112
 * @param subevent_code
 * @param dest
 * @param foundation_status
 * @param transmit_count
 * @param transmit_interval_steps_ms
 */
#define MESH_SUBEVENT_CONFIGURATION_NETWORK_TRANSMIT                                    0x57u


#endif
