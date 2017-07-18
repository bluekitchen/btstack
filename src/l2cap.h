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
#include "btstack_util.h"
#include "bluetooth.h"

#if defined __cplusplus
extern "C" {
#endif
    
// check L2CAP MTU
#ifdef ENABLE_CLASSIC
#if (L2CAP_HEADER_SIZE + L2CAP_MINIMAL_MTU) > HCI_ACL_PAYLOAD_SIZE
#error "HCI_ACL_PAYLOAD_SIZE too small for minimal L2CAP MTU of 48 bytes"
#endif    
#endif
#ifdef ENABLE_BLE
#if (L2CAP_HEADER_SIZE + L2CAP_LE_DEFAULT_MTU) > HCI_ACL_PAYLOAD_SIZE
#error "HCI_ACL_PAYLOAD_SIZE too small for minimal L2CAP LE MTU of 23 bytes"
#endif
#endif

#define L2CAP_LE_AUTOMATIC_CREDITS 0xffff

// private structs
typedef enum {
    L2CAP_STATE_CLOSED = 1,           // no baseband
    L2CAP_STATE_WILL_SEND_CREATE_CONNECTION,
    L2CAP_STATE_WAIT_CONNECTION_COMPLETE,
    L2CAP_STATE_WAIT_REMOTE_SUPPORTED_FEATURES,
    L2CAP_STATE_WAIT_INCOMING_SECURITY_LEVEL_UPDATE,
    L2CAP_STATE_WAIT_OUTGOING_SECURITY_LEVEL_UPDATE,
    L2CAP_STATE_WAIT_INCOMING_EXTENDED_FEATURES,    // only for Enhanced Retransmission Mode
    L2CAP_STATE_WAIT_OUTGOING_EXTENDED_FEATURES,    // only for Enhanced Retransmission Mode
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
    L2CAP_STATE_WILL_SEND_LE_CONNECTION_REQUEST,
    L2CAP_STATE_WILL_SEND_LE_CONNECTION_RESPONSE_DECLINE,
    L2CAP_STATE_WILL_SEND_LE_CONNECTION_RESPONSE_ACCEPT,
    L2CAP_STATE_WAIT_LE_CONNECTION_RESPONSE,
    L2CAP_STATE_INVALID,
} L2CAP_STATE;

typedef enum {
    L2CAP_CHANNEL_STATE_VAR_NONE                   = 0,
    L2CAP_CHANNEL_STATE_VAR_RCVD_CONF_REQ          = 1 << 0,
    L2CAP_CHANNEL_STATE_VAR_RCVD_CONF_RSP          = 1 << 1,
    L2CAP_CHANNEL_STATE_VAR_SEND_CONF_REQ          = 1 << 2,
    L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP          = 1 << 3,
    L2CAP_CHANNEL_STATE_VAR_SENT_CONF_REQ          = 1 << 4,
    L2CAP_CHANNEL_STATE_VAR_SENT_CONF_RSP          = 1 << 5,
    L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP_MTU      = 1 << 6,   // in CONF RSP, add MTU field
    L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP_CONT     = 1 << 7,   // in CONF RSP, set CONTINUE flag
    L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP_INVALID  = 1 << 8,   // in CONF RSP, send UNKNOWN OPTIONS
    L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP_REJECTED = 1 << 9,   // in CONF RSP, send Unacceptable Parameters (ERTM)
    L2CAP_CHANNEL_STATE_VAR_BASIC_FALLBACK_TRIED   = 1 << 10,  // set when ERTM was requested but we want only Basic mode (ERM)
    L2CAP_CHANNEL_STATE_VAR_SEND_CMD_REJ_UNKNOWN   = 1 << 11,  // send CMD_REJ with reason unknown
    L2CAP_CHANNEL_STATE_VAR_SEND_CONN_RESP_PEND    = 1 << 12,  // send Connection Respond with pending
    L2CAP_CHANNEL_STATE_VAR_INCOMING               = 1 << 15,  // channel is incoming
} L2CAP_CHANNEL_STATE_VAR;

typedef struct {
    uint8_t tx_seq;
    uint16_t pos;
    uint16_t sdu_length;
} l2cap_ertm_rx_packet_state_t;

typedef struct {
    btstack_timer_source_t retransmission_timer;
    btstack_timer_source_t monitor_timer;
    uint16_t len;
    uint8_t tx_seq;
    uint8_t retry_count;
    uint8_t retransmission_requested;
    uint8_t retransmission_final;
} l2cap_ertm_tx_packet_state_t;


// info regarding an actual connection
typedef struct {
    // linked list - assert: first field
    btstack_linked_item_t    item;
    
    // packet handler
    btstack_packet_handler_t packet_handler;

    // timer
    btstack_timer_source_t rtx; // also used for ertx

    L2CAP_STATE state;
    L2CAP_CHANNEL_STATE_VAR state_var;

    // info
    hci_con_handle_t con_handle;

    bd_addr_t address;
    bd_addr_type_t address_type;
    
    uint8_t   remote_sig_id;    // used by other side, needed for delayed response
    uint8_t   local_sig_id;     // own signaling identifier
    
    uint16_t  local_cid;
    uint16_t  remote_cid;
    
    uint16_t  local_mtu;
    uint16_t  remote_mtu;

    uint16_t  flush_timeout;    // default 0xffff

    uint16_t  psm;
    
    gap_security_level_t required_security_level;

    uint8_t   reason; // used in decline internal
    uint8_t   waiting_for_can_send_now;

    // LE Data Channels

    // incoming SDU
    uint8_t * receive_sdu_buffer;
    uint16_t  receive_sdu_len;
    uint16_t  receive_sdu_pos;

    // outgoing SDU
    uint8_t  * send_sdu_buffer;
    uint16_t   send_sdu_len;
    uint16_t   send_sdu_pos;

    // max PDU size
    uint16_t  remote_mps;

    // credits for outgoing traffic
    uint16_t credits_outgoing;
    
    // number of packets remote will be granted
    uint16_t new_credits_incoming;

    // credits for incoming traffic
    uint16_t credits_incoming;

    // automatic credits incoming
    uint16_t automatic_credits;

#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
    // l2cap channel mode: basic or enhanced retransmission mode
    l2cap_channel_mode_t mode;
    
    // local/remote config options
    uint16_t local_retransmission_timeout_ms;
    uint16_t local_monitor_timeout_ms;

    uint16_t remote_retransmission_timeout_ms;
    uint16_t remote_monitor_timeout_ms;

    uint8_t remote_tx_window_size;

    uint8_t local_max_transmit;
    uint8_t remote_max_transmit;
    
    // if ertm is not mandatory, allow fallback to L2CAP Basic Mode - flag
    uint8_t ertm_mandatory;

    // sender: buffer index of oldest packet
    uint8_t tx_read_index;

    // sender: buffer index to store next tx packet
    uint8_t tx_write_index;

    // sender: buffer index of packet to send next
    uint8_t tx_send_index;

    // sender: next seq nr used for sending
    uint8_t next_tx_seq;

    // sender: selective retransmission requested
    uint8_t srej_active;

    // receiver: send RR frame - flag
    uint8_t send_supervisor_frame_receiver_ready;

    // receiver: send RR frame with poll bit set
    uint8_t send_supervisor_frame_receiver_ready_poll;

    // receiver: send RR frame with final bit set
    uint8_t send_supervisor_frame_receiver_ready_final;

    // receiver: send RNR frame - flag
    uint8_t send_supervisor_frame_receiver_not_ready;

    // receiver: send REJ frame - flag
    uint8_t send_supervisor_frame_reject;

    // receiver: send SREJ frame - flag
    uint8_t send_supervisor_frame_selective_reject;

    // receiver: value of tx_seq in next expected i-frame
    uint8_t expected_tx_seq;

    // receiver: request transmiissoin with tx_seq = req_seq and ack up to and including req_seq
    uint8_t req_seq;

    // max um out-of-order packets // tx_window
    uint8_t num_rx_buffers;

    // max num of unacknowledged outgoing packets
    uint8_t num_tx_buffers;

    // re-assembly state
    l2cap_ertm_rx_packet_state_t * rx_packets_state;

    // retransmission state
    l2cap_ertm_tx_packet_state_t * tx_packets_state;

    // data, each of size local_mtu
    uint8_t * rx_packets_data;

    // data, each of size local_mtu
    uint8_t * tx_packets_data;
#endif    
} l2cap_channel_t;

// info regarding potential connections
typedef struct {
    // linked list - assert: first field
    btstack_linked_item_t    item;
    
    // service id
    uint16_t  psm;
    
    // incoming MTU
    uint16_t mtu;
    
    // internal connection
    btstack_packet_handler_t packet_handler;

    // required security level
    gap_security_level_t required_security_level;

} l2cap_service_t;


typedef struct l2cap_signaling_response {
    hci_con_handle_t handle;
    uint8_t  sig_id;
    uint8_t  code;
    uint16_t cid;  // source cid for CONNECTION REQUEST
    uint16_t data; // infoType for INFORMATION REQUEST, result for CONNECTION REQUEST and COMMAND UNKNOWN
} l2cap_signaling_response_t;


void l2cap_register_fixed_channel(btstack_packet_handler_t packet_handler, uint16_t channel_id);
int  l2cap_can_send_fixed_channel_packet_now(hci_con_handle_t con_handle, uint16_t channel_id);
void l2cap_request_can_send_fix_channel_now_event(hci_con_handle_t con_handle, uint16_t channel_id);
int  l2cap_send_connectionless(hci_con_handle_t con_handle, uint16_t cid, uint8_t *data, uint16_t len);
int  l2cap_send_prepared_connectionless(hci_con_handle_t con_handle, uint16_t cid, uint16_t len);

// PTS Testing
int l2cap_send_echo_request(hci_con_handle_t con_handle, uint8_t *data, uint16_t len);
void l2cap_require_security_level_2_for_outgoing_sdp(void);

// Used by RFCOMM - similar to l2cap_can-send_packet_now but does not check if outgoing buffer is reserved
int  l2cap_can_send_prepared_packet_now(uint16_t local_cid);

/* API_START */

/** 
 * @brief Set up L2CAP and register L2CAP with HCI layer.
 */
void l2cap_init(void);

/** 
 * @brief Registers packet handler for LE Connection Parameter Update events
 */
void l2cap_register_packet_handler(void (*handler)(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size));

/** 
 * @brief Get max MTU for Classic connections based on btstack configuration
 */
uint16_t l2cap_max_mtu(void);

/** 
 * @brief Get max MTU for LE connections based on btstack configuration
 */
uint16_t l2cap_max_le_mtu(void);

/** 
 * @brief Creates L2CAP channel to the PSM of a remote device with baseband address. A new baseband connection will be initiated if necessary.
 * @param packet_handler
 * @param address
 * @param psm
 * @param mtu
 * @param local_cid
 * @return status
 */
uint8_t l2cap_create_channel(btstack_packet_handler_t packet_handler, bd_addr_t address, uint16_t psm, uint16_t mtu, uint16_t * out_local_cid);

/** 
 * @brief Creates L2CAP channel to the PSM of a remote device with baseband address using Enhanced Retransmission Mode. 
 *        A new baseband connection will be initiated if necessary.
 * @param packet_handler
 * @param address
 * @param psm
 * @param ertm_mandatory If not mandatory, the use of ERTM can be decided by the remote 
 * @param max_transmit Number of retransmissions that L2CAP is allowed to try before accepting that a packet and the channel is lost.
 * @param retransmission_timeout_ms Recommended : 2000 ms (ACL Flush timeout not used)
 * @param monitor_timeout_ms Recommended: 12000 ms (ACL Flush timeout not used)
 * @param num_tx_buffers Number of unacknowledged packets stored in buffer
 * @param num_rx_buffers Number of packets that can be received out of order (-> our tx_window size)
 * @param buffer to store out-of-order packets and unacknowledged outgoing packets with their tretransmission timers
 * @param size of buffer
 * @param local_cid
 * @return status
 */
uint8_t l2cap_create_ertm_channel(btstack_packet_handler_t packet_handler, bd_addr_t address, uint16_t psm, 
    int ertm_mandatory, uint8_t max_transmit, uint16_t retransmission_timeout_ms, uint16_t monitor_timeout_ms,
    uint8_t num_tx_buffers, uint8_t num_rx_buffers, uint8_t * buffer, uint32_t size, uint16_t * out_local_cid);

/** 
 * @brief Disconnects L2CAP channel with given identifier. 
 */
void l2cap_disconnect(uint16_t local_cid, uint8_t reason);

/** 
 * @brief Queries the maximal transfer unit (MTU) for L2CAP channel with given identifier. 
 */
uint16_t l2cap_get_remote_mtu_for_local_cid(uint16_t local_cid);

/** 
 * @brief Sends L2CAP data packet to the channel with given identifier.
 */
int l2cap_send(uint16_t local_cid, uint8_t *data, uint16_t len);

/** 
 * @brief Registers L2CAP service with given PSM and MTU, and assigns a packet handler.
 */
uint8_t l2cap_register_service(btstack_packet_handler_t packet_handler, uint16_t psm, uint16_t mtu, gap_security_level_t security_level);

/** 
 * @brief Unregisters L2CAP service with given PSM.
 */
uint8_t l2cap_unregister_service(uint16_t psm);

/** 
 * @brief Accepts incoming L2CAP connection.
 */
void l2cap_accept_connection(uint16_t local_cid);

/** 
 * @brief Accepts incoming L2CAP connection for Enhanced Retransmission Mode
 * @param ertm_mandatory If not mandatory, the use of ERTM can be decided by the remote 
 * @param max_transmit Number of retransmissions that L2CAP is allowed to try before accepting that a packet and the channel is lost. Recommended: 1
 * @param retransmission_timeout_ms Recommended : 2000 ms (ACL Flush timeout not used)
 * @param monitor_timeout_ms Recommended: 12000 ms (ACL Flush timeout not used)
 * @param num_tx_buffers Number of unacknowledged packets stored in buffer
 * @param num_rx_buffers Number of packets that can be received out of order (-> our tx_window size)
 * @param buffer to store out-of-order packets and unacknowledged outgoing packets with their tretransmission timers
 * @param size of buffer
 * @return status
 */
uint8_t l2cap_accept_ertm_connection(uint16_t local_cid, int ertm_mandatory, uint8_t max_transmit,
    uint16_t retransmission_timeout_ms, uint16_t monitor_timeout_ms, uint8_t num_tx_buffers, uint8_t num_rx_buffers, uint8_t * buffer, uint32_t size);

/** 
 * @brief Deny incoming L2CAP connection.
 */
void l2cap_decline_connection(uint16_t local_cid);

/** 
 * @brief Check if outgoing buffer is available and that there's space on the Bluetooth module
 */
int  l2cap_can_send_packet_now(uint16_t local_cid);    

/** 
 * @brief Request emission of L2CAP_EVENT_CAN_SEND_NOW as soon as possible
 * @note L2CAP_EVENT_CAN_SEND_NOW might be emitted during call to this function
 *       so packet handler should be ready to handle it
 * @param local_cid
 */
void l2cap_request_can_send_now_event(uint16_t local_cid);

/** 
 * @brief Reserve outgoing buffer
 */
int  l2cap_reserve_packet_buffer(void);

/** 
 * @brief Get outgoing buffer and prepare data.
 */
uint8_t *l2cap_get_outgoing_buffer(void);

/** 
 * @brief Send L2CAP packet prepared in outgoing buffer to channel
 */
int l2cap_send_prepared(uint16_t local_cid, uint16_t len);

/** 
 * @brief Release outgoing buffer (only needed if l2cap_send_prepared is not called)
 */
void l2cap_release_packet_buffer(void);


//
// LE Connection Oriented Channels feature with the LE Credit Based Flow Control Mode == LE Data Channel
//


/**
 * @brief Register L2CAP LE Data Channel service
 * @note MTU and initial credits are specified in l2cap_le_accept_connection(..) call
 * @param packet_handler
 * @param psm
 * @param security_level
 */
uint8_t l2cap_le_register_service(btstack_packet_handler_t packet_handler, uint16_t psm, gap_security_level_t security_level);

/**
 * @brief Unregister L2CAP LE Data Channel service
 * @param psm
 */

uint8_t l2cap_le_unregister_service(uint16_t psm);

/*
 * @brief Accept incoming LE Data Channel connection
 * @param local_cid             L2CAP LE Data Channel Identifier
 * @param receive_buffer        buffer used for reassembly of L2CAP LE Information Frames into service data unit (SDU) with given MTU
 * @param receive_buffer_size   buffer size equals MTU
 * @param initial_credits       Number of initial credits provided to peer or L2CAP_LE_AUTOMATIC_CREDITS to enable automatic credits
 */

uint8_t l2cap_le_accept_connection(uint16_t local_cid, uint8_t * receive_sdu_buffer, uint16_t mtu, uint16_t initial_credits);

/** 
 * @brief Deny incoming LE Data Channel connection due to resource constraints
 * @param local_cid             L2CAP LE Data Channel Identifier
 */

uint8_t l2cap_le_decline_connection(uint16_t local_cid);

/**
 * @brief Create LE Data Channel
 * @param packet_handler        Packet handler for this connection
 * @param con_handle            ACL-LE HCI Connction Handle
 * @param psm                   Service PSM to connect to
 * @param receive_buffer        buffer used for reassembly of L2CAP LE Information Frames into service data unit (SDU) with given MTU
 * @param receive_buffer_size   buffer size equals MTU
 * @param initial_credits       Number of initial credits provided to peer or L2CAP_LE_AUTOMATIC_CREDITS to enable automatic credits
 * @param security_level        Minimum required security level
 * @param out_local_cid         L2CAP LE Channel Identifier is stored here
 */
uint8_t l2cap_le_create_channel(btstack_packet_handler_t packet_handler, hci_con_handle_t con_handle, 
    uint16_t psm, uint8_t * receive_sdu_buffer, uint16_t mtu, uint16_t initial_credits, gap_security_level_t security_level,
    uint16_t * out_local_cid);

/**
 * @brief Provide credtis for LE Data Channel
 * @param local_cid             L2CAP LE Data Channel Identifier
 * @param credits               Number additional credits for peer
 */
uint8_t l2cap_le_provide_credits(uint16_t cid, uint16_t credits);

/**
 * @brief Check if packet can be scheduled for transmission
 * @param local_cid             L2CAP LE Data Channel Identifier
 */
int l2cap_le_can_send_now(uint16_t cid);

/**
 * @brief Request emission of L2CAP_EVENT_LE_CAN_SEND_NOW as soon as possible
 * @note L2CAP_EVENT_CAN_SEND_NOW might be emitted during call to this function
 *       so packet handler should be ready to handle it
 * @param local_cid             L2CAP LE Data Channel Identifier
 */
uint8_t l2cap_le_request_can_send_now_event(uint16_t cid);

/**
 * @brief Send data via LE Data Channel
 * @note Since data larger then the maximum PDU needs to be segmented into multiple PDUs, data needs to stay valid until ... event
 * @param local_cid             L2CAP LE Data Channel Identifier
 * @param data                  data to send
 * @param size                  data size
 */
uint8_t l2cap_le_send_data(uint16_t cid, uint8_t * data, uint16_t size);

/**
 * @brief Disconnect from LE Data Channel
 * @param local_cid             L2CAP LE Data Channel Identifier
 */
uint8_t l2cap_le_disconnect(uint16_t cid);

/* API_END */

/**
 * @brief ERTM Set channel as busy.
 * @note Can be cleared by l2cap_ertm_set_ready
 * @param local_cid 
 */
uint8_t l2cap_ertm_set_busy(uint16_t local_cid);

/**
 * @brief ERTM Set channel as ready
 * @note Used after l2cap_ertm_set_busy
 * @param local_cid 
 */
uint8_t l2cap_ertm_set_ready(uint16_t local_cid);

#if defined __cplusplus
}
#endif

#endif // __L2CAP_H
