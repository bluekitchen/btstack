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
 * @title Host Controler Interface (HCI)
 *
 */

#ifndef HCI_H
#define HCI_H

#include "btstack_config.h"

#include "btstack_bool.h"
#include "btstack_chipset.h"
#include "btstack_control.h"
#include "btstack_linked_list.h"
#include "btstack_util.h"
#include "hci_cmd.h"
#include "gap.h"
#include "hci_transport.h"
#include "btstack_run_loop.h"

#ifdef ENABLE_CLASSIC
#include "classic/btstack_link_key_db.h"
#endif

#ifdef ENABLE_BLE
#include "ble/att_db.h"
#endif

#ifdef HAVE_SCO_TRANSPORT
#include "btstack_sco_transport.h"
#endif

#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>

#if defined __cplusplus
extern "C" {
#endif
     
// packet buffer sizes
#define HCI_CMD_HEADER_SIZE          3
#define HCI_ACL_HEADER_SIZE          4
#define HCI_SCO_HEADER_SIZE          3
#define HCI_EVENT_HEADER_SIZE        2
#define HCI_ISO_HEADER_SIZE          4

#define HCI_EVENT_PAYLOAD_SIZE     255
#define HCI_CMD_PAYLOAD_SIZE       255

// default for ISO streams: 48_6_2 : 2 x 155
#ifndef HCI_ISO_PAYLOAD_SIZE
#define HCI_ISO_PAYLOAD_SIZE 310
#endif

// Max HCI Command LE payload size:
// 64 from LE Generate DHKey command
// 32 from LE Encrypt command
#if defined(ENABLE_LE_SECURE_CONNECTIONS) && !defined(ENABLE_MICRO_ECC_FOR_LE_SECURE_CONNECTIONS)
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
    
// size of hci incoming buffer, big enough for event or acl packet without H4 packet type
#ifdef HCI_INCOMING_PACKET_BUFFER_SIZE
    #if HCI_INCOMING_PACKET_BUFFER_SIZE < HCI_ACL_BUFFER_SIZE
        #error HCI_INCOMING_PACKET_BUFFER_SIZE must be equal or larger than HCI_ACL_BUFFER_SIZE
    #endif
    #if HCI_INCOMING_PACKET_BUFFER_SIZE < HCI_EVENT_BUFFER_SIZE
        #error HCI_INCOMING_PACKET_BUFFER_SIZE must be equal or larger than HCI_EVENT_BUFFER_SIZE
    #endif
#else
    #if HCI_ACL_BUFFER_SIZE > HCI_EVENT_BUFFER_SIZE
        #define HCI_INCOMING_PACKET_BUFFER_SIZE HCI_ACL_BUFFER_SIZE
    #else
        #define HCI_INCOMING_PACKET_BUFFER_SIZE HCI_EVENT_BUFFER_SIZE
    #endif
#endif

// size of hci outgoing buffer, big enough for command or acl packet without H4 packet type
#ifdef HCI_OUTGOING_PACKET_BUFFER_SIZE
    #if HCI_OUTGOING_PACKET_BUFFER_SIZE < HCI_ACL_BUFFER_SIZE
        #error HCI_OUTGOING_PACKET_BUFFER_SIZE must be equal or larger than HCI_ACL_BUFFER_SIZE
    #endif
    #if HCI_OUTGOING_PACKET_BUFFER_SIZE < HCI_CMD_BUFFER_SIZE
        #error HCI_OUTGOING_PACKET_BUFFER_SIZE must be equal or larger than HCI_CMD_BUFFER_SIZE
    #endif
#else
    #if HCI_ACL_BUFFER_SIZE > HCI_CMD_BUFFER_SIZE
        #define HCI_OUTGOING_PACKET_BUFFER_SIZE HCI_ACL_BUFFER_SIZE
    #else
        #define HCI_OUTGOING_PACKET_BUFFER_SIZE HCI_CMD_BUFFER_SIZE
    #endif
#endif

// additional pre-buffer space for packets to Bluetooth module
// - H4 requires 1 byte for the packet type
// - h5 requires 4 bytes for H5 header
#ifndef HCI_OUTGOING_PRE_BUFFER_SIZE
    #ifdef HAVE_HOST_CONTROLLER_API
        #define HCI_OUTGOING_PRE_BUFFER_SIZE 0
    #else
        #ifdef ENABLE_H5
            #define HCI_OUTGOING_PRE_BUFFER_SIZE 4
        #else
            #define HCI_OUTGOING_PRE_BUFFER_SIZE 1
        #endif
    #endif
#endif

// BNEP may uncompress the IP Header by 16 bytes, GATT Client requires six additional bytes for long characteristic reads
// wih service_id + connection_id
#ifndef HCI_INCOMING_PRE_BUFFER_SIZE
#ifdef ENABLE_CLASSIC
#define HCI_INCOMING_PRE_BUFFER_SIZE (16 - HCI_ACL_HEADER_SIZE - 4)
#else
#define HCI_INCOMING_PRE_BUFFER_SIZE 6
#endif
#endif

// 
#define IS_COMMAND(packet, command) ( little_endian_read_16(packet,0) == command.opcode )

// check if command complete event for given command
#define HCI_EVENT_IS_COMMAND_COMPLETE(event,cmd) ( (event[0] == HCI_EVENT_COMMAND_COMPLETE) && (little_endian_read_16(event,3) == cmd.opcode) )
#define HCI_EVENT_IS_COMMAND_STATUS(event,cmd)   ( (event[0] == HCI_EVENT_COMMAND_STATUS)   && (little_endian_read_16(event,4) == cmd.opcode) )

// Code+Len=2, Pkts+Opcode=3; total=5
#define OFFSET_OF_DATA_IN_COMMAND_COMPLETE 5

// ACL Packet
#define READ_ACL_CONNECTION_HANDLE( buffer ) ( little_endian_read_16(buffer,0) & 0x0fff)
#define READ_SCO_CONNECTION_HANDLE( buffer ) ( little_endian_read_16(buffer,0) & 0x0fff)
#define READ_ISO_CONNECTION_HANDLE( buffer ) ( little_endian_read_16(buffer,0) & 0x0fff)
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
    // L2CAP
    CON_PARAMETER_UPDATE_SEND_REQUEST,
    CON_PARAMETER_UPDATE_SEND_RESPONSE,
    CON_PARAMETER_UPDATE_CHANGE_HCI_CON_PARAMETERS,
    CON_PARAMETER_UPDATE_DENY,
    // HCI - in response to HCI_SUBEVENT_LE_REMOTE_CONNECTION_PARAMETER_REQUEST
    CON_PARAMETER_UPDATE_REPLY,
    CON_PARAMETER_UPDATE_NEGATIVE_REPLY,
} le_con_parameter_update_state_t;

// Authentication flags
typedef enum {
    AUTH_FLAG_NONE                                = 0x0000,
    AUTH_FLAG_HANDLE_LINK_KEY_REQUEST             = 0x0001,
    AUTH_FLAG_DENY_PIN_CODE_REQUEST               = 0x0002,
    AUTH_FLAG_RECV_IO_CAPABILITIES_REQUEST        = 0x0004,
    AUTH_FLAG_RECV_IO_CAPABILITIES_RESPONSE       = 0x0008,
    AUTH_FLAG_SEND_IO_CAPABILITIES_REPLY          = 0x0010,
    AUTH_FLAG_SEND_IO_CAPABILITIES_NEGATIVE_REPLY = 0x0020,
    AUTH_FLAG_SEND_USER_CONFIRM_REPLY             = 0x0040,
    AUTH_FLAG_SEND_USER_CONFIRM_NEGATIVE_REPLY    = 0x0080,
    AUTH_FLAG_SEND_USER_PASSKEY_REPLY             = 0x0100,

    // Classic OOB
    AUTH_FLAG_SEND_REMOTE_OOB_DATA_REPLY          = 0x0200,

    // pairing status
    AUTH_FLAG_LEGACY_PAIRING_ACTIVE               = 0x0400,
    AUTH_FLAG_SSP_PAIRING_ACTIVE                  = 0x0800,
    AUTH_FLAG_PAIRING_ACTIVE_MASK                 = (AUTH_FLAG_LEGACY_PAIRING_ACTIVE | AUTH_FLAG_SSP_PAIRING_ACTIVE),

    // connection status
    AUTH_FLAG_CONNECTION_AUTHENTICATED            = 0x1000,
    AUTH_FLAG_CONNECTION_ENCRYPTED                = 0x2000,

} hci_authentication_flags_t;

