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

#include "btstack_config.h"

#include "btstack_chipset.h"
#include "btstack_control.h"
#include "btstack_linked_list.h"
#include "btstack_util.h"
#include "classic/btstack_link_key_db.h"
#include "hci_cmd.h"
#include "gap.h"
#include "hci_transport.h"

#ifdef ENABLE_BLE
#include "ble/att_db.h"
#endif

#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>

#if defined __cplusplus
extern "C" {
#endif
     
// packet buffer sizes

// Max HCI Commadn LE payload size:
// 64 from LE Generate DHKey command
// 32 from LE Encrypt command
#if defined(ENABLE_LE_SECURE_CONNECTIONS) && !defined(HAVE_HCI_CONTROLLER_DHKEY_SUPPORT)
#define HCI_CMD_PAYLOAD_SIZE_LE 64
#else
#define HCI_CMD_PAYLOAD_SIZE_LE 32
#endif

// HCI_ACL_PAYLOAD_SIZE is configurable and defined in config.h
// addition byte in even to terminate remote name request with '\0'
#define HCI_EVENT_BUFFER_SIZE      (HCI_EVENT_HEADER_SIZE + HCI_EVENT_PAYLOAD_SIZE + 1)

#ifdef ENABLE_CLASSIC
#define HCI_CMD_BUFFER_SIZE        (HCI_CMD_HEADER_SIZE   + HCI_CMD_PAYLOAD_SIZE)
#else
#define HCI_CMD_BUFFER_SIZE        (HCI_CMD_HEADER_SIZE   + HCI_CMD_PAYLOAD_SIZE_LE)
#endif

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
#ifdef HAVE_HOST_CONTROLLER_API
#define HCI_OUTGOING_PRE_BUFFER_SIZE 0
#else
#define HCI_OUTGOING_PRE_BUFFER_SIZE 1
#endif

// BNEP may uncompress the IP Header by 16 bytes
#ifndef HCI_INCOMING_PRE_BUFFER_SIZE
#ifdef ENABLE_CLASSIC
#define HCI_INCOMING_PRE_BUFFER_SIZE (16 - HCI_ACL_HEADER_SIZE - 4)
#else
#define HCI_INCOMING_PRE_BUFFER_SIZE 0
#endif
#endif

// 
#define IS_COMMAND(packet, command) (little_endian_read_16(packet,0) == command.opcode)

// check if command complete event for given command
#define HCI_EVENT_IS_COMMAND_COMPLETE(event,cmd) ( event[0] == HCI_EVENT_COMMAND_COMPLETE && little_endian_read_16(event,3) == cmd.opcode)
#define HCI_EVENT_IS_COMMAND_STATUS(event,cmd) ( event[0] == HCI_EVENT_COMMAND_STATUS && little_endian_read_16(event,4) == cmd.opcode)

// Code+Len=2, Pkts+Opcode=3; total=5
#define OFFSET_OF_DATA_IN_COMMAND_COMPLETE 5

// ACL Packet
#define READ_ACL_CONNECTION_HANDLE( buffer ) ( little_endian_read_16(buffer,0) & 0x0fff)
#define READ_SCO_CONNECTION_HANDLE( buffer ) ( little_endian_read_16(buffer,0) & 0x0fff)
#define READ_ACL_FLAGS( buffer )      ( buffer[1] >> 4 )
#define READ_ACL_LENGTH( buffer )     (little_endian_read_16(buffer, 2))

// Sneak peak into L2CAP Packet
#define READ_L2CAP_LENGTH(buffer)     ( little_endian_read_16(buffer, 4))
#define READ_L2CAP_CHANNEL_ID(buffer) ( little_endian_read_16(buffer, 6))

/**
 * LE connection parameter update state
 */ 

typedef enum {
    CON_PARAMETER_UPDATE_NONE,
    CON_PARAMETER_UPDATE_SEND_REQUEST,
    CON_PARAMETER_UPDATE_SEND_RESPONSE,
    CON_PARAMETER_UPDATE_CHANGE_HCI_CON_PARAMETERS,
    CON_PARAMETER_UPDATE_DENY
} le_con_parameter_update_state_t;

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

typedef enum {
    LE_CONNECTING_IDLE,
    LE_CONNECTING_DIRECT,
    LE_CONNECTING_WHITELIST,
} le_connecting_state_t;

#ifdef ENABLE_BLE

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

    // RESPONDER ROLE
    // state = 35
    SM_RESPONDER_IDLE,
    SM_RESPONDER_SEND_SECURITY_REQUEST,
    SM_RESPONDER_PH0_RECEIVED_LTK_REQUEST,
    SM_RESPONDER_PH0_SEND_LTK_REQUESTED_NEGATIVE_REPLY,
    SM_RESPONDER_PH1_W4_PAIRING_REQUEST,
    SM_RESPONDER_PH1_PAIRING_REQUEST_RECEIVED,
    SM_RESPONDER_PH1_SEND_PAIRING_RESPONSE,
    SM_RESPONDER_PH1_W4_PAIRING_CONFIRM,
    SM_RESPONDER_PH2_W4_PAIRING_RANDOM,
    SM_RESPONDER_PH2_W4_LTK_REQUEST,
    SM_RESPONDER_PH2_SEND_LTK_REPLY,

    // Phase 4: re-establish previously distributed LTK
    // state == 46
    SM_RESPONDER_PH4_Y_GET_ENC,
    SM_RESPONDER_PH4_Y_W4_ENC,
    SM_RESPONDER_PH4_LTK_GET_ENC,
    SM_RESPONDER_PH4_LTK_W4_ENC,
    SM_RESPONDER_PH4_SEND_LTK_REPLY,

    // INITITIATOR ROLE
    // state = 51
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

    // LE Secure Connections
    SM_SC_RECEIVED_LTK_REQUEST,
    SM_SC_SEND_PUBLIC_KEY_COMMAND,
    SM_SC_W4_PUBLIC_KEY_COMMAND,
    SM_SC_W2_GET_RANDOM_A,
    SM_SC_W4_GET_RANDOM_A,
    SM_SC_W2_GET_RANDOM_B,
    SM_SC_W4_GET_RANDOM_B,
    SM_SC_W2_CMAC_FOR_CONFIRMATION,
    SM_SC_W4_CMAC_FOR_CONFIRMATION,
    SM_SC_SEND_CONFIRMATION,
    SM_SC_W2_CMAC_FOR_CHECK_CONFIRMATION,    
    SM_SC_W4_CMAC_FOR_CHECK_CONFIRMATION,    
    SM_SC_W4_CONFIRMATION,
    SM_SC_SEND_PAIRING_RANDOM,
    SM_SC_W4_PAIRING_RANDOM,
    SM_SC_W2_CALCULATE_G2,
    SM_SC_W4_CALCULATE_G2,
    SM_SC_W2_CALCULATE_F5_SALT,
    SM_SC_W4_CALCULATE_F5_SALT,
    SM_SC_W2_CALCULATE_F5_MACKEY,
    SM_SC_W4_CALCULATE_F5_MACKEY,
    SM_SC_W2_CALCULATE_F5_LTK,
    SM_SC_W4_CALCULATE_F5_LTK,
    SM_SC_W2_CALCULATE_F6_FOR_DHKEY_CHECK,
    SM_SC_W4_CALCULATE_F6_FOR_DHKEY_CHECK,
    SM_SC_W2_CALCULATE_F6_TO_VERIFY_DHKEY_CHECK,
    SM_SC_W4_CALCULATE_F6_TO_VERIFY_DHKEY_CHECK,
    SM_SC_W4_USER_RESPONSE,
    SM_SC_SEND_DHKEY_CHECK_COMMAND,
    SM_SC_W4_DHKEY_CHECK_COMMAND,
    SM_SC_W4_LTK_REQUEST_SC,
    SM_SC_W2_CALCULATE_H6_ILK,
    SM_SC_W4_CALCULATE_H6_ILK,
    SM_SC_W2_CALCULATE_H6_BR_EDR_LINK_KEY,
    SM_SC_W4_CALCULATE_H6_BR_EDR_LINK_KEY,
} security_manager_state_t;

