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
 *  l2cap.h
 *
 *  Logical Link Control and Adaption Protocol (L2CAP)
 *
 *  Created by Matthias Ringwald on 5/16/09.
 */

#ifndef __L2CAP_H
#define __L2CAP_H

#include "hci.h"
#include "l2cap_signaling.h"
#include <btstack/utils.h>
#include <btstack/btstack.h>

#if defined __cplusplus
extern "C" {
#endif
    
#define L2CAP_SIG_ID_INVALID 0

#define L2CAP_HEADER_SIZE 4

// size of HCI ACL + L2CAP Header for regular data packets (8)
#define COMPLETE_L2CAP_HEADER (HCI_ACL_HEADER_SIZE + L2CAP_HEADER_SIZE)
    
// minimum signaling MTU
#define L2CAP_MINIMAL_MTU 48
#define L2CAP_DEFAULT_MTU 672
    
// Minimum/default MTU
#define L2CAP_LE_DEFAULT_MTU  23

// check L2CAP MTU
#if (L2CAP_MINIMAL_MTU + L2CAP_HEADER_SIZE) > HCI_ACL_PAYLOAD_SIZE
#error "HCI_ACL_PAYLOAD_SIZE too small for minimal L2CAP MTU of 48 bytes"
#endif    
    
// L2CAP Fixed Channel IDs    
#define L2CAP_CID_SIGNALING                 0x0001
#define L2CAP_CID_CONNECTIONLESS_CHANNEL    0x0002
#define L2CAP_CID_ATTRIBUTE_PROTOCOL        0x0004
#define L2CAP_CID_SIGNALING_LE              0x0005
#define L2CAP_CID_SECURITY_MANAGER_PROTOCOL 0x0006

// L2CAP Configuration Result Codes
#define L2CAP_CONF_RESULT_UNKNOWN_OPTIONS   0x0003

// L2CAP Reject Result Codes
#define L2CAP_REJ_CMD_UNKNOWN               0x0000
    
// Response Timeout eXpired
#define L2CAP_RTX_TIMEOUT_MS   10000

// Extended Response Timeout eXpired
#define L2CAP_ERTX_TIMEOUT_MS 120000

// private structs
typedef enum {
    L2CAP_STATE_CLOSED = 1,           // no baseband
    L2CAP_STATE_WILL_SEND_CREATE_CONNECTION,
    L2CAP_STATE_WAIT_CONNECTION_COMPLETE,
    L2CAP_STATE_WAIT_REMOTE_SUPPORTED_FEATURES,
    L2CAP_STATE_WAIT_INCOMING_SECURITY_LEVEL_UPDATE,
    L2CAP_STATE_WAIT_OUTGOING_SECURITY_LEVEL_UPDATE,
    L2CAP_STATE_WAIT_CLIENT_ACCEPT_OR_REJECT,
    L2CAP_STATE_WAIT_CONNECT_RSP, // from peer
    L2CAP_STATE_CONFIG,
    L2CAP_STATE_OPEN,
    L2CAP_STATE_WAIT_DISCONNECT,  // from application
    L2CAP_STATE_WILL_SEND_CONNECTION_REQUEST,
    L2CAP_STATE_WILL_SEND_CONNECTION_RESPONSE_INSUFFICIENT_SECURITY,
    L2CAP_STATE_WILL_SEND_CONNECTION_RESPONSE_DECLINE,
    L2CAP_STATE_WILL_SEND_CONNECTION_RESPONSE_ACCEPT,   
    L2CAP_STATE_WILL_SEND_DISCONNECT_REQUEST,
    L2CAP_STATE_WILL_SEND_DISCONNECT_RESPONSE,
    L2CAP_STATE_INVALID,
} L2CAP_STATE;

typedef enum {
    L2CAP_CHANNEL_STATE_VAR_NONE                  = 0,
    L2CAP_CHANNEL_STATE_VAR_RCVD_CONF_REQ         = 1 << 0,
    L2CAP_CHANNEL_STATE_VAR_RCVD_CONF_RSP         = 1 << 1,
    L2CAP_CHANNEL_STATE_VAR_SEND_CONF_REQ         = 1 << 2,
    L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP         = 1 << 3,
    L2CAP_CHANNEL_STATE_VAR_SENT_CONF_REQ         = 1 << 4,
    L2CAP_CHANNEL_STATE_VAR_SENT_CONF_RSP         = 1 << 5,
    L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP_MTU     = 1 << 6,   // in CONF RSP, add MTU field
    L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP_CONT    = 1 << 7,   // in CONF RSP, set CONTINUE flag
    L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP_INVALID = 1 << 8,   // in CONF RSP, send UNKNOWN OPTIONS
    L2CAP_CHANNEL_STATE_VAR_SEND_CMD_REJ_UNKNOWN  = 1 << 9,   // send CMD_REJ with reason unknown
    L2CAP_CHANNEL_STATE_VAR_SEND_CONN_RESP_PEND   = 1 << 10,  // send Connection Respond with pending
} L2CAP_CHANNEL_STATE_VAR;

// info regarding an actual connection
typedef struct {
    // linked list - assert: first field
    linked_item_t    item;
    
    L2CAP_STATE state;
    L2CAP_CHANNEL_STATE_VAR state_var;
    
    bd_addr_t address;
    hci_con_handle_t handle;
    
    uint8_t   remote_sig_id;    // used by other side, needed for delayed response
    uint8_t   local_sig_id;     // own signaling identifier
    
    uint16_t  local_cid;
    uint16_t  remote_cid;
    
    uint16_t  local_mtu;
    uint16_t  remote_mtu;
    
    uint16_t  flush_timeout;    // default 0xffff

    uint16_t  psm;
    
    gap_security_level_t required_security_level;

    uint8_t   packets_granted;    // number of L2CAP/ACL packets client is allowed to send
    
    uint8_t   reason; // used in decline internal
    
    timer_source_t rtx; // also used for ertx

    // client connection
    void * connection;
    
    // internal connection
    btstack_packet_handler_t packet_handler;
    
} l2cap_channel_t;

// info regarding potential connections
typedef struct {
    // linked list - assert: first field
    linked_item_t    item;
    
    // service id
    uint16_t  psm;
    
    // incoming MTU
    uint16_t mtu;
    
    // client connection
    void *connection;    
    
    // internal connection
    btstack_packet_handler_t packet_handler;

    // required security level
    gap_security_level_t required_security_level;    
} l2cap_service_t;


typedef struct l2cap_signaling_response {
    hci_con_handle_t handle;
    uint8_t  sig_id;
    uint8_t  code;
    uint16_t data; // infoType for INFORMATION REQUEST, result for CONNECTION request and command unknown
} l2cap_signaling_response_t;
    

void l2cap_block_new_credits(uint8_t blocked);

int  l2cap_can_send_fixed_channel_packet_now(uint16_t handle);

// @deprecated use l2cap_can_send_fixed_channel_packet_now instead
int  l2cap_can_send_connectionless_packet_now(void);

int l2cap_send_echo_request(uint16_t handle, uint8_t *data, uint16_t len);

void l2cap_require_security_level_2_for_outgoing_sdp(void);  // for PTS testing only

/* API_START */

/** 
 * @brief Set up L2CAP and register L2CAP with HCI layer.
 */
void l2cap_init(void);

/** 
 * @brief Registers a packet handler that handles HCI and general BTstack events.
 */
void l2cap_register_packet_handler(void (*handler)(void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size));

/** 
 * @brief Creates L2CAP channel to the PSM of a remote device with baseband address. A new baseband connection will be initiated if necessary.
 */
void l2cap_create_channel_internal(void * connection, btstack_packet_handler_t packet_handler, bd_addr_t address, uint16_t psm, uint16_t mtu);

/** 
 * @brief Disconnects L2CAP channel with given identifier. 
 */
void l2cap_disconnect_internal(uint16_t local_cid, uint8_t reason);

/** 
 * @brief Queries the maximal transfer unit (MTU) for L2CAP channel with given identifier. 
 */
uint16_t l2cap_get_remote_mtu_for_local_cid(uint16_t local_cid);

/** 
 * @brief Sends L2CAP data packet to the channel with given identifier.
 */
int l2cap_send_internal(uint16_t local_cid, uint8_t *data, uint16_t len);

/** 
 * @brief Registers L2CAP service with given PSM and MTU, and assigns a packet handler. On embedded systems, use NULL for connection parameter.
 */
void l2cap_register_service_internal(void *connection, btstack_packet_handler_t packet_handler, uint16_t psm, uint16_t mtu, gap_security_level_t security_level);

/** 
 * @brief Unregisters L2CAP service with given PSM.  On embedded systems, use NULL for connection parameter.
 */
void l2cap_unregister_service_internal(void *connection, uint16_t psm);

/** 
 * @brief Accepts/Deny incoming L2CAP connection.
 */
void l2cap_accept_connection_internal(uint16_t local_cid);
void l2cap_decline_connection_internal(uint16_t local_cid, uint8_t reason);

/** 
 * @brief Request LE connection parameter update
 */
int l2cap_le_request_connection_parameter_update(uint16_t handle, uint16_t interval_min, uint16_t interval_max, uint16_t slave_latency, uint16_t timeout_multiplier);

/** 
 * @brief Non-blocking UART write
 */
int  l2cap_can_send_packet_now(uint16_t local_cid);    
int  l2cap_reserve_packet_buffer(void);
void l2cap_release_packet_buffer(void);

/** 
 * @brief Get outgoing buffer and prepare data.
 */
uint8_t *l2cap_get_outgoing_buffer(void);

int l2cap_send_prepared(uint16_t local_cid, uint16_t len);

int l2cap_send_prepared_connectionless(uint16_t handle, uint16_t cid, uint16_t len);

/** 
 * @brief Bluetooth 4.0 - allows to register handler for Attribute Protocol and Security Manager Protocol.
 */
void l2cap_register_fixed_channel(btstack_packet_handler_t packet_handler, uint16_t channel_id);

uint16_t l2cap_max_mtu(void);
uint16_t l2cap_max_le_mtu(void);

int  l2cap_send_connectionless(uint16_t handle, uint16_t cid, uint8_t *data, uint16_t len);
/* API_END */

#if defined __cplusplus
}
#endif

#endif // __L2CAP_H