// GAP Connection Tasks
#define GAP_CONNECTION_TASK_WRITE_AUTOMATIC_FLUSH_TIMEOUT 0x0001u
#define GAP_CONNECTION_TASK_WRITE_SUPERVISION_TIMEOUT     0x0002u
#define GAP_CONNECTION_TASK_READ_RSSI                     0x0004u
#define GAP_CONNECTION_TASK_LE_READ_REMOTE_FEATURES       0x0008u

/**
 * Connection State 
 */
typedef enum {
    SEND_CREATE_CONNECTION = 0,
    SENT_CREATE_CONNECTION,
    RECEIVED_CONNECTION_REQUEST,
    ACCEPTED_CONNECTION_REQUEST,
    OPEN,
    SEND_DISCONNECT,
    SENT_DISCONNECT,
    RECEIVED_DISCONNECTION_COMPLETE,
    ANNOUNCED // connection handle announced in advertisement set terminated event
} CONNECTION_STATE;

// bonding flags
enum {
    // remote features
    BONDING_REMOTE_FEATURES_QUERY_ACTIVE      =  0x0001,
    BONDING_REQUEST_REMOTE_FEATURES_PAGE_0    =  0x0002,
    BONDING_REQUEST_REMOTE_FEATURES_PAGE_1    =  0x0004,
    BONDING_REQUEST_REMOTE_FEATURES_PAGE_2    =  0x0008,
    BONDING_RECEIVED_REMOTE_FEATURES          =  0x0010,
    BONDING_REMOTE_SUPPORTS_SSP_CONTROLLER    =  0x0020,
    BONDING_REMOTE_SUPPORTS_SSP_HOST          =  0x0040,
    BONDING_REMOTE_SUPPORTS_SC_CONTROLLER     =  0x0080,
    BONDING_REMOTE_SUPPORTS_SC_HOST           =  0x0100,
    // other
    BONDING_DISCONNECT_SECURITY_BLOCK         =  0x0200,
    BONDING_DISCONNECT_DEDICATED_DONE         =  0x0400,
    BONDING_SEND_AUTHENTICATE_REQUEST         =  0x0800,
    BONDING_SENT_AUTHENTICATE_REQUEST         =  0x1000,
    BONDING_SEND_ENCRYPTION_REQUEST           =  0x2000,
    BONDING_SEND_READ_ENCRYPTION_KEY_SIZE     =  0x4000,
    BONDING_DEDICATED                         =  0x8000,
    BONDING_DEDICATED_DEFER_DISCONNECT        = 0x10000,
    BONDING_EMIT_COMPLETE_ON_DISCONNECT       = 0x20000,
};

typedef enum {
    BLUETOOTH_OFF = 1,
    BLUETOOTH_ON,
    BLUETOOTH_ACTIVE
} BLUETOOTH_STATE;

typedef enum {
    LE_CONNECTING_IDLE,
    LE_CONNECTING_CANCEL,
    LE_CONNECTING_DIRECT,
    LE_CONNECTING_WHITELIST,
} le_connecting_state_t;

typedef enum {
    ATT_BEARER_UNENHANCED_LE,
    ATT_BEARER_UNENHANCED_CLASSIC,
    ATT_BEARER_ENHANCED_LE,
    ATT_BEARER_ENHANCED_CLASSIC
} att_bearer_type_t;

#ifdef ENABLE_BLE

//
// SM internal types and globals
//

typedef enum {

    // general states
    SM_GENERAL_IDLE,
    SM_GENERAL_SEND_PAIRING_FAILED,
    SM_GENERAL_TIMEOUT, // no other security messages are exchanged
    SM_GENERAL_REENCRYPTION_FAILED,

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
    SM_PH2_C1_GET_ENC_A,
    SM_PH2_C1_W4_ENC_A,

    // calculate confirm value for remote side
    SM_PH2_C1_GET_ENC_C,
    SM_PH2_C1_W4_ENC_C,

    SM_PH2_C1_SEND_PAIRING_CONFIRM,
    SM_PH2_SEND_PAIRING_RANDOM,

    // calc STK
    SM_PH2_CALC_STK,
    SM_PH2_W4_STK,

    SM_PH2_W4_CONNECTION_ENCRYPTED,

    // Phase 3: Transport Specific Key Distribution
    // calculate DHK, Y, EDIV, and LTK
    SM_PH3_Y_GET_ENC,
    SM_PH3_Y_W4_ENC,
    SM_PH3_LTK_GET_ENC,

    // exchange keys
    SM_PH3_DISTRIBUTE_KEYS,
    SM_PH3_RECEIVE_KEYS,

    // Phase 4: re-establish previously distributed LTK
    SM_PH4_W4_CONNECTION_ENCRYPTED,

    // RESPONDER ROLE
    SM_RESPONDER_IDLE,
    SM_RESPONDER_SEND_SECURITY_REQUEST,
    SM_RESPONDER_PH0_RECEIVED_LTK_REQUEST,
    SM_RESPONDER_PH0_RECEIVED_LTK_W4_IRK,
    SM_RESPONDER_PH0_SEND_LTK_REQUESTED_NEGATIVE_REPLY,
    SM_RESPONDER_PH1_W4_PAIRING_REQUEST,
    SM_RESPONDER_PH1_PAIRING_REQUEST_RECEIVED,
    SM_RESPONDER_PH1_PAIRING_REQUEST_RECEIVED_W4_IRK,
    SM_RESPONDER_PH1_SEND_PAIRING_RESPONSE,
    SM_RESPONDER_PH1_W4_PAIRING_CONFIRM,
    SM_RESPONDER_PH2_W4_PAIRING_RANDOM,
    SM_RESPONDER_PH2_W4_LTK_REQUEST,
    SM_RESPONDER_PH2_SEND_LTK_REPLY,
    SM_RESPONDER_PH4_Y_W4_ENC,
    SM_RESPONDER_PH4_SEND_LTK_REPLY,

    // INITIATOR ROLE
    SM_INITIATOR_CONNECTED,
    SM_INITIATOR_PH1_W2_SEND_PAIRING_REQUEST,
    SM_INITIATOR_PH1_W4_PAIRING_RESPONSE,
    SM_INITIATOR_PH2_W4_PAIRING_CONFIRM,
    SM_INITIATOR_PH2_W4_PAIRING_RANDOM,
    SM_INITIATOR_PH3_SEND_START_ENCRYPTION,
    SM_INITIATOR_PH4_HAS_LTK,

    // LE Secure Connections
    SM_SC_RECEIVED_LTK_REQUEST,
    SM_SC_SEND_PUBLIC_KEY_COMMAND,
    SM_SC_W4_PUBLIC_KEY_COMMAND,
    SM_SC_W4_LOCAL_NONCE,
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
    SM_SC_W4_CALCULATE_DHKEY,
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
    SM_SC_W2_CALCULATE_ILK_USING_H6,
	SM_SC_W2_CALCULATE_ILK_USING_H7,
    SM_SC_W4_CALCULATE_ILK,
    SM_SC_W2_CALCULATE_BR_EDR_LINK_KEY,
    SM_SC_W4_CALCULATE_BR_EDR_LINK_KEY,

    // Classic
    SM_BR_EDR_W4_ENCRYPTION_COMPLETE,
    SM_BR_EDR_INITIATOR_W4_FIXED_CHANNEL_MASK,
    SM_BR_EDR_INITIATOR_SEND_PAIRING_REQUEST,
    SM_BR_EDR_INITIATOR_W4_PAIRING_RESPONSE,
    SM_BR_EDR_RESPONDER_W4_PAIRING_REQUEST,
    SM_BR_EDR_RESPONDER_PAIRING_REQUEST_RECEIVED,
    SM_BR_EDR_DISTRIBUTE_KEYS,
    SM_BR_EDR_RECEIVE_KEYS,
    SM_BR_EDR_W2_CALCULATE_ILK_USING_H6,
    SM_BR_EDR_W2_CALCULATE_ILK_USING_H7,
    SM_BR_EDR_W4_CALCULATE_ILK,
    SM_BR_EDR_W2_CALCULATE_LE_LTK,
    SM_BR_EDR_W4_CALCULATE_LE_LTK,
} security_manager_state_t;

