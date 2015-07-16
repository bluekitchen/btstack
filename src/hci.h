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
 *  hci.h
 *
 *  Created by Matthias Ringwald on 4/29/09.
 *
 */

#ifndef __HCI_H
#define __HCI_H

#include "btstack-config.h"

#include <btstack/hci_cmds.h>
#include <btstack/utils.h>
#include "hci_transport.h"
#include "bt_control.h"
#include "remote_device_db.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <btstack/linked_list.h>

#if defined __cplusplus
extern "C" {
#endif
    
// packet header sizes
#define HCI_CMD_HEADER_SIZE          3
#define HCI_ACL_HEADER_SIZE   	     4
#define HCI_SCO_HEADER_SIZE  	     3
#define HCI_EVENT_HEADER_SIZE        2

// packet sizes (max payload)
#define HCI_ACL_DM1_SIZE            17
#define HCI_ACL_DH1_SIZE            27
#define HCI_ACL_2DH1_SIZE           54
#define HCI_ACL_3DH1_SIZE           83
#define HCI_ACL_DM3_SIZE           121
#define HCI_ACL_DH3_SIZE           183
#define HCI_ACL_DM5_SIZE           224
#define HCI_ACL_DH5_SIZE           339
#define HCI_ACL_2DH3_SIZE          367
#define HCI_ACL_3DH3_SIZE          552
#define HCI_ACL_2DH5_SIZE          679
#define HCI_ACL_3DH5_SIZE         1021
       
#define HCI_EVENT_PAYLOAD_SIZE     255
#define HCI_CMD_PAYLOAD_SIZE       255

#define LE_ADVERTISING_DATA_SIZE    31
    
// packet buffer sizes
// HCI_ACL_PAYLOAD_SIZE is configurable and defined in config.h
#define HCI_EVENT_BUFFER_SIZE      (HCI_EVENT_HEADER_SIZE + HCI_EVENT_PAYLOAD_SIZE)
#define HCI_CMD_BUFFER_SIZE        (HCI_CMD_HEADER_SIZE   + HCI_CMD_PAYLOAD_SIZE)
#define HCI_ACL_BUFFER_SIZE        (HCI_ACL_HEADER_SIZE   + HCI_ACL_PAYLOAD_SIZE)
    
// size of hci buffers, big enough for command, event, or acl packet without H4 packet type
// @note cmd buffer is bigger than event buffer
#ifdef HCI_PACKET_BUFFER_SIZE
    #if HCI_PACKET_BUFFER_SIZE < HCI_ACL_BUFFER_SIZE
        #error HCI_PACKET_BUFFER_SIZE must be equal or larger than HCI_ACL_BUFFER_SIZE
    #endif
    #if HCI_PACKET_BUFFER_SIZE < HCI_CMD_BUFFER_SIZE
        #error HCI_PACKET_BUFFER_SIZE must be equal or larger than HCI_CMD_BUFFER_SIZE
    #endif
#else
    #if HCI_ACL_BUFFER_SIZE > HCI_CMD_BUFFER_SIZE
        #define HCI_PACKET_BUFFER_SIZE HCI_ACL_BUFFER_SIZE
    #else
        #define HCI_PACKET_BUFFER_SIZE HCI_CMD_BUFFER_SIZE
    #endif
#endif

// additional pre-buffer space for packets to Bluetooth module, for now, used for HCI Transport H4 DMA
#define HCI_OUTGOING_PRE_BUFFER_SIZE 1

// BNEP may uncompress the IP Header by 16 bytes
#ifdef HAVE_BNEP
#define HCI_INCOMING_PRE_BUFFER_SIZE (16 - HCI_ACL_HEADER_SIZE - 4)
#endif 
#ifndef HCI_INCOMING_PRE_BUFFER_SIZE
    #define HCI_INCOMING_PRE_BUFFER_SIZE 0
#endif

// OGFs
#define OGF_LINK_CONTROL          0x01
#define OGF_LINK_POLICY           0x02
#define OGF_CONTROLLER_BASEBAND   0x03
#define OGF_INFORMATIONAL_PARAMETERS 0x04
#define OGF_STATUS_PARAMETERS     0x05
#define OGF_LE_CONTROLLER 0x08
#define OGF_BTSTACK 0x3d
#define OGF_VENDOR  0x3f

// cmds for BTstack 
// get state: @returns HCI_STATE
#define BTSTACK_GET_STATE                                  0x01

// set power mode: @param HCI_POWER_MODE
#define BTSTACK_SET_POWER_MODE                             0x02

// set capture mode: @param on
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

// create l2cap channel: @param bd_addr(48), psm (16)
#define L2CAP_CREATE_CHANNEL                               0x20

// disconnect l2cap disconnect, @param channel(16), reason(8)
#define L2CAP_DISCONNECT                                   0x21

// register l2cap service: @param psm(16), mtu (16)
#define L2CAP_REGISTER_SERVICE                             0x22

// unregister l2cap disconnect, @param psm(16)
#define L2CAP_UNREGISTER_SERVICE                           0x23

// accept connection @param bd_addr(48), dest cid (16)
#define L2CAP_ACCEPT_CONNECTION                            0x24

// decline l2cap disconnect,@param bd_addr(48), dest cid (16), reason(8)
#define L2CAP_DECLINE_CONNECTION                           0x25

// create l2cap channel: @param bd_addr(48), psm (16), mtu (16)
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
#define RFCOMM_DISCONNECT			0x41
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

// 
#define IS_COMMAND(packet, command) (READ_BT_16(packet,0) == command.opcode)


/**
 * LE connection parameter update state
 */ 

typedef enum {
    CON_PARAMETER_UPDATE_NONE,
    CON_PARAMETER_UPDATE_SEND_RESPONSE,
    CON_PARAMETER_UPDATE_CHANGE_HCI_CON_PARAMETERS,
    CON_PARAMETER_UPDATE_DENY
} le_con_parameter_update_state_t;

typedef struct le_connection_parameter_range{
    uint16_t le_conn_interval_min;
    uint16_t le_conn_interval_max;
    uint16_t le_conn_latency_min;
    uint16_t le_conn_latency_max;
    uint16_t le_supervision_timeout_min;
    uint16_t le_supervision_timeout_max;
} le_connection_parameter_range_t;

// Authentication flags
typedef enum {
    AUTH_FLAGS_NONE                = 0x0000,
    RECV_LINK_KEY_REQUEST          = 0x0001,
    HANDLE_LINK_KEY_REQUEST        = 0x0002,
    SENT_LINK_KEY_REPLY            = 0x0004,
    SENT_LINK_KEY_NEGATIVE_REQUEST = 0x0008,
    RECV_LINK_KEY_NOTIFICATION     = 0x0010,
    DENY_PIN_CODE_REQUEST          = 0x0040,
    RECV_IO_CAPABILITIES_REQUEST   = 0x0080,
    SEND_IO_CAPABILITIES_REPLY     = 0x0100,
    SEND_USER_CONFIRM_REPLY        = 0x0200,
    SEND_USER_PASSKEY_REPLY        = 0x0400,

    // pairing status
    LEGACY_PAIRING_ACTIVE          = 0x2000,
    SSP_PAIRING_ACTIVE             = 0x4000,

    // connection status
    CONNECTION_ENCRYPTED           = 0x8000,
} hci_authentication_flags_t;

/**
 * Connection State 
 */
typedef enum {
    SEND_CREATE_CONNECTION = 0,
    SENT_CREATE_CONNECTION,
    SEND_CANCEL_CONNECTION,
    SENT_CANCEL_CONNECTION,
    RECEIVED_CONNECTION_REQUEST,
    ACCEPTED_CONNECTION_REQUEST,
    REJECTED_CONNECTION_REQUEST,
    OPEN,
    SEND_DISCONNECT,
    SENT_DISCONNECT,
    RECEIVED_DISCONNECTION_COMPLETE
} CONNECTION_STATE;

// bonding flags
enum {
    BONDING_REQUEST_REMOTE_FEATURES   = 0x01,
    BONDING_RECEIVED_REMOTE_FEATURES  = 0x02,
    BONDING_REMOTE_SUPPORTS_SSP       = 0x04,
    BONDING_DISCONNECT_SECURITY_BLOCK = 0x08,
    BONDING_DISCONNECT_DEDICATED_DONE = 0x10,
    BONDING_SEND_AUTHENTICATE_REQUEST = 0x20,
    BONDING_SEND_ENCRYPTION_REQUEST   = 0x40,
    BONDING_DEDICATED                 = 0x80,
    BONDING_EMIT_COMPLETE_ON_DISCONNECT = 0x100
};

typedef enum {
    BLUETOOTH_OFF = 1,
    BLUETOOTH_ON,
    BLUETOOTH_ACTIVE
} BLUETOOTH_STATE;

// le central scanning state
typedef enum {
    LE_SCAN_IDLE,
    LE_START_SCAN,
    LE_SCANNING,
    LE_STOP_SCAN,
} le_scanning_state_t;

//
// SM internal types and globals
//

typedef enum {

    // general states
    // state = 0
    SM_GENERAL_IDLE,
    SM_GENERAL_SEND_PAIRING_FAILED,
    SM_GENERAL_TIMEOUT, // no other security messages are exchanged

    // Phase 1: Pairing Feature Exchange
    SM_PH1_W4_USER_RESPONSE,

    // Phase 2: Authenticating and Encrypting

    // get random number for use as TK Passkey if we show it 
    SM_PH2_GET_RANDOM_TK,
    SM_PH2_W4_RANDOM_TK,

    // get local random number for confirm c1
    SM_PH2_C1_GET_RANDOM_A,
    SM_PH2_C1_W4_RANDOM_A,
    SM_PH2_C1_GET_RANDOM_B,
    SM_PH2_C1_W4_RANDOM_B,

    // calculate confirm value for local side
    // state = 10
    SM_PH2_C1_GET_ENC_A,
    SM_PH2_C1_W4_ENC_A,
    SM_PH2_C1_GET_ENC_B,
    SM_PH2_C1_W4_ENC_B,

    // calculate confirm value for remote side
    SM_PH2_C1_GET_ENC_C,
    SM_PH2_C1_W4_ENC_C,
    SM_PH2_C1_GET_ENC_D,
    SM_PH2_C1_W4_ENC_D,

    SM_PH2_C1_SEND_PAIRING_CONFIRM,
    SM_PH2_SEND_PAIRING_RANDOM,

    // calc STK
    // state = 20
    SM_PH2_CALC_STK,
    SM_PH2_W4_STK,

    SM_PH2_W4_CONNECTION_ENCRYPTED,

    // Phase 3: Transport Specific Key Distribution
    
    // calculate DHK, Y, EDIV, and LTK
    SM_PH3_GET_RANDOM,
    SM_PH3_W4_RANDOM,
    SM_PH3_GET_DIV,
    SM_PH3_W4_DIV,
    SM_PH3_Y_GET_ENC,
    SM_PH3_Y_W4_ENC,
    SM_PH3_LTK_GET_ENC,
    // state = 30
    SM_PH3_LTK_W4_ENC,
    SM_PH3_CSRK_GET_ENC,
    SM_PH3_CSRK_W4_ENC,

    // exchange keys
    SM_PH3_DISTRIBUTE_KEYS,
    SM_PH3_RECEIVE_KEYS,

    // Phase 4: re-establish previously distributed LTK
    SM_PH4_Y_GET_ENC,
    SM_PH4_Y_W4_ENC,
    SM_PH4_LTK_GET_ENC,
    SM_PH4_LTK_W4_ENC,
    SM_PH4_SEND_LTK,

    // RESPONDER ROLE
    // state = 40
    SM_RESPONDER_SEND_SECURITY_REQUEST,
    SM_RESPONDER_PH0_RECEIVED_LTK,
    SM_RESPONDER_PH0_SEND_LTK_REQUESTED_NEGATIVE_REPLY,
    SM_RESPONDER_PH1_W4_PAIRING_REQUEST,
    SM_RESPONDER_PH1_PAIRING_REQUEST_RECEIVED,
    SM_RESPONDER_PH1_SEND_PAIRING_RESPONSE,
    SM_RESPONDER_PH1_W4_PAIRING_CONFIRM,
    SM_RESPONDER_PH2_W4_PAIRING_RANDOM,
    SM_RESPONDER_PH2_W4_LTK_REQUEST,
    SM_RESPONDER_PH2_SEND_LTK_REPLY,

    // INITITIATOR ROLE
    SM_INITIATOR_CONNECTED,
    SM_INITIATOR_PH0_HAS_LTK,
    SM_INITIATOR_PH0_SEND_START_ENCRYPTION,
    SM_INITIATOR_PH0_W4_CONNECTION_ENCRYPTED,
    SM_INITIATOR_PH1_W2_SEND_PAIRING_REQUEST,
    SM_INITIATOR_PH1_SEND_PAIRING_REQUEST,
    SM_INITIATOR_PH1_W4_PAIRING_RESPONSE,
    SM_INITIATOR_PH2_W4_PAIRING_CONFIRM,
    SM_INITIATOR_PH2_W4_PAIRING_RANDOM,
    SM_INITIATOR_PH3_SEND_START_ENCRYPTION,

} security_manager_state_t;

typedef enum {
    CSRK_LOOKUP_IDLE,
    CSRK_LOOKUP_W4_READY,
    CSRK_LOOKUP_STARTED,
} csrk_lookup_state_t;

// Authorization state
typedef enum {
    AUTHORIZATION_UNKNOWN,
    AUTHORIZATION_PENDING,
    AUTHORIZATION_DECLINED,
    AUTHORIZATION_GRANTED
} authorization_state_t;

typedef struct sm_pairing_packet {
    uint8_t code;
    uint8_t io_capability;
    uint8_t oob_data_flag;
    uint8_t auth_req;
    uint8_t max_encryption_key_size;
    uint8_t initiator_key_distribution;
    uint8_t responder_key_distribution;
} sm_pairing_packet_t;

// connection info available as long as connection exists
typedef struct sm_connection {
    uint16_t                 sm_handle;
    uint8_t                  sm_role;   // 0 - IamMaster, 1 = IamSlave
    bd_addr_t                sm_peer_address;
    uint8_t                  sm_peer_addr_type;
    security_manager_state_t sm_engine_state;
    csrk_lookup_state_t      sm_csrk_lookup_state;
    uint8_t                  sm_connection_encrypted;
    uint8_t                  sm_connection_authenticated;   // [0..1]
    uint8_t                  sm_actual_encryption_key_size;
    sm_pairing_packet_t      sm_m_preq;  // only used during c1
    authorization_state_t    sm_connection_authorization_state;
    uint16_t                 sm_local_ediv;
    uint8_t                  sm_local_rand[8];
    int                      sm_le_db_index;
} sm_connection_t;

typedef struct {
    // linked list - assert: first field
    linked_item_t    item;
    
    // remote side
    bd_addr_t address;
    
    // module handle
    hci_con_handle_t con_handle;

    // le public, le random, classic
    bd_addr_type_t address_type;

    // connection state
    CONNECTION_STATE state;
    
    // bonding
    uint16_t bonding_flags;
    uint8_t  bonding_status;
    // requested security level
    gap_security_level_t requested_security_level;

    // 
    link_key_type_t link_key_type;

    // errands
    uint32_t authentication_flags;

    timer_source_t timeout;
    
#ifdef HAVE_TIME
    // timer
    struct timeval timestamp;
#endif
#ifdef HAVE_TICK
    uint32_t timestamp; // timeout in system ticks
#endif
    
    // ACL packet recombination - PRE_BUFFER + ACL Header + ACL payload
    uint8_t  acl_recombination_buffer[HCI_INCOMING_PRE_BUFFER_SIZE + 4 + HCI_ACL_BUFFER_SIZE];
    uint16_t acl_recombination_pos;
    uint16_t acl_recombination_length;
    
    // number packets sent to controller
    uint8_t num_acl_packets_sent;
    uint8_t num_sco_packets_sent;

    // LE Connection parameter update
    le_con_parameter_update_state_t le_con_parameter_update_state;
    uint16_t le_conn_interval_min;
    uint16_t le_conn_interval_max;
    uint16_t le_conn_latency;
    uint16_t le_supervision_timeout;
    uint16_t le_update_con_parameter_response;

#ifdef HAVE_BLE
    // LE Security Manager
    sm_connection_t sm_connection;
#endif

} hci_connection_t;


/** 
 * HCI Inititizlization State Machine
 */
typedef enum hci_init_state{
    HCI_INIT_SEND_RESET = 0,
    HCI_INIT_W4_SEND_RESET,
    HCI_INIT_SEND_READ_LOCAL_VERSION_INFORMATION,
    HCI_INIT_W4_SEND_READ_LOCAL_VERSION_INFORMATION,

    HCI_INIT_SET_BD_ADDR,
    HCI_INIT_W4_SET_BD_ADDR,

    HCI_INIT_SEND_RESET_ST_WARM_BOOT,
    HCI_INIT_W4_SEND_RESET_ST_WARM_BOOT,

    HCI_INIT_SEND_BAUD_CHANGE,
    HCI_INIT_W4_SEND_BAUD_CHANGE,
    HCI_INIT_CUSTOM_INIT,
    HCI_INIT_W4_CUSTOM_INIT,
    HCI_INIT_SEND_RESET_CSR_WARM_BOOT,
    HCI_INIT_W4_CUSTOM_INIT_CSR_WARM_BOOT,

    HCI_INIT_READ_BD_ADDR,
    HCI_INIT_W4_READ_BD_ADDR,
    HCI_INIT_READ_LOCAL_SUPPORTED_COMMANDS,
    HCI_INIT_W4_READ_LOCAL_SUPPORTED_COMMANDS,

    HCI_INIT_READ_BUFFER_SIZE,
    HCI_INIT_W4_READ_BUFFER_SIZE,
    HCI_INIT_READ_LOCAL_SUPPORTED_FEATUES,
    HCI_INIT_W4_READ_LOCAL_SUPPORTED_FEATUES,
    HCI_INIT_SET_EVENT_MASK,
    HCI_INIT_W4_SET_EVENT_MASK,
    HCI_INIT_WRITE_SIMPLE_PAIRING_MODE,
    HCI_INIT_W4_WRITE_SIMPLE_PAIRING_MODE,
    HCI_INIT_WRITE_PAGE_TIMEOUT,
    HCI_INIT_W4_WRITE_PAGE_TIMEOUT,

    HCI_INIT_WRITE_CLASS_OF_DEVICE,
    HCI_INIT_W4_WRITE_CLASS_OF_DEVICE,
    HCI_INIT_WRITE_LOCAL_NAME,
    HCI_INIT_W4_WRITE_LOCAL_NAME,
    HCI_INIT_WRITE_SCAN_ENABLE,
    HCI_INIT_W4_WRITE_SCAN_ENABLE,
    HCI_INIT_LE_READ_BUFFER_SIZE,
    HCI_INIT_W4_LE_READ_BUFFER_SIZE,
    HCI_INIT_WRITE_LE_HOST_SUPPORTED,
    HCI_INIT_W4_WRITE_LE_HOST_SUPPORTED,

    HCI_INIT_LE_SET_SCAN_PARAMETERS,
    HCI_INIT_W4_LE_SET_SCAN_PARAMETERS,

    HCI_INIT_DONE,

    HCI_FALLING_ASLEEP_DISCONNECT,
    HCI_FALLING_ASLEEP_W4_WRITE_SCAN_ENABLE,
    HCI_FALLING_ASLEEP_COMPLETE,

    HCI_INIT_AFTER_SLEEP

} hci_substate_t;

enum {
    LE_ADVERTISEMENT_TASKS_DISABLE     = 1 << 0,
    LE_ADVERTISEMENT_TASKS_SET_DATA    = 1 << 1,
    LE_ADVERTISEMENT_TASKS_SET_PARAMS  = 1 << 2,
    LE_ADVERTISEMENT_TASKS_ENABLE      = 1 << 4,
};

/**
 * main data structure
 */
typedef struct {
    // transport component with configuration
    hci_transport_t  * hci_transport;
    void             * config;
    
    // basic configuration
    const char         * local_name;
    uint32_t           class_of_device;
    bd_addr_t          local_bd_addr;
    uint8_t            ssp_enable;
    uint8_t            ssp_io_capability;
    uint8_t            ssp_authentication_requirement;
    uint8_t            ssp_auto_accept;

    // hardware power controller
    bt_control_t     * control;
    
    // list of existing baseband connections
    linked_list_t     connections;

    // single buffer for HCI packet assembly + additional prebuffer for H4 drivers
    uint8_t   hci_packet_buffer_prefix[HCI_OUTGOING_PRE_BUFFER_SIZE];
    uint8_t   hci_packet_buffer[HCI_PACKET_BUFFER_SIZE]; // opcode (16), len(8)
    uint8_t   hci_packet_buffer_reserved;
    uint16_t  acl_fragmentation_pos;
    uint16_t  acl_fragmentation_total_size;
     
    /* host to controller flow control */
    uint8_t  num_cmd_packets;
    uint8_t  acl_packets_total_num;
    uint16_t acl_data_packet_length;
    uint8_t  sco_packets_total_num;
    uint8_t  sco_data_packet_length;
    uint8_t  le_acl_packets_total_num;
    uint16_t le_data_packets_length;

    /* local supported features */
    uint8_t local_supported_features[8];

    /* local supported commands summary - complete info is 64 bytes */
    /* 0 - read buffer size */
    /* 1 - write le host supported */
    uint8_t local_supported_commands[1];

    /* bluetooth device information from hci read local version information */
    // uint16_t hci_version;
    // uint16_t hci_revision;
    // uint16_t lmp_version;
    uint16_t manufacturer;
    // uint16_t lmp_subversion;

    // usable packet types given acl_data_packet_length and HCI_ACL_BUFFER_SIZE
    uint16_t packet_types;
    
    /* callback to L2CAP layer */
    void (*packet_handler)(uint8_t packet_type, uint8_t *packet, uint16_t size);

    /* remote device db */
    remote_device_db_t const*remote_device_db;
    
    /* hci state machine */
    HCI_STATE      state;
    hci_substate_t substate;
    timer_source_t timeout;
    uint8_t   cmds_ready;
    
    uint16_t  last_cmd_opcode;

    uint8_t   discoverable;
    uint8_t   connectable;
    uint8_t   bondable;

    /* buffer for scan enable cmd - 0xff no change */
    uint8_t   new_scan_enable_value;
    
    // buffer for single connection decline
    uint8_t   decline_reason;
    bd_addr_t decline_addr;

    uint8_t   adv_addr_type;
    bd_addr_t adv_address;

    le_scanning_state_t le_scanning_state;

    // buffer for le scan type command - 0xff not set
    uint8_t  le_scan_type;
    uint16_t le_scan_interval;  
    uint16_t le_scan_window;

    le_connection_parameter_range_t le_connection_parameter_range;

    uint8_t  * le_advertisements_data;
    uint8_t    le_advertisements_data_len;

    uint8_t  le_advertisements_active;
    uint8_t  le_advertisements_enabled;
    uint8_t  le_advertisements_todo;

    uint16_t le_advertisements_interval_min;
    uint16_t le_advertisements_interval_max;
    uint8_t  le_advertisements_type;
    uint8_t  le_advertisements_own_address_type;
    uint8_t  le_advertisements_direct_address_type;
    uint8_t  le_advertisements_channel_map;
    uint8_t  le_advertisements_filter_policy;
    bd_addr_t le_advertisements_direct_address;

    // custom BD ADDR
    bd_addr_t custom_bd_addr; 
    uint8_t   custom_bd_addr_set;

    // hardware error handler
    void (*hardware_error_callback)(void);

} hci_stack_t;

/**
 * set connection iterator
 */
void hci_connections_get_iterator(linked_list_iterator_t *it);

// create and send hci command packets based on a template and a list of parameters
uint16_t hci_create_cmd(uint8_t *hci_cmd_buffer, hci_cmd_t *cmd, ...);
uint16_t hci_create_cmd_internal(uint8_t *hci_cmd_buffer, const hci_cmd_t *cmd, va_list argptr);

/**
 * run the hci control loop once
 */
void hci_run(void);

// send ACL packet prepared in hci packet buffer
int hci_send_acl_packet_buffer(int size);

// send SCO packet prepared in hci packet buffer
int hci_send_sco_packet_buffer(int size);


int hci_can_send_acl_packet_now(hci_con_handle_t con_handle);
int hci_can_send_prepared_acl_packet_now(hci_con_handle_t con_handle);
int hci_can_send_sco_packet_now(hci_con_handle_t con_handle);
int hci_can_send_prepared_sco_packet_now(hci_con_handle_t con_handle);

// reserves outgoing packet buffer. @returns 1 if successful
int  hci_reserve_packet_buffer(void);
void hci_release_packet_buffer(void);

// used for internal checks in l2cap[-le].c
int hci_is_packet_buffer_reserved(void);

// get point to packet buffer
uint8_t* hci_get_outgoing_packet_buffer(void);


hci_connection_t * hci_connection_for_handle(hci_con_handle_t con_handle);
hci_connection_t * hci_connection_for_bd_addr_and_type(bd_addr_t addr, bd_addr_type_t addr_type);
int      hci_is_le_connection(hci_connection_t * connection);
uint8_t  hci_number_outgoing_packets(hci_con_handle_t handle);
uint8_t  hci_number_free_acl_slots_for_handle(hci_con_handle_t con_handle);
int      hci_authentication_active_for_handle(hci_con_handle_t handle);
uint16_t hci_max_acl_data_packet_length(void);
uint16_t hci_max_acl_le_data_packet_length(void);
uint16_t hci_usable_acl_packet_types(void);
int      hci_non_flushable_packet_boundary_flag_supported(void);

void hci_disconnect_all(void);

void hci_emit_state(void);
void hci_emit_connection_complete(hci_connection_t *conn, uint8_t status);
void hci_emit_l2cap_check_timeout(hci_connection_t *conn);
void hci_emit_disconnection_complete(uint16_t handle, uint8_t reason);
void hci_emit_nr_connections_changed(void);
void hci_emit_hci_open_failed(void);
void hci_emit_btstack_version(void);
void hci_emit_system_bluetooth_enabled(uint8_t enabled);
void hci_emit_remote_name_cached(bd_addr_t addr, device_name_t *name);
void hci_emit_discoverable_enabled(uint8_t enabled);
void hci_emit_security_level(hci_con_handle_t con_handle, gap_security_level_t level);
void hci_emit_dedicated_bonding_result(bd_addr_t address, uint8_t status);

// query if the local side supports SSP
int hci_local_ssp_activated(void);

// query if the remote side supports SSP
int hci_remote_ssp_supported(hci_con_handle_t con_handle);

// query if both sides support SSP
int hci_ssp_supported_on_both_sides(hci_con_handle_t handle);

// disable automatic L2CAP disconnect for testing
void hci_disable_l2cap_timeout_check(void);

// disconnect because of security block
void hci_disconnect_security_block(hci_con_handle_t con_handle);

// send complete CMD packet
int hci_send_cmd_packet(uint8_t *packet, int size);


/* API_START */

le_connection_parameter_range_t gap_le_get_connection_parameter_range(void);
void gap_le_set_connection_parameter_range(le_connection_parameter_range_t range);

/* LE Client Start */

le_command_status_t le_central_start_scan(void);
le_command_status_t le_central_stop_scan(void);
le_command_status_t le_central_connect(bd_addr_t addr, bd_addr_type_t addr_type);
le_command_status_t le_central_connect_cancel(void);
le_command_status_t gap_disconnect(hci_con_handle_t handle);
void le_central_set_scan_parameters(uint8_t scan_type, uint16_t scan_interval, uint16_t scan_window);

/* LE Client End */
    
void hci_connectable_control(uint8_t enable);
void hci_close(void);

/** 
 * @note New functions replacing: hci_can_send_packet_now[_using_packet_buffer]
 */
int hci_can_send_command_packet_now(void);
    
/**
 * @brief Gets local address.
 */
void hci_local_bd_addr(bd_addr_t address_buffer);

/**
 * @brief Set up HCI. Needs to be called before any other function.
 */
void hci_init(hci_transport_t *transport, void *config, bt_control_t *control, remote_device_db_t const* remote_device_db);

/**
 * @brief Set class of device that will be set during Bluetooth init.
 */
void hci_set_class_of_device(uint32_t class_of_device);

/**
 * @brief Set Public BD ADDR - passed on to Bluetooth chipset if supported in bt_control_h
 */
void hci_set_bd_addr(bd_addr_t addr);

/**
 * @brief Registers a packet handler. Used if L2CAP is not used (rarely). 
 */
void hci_register_packet_handler(void (*handler)(uint8_t packet_type, uint8_t *packet, uint16_t size));

/**
 * @brief Requests the change of BTstack power mode.
 */
int  hci_power_control(HCI_POWER_MODE mode);

/**
 * @brief Allows to control if device is discoverable. OFF by default.
 */
void hci_discoverable_control(uint8_t enable);

/**
 * @brief Creates and sends HCI command packets based on a template and a list of parameters. Will return error if outgoing data buffer is occupied. 
 */
int hci_send_cmd(const hci_cmd_t *cmd, ...);

/**
 * @brief Deletes link key for remote device with baseband address.
 */
void hci_drop_link_key_for_bd_addr(bd_addr_t addr);

/* Configure Secure Simple Pairing */

/**
 * @brief Enable will enable SSP during init.
 */
void hci_ssp_set_enable(int enable);

/**
 * @brief If set, BTstack will respond to io capability request using authentication requirement.
 */
void hci_ssp_set_io_capability(int ssp_io_capability);
void hci_ssp_set_authentication_requirement(int authentication_requirement);

/**
 * @brief If set, BTstack will confirm a numeric comparison and enter '000000' if requested.
 */
void hci_ssp_set_auto_accept(int auto_accept);

/**
 * @brief Get addr type and address used in advertisement packets.
 */
void hci_le_advertisement_address(uint8_t * addr_type, bd_addr_t addr);

/**
 * @brief Set callback for Bluetooth Hardware Error
 */
void hci_set_hardware_error_callback(void (*fn)(void));

/* API_END */

/**
 * @brief Set Advertisement Parameters
 * @param adv_int_min
 * @param adv_int_max
 * @param adv_type
 * @param own_address_type
 * @param direct_address_type
 * @param direct_address
 * @param channel_map
 * @param filter_policy
 *
 * @note internal use. use gap_advertisements_set_params from gap_le.h instead.
 */
void hci_le_advertisements_set_params(uint16_t adv_int_min, uint16_t adv_int_max, uint8_t adv_type,
    uint8_t own_address_type, uint8_t direct_address_typ, bd_addr_t direct_address,
    uint8_t channel_map, uint8_t filter_policy);

#if defined __cplusplus
}
#endif

#endif // __HCI_H