typedef enum {
    IRK_LOOKUP_IDLE,
    IRK_LOOKUP_W4_READY,
    IRK_LOOKUP_STARTED,
    IRK_LOOKUP_SUCCEEDED,
    IRK_LOOKUP_FAILED
} irk_lookup_state_t;

// Authorization state
typedef enum {
    AUTHORIZATION_UNKNOWN,
    AUTHORIZATION_PENDING,
    AUTHORIZATION_DECLINED,
    AUTHORIZATION_GRANTED
} authorization_state_t;

typedef uint8_t sm_pairing_packet_t[7];

// connection info available as long as connection exists
typedef struct sm_connection {
    hci_con_handle_t         sm_handle;
    uint8_t                  sm_role;   // 0 - IamMaster, 1 = IamSlave
    uint8_t                  sm_security_request_received;
    uint8_t                  sm_bonding_requested;
    uint8_t                  sm_peer_addr_type;
    bd_addr_t                sm_peer_address;
    security_manager_state_t sm_engine_state;
    irk_lookup_state_t      sm_irk_lookup_state;
    uint8_t                  sm_connection_encrypted;
    uint8_t                  sm_connection_authenticated;   // [0..1]
    uint8_t                  sm_actual_encryption_key_size;
    sm_pairing_packet_t      sm_m_preq;  // only used during c1
    authorization_state_t    sm_connection_authorization_state;
    uint16_t                 sm_local_ediv;
    uint8_t                  sm_local_rand[8];
    int                      sm_le_db_index;
} sm_connection_t;