typedef enum {
    IRK_LOOKUP_IDLE,
    IRK_LOOKUP_W4_READY,
    IRK_LOOKUP_STARTED,
    IRK_LOOKUP_SUCCEEDED,
    IRK_LOOKUP_FAILED
} irk_lookup_state_t;

typedef uint8_t sm_pairing_packet_t[7];

// connection info available as long as connection exists
typedef struct sm_connection {
    hci_con_handle_t         sm_handle;
    uint16_t                 sm_cid;
    uint8_t                  sm_role;   // 0 - IamMaster, 1 = IamSlave
    bool                     sm_security_request_received;
    bool                     sm_pairing_requested;
    uint8_t                  sm_peer_addr_type;
    bd_addr_t                sm_peer_address;
    uint8_t                  sm_own_addr_type;
    bd_addr_t                sm_own_address;
    security_manager_state_t sm_engine_state;
    irk_lookup_state_t       sm_irk_lookup_state;
    uint8_t                  sm_pairing_failed_reason;
    uint8_t                  sm_connection_encrypted;       // [0..2]
    uint8_t                  sm_connection_authenticated;   // [0..1]
    bool                     sm_connection_sc;
    uint8_t                  sm_actual_encryption_key_size;
    sm_pairing_packet_t      sm_m_preq;  // only used during c1
    authorization_state_t    sm_connection_authorization_state;
    uint16_t                 sm_local_ediv;
    uint8_t                  sm_local_rand[8];
    int                      sm_le_db_index;
    bool                     sm_pairing_active;
    bool                     sm_reencryption_active;
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
    ATT_SERVER_RESPONSE_PENDING,
} att_server_state_t;

typedef struct {
    att_server_state_t      state;

    uint8_t                 peer_addr_type;
    bd_addr_t               peer_address;

    att_bearer_type_t       bearer_type;

    int                     ir_le_device_db_index;
    bool                    ir_lookup_active;
    bool                    pairing_active;

    uint16_t                value_indication_handle;    
    btstack_timer_source_t  value_indication_timer;

    btstack_linked_list_t   notification_requests;
    btstack_linked_list_t   indication_requests;

#if defined(ENABLE_GATT_OVER_CLASSIC) || defined(ENABLE_GATT_OVER_EATT)
    // unified (client + server) att bearer
    uint16_t                l2cap_cid;
    bool                    send_requests[2];
    bool                    outgoing_connection_active;
    bool                    incoming_connection_request;
    bool                    eatt_outgoing_active;
#endif

    uint16_t                request_size;
    uint8_t                 request_buffer[ATT_REQUEST_BUFFER_SIZE];

} att_server_t;

#endif

typedef enum {
    L2CAP_INFORMATION_STATE_IDLE = 0,
    L2CAP_INFORMATION_STATE_W2_SEND_EXTENDED_FEATURE_REQUEST,
    L2CAP_INFORMATION_STATE_W4_EXTENDED_FEATURE_RESPONSE,
    L2CAP_INFORMATION_STATE_W2_SEND_FIXED_CHANNELS_REQUEST,
    L2CAP_INFORMATION_STATE_W4_FIXED_CHANNELS_RESPONSE,
    L2CAP_INFORMATION_STATE_DONE
} l2cap_information_state_t;

typedef struct {
    l2cap_information_state_t information_state;
    uint16_t                  extended_feature_mask;
    uint16_t                  fixed_channels_supported;    // Core V5.3 - only first octet used
} l2cap_state_t;

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
    hci_role_t role;

    // connection state
    CONNECTION_STATE state;
    
    // bonding
    uint32_t bonding_flags;
    uint8_t  bonding_status;

    // encryption key size (in octets)
    uint8_t encryption_key_size;

    // requested security level
    gap_security_level_t requested_security_level;
    
    // link key and its type for Classic connections
    // LTK and LTK valid flag for LE connections
    link_key_t      link_key;
    link_key_type_t link_key_type;

#ifdef ENABLE_CLASSIC
    // remote supported SCO packets based on remote supported features mask
    uint16_t remote_supported_sco_packets;

    // remote supported features
    /* bit 0 - eSCO */
    /* bit 1 - extended features */
    uint8_t remote_supported_features[1];

    // IO Capabilities Response
    uint8_t io_cap_request_auth_req;
    uint8_t io_cap_response_auth_req;
    uint8_t io_cap_response_io;
#ifdef ENABLE_CLASSIC_PAIRING_OOB
    uint8_t io_cap_response_oob_data;
#endif

    // connection mode, default ACL_CONNECTION_MODE_ACTIVE
    uint8_t connection_mode;

    // enter/exit sniff mode requests
    uint16_t sniff_min_interval;    // 0: idle, 0xffff exit sniff, else enter sniff
    uint16_t sniff_max_interval;
    uint16_t sniff_attempt;
    uint16_t sniff_timeout;

    // sniff subrating
    uint16_t sniff_subrating_max_latency;   // 0xffff = not set
    uint16_t sniff_subrating_min_remote_timeout;
    uint16_t sniff_subrating_min_local_timeout;

    // QoS
    hci_service_type_t qos_service_type;
    uint32_t qos_token_rate;
    uint32_t qos_peak_bandwidth;
    uint32_t qos_latency;
    uint32_t qos_delay_variation;

#ifdef ENABLE_SCO_OVER_HCI
    // track SCO rx event
    uint32_t sco_established_ms;
    uint8_t  sco_tx_active;
#endif
    // generate sco can send now based on received packets, using timeout below
    uint8_t  sco_tx_ready;

    // SCO payload length
    uint16_t sco_payload_length;

    // request role switch
    hci_role_t request_role;

    btstack_timer_source_t timeout_sco;
#endif /* ENABLE_CLASSIC */

    // authentication and other errands
    uint16_t authentication_flags;

    // gap connection tasks, see GAP_CONNECTION_TASK_x
    uint16_t gap_connection_tasks;

    btstack_timer_source_t timeout;

    // timeout in system ticks (HAVE_EMBEDDED_TICK) or milliseconds (HAVE_EMBEDDED_TIME_MS)
    uint32_t timestamp;

    // ACL packet recombination - PRE_BUFFER + ACL Header + ACL payload
    uint8_t  acl_recombination_buffer[HCI_INCOMING_PRE_BUFFER_SIZE + 4 + HCI_ACL_BUFFER_SIZE];
    uint16_t acl_recombination_pos;
    uint16_t acl_recombination_length;
    

    // number packets sent to controller
    uint8_t num_packets_sent;

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
    uint16_t le_connection_interval;

    // LE PHY Update via set phy command
    uint8_t le_phy_update_all_phys;      // 0xff for idle
    uint8_t le_phy_update_tx_phys;
    uint8_t le_phy_update_rx_phys;
    int8_t  le_phy_update_phy_options;

    // LE Subrating
    uint16_t le_subrate_min;
    uint16_t le_subrate_max;
    uint16_t le_subrate_max_latency;
    uint16_t le_subrate_continuation_number;
    uint16_t le_subrate_supervision_timeout;

    // LE Security Manager
    sm_connection_t sm_connection;