//
// ATT Server
//

// max ATT request matches L2CAP PDU -- allow to use smaller buffer
#ifndef ATT_REQUEST_BUFFER_SIZE
#define ATT_REQUEST_BUFFER_SIZE HCI_ACL_PAYLOAD_SIZE
#endif

typedef enum {
    ATT_SERVER_IDLE,
    ATT_SERVER_REQUEST_RECEIVED,
    ATT_SERVER_W4_SIGNED_WRITE_VALIDATION,
    ATT_SERVER_REQUEST_RECEIVED_AND_VALIDATED,
} att_server_state_t;

typedef struct {
    att_server_state_t      state;

    uint8_t                 peer_addr_type;
    bd_addr_t               peer_address;

    int                     ir_le_device_db_index;
    int                     ir_lookup_active;

    int                     value_indication_handle;    
    btstack_timer_source_t  value_indication_timer;

    att_connection_t        connection;

    uint16_t                request_size;
    uint8_t                 request_buffer[ATT_REQUEST_BUFFER_SIZE];

} att_server_t;

#endif

#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
typedef enum {
    L2CAP_INFORMATION_STATE_IDLE = 0,
    L2CAP_INFORMATION_STATE_W2_SEND_EXTENDED_FEATURE_REQUEST,
    L2CAP_INFORMATION_STATE_W4_EXTENDED_FEATURE_RESPONSE,
    L2CAP_INFORMATION_STATE_DONE
} l2cap_information_state_t;

typedef struct {
    l2cap_information_state_t information_state;
    uint16_t                  extended_feature_mask;
} l2cap_state_t;
#endif

//
typedef struct {
    // linked list - assert: first field
    btstack_linked_item_t    item;
    
    // remote side
    bd_addr_t address;
    
    // module handle
    hci_con_handle_t con_handle;

    // le public, le random, classic
    bd_addr_type_t address_type;

    // role: 0 - master, 1 - slave
    uint8_t role;

    // connection state
    CONNECTION_STATE state;
    
    // bonding
    uint16_t bonding_flags;
    uint8_t  bonding_status;
    // requested security level
    gap_security_level_t requested_security_level;

    // 
    link_key_type_t link_key_type;

    // remote supported features
    uint8_t remote_supported_feature_eSCO;

    // errands
    uint32_t authentication_flags;

    btstack_timer_source_t timeout;
    
    // timeout in system ticks (HAVE_EMBEDDED_TICK) or milliseconds (HAVE_EMBEDDED_TIME_MS)
    uint32_t timestamp;

    // ACL packet recombination - PRE_BUFFER + ACL Header + ACL payload
    uint8_t  acl_recombination_buffer[HCI_INCOMING_PRE_BUFFER_SIZE + 4 + HCI_ACL_BUFFER_SIZE];
    uint16_t acl_recombination_pos;
    uint16_t acl_recombination_length;
    
    // number packets sent to controller
    uint8_t num_acl_packets_sent;
    uint8_t num_sco_packets_sent;

#ifdef ENABLE_HCI_CONTROLLER_TO_HOST_FLOW_CONTROL
    uint8_t num_packets_completed;
#endif

    // LE Connection parameter update
    le_con_parameter_update_state_t le_con_parameter_update_state;
    uint8_t  le_con_param_update_identifier;
    uint16_t le_conn_interval_min;
    uint16_t le_conn_interval_max;
    uint16_t le_conn_latency;
    uint16_t le_supervision_timeout;

#ifdef ENABLE_BLE
    // LE Security Manager
    sm_connection_t sm_connection;

    // ATT Server
    att_server_t    att_server;
#endif

#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
    l2cap_state_t l2cap_state;
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
    HCI_INIT_SEND_READ_LOCAL_NAME,
    HCI_INIT_W4_SEND_READ_LOCAL_NAME,

    HCI_INIT_SEND_BAUD_CHANGE,
    HCI_INIT_W4_SEND_BAUD_CHANGE,
    HCI_INIT_CUSTOM_INIT,
    HCI_INIT_W4_CUSTOM_INIT,
    HCI_INIT_SEND_RESET_CSR_WARM_BOOT,
    HCI_INIT_W4_CUSTOM_INIT_CSR_WARM_BOOT,
    HCI_INIT_W4_CUSTOM_INIT_CSR_WARM_BOOT_LINK_RESET,
    HCI_INIT_W4_CUSTOM_INIT_BCM_DELAY,

    HCI_INIT_READ_LOCAL_SUPPORTED_COMMANDS,
    HCI_INIT_W4_READ_LOCAL_SUPPORTED_COMMANDS,

    HCI_INIT_SEND_BAUD_CHANGE_BCM,
    HCI_INIT_W4_SEND_BAUD_CHANGE_BCM,

    HCI_INIT_SET_BD_ADDR,
    HCI_INIT_W4_SET_BD_ADDR,

    HCI_INIT_SEND_RESET_ST_WARM_BOOT,
    HCI_INIT_W4_SEND_RESET_ST_WARM_BOOT,

    HCI_INIT_READ_BD_ADDR,
    HCI_INIT_W4_READ_BD_ADDR,

    HCI_INIT_READ_BUFFER_SIZE,
    HCI_INIT_W4_READ_BUFFER_SIZE,
    HCI_INIT_READ_LOCAL_SUPPORTED_FEATURES,
    HCI_INIT_W4_READ_LOCAL_SUPPORTED_FEATURES,

#ifdef ENABLE_HCI_CONTROLLER_TO_HOST_FLOW_CONTROL
    HCI_INIT_HOST_BUFFER_SIZE,
    HCI_INIT_W4_HOST_BUFFER_SIZE,
    HCI_INIT_SET_CONTROLLER_TO_HOST_FLOW_CONTROL,
    HCI_INIT_W4_SET_CONTROLLER_TO_HOST_FLOW_CONTROL,
#endif

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
    HCI_INIT_WRITE_EIR_DATA,
    HCI_INIT_W4_WRITE_EIR_DATA,
    HCI_INIT_WRITE_INQUIRY_MODE,
    HCI_INIT_W4_WRITE_INQUIRY_MODE,
    HCI_INIT_WRITE_SCAN_ENABLE,
    HCI_INIT_W4_WRITE_SCAN_ENABLE,
    
    // SCO over HCI
    HCI_INIT_WRITE_SYNCHRONOUS_FLOW_CONTROL_ENABLE,
    HCI_INIT_W4_WRITE_SYNCHRONOUS_FLOW_CONTROL_ENABLE,
    HCI_INIT_WRITE_DEFAULT_ERRONEOUS_DATA_REPORTING,
    HCI_INIT_W4_WRITE_DEFAULT_ERRONEOUS_DATA_REPORTING,

    // SCO over HCI Broadcom
    HCI_INIT_BCM_WRITE_SCO_PCM_INT,
    HCI_INIT_W4_BCM_WRITE_SCO_PCM_INT,

#ifdef ENABLE_BLE
    HCI_INIT_LE_READ_BUFFER_SIZE,
    HCI_INIT_W4_LE_READ_BUFFER_SIZE,
    HCI_INIT_WRITE_LE_HOST_SUPPORTED,
    HCI_INIT_W4_WRITE_LE_HOST_SUPPORTED,
#endif

#ifdef ENABLE_LE_DATA_LENGTH_EXTENSION
    HCI_INIT_LE_READ_MAX_DATA_LENGTH,
    HCI_INIT_W4_LE_READ_MAX_DATA_LENGTH,
    HCI_INIT_LE_WRITE_SUGGESTED_DATA_LENGTH,
    HCI_INIT_W4_LE_WRITE_SUGGESTED_DATA_LENGTH,
#endif
    
#ifdef ENABLE_LE_CENTRAL
    HCI_INIT_READ_WHITE_LIST_SIZE,
    HCI_INIT_W4_READ_WHITE_LIST_SIZE,

    HCI_INIT_LE_SET_SCAN_PARAMETERS,
    HCI_INIT_W4_LE_SET_SCAN_PARAMETERS,
#endif

    HCI_INIT_DONE,

    HCI_FALLING_ASLEEP_DISCONNECT,
    HCI_FALLING_ASLEEP_W4_WRITE_SCAN_ENABLE,
    HCI_FALLING_ASLEEP_COMPLETE,

    HCI_INIT_AFTER_SLEEP

} hci_substate_t;