#ifdef ENABLE_LE_LIMIT_ACL_FRAGMENT_BY_MAX_OCTETS
    uint16_t le_max_tx_octets;
#endif

    // ATT Connection
    att_connection_t att_connection;

    // ATT Server
    att_server_t    att_server;

#ifdef ENABLE_LE_PERIODIC_ADVERTISING
    hci_con_handle_t le_past_sync_handle;
    uint8_t          le_past_advertising_handle;
    uint16_t         le_past_service_data;
#endif

#endif

    l2cap_state_t l2cap_state;

#ifdef ENABLE_CLASSIC_PAIRING_OOB
    const uint8_t * classic_oob_c_192;
    const uint8_t * classic_oob_r_192;
    const uint8_t * classic_oob_c_256;
    const uint8_t * classic_oob_r_256;
#endif

} hci_connection_t;

#ifdef ENABLE_LE_ISOCHRONOUS_STREAMS


typedef enum {
    HCI_ISO_TYPE_INVALID = 0,
    HCI_ISO_TYPE_BIS,
    HCI_ISO_TYPE_CIS,
} hci_iso_type_t;

#define HCI_ISO_GROUP_ID_SINGLE_CIS 0xfe
#define HCI_ISO_GROUP_ID_INVALID    0xff

typedef enum{
    HCI_ISO_STREAM_STATE_IDLE,
    HCI_ISO_STREAM_W4_USER,
    HCI_ISO_STREAM_W2_ACCEPT,
    HCI_ISO_STREAM_W2_REJECT,
    HCI_ISO_STREAM_STATE_REQUESTED,
    HCI_ISO_STREAM_STATE_W4_ESTABLISHED,
    HCI_ISO_STREAM_STATE_ESTABLISHED,
    HCI_ISO_STREAM_STATE_W2_SETUP_ISO_INPUT,
    HCI_ISO_STREAM_STATE_W4_ISO_SETUP_INPUT,
    HCI_ISO_STREAM_STATE_W2_SETUP_ISO_OUTPUT,
    HCI_ISO_STREAM_STATE_W4_ISO_SETUP_OUTPUT,
    HCI_ISO_STREAM_STATE_ACTIVE,
    HCI_ISO_STREAM_STATE_W2_CLOSE,
    HCI_ISO_STREAM_STATE_W4_DISCONNECTED,
} hci_iso_stream_state_t;

typedef struct {
    // linked list - assert: first field
    btstack_linked_item_t    item;

    // state
    hci_iso_stream_state_t state;

    // iso type: bis or cis
    hci_iso_type_t iso_type;

    // group_id: big_handle or cig_id
    uint8_t group_id;

    // stream_id: bis_index or cis_id
    uint8_t stream_id;

    // only valid for HCI_ISO_TYPE_CIS
    hci_con_handle_t cis_handle;
    hci_con_handle_t acl_handle;

    // connection info
    uint8_t  number_of_subevents;
    uint8_t  burst_number_c_to_p;
    uint8_t  burst_number_p_to_c;
    uint8_t  flush_timeout_c_to_p;
    uint8_t  flush_timeout_p_to_c;
    uint16_t max_sdu_c_to_p;
    uint16_t max_sdu_p_to_c;
    uint16_t iso_interval_1250us;

    // re-assembly buffer (includes ISO packet header with timestamp)
    uint16_t reassembly_pos;
    uint8_t  reassembly_buffer[12 + HCI_ISO_PAYLOAD_SIZE];

    // number packets sent to controller
    uint8_t num_packets_sent;

    // packets to skip due to queuing them to late before
    uint8_t num_packets_to_skip;

    // request to send
    bool can_send_now_requested;

    // ready to send
    bool emit_ready_to_send;

} hci_iso_stream_t;
#endif

/**
 * HCI Initialization State Machine
 */
typedef enum hci_init_state{
    HCI_INIT_SEND_RESET = 0,
    HCI_INIT_W4_SEND_RESET,
    HCI_INIT_SEND_READ_LOCAL_VERSION_INFORMATION,
    HCI_INIT_W4_SEND_READ_LOCAL_VERSION_INFORMATION,

#ifndef HAVE_HOST_CONTROLLER_API
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

    // Support for Pre-Init before HCI Reset
    HCI_INIT_CUSTOM_PRE_INIT,
    HCI_INIT_W4_CUSTOM_PRE_INIT,
#endif

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
    HCI_INIT_SET_EVENT_MASK_2,
    HCI_INIT_W4_SET_EVENT_MASK_2,

#ifdef ENABLE_CLASSIC
    HCI_INIT_WRITE_SIMPLE_PAIRING_MODE,
    HCI_INIT_W4_WRITE_SIMPLE_PAIRING_MODE,
    HCI_INIT_WRITE_INQUIRY_MODE,
    HCI_INIT_W4_WRITE_INQUIRY_MODE,
    HCI_INIT_WRITE_SECURE_CONNECTIONS_HOST_ENABLE,
    HCI_INIT_W4_WRITE_SECURE_CONNECTIONS_HOST_ENABLE,
    HCI_INIT_SET_MIN_ENCRYPTION_KEY_SIZE,
    HCI_INIT_W4_SET_MIN_ENCRYPTION_KEY_SIZE,

#ifdef ENABLE_SCO_OVER_HCI
    // SCO over HCI
    HCI_INIT_WRITE_SYNCHRONOUS_FLOW_CONTROL_ENABLE,
    HCI_INIT_W4_WRITE_SYNCHRONOUS_FLOW_CONTROL_ENABLE,
    HCI_INIT_WRITE_DEFAULT_ERRONEOUS_DATA_REPORTING,
    HCI_INIT_W4_WRITE_DEFAULT_ERRONEOUS_DATA_REPORTING,
#endif

#if defined(ENABLE_SCO_OVER_HCI) || defined(ENABLE_SCO_OVER_PCM)
    // Broadcom SCO Routing and Configuration
    HCI_INIT_BCM_WRITE_SCO_PCM_INT,
    HCI_INIT_W4_BCM_WRITE_SCO_PCM_INT,
#endif
#ifdef ENABLE_SCO_OVER_PCM
    HCI_INIT_BCM_WRITE_I2SPCM_INTERFACE_PARAM,
    HCI_INIT_W4_BCM_WRITE_I2SPCM_INTERFACE_PARAM,
    HCI_INIT_BCM_WRITE_PCM_DATA_FORMAT_PARAM,
    HCI_INIT_W4_BCM_WRITE_PCM_DATA_FORMAT_PARAM,
#ifdef HAVE_BCM_PCM2
    HCI_INIT_BCM_PCM2_SETUP,
    HCI_INIT_W4_BCM_PCM2_SETUP,
#endif
#endif
#endif

#ifdef ENABLE_BLE
    HCI_INIT_LE_READ_BUFFER_SIZE,
    HCI_INIT_W4_LE_READ_BUFFER_SIZE,
    HCI_INIT_WRITE_LE_HOST_SUPPORTED,
    HCI_INIT_W4_WRITE_LE_HOST_SUPPORTED,
    HCI_INIT_LE_SET_EVENT_MASK,
    HCI_INIT_W4_LE_SET_EVENT_MASK,
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
#endif

#ifdef ENABLE_LE_PERIPHERAL
#ifdef ENABLE_LE_EXTENDED_ADVERTISING
    HCI_INIT_LE_READ_MAX_ADV_DATA_LEN,
    HCI_INIT_W4_LE_READ_MAX_ADV_DATA_LEN,
#endif
#endif

#ifdef ENABLE_LE_ISOCHRONOUS_STREAMS
    HCI_INIT_LE_SET_HOST_FEATURE_CONNECTED_ISO_STREAMS,
    HCI_INIT_W4_LE_SET_HOST_FEATURE_CONNECTED_ISO_STREAMS,
#endif

#ifdef ENABLE_BLE
    HCI_INIT_LE_SET_HOST_FEATURE_CONNECTION_SUBRATING,
    HCI_INIT_W4_LE_SET_HOST_FEATURE_CONNECTION_SUBRATING,
#endif

    HCI_INIT_DONE,

    HCI_FALLING_ASLEEP_DISCONNECT,
    HCI_FALLING_ASLEEP_W4_WRITE_SCAN_ENABLE,
    HCI_FALLING_ASLEEP_COMPLETE,

    HCI_INIT_AFTER_SLEEP,

    HCI_HALTING_CLASSIC_STOP,
    HCI_HALTING_LE_ADV_STOP,
    HCI_HALTING_LE_SCAN_STOP,
    HCI_HALTING_DISCONNECT_ALL,
    HCI_HALTING_W4_CLOSE_TIMER,
    HCI_HALTING_CLOSE,
    HCI_HALTING_CLOSE_DISCARDING_CONNECTIONS,

} hci_substate_t;