enum {
    LE_ADVERTISEMENT_TASKS_DISABLE       = 1 << 0,
    LE_ADVERTISEMENT_TASKS_SET_ADV_DATA  = 1 << 1,
    LE_ADVERTISEMENT_TASKS_SET_SCAN_DATA = 1 << 2,
    LE_ADVERTISEMENT_TASKS_SET_PARAMS    = 1 << 3,
    LE_ADVERTISEMENT_TASKS_ENABLE        = 1 << 4,
};

enum {
    LE_WHITELIST_ON_CONTROLLER          = 1 << 0,
    LE_WHITELIST_ADD_TO_CONTROLLER      = 1 << 1,
    LE_WHITELIST_REMOVE_FROM_CONTROLLER = 1 << 2,
};

typedef struct {
    btstack_linked_item_t  item;
    bd_addr_t      address;
    bd_addr_type_t address_type;
    uint8_t        state;   
} whitelist_entry_t;

/**
 * main data structure
 */
typedef struct {
    // transport component with configuration
    const hci_transport_t * hci_transport;
    const void            * config;
    
    // chipset driver
    const btstack_chipset_t * chipset;

    // hardware power controller
    const btstack_control_t * control;

    /* link key db */
    const btstack_link_key_db_t * link_key_db;

    // list of existing baseband connections
    btstack_linked_list_t     connections;

    /* callback to L2CAP layer */
    btstack_packet_handler_t acl_packet_handler;

    /* callback for SCO data */
    btstack_packet_handler_t sco_packet_handler;

    /* callbacks for events */
    btstack_linked_list_t event_handlers;

    // hardware error callback
    void (*hardware_error_callback)(uint8_t error);

    // basic configuration
    const char *       local_name;
    const uint8_t *    eir_data;
    uint32_t           class_of_device;
    bd_addr_t          local_bd_addr;
    uint8_t            ssp_enable;
    uint8_t            ssp_io_capability;
    uint8_t            ssp_authentication_requirement;
    uint8_t            ssp_auto_accept;
    inquiry_mode_t     inquiry_mode;

    // single buffer for HCI packet assembly + additional prebuffer for H4 drivers
    uint8_t   * hci_packet_buffer;
    uint8_t   hci_packet_buffer_data[HCI_OUTGOING_PRE_BUFFER_SIZE + HCI_PACKET_BUFFER_SIZE];
    uint8_t   hci_packet_buffer_reserved;
    uint16_t  acl_fragmentation_pos;
    uint16_t  acl_fragmentation_total_size;
     
    /* host to controller flow control */
    uint8_t  num_cmd_packets;
    uint8_t  acl_packets_total_num;
    uint16_t acl_data_packet_length;
    uint8_t  sco_packets_total_num;
    uint8_t  sco_data_packet_length;
    uint8_t  synchronous_flow_control_enabled;
    uint8_t  le_acl_packets_total_num;
    uint16_t le_data_packets_length;
    uint8_t  sco_waiting_for_can_send_now;

    /* local supported features */
    uint8_t local_supported_features[8];

    /* local supported commands summary - complete info is 64 bytes */
    /* 0 - Read Buffer Size */
    /* 1 - Write Le Host Supported */
    /* 2 - Write Synchronous Flow Control Enable (Octet 10/bit 4) */
    /* 3 - Write Default Erroneous Data Reporting (Octet 18/bit 3) */
    /* 4 - LE Write Suggested Default Data Length (Octet 34/bit 0) */
    /* 5 - LE Read Maximum Data Length (Octet 35/bit 3) */
    uint8_t local_supported_commands[1];

    /* bluetooth device information from hci read local version information */
    // uint16_t hci_version;
    // uint16_t hci_revision;
    // uint16_t lmp_version;
    uint16_t manufacturer;
    // uint16_t lmp_subversion;

    // usable packet types given acl_data_packet_length and HCI_ACL_BUFFER_SIZE
    uint16_t packet_types;
    
    
    /* hci state machine */
    HCI_STATE      state;
    hci_substate_t substate;
    btstack_timer_source_t timeout;

    uint16_t  last_cmd_opcode;

    uint8_t   cmds_ready;

    /* buffer for scan enable cmd - 0xff no change */
    uint8_t   new_scan_enable_value;

    uint8_t   discoverable;
    uint8_t   connectable;
    uint8_t   bondable;

    uint8_t   inquiry_state;    // see hci.c for state defines

    bd_addr_t remote_name_addr;
    uint16_t  remote_name_clock_offset;
    uint8_t   remote_name_page_scan_repetition_mode;
    uint8_t   remote_name_state;    // see hci.c for state defines

    bd_addr_t gap_pairing_addr;
    uint8_t   gap_pairing_state;    // see hci.c for state defines
    union {
        const char * gap_pairing_pin;
        uint32_t     gap_pairing_passkey;
    };
    
    uint16_t  sco_voice_setting;
    uint16_t  sco_voice_setting_active;

    uint8_t   loopback_mode;

    // buffer for single connection decline
    uint8_t   decline_reason;
    bd_addr_t decline_addr;

#ifdef ENABLE_HCI_CONTROLLER_TO_HOST_FLOW_CONTROL
    uint8_t   host_completed_packets;
#endif

#ifdef ENABLE_BLE
    uint8_t   le_own_addr_type;
    bd_addr_t le_random_address;
    uint8_t   le_random_address_set;
#endif

#ifdef ENABLE_LE_CENTRAL
    le_scanning_state_t   le_scanning_state;
    le_connecting_state_t le_connecting_state;

    // buffer for le scan type command - 0xff not set
    uint8_t  le_scan_type;
    uint16_t le_scan_interval;  
    uint16_t le_scan_window;

    // LE Whitelist Management
    uint8_t               le_whitelist_capacity;
    btstack_linked_list_t le_whitelist;

    // Connection parameters
    uint16_t le_connection_interval_min;
    uint16_t le_connection_interval_max;
    uint16_t le_connection_latency;
    uint16_t le_supervision_timeout;
    uint16_t le_minimum_ce_length;
    uint16_t le_maximum_ce_length;
#endif

    le_connection_parameter_range_t le_connection_parameter_range;

#ifdef ENABLE_LE_PERIPHERAL
    uint8_t  * le_advertisements_data;
    uint8_t    le_advertisements_data_len;

    uint8_t  * le_scan_response_data;
    uint8_t    le_scan_response_data_len;

    uint8_t  le_advertisements_active;
    uint8_t  le_advertisements_enabled;
    uint8_t  le_advertisements_todo;

    uint16_t le_advertisements_interval_min;
    uint16_t le_advertisements_interval_max;
    uint8_t  le_advertisements_type;
    uint8_t  le_advertisements_direct_address_type;
    uint8_t  le_advertisements_channel_map;
    uint8_t  le_advertisements_filter_policy;
    bd_addr_t le_advertisements_direct_address;
#endif

#ifdef ENABLE_LE_DATA_LENGTH_EXTENSION
    // LE Data Length
    uint16_t le_supported_max_tx_octets;
    uint16_t le_supported_max_tx_time;
#endif

    // custom BD ADDR
    bd_addr_t custom_bd_addr; 
    uint8_t   custom_bd_addr_set;

} hci_stack_t;