#define GAP_TASK_SET_LOCAL_NAME                 0x01
#define GAP_TASK_SET_EIR_DATA                   0x02
#define GAP_TASK_SET_CLASS_OF_DEVICE            0x04
#define GAP_TASK_SET_DEFAULT_LINK_POLICY        0x08
#define GAP_TASK_WRITE_SCAN_ENABLE              0x10
#define GAP_TASK_WRITE_PAGE_SCAN_ACTIVITY       0x20
#define GAP_TASK_WRITE_PAGE_SCAN_TYPE           0x40
#define GAP_TASK_WRITE_PAGE_TIMEOUT             0x80
#define GAP_TASK_WRITE_INQUIRY_SCAN_ACTIVITY   0x100
#define GAP_TASK_WRITE_INQUIRY_TX_POWER_LEVEL  0x200

enum {
    // Tasks
    LE_ADVERTISEMENT_TASKS_SET_ADV_DATA         = 1 << 0,
    LE_ADVERTISEMENT_TASKS_SET_SCAN_DATA        = 1 << 1,
    LE_ADVERTISEMENT_TASKS_SET_PARAMS           = 1 << 2,
    LE_ADVERTISEMENT_TASKS_SET_PERIODIC_PARAMS  = 1 << 3,
    LE_ADVERTISEMENT_TASKS_SET_PERIODIC_DATA    = 1 << 4,
    LE_ADVERTISEMENT_TASKS_REMOVE_SET           = 1 << 5,
    LE_ADVERTISEMENT_TASKS_SET_ADDRESS          = 1 << 6,
    LE_ADVERTISEMENT_TASKS_SET_ADDRESS_SET_0    = 1 << 7,
    LE_ADVERTISEMENT_TASKS_PRIVACY_NOTIFY       = 1 << 8,
};

enum {
    // State
    LE_ADVERTISEMENT_STATE_PARAMS_SET       = 1 << 0,
    LE_ADVERTISEMENT_STATE_ACTIVE           = 1 << 1,
    LE_ADVERTISEMENT_STATE_ENABLED          = 1 << 2,
    LE_ADVERTISEMENT_STATE_PERIODIC_ACTIVE  = 1 << 3,
    LE_ADVERTISEMENT_STATE_PERIODIC_ENABLED = 1 << 4,
    LE_ADVERTISEMENT_STATE_PRIVACY_PENDING  = 1 << 5,
};

enum {
    LE_WHITELIST_ON_CONTROLLER          = 1 << 0,
    LE_WHITELIST_ADD_TO_CONTROLLER      = 1 << 1,
    LE_WHITELIST_REMOVE_FROM_CONTROLLER = 1 << 2,
};

enum {
    LE_PERIODIC_ADVERTISER_LIST_ENTRY_ON_CONTROLLER          = 1 << 0,
    LE_PERIODIC_ADVERTISER_LIST_ENTRY_ADD_TO_CONTROLLER      = 1 << 1,
    LE_PERIODIC_ADVERTISER_LIST_ENTRY_REMOVE_FROM_CONTROLLER = 1 << 2,
};

typedef struct {
    btstack_linked_item_t  item;
    bd_addr_t      address;
    bd_addr_type_t address_type;
    uint8_t        state;   
} whitelist_entry_t;

typedef struct {
    btstack_linked_item_t  item;
    bd_addr_t      address;
    bd_addr_type_t address_type;
    uint8_t        sid;
    uint8_t        state;
} periodic_advertiser_list_entry_t;

#define MAX_NUM_RESOLVING_LIST_ENTRIES 64
typedef enum {
    LE_RESOLVING_LIST_SEND_ENABLE_ADDRESS_RESOLUTION,
    LE_RESOLVING_LIST_READ_SIZE,
    LE_RESOLVING_LIST_SEND_CLEAR,
    LE_RESOLVING_LIST_SET_IRK,
	LE_RESOLVING_LIST_UPDATES_ENTRIES,
    LE_RESOLVING_LIST_DONE
} le_resolving_list_state_t;

/**
 * main data structure
 */
typedef struct {
    // transport component with configuration
    const hci_transport_t * hci_transport;
    const void            * config;
    
    // chipset driver
    const btstack_chipset_t * chipset;

    // chipset driver requires pre-init
    bool chipset_pre_init;

    // hardware power controller
    const btstack_control_t * control;

#ifdef ENABLE_CLASSIC
    /* link key db */
    const btstack_link_key_db_t * link_key_db;
#endif

    // list of existing baseband connections
    btstack_linked_list_t     connections;

    /* callback to L2CAP layer */
    btstack_packet_handler_t acl_packet_handler;

    /* callback for SCO data */
    btstack_packet_handler_t sco_packet_handler;

    /* callbacks for events */
    btstack_linked_list_t event_handlers;

#ifdef ENABLE_CLASSIC
    /* callback for reject classic connection */
    int (*gap_classic_accept_callback)(bd_addr_t addr, hci_link_type_t link_type);
#endif

    // hardware error callback
    void (*hardware_error_callback)(uint8_t error);

#ifdef ENABLE_LE_ISOCHRONOUS_STREAMS
    /* callback for ISO data */
    btstack_packet_handler_t iso_packet_handler;

    /* fragmentation for ISO data */
    uint16_t  iso_fragmentation_pos;
    uint16_t  iso_fragmentation_total_size;
    bool      iso_fragmentation_tx_active;

    uint8_t   iso_packets_to_queue;
    // group id and type of active operation
    hci_iso_type_t iso_active_operation_type;
    uint8_t iso_active_operation_group_id;

    // list of iso streams
    btstack_linked_list_t iso_streams;

    // list of BIGs and BIG Syncs
    btstack_linked_list_t le_audio_bigs;
    btstack_linked_list_t le_audio_big_syncs;

    // list of CIGs
    btstack_linked_list_t le_audio_cigs;
#endif

    // basic configuration
    const char *       local_name;
    const uint8_t *    eir_data;
    uint32_t           class_of_device;
    bd_addr_t          local_bd_addr;
    uint16_t           default_link_policy_settings;
    uint8_t            allow_role_switch;
    uint8_t            ssp_enable;
    uint8_t            ssp_io_capability;
    uint8_t            ssp_authentication_requirement;
    uint8_t            ssp_auto_accept;
    bool               secure_connections_enable;
    bool               secure_connections_active;
    inquiry_mode_t     inquiry_mode;

#ifdef ENABLE_CLASSIC
    /* GAP tasks, see GAP_TASK_* */
    uint16_t           gap_tasks_classic;

    /* write page scan activity */
    uint16_t           new_page_scan_interval;
    uint16_t           new_page_scan_window;

    /* write page scan type */
    uint8_t            new_page_scan_type;

    /* write page timeout */
    uint16_t           page_timeout;

    // Errata-11838 mandates 7 bytes for GAP Security Level 1-3, we use 16 as default
    uint8_t            gap_required_encyrption_key_size;

    uint16_t           link_supervision_timeout;
    uint16_t           automatic_flush_timeout;

    gap_security_level_t gap_security_level;
    gap_security_level_t gap_minimal_service_security_level;
    gap_security_mode_t  gap_security_mode;

    uint32_t            inquiry_lap;      // GAP_IAC_GENERAL_INQUIRY or GAP_IAC_LIMITED_INQUIRY
    uint16_t            inquiry_scan_interval;
    uint16_t            inquiry_scan_window;
    int8_t              inquiry_tx_power_level;
    
    bool                gap_secure_connections_only_mode;
#endif

    // single buffer for HCI packet assembly + additional prebuffer for H4 drivers
    uint8_t   * hci_packet_buffer;
    uint8_t   hci_packet_buffer_data[HCI_OUTGOING_PRE_BUFFER_SIZE + HCI_OUTGOING_PACKET_BUFFER_SIZE];
    bool      hci_packet_buffer_reserved;
    uint16_t  acl_fragmentation_pos;
    uint16_t  acl_fragmentation_total_size;
    uint8_t   acl_fragmentation_tx_active;
     
    /* host to controller flow control */
    uint8_t  num_cmd_packets;
    uint8_t  acl_packets_total_num;
    uint16_t acl_data_packet_length;
    uint8_t  sco_packets_total_num;
    uint8_t  sco_data_packet_length;
    uint8_t  synchronous_flow_control_enabled;
    uint8_t  le_acl_packets_total_num;
    uint16_t le_data_packets_length;
    uint8_t  le_iso_packets_total_num;
    uint16_t le_iso_packets_length;
    uint8_t  sco_waiting_for_can_send_now;
    bool     sco_can_send_now;

    /* local supported features */
    uint8_t local_supported_features[8];

    /* local supported commands summary - complete info is 64 bytes */
    uint32_t local_supported_commands;

    /* bluetooth device information from hci read local version information */
    // uint16_t hci_version;
    // uint16_t hci_revision;
    // uint16_t lmp_version;
    uint16_t manufacturer;
    // uint16_t lmp_subversion;

    // usable ACL packet types given HCI_ACL_BUFFER_SIZE and local supported features
    uint16_t usable_packet_types_acl;

    // enabled ACL packet types
    uint16_t enabled_packet_types_acl;

    // usable SCO packet types given local supported features
    uint16_t usable_packet_types_sco;

    /* hci state machine */
    HCI_STATE      state;
    hci_substate_t substate;
    btstack_timer_source_t timeout;
    btstack_chipset_result_t chipset_result;

    uint16_t  last_cmd_opcode;

    uint8_t   cmds_ready;

    /* buffer for scan enable cmd - 0xff no change */
    uint8_t   new_scan_enable_value;

    uint8_t   discoverable;
    uint8_t   connectable;
    uint8_t   bondable;

    uint8_t   inquiry_state;    // see hci.c for state defines
    uint16_t  inquiry_max_period_length;
    uint16_t  inquiry_min_period_length;

    bd_addr_t remote_name_addr;
    uint16_t  remote_name_clock_offset;
    uint8_t   remote_name_page_scan_repetition_mode;
    uint8_t   remote_name_state;    // see hci.c for state defines

    bd_addr_t gap_pairing_addr;
    uint8_t   gap_pairing_state;    // see hci.c for state defines
    uint8_t   gap_pairing_pin_len;
    union {
        const uint8_t * gap_pairing_pin;
        uint32_t     gap_pairing_passkey;
    } gap_pairing_input;
    
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

    // LE Whitelist Management
    uint8_t               le_whitelist_capacity;
    btstack_linked_list_t le_whitelist;

    // Connection parameters
    uint16_t le_connection_scan_interval;
    uint16_t le_connection_scan_window;
    uint16_t le_connection_interval_min;
    uint16_t le_connection_interval_max;
    uint16_t le_connection_latency;
    uint16_t le_supervision_timeout;
    uint16_t le_minimum_ce_length;
    uint16_t le_maximum_ce_length;

    // GAP Privacy
    btstack_linked_list_t gap_privacy_clients;

#ifdef ENABLE_HCI_COMMAND_STATUS_DISCARDED_FOR_FAILED_CONNECTIONS_WORKAROUND
    hci_con_handle_t hci_command_con_handle;
#endif
#endif

#ifdef ENABLE_LE_CENTRAL
    bool   le_scanning_enabled;
    bool   le_scanning_active;

    le_connecting_state_t le_connecting_state;
    le_connecting_state_t le_connecting_request;

    bool     le_scanning_param_update;
    uint8_t  le_scan_filter_duplicates;
    uint8_t  le_scan_type;
    uint8_t  le_scan_filter_policy;
    uint8_t  le_scan_phys;
    uint16_t le_scan_interval;
    uint16_t le_scan_window;

    uint8_t  le_connection_own_addr_type;
    uint8_t  le_connection_phys;
    bd_addr_t le_connection_own_address;

#ifdef ENABLE_LE_EXTENDED_ADVERTISING
    btstack_linked_list_t le_periodic_advertiser_list;
    uint16_t        le_periodic_terminate_sync_handle;

    // Periodic Advertising Sync parameters
    uint8_t         le_periodic_sync_options;
    uint8_t         le_periodic_sync_advertising_sid;
    bd_addr_type_t  le_periodic_sync_advertiser_address_type;
    bd_addr_t       le_periodic_sync_advertiser_address;
    uint16_t        le_periodic_sync_skip;
    uint16_t        le_periodic_sync_timeout;
    uint8_t         le_periodic_sync_cte_type;
    le_connecting_state_t le_periodic_sync_state;
    le_connecting_state_t le_periodic_sync_request;

    // Periodic Advertising Sync Transfer (PAST)
    bool     le_past_set_default_params;
    uint8_t  le_past_mode;
    uint16_t le_past_skip;
    uint16_t le_past_sync_timeout;
    uint8_t  le_past_cte_type;
#endif
#endif

    le_connection_parameter_range_t le_connection_parameter_range;

    // TODO: move LE_ADVERTISEMENT_TASKS_SET_ADDRESS flag which is used for both roles into
    //  some generic gap_le variable
    uint16_t  le_advertisements_todo;

#ifdef ENABLE_LE_PERIPHERAL
    uint8_t  * le_advertisements_data;
    uint8_t    le_advertisements_data_len;

    uint8_t  * le_scan_response_data;
    uint8_t    le_scan_response_data_len;

    uint16_t le_advertisements_interval_min;
    uint16_t le_advertisements_interval_max;
    uint8_t  le_advertisements_type;
    uint8_t  le_advertisements_direct_address_type;
    uint8_t  le_advertisements_channel_map;
    uint8_t  le_advertisements_filter_policy;
    bd_addr_t le_advertisements_direct_address;
    uint8_t   le_advertisements_own_addr_type;
    bd_addr_t le_advertisements_own_address;

    uint8_t  le_advertisements_state;

    bool     le_advertisements_enabled_for_current_roles;
    uint8_t le_max_number_peripheral_connections;

#ifdef ENABLE_LE_EXTENDED_ADVERTISING
    btstack_linked_list_t le_advertising_sets;
    uint16_t le_maximum_advertising_data_length;
    uint8_t  le_advertising_set_in_current_command;
    uint16_t le_resolvable_private_address_update_s;
#endif
#endif

#ifdef ENABLE_LE_DATA_LENGTH_EXTENSION
    // LE Data Length
    uint16_t le_supported_max_tx_octets;
    uint16_t le_supported_max_tx_time;
#endif

    // custom BD ADDR
    bd_addr_t custom_bd_addr; 
    uint8_t   custom_bd_addr_set;

#ifdef ENABLE_CLASSIC
    uint8_t master_slave_policy;
#endif

    // address and address_type of active create connection command (ACL, SCO, LE)
    bd_addr_t      outgoing_addr;
    bd_addr_type_t outgoing_addr_type;

    // LE Resolving List
#ifdef ENABLE_LE_PRIVACY_ADDRESS_RESOLUTION
    le_privacy_mode_t         le_privacy_mode;
    le_resolving_list_state_t le_resolving_list_state;
    uint16_t                  le_resolving_list_size;
    uint8_t                   le_resolving_list_add_entries[(MAX_NUM_RESOLVING_LIST_ENTRIES + 7) / 8];
    uint8_t                   le_resolving_list_set_privacy_mode[(MAX_NUM_RESOLVING_LIST_ENTRIES + 7) / 8];
	uint8_t                   le_resolving_list_remove_entries[(MAX_NUM_RESOLVING_LIST_ENTRIES + 7) / 8];
#endif

#ifdef ENABLE_CLASSIC_PAIRING_OOB
	bool                      classic_read_local_oob_data;
	hci_con_handle_t          classic_oob_con_handle;
#endif

#ifdef HAVE_SCO_TRANSPORT
	const btstack_sco_transport_t * sco_transport;
#endif
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
 * @brief Enable custom init for chipset driver to send HCI commands before HCI Reset
 */
void hci_enable_custom_pre_init(void);

/**
 * @brief Configure Bluetooth hardware control. Has to be called before power on.
 * @[aram hardware_control implementation
 */
void hci_set_control(const btstack_control_t *hardware_control);

#ifdef HAVE_SCO_TRANSPORT
/**
 * @brief Set SCO Transport implementation for SCO over PCM mode
 * @param sco_transport that sends SCO over I2S or PCM interface
 */
void hci_set_sco_transport(const btstack_sco_transport_t *sco_transport);
#endif

#ifdef ENABLE_CLASSIC
/**
 * @brief Configure Bluetooth hardware control. Has to be called before power on.
 */
void hci_set_link_key_db(btstack_link_key_db_t const * link_key_db);
#endif

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
 * @brief Set number of ISO packets to buffer for BIS/CIS
 * @param num_packets (default = 1)
 */
void hci_set_num_iso_packets_to_queue(uint8_t num_packets);

/**
 * @brief Set inquiry mode: standard, with RSSI, with RSSI + Extended Inquiry Results. Has to be called before power on.
 * @param inquriy_mode see bluetooth_defines.h
 */
void hci_set_inquiry_mode(inquiry_mode_t inquriy_mode);

/**
 * @brief Requests the change of BTstack power mode.
 * @param power_mode
 * @return 0 if success, otherwise error
 */
int  hci_power_control(HCI_POWER_MODE power_mode);

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
 * @brief Remove event packet handler.
 */
void hci_remove_event_handler(btstack_packet_callback_registration_t * callback_handler);

/**
 * @brief Registers a packet handler for ACL data. Used by L2CAP
 */
void hci_register_acl_packet_handler(btstack_packet_handler_t handler);

/**
 * @brief Registers a packet handler for SCO data. Used for HSP and HFP profiles.
 */
void hci_register_sco_packet_handler(btstack_packet_handler_t handler);

/**
 * @brief Registers a packet handler for ISO data. Used for LE Audio profiles
 */
void hci_register_iso_packet_handler(btstack_packet_handler_t handler);

// Sending HCI Commands

/** 
 * @brief Check if CMD packet can be sent to controller
 * @return true if command can be sent
 */
bool hci_can_send_command_packet_now(void);

/**
 * @brief Creates and sends HCI command packets based on a template and a list of parameters. Will return error if outgoing data buffer is occupied.
 * @return status
 */
uint8_t hci_send_cmd(const hci_cmd_t * cmd, ...);


// Sending SCO Packets

/** @brief Get SCO payload length for existing SCO connection and current SCO Voice setting
 *  @note  Using SCO packets of the exact length is required for USB transfer in general and some H4 controllers as well
 *  @param sco_con_handle
 *  @return Length of SCO payload in bytes (not audio frames) incl. 3 byte header
 */
uint16_t hci_get_sco_packet_length_for_connection(hci_con_handle_t sco_con_handle);

/** @brief Get SCO packet length for one of the existing SCO connections and current SCO Voice setting
 *  @deprecated Please use hci_get_sco_packet_length_for_connection instead
 *  @note  Using SCO packets of the exact length is required for USB transfer
 *  @return Length of SCO packets in bytes (not audio frames) incl. 3 byte header
 */
uint16_t hci_get_sco_packet_length(void);

/**
 * @brief Request emission of HCI_EVENT_SCO_CAN_SEND_NOW as soon as possible
 * @note HCI_EVENT_SCO_CAN_SEND_NOW might be emitted during call to this function
 *       so packet handler should be ready to handle it
 */
void hci_request_sco_can_send_now_event(void);

/**
 * @brief Check HCI packet buffer and if SCO packet can be sent to controller
 * @return true if sco packet can be sent
 */
bool hci_can_send_sco_packet_now(void);

/**
 * @brief Check if SCO packet can be sent to controller
 * @return true if sco packet can be sent
 */
bool hci_can_send_prepared_sco_packet_now(void);

/**
 * @brief Send SCO packet prepared in HCI packet buffer
 */
uint8_t hci_send_sco_packet_buffer(int size);

/**
 * @brief Request emission of HCI_EVENT_BIS_CAN_SEND_NOW for all BIS as soon as possible
 * @param big_handle
 * @note HCI_EVENT_ISO_CAN_SEND_NOW might be emitted during call to this function
 *       so packet handler should be ready to handle it
 */
uint8_t hci_request_bis_can_send_now_events(uint8_t big_handle);

/**
 * @brief Request emission of HCI_EVENT_CIS_CAN_SEND_NOW for CIS as soon as possible
 * @param cis_con_handle
 * @note HCI_EVENT_CIS_CAN_SEND_NOW might be emitted during call to this function
 *       so packet handler should be ready to handle it
 */
uint8_t hci_request_cis_can_send_now_events(hci_con_handle_t cis_con_handle);

/**
 * @brief Send ISO packet prepared in HCI packet buffer
 */
uint8_t hci_send_iso_packet_buffer(uint16_t size);

/**
 * Reserves outgoing packet buffer.
 * @note Must only be called after a 'can send now' check or event
 * @note Asserts if packet buffer is already reserved
 */
void hci_reserve_packet_buffer(void);

/**
 * Get pointer for outgoing packet buffer
 */
uint8_t* hci_get_outgoing_packet_buffer(void);

/**
 * Release outgoing packet buffer\
 * @note only called instead of hci_send_prepared
 */
void hci_release_packet_buffer(void);

/**
* @brief Sets the master/slave policy
* @param policy (0: attempt to become master, 1: let connecting device decide)
*/
void hci_set_master_slave_policy(uint8_t policy);

/**
 * @brief Check if Controller supports BR/EDR (Bluetooth Classic)
 * @return true if supported
 * @note only valid in working state
 */
bool hci_classic_supported(void);

/**
 * @brief Check if Controller supports LE (Bluetooth Low Energy)
 * @return true if supported
 * @note only valid in working state
 */
bool hci_le_supported(void);

/**
 * @brief Check if LE Extended Advertising is supported
 * @return true if supported
 */
bool hci_le_extended_advertising_supported(void);

/** @brief Check if address type corresponds to LE connection
 *  @bparam address_type
 *  @erturn true if LE connection
 */
bool hci_is_le_connection_type(bd_addr_type_t address_type);

/** @brief Check if address type corresponds to Identity Address
 *  @bparam address_type
 *  @erturn true if LE connection
 */
bool hci_is_le_identity_address_type(bd_addr_type_t address_type);

/* API_END */


/**
 * va_list version of hci_send_cmd, call hci_send_cmd_packet
 * @return status
 */
uint8_t hci_send_cmd_va_arg(const hci_cmd_t * cmd, va_list argptr);

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
hci_connection_t * hci_connection_for_bd_addr_and_type(const bd_addr_t addr, bd_addr_type_t addr_type);

/**
 * Check if outgoing packet buffer is reserved. Used for internal checks in l2cap.c
 * @return true if packet buffer is reserved
 */
bool hci_is_packet_buffer_reserved(void);

/**
 * Check hci packet buffer is free and a classic acl packet can be sent to controller
 * @return true if ACL Classic packet can be sent now
 */
bool hci_can_send_acl_classic_packet_now(void);

/**
 * Check hci packet buffer is free and an LE acl packet can be sent to controller
 * @return true if ACL LE packet can be sent now
 */
bool hci_can_send_acl_le_packet_now(void);

/**
 * Check hci packet buffer is free and an acl packet for the given handle can be sent to controller
 * @return true if ACL packet for con_handle can be sent now
 */
bool hci_can_send_acl_packet_now(hci_con_handle_t con_handle);

/**
 * Check if acl packet for the given handle can be sent to controller
 * @return true if ACL packet for con_handle can be sent now
 */
bool hci_can_send_prepared_acl_packet_now(hci_con_handle_t con_handle);

/**
 * Send acl packet prepared in hci packet buffer
 * @return status
 */
uint8_t hci_send_acl_packet_buffer(int size);

/**
 * Check if authentication is active. It delays automatic disconnect while no L2CAP connection
 * Called by l2cap.
 */
bool hci_authentication_active_for_handle(hci_con_handle_t handle);

/**
 * Get maximal ACL Classic data packet length based on used buffer size. Called by L2CAP
 */
uint16_t hci_max_acl_data_packet_length(void);

/**
 * Get supported ACL packet types. Already flipped for create connection. Called by L2CAP
 */
uint16_t hci_usable_acl_packet_types(void);

/**
 * Set filter for set of ACL packet types returned by hci_usable_acl_packet_types
 * @param packet_types see CL_PACKET_TYPES_* in bluetooth.h, default: ACL_PACKET_TYPES_ALL
 */
void hci_enable_acl_packet_types(uint16_t packet_types);

/**
 * Get supported SCO packet types. Not flipped. Called by HFP
  */
uint16_t hci_usable_sco_packet_types(void);

/**
 * Check if ACL packets marked as non flushable can be sent. Called by L2CAP
 */
bool hci_non_flushable_packet_boundary_flag_supported(void);

/**
 * Return current automatic flush timeout setting
 */
uint16_t hci_automatic_flush_timeout(void);

/**
 * Check if remote supported features query has completed
 */
bool hci_remote_features_available(hci_con_handle_t con_handle);

/**
 * Trigger remote supported features query
 */
void hci_remote_features_query(hci_con_handle_t con_handle);

/**
 * Check if extended SCO Link is supported
 */
bool hci_extended_sco_link_supported(void);

/**
 * Check if SSP is supported on both sides. Called by L2CAP
 */
bool gap_ssp_supported_on_both_sides(hci_con_handle_t handle);

/**
 * Disconn because of security block. Called by L2CAP
 */
void hci_disconnect_security_block(hci_con_handle_t con_handle);

/**
 * Query if remote side supports eSCO
 * @param con_handle
 */
bool hci_remote_esco_supported(hci_con_handle_t con_handle);

/**
 * Query remote supported SCO packets based on remote supported features
 * @param con_handle
 */
uint16_t hci_remote_sco_packet_types(hci_con_handle_t con_handle);

/**
 * Emit current HCI state. Called by daemon
 */
void hci_emit_state(void);

/** 
 * Send complete CMD packet. Called by daemon and hci_send_cmd_va_arg
 * @return status
 */
uint8_t hci_send_cmd_packet(uint8_t *packet, int size);

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
 * @naote internal use. use gap_random_address_set from gap.h instead
 */
void hci_le_random_address_set(const bd_addr_t random_address);

/**
 * @note internal use by sm
 */
void hci_load_le_device_db_entry_into_resolving_list(uint16_t le_device_db_index);

/**
 * @note internal use by sm
 */
void hci_remove_le_device_db_entry_from_resolving_list(uint16_t le_device_db_index);

/**
 * @note internal use
 */
uint16_t hci_number_free_acl_slots_for_connection_type(bd_addr_type_t address_type);

/**
 * @brief Clear Periodic Advertiser List
 * @return status
 */
uint8_t gap_periodic_advertiser_list_clear(void);

/**
 * @brief Add entry to Periodic Advertiser List
 * @param address_type
 * @param address
 * @param advertising_sid
 * @return
 */
uint8_t gap_periodic_advertiser_list_add(bd_addr_type_t address_type, const bd_addr_t address, uint8_t advertising_sid);

/**
 * Remove entry from Periodic Advertising List
 * @param address_type
 * @param address
 * @param advertising_sid
 * @return
 */
uint8_t gap_periodic_advertiser_list_remove(bd_addr_type_t address_type, const bd_addr_t address, uint8_t advertising_sid);

/**
 * @brief Synchronize with a periodic advertising train from an advertiser and begin receiving periodic advertising packets.
 * @param options
 * @param advertising_sid
 * @param advertiser_address_type
 * @param advertiser_address
 * @param skip
 * @param sync_timeout
 * @param sync_cte_type
 * @return
 */
uint8_t gap_periodic_advertising_create_sync(uint8_t options, uint8_t advertising_sid, bd_addr_type_t advertiser_address_type,
                                             bd_addr_t advertiser_address, uint16_t skip, uint16_t sync_timeout, uint8_t sync_cte_type);

/**
 * @brief Cancel sync periodic advertising train while it is pending.
 * @return status
 */
uint8_t gap_periodic_advertising_create_sync_cancel(void);

/**
 * @biref Stop reception of the periodic advertising train
 * @param sync_handle
 * @return status
 */
uint8_t gap_periodic_advertising_terminate_sync(uint16_t sync_handle);

/**
 * @brief Get Controller Manufacturer
 * @returns company_id - see bluetooth_company_id.h
 */
uint16_t hci_get_manufacturer(void);

/**
 *  Get Classic Allow Role Switch param
 */
uint8_t hci_get_allow_role_switch(void);

/**
 * Get state
 */
HCI_STATE hci_get_state(void);

/**
 * @brief De-Init HCI
 */
void hci_deinit(void);

// defer disconnect on dedicated bonding complete, used internally for CTKD
uint8_t hci_dedicated_bonding_defer_disconnect(hci_con_handle_t con_handle, bool defer);

// Only for PTS testing

// Disable automatic L2CAP disconnect if no L2CAP connection is established
void hci_disable_l2cap_timeout_check(void);

#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
// setup test connections, used for fuzzing
void hci_setup_test_connections_fuzz(void);

// free all connections, used for fuzzing
void hci_free_connections_fuzz(void);

// simulate stack bootup
void hci_simulate_working_fuzz(void);

// get hci struct
hci_stack_t * hci_get_stack();

#endif

#if defined __cplusplus
}
#endif

#endif // HCI_H