/* API_START */

    
// HCI init and configuration


/**
 * @brief Set up HCI. Needs to be called before any other function.
 */
void hci_init(const hci_transport_t *transport, const void *config);

/**
 * @brief Configure Bluetooth chipset driver. Has to be called before power on, or right after receiving the local version information.
 */
void hci_set_chipset(const btstack_chipset_t *chipset_driver);

/**
 * @brief Configure Bluetooth hardware control. Has to be called before power on.
 */
void hci_set_control(const btstack_control_t *hardware_control);

/**
 * @brief Configure Bluetooth hardware control. Has to be called before power on.
 */
void hci_set_link_key_db(btstack_link_key_db_t const * link_key_db);

/**
 * @brief Set callback for Bluetooth Hardware Error
 */
void hci_set_hardware_error_callback(void (*fn)(uint8_t error));

/**
 * @brief Set Public BD ADDR - passed on to Bluetooth chipset during init if supported in bt_control_h
 */
void hci_set_bd_addr(bd_addr_t addr);

/** 
 * @brief Configure Voice Setting for use with SCO data in HSP/HFP
 */
void hci_set_sco_voice_setting(uint16_t voice_setting);

/**
 * @brief Get SCO Voice Setting
 * @return current voice setting
 */
uint16_t hci_get_sco_voice_setting(void);

/**
 * @brief Set inquiry mode: standard, with RSSI, with RSSI + Extended Inquiry Results. Has to be called before power on.
 * @param inquriy_mode see bluetooth_defines.h
 */
void hci_set_inquiry_mode(inquiry_mode_t mode);

/**
 * @brief Requests the change of BTstack power mode.
 */
int  hci_power_control(HCI_POWER_MODE mode);

/**
 * @brief Shutdown HCI
 */
void hci_close(void);


// Callback registration


/**
 * @brief Add event packet handler. 
 */
void hci_add_event_handler(btstack_packet_callback_registration_t * callback_handler);

/**
 * @brief Registers a packet handler for ACL data. Used by L2CAP
 */
void hci_register_acl_packet_handler(btstack_packet_handler_t handler);

/**
 * @brief Registers a packet handler for SCO data. Used for HSP and HFP profiles.
 */
void hci_register_sco_packet_handler(btstack_packet_handler_t handler);


// Sending HCI Commands

/** 
 * @brief Check if CMD packet can be sent to controller
 */
int hci_can_send_command_packet_now(void);

/**
 * @brief Creates and sends HCI command packets based on a template and a list of parameters. Will return error if outgoing data buffer is occupied. 
 */
int hci_send_cmd(const hci_cmd_t *cmd, ...);


// Sending SCO Packets

/** @brief Get SCO packet length for current SCO Voice setting
 *  @note  Using SCO packets of the exact length is required for USB transfer
 *  @return Length of SCO packets in bytes (not audio frames) incl. 3 byte header
 */
int hci_get_sco_packet_length(void);

/**
 * @brief Request emission of HCI_EVENT_SCO_CAN_SEND_NOW as soon as possible
 * @note HCI_EVENT_SCO_CAN_SEND_NOW might be emitted during call to this function
 *       so packet handler should be ready to handle it
 */
void hci_request_sco_can_send_now_event(void);

/**
 * @brief Check HCI packet buffer and if SCO packet can be sent to controller
 */
int hci_can_send_sco_packet_now(void);

/**
 * @brief Check if SCO packet can be sent to controller
 */
int hci_can_send_prepared_sco_packet_now(void);

/**
 * @brief Send SCO packet prepared in HCI packet buffer
 */
int hci_send_sco_packet_buffer(int size);


// Outgoing packet buffer, also used for SCO packets
// see hci_can_send_prepared_sco_packet_now amn hci_send_sco_packet_buffer

/**
 * Reserves outgoing packet buffer.
 * @return 1 on success
 */
int hci_reserve_packet_buffer(void);

/**
 * Get pointer for outgoing packet buffer
 */
uint8_t* hci_get_outgoing_packet_buffer(void);

/**
 * Release outgoing packet buffer\
 * @note only called instead of hci_send_preparared
 */
void hci_release_packet_buffer(void);


/* API_END */


/**
 * va_list version of hci_send_cmd
 */
int hci_send_cmd_va_arg(const hci_cmd_t *cmd, va_list argtr);

/**
 * Get connection iterator. Only used by l2cap.c and sm.c
 */
void hci_connections_get_iterator(btstack_linked_list_iterator_t *it);

/**
 * Get internal hci_connection_t for given handle. Used by L2CAP, SM, daemon
 */
hci_connection_t * hci_connection_for_handle(hci_con_handle_t con_handle);

/**
 * Get internal hci_connection_t for given Bluetooth addres. Called by L2CAP
 */
hci_connection_t * hci_connection_for_bd_addr_and_type(bd_addr_t addr, bd_addr_type_t addr_type);

/**
 * Check if outgoing packet buffer is reserved. Used for internal checks in l2cap.c
 */
int hci_is_packet_buffer_reserved(void);

/**
 * Check hci packet buffer is free and a classic acl packet can be sent to controller
 */
int hci_can_send_acl_classic_packet_now(void);

/**
 * Check hci packet buffer is free and an LE acl packet can be sent to controller
 */
int hci_can_send_acl_le_packet_now(void);

/**
 * Check hci packet buffer is free and an acl packet for the given handle can be sent to controller
 */
int hci_can_send_acl_packet_now(hci_con_handle_t con_handle);

/**
 * Check if acl packet for the given handle can be sent to controller
 */
int hci_can_send_prepared_acl_packet_now(hci_con_handle_t con_handle);

/**
 * Send acl packet prepared in hci packet buffer
 */
int hci_send_acl_packet_buffer(int size);

/**
 * Check if authentication is active. It delays automatic disconnect while no L2CAP connection
 * Called by l2cap.
 */
int hci_authentication_active_for_handle(hci_con_handle_t handle);

/**
 * Get maximal ACL Classic data packet length based on used buffer size. Called by L2CAP
 */
uint16_t hci_max_acl_data_packet_length(void);

/**
 * Get supported packet types. Called by L2CAP
 */
uint16_t hci_usable_acl_packet_types(void);

/**
 * Check if ACL packets marked as non flushable can be sent. Called by L2CAP
 */
int hci_non_flushable_packet_boundary_flag_supported(void);

/**
 * Check if extended SCO Link is supported
 */
int hci_extended_sco_link_supported(void);

/**
 * Check if SSP is supported on both sides. Called by L2CAP
 */
int gap_ssp_supported_on_both_sides(hci_con_handle_t handle);

/**
 * Disconn because of security block. Called by L2CAP
 */
void hci_disconnect_security_block(hci_con_handle_t con_handle);

/**
 * Query if remote side supports eSCO
 */
int hci_remote_esco_supported(hci_con_handle_t con_handle);

/**
 * Emit current HCI state. Called by daemon
 */
void hci_emit_state(void);

/** 
 * Send complete CMD packet. Called by daemon
 */
int hci_send_cmd_packet(uint8_t *packet, int size);

/**
 * Disconnect all HCI connections. Called by daemon
 */
void hci_disconnect_all(void);

/**
 * Get number of free acl slots for packets of given handle. Called by daemon
 */
int hci_number_free_acl_slots_for_handle(hci_con_handle_t con_handle);

/**
 * @brief Set Advertisement Parameters
 * @param adv_int_min
 * @param adv_int_max
 * @param adv_type
 * @param direct_address_type
 * @param direct_address
 * @param channel_map
 * @param filter_policy
 *
 * @note internal use. use gap_advertisements_set_params from gap.h instead.
 */
void hci_le_advertisements_set_params(uint16_t adv_int_min, uint16_t adv_int_max, uint8_t adv_type, 
    uint8_t direct_address_typ, bd_addr_t direct_address, uint8_t channel_map, uint8_t filter_policy);

/** 
 * 
 * @note internal use. use gap_random_address_set_mode from gap.h instead.
 */
void hci_le_set_own_address_type(uint8_t own_address_type);

/**
 * @brief Get Manufactured
 * @return manufacturer id
 */
uint16_t hci_get_manufacturer(void);

// Only for PTS testing

/** 
 * Disable automatic L2CAP disconnect if no L2CAP connection is established
 */
void hci_disable_l2cap_timeout_check(void);

/**
 * Get state
 */
HCI_STATE hci_get_state(void);

#if defined __cplusplus
}
#endif

#endif // __HCI_H
