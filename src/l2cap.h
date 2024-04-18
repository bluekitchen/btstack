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
 * @title L2CAP
 *
 * Logical Link Control and Adaption Protocol 
 *
 */

#ifndef L2CAP_H
#define L2CAP_H

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
    L2CAP_STATE_EMIT_OPEN_FAILED_AND_DISCARD,
    L2CAP_STATE_WILL_SEND_ENHANCED_CONNECTION_REQUEST,
    L2CAP_STATE_WAIT_ENHANCED_CONNECTION_RESPONSE,
    L2CAP_STATE_WILL_SEND_ENHANCED_CONNECTION_RESPONSE,
    L2CAP_STATE_WILL_SEND_EHNANCED_RENEGOTIATION_REQUEST,
    L2CAP_STATE_WAIT_ENHANCED_RENEGOTIATION_RESPONSE,
    L2CAP_STATE_INVALID,
} L2CAP_STATE;

#define L2CAP_CHANNEL_STATE_VAR_NONE                   0
#define L2CAP_CHANNEL_STATE_VAR_RCVD_CONF_REQ          1 << 0
#define L2CAP_CHANNEL_STATE_VAR_RCVD_CONF_RSP          1 << 1
#define L2CAP_CHANNEL_STATE_VAR_SEND_CONF_REQ          1 << 2
#define L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP          1 << 3
#define L2CAP_CHANNEL_STATE_VAR_SENT_CONF_REQ          1 << 4
#define L2CAP_CHANNEL_STATE_VAR_SENT_CONF_RSP          1 << 5
#define L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP_MTU      1 << 6   // in CONF RSP, add MTU option
#define L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP_ERTM     1 << 7   // in CONF RSP, add Retransmission and Flowcontrol option
#define L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP_CONT     1 << 8   // in CONF RSP, set CONTINUE flag
#define L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP_INVALID  1 << 9   // in CONF RSP, send UNKNOWN OPTIONS
#define L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP_REJECTED 1 << 10  // in CONF RSP, send Unacceptable Parameters (ERTM)
#define L2CAP_CHANNEL_STATE_VAR_BASIC_FALLBACK_TRIED   1 << 11  // set when ERTM was requested but we want only Basic mode (ERM)
#define L2CAP_CHANNEL_STATE_VAR_SEND_CONN_RESP_PEND    1 << 12  // send Connection Respond with pending
#define L2CAP_CHANNEL_STATE_VAR_SENT_CONN_RESP_PEND    1 << 13  // send CMD_REJ with reason unknown
#define L2CAP_CHANNEL_STATE_VAR_INCOMING               1 << 15  // channel is incoming


typedef enum {
    L2CAP_CHANNEL_TYPE_CLASSIC,         // Classic Basic or ERTM
    L2CAP_CHANNEL_TYPE_CONNECTIONLESS,  // Classic Connectionless
    L2CAP_CHANNEL_TYPE_CHANNEL_CBM,     // LE
    L2CAP_CHANNEL_TYPE_FIXED_LE,        // LE ATT + SM
    L2CAP_CHANNEL_TYPE_FIXED_CLASSIC,   // Classic SM
    L2CAP_CHANNEL_TYPE_CHANNEL_ECBM     // Classic + LE
} l2cap_channel_type_t;


/*
 * @brief L2CAP Segmentation And Reassembly packet type in I-Frames
 */
typedef enum {
    L2CAP_SEGMENTATION_AND_REASSEMBLY_UNSEGMENTED_L2CAP_SDU = 0,
    L2CAP_SEGMENTATION_AND_REASSEMBLY_START_OF_L2CAP_SDU,
    L2CAP_SEGMENTATION_AND_REASSEMBLY_END_OF_L2CAP_SDU,
    L2CAP_SEGMENTATION_AND_REASSEMBLY_CONTINUATION_OF_L2CAP_SDU
} l2cap_segmentation_and_reassembly_t;

typedef struct {
    l2cap_segmentation_and_reassembly_t sar;
    uint16_t len;
    uint8_t  valid;
} l2cap_ertm_rx_packet_state_t;

typedef struct {
    l2cap_segmentation_and_reassembly_t sar;
    uint16_t len;
    uint8_t tx_seq;
    uint8_t retry_count;
    uint8_t retransmission_requested;
} l2cap_ertm_tx_packet_state_t;

typedef struct {
    // If not mandatory, the use of ERTM can be decided by the remote 
    uint8_t  ertm_mandatory; 

    // Number of retransmissions that L2CAP is allowed to try before accepting that a packet and the channel is lost.
    uint8_t  max_transmit;

    // time before retransmission of i-frame / Recommended : 2000 ms (ACL Flush timeout not used)
    uint16_t retransmission_timeout_ms;

    // time after withc s-frames are sent / Recommended: 12000 ms (ACL Flush timeout not used)
    uint16_t monitor_timeout_ms;

    // MTU for incoming SDUs
    uint16_t local_mtu;

    // Number of buffers for outgoing data
    uint8_t num_tx_buffers;

    // Number of packets that can be received out of order (-> our tx_window size)
    uint8_t num_rx_buffers;

    // Frame Check Sequence (FCS) Option
    uint8_t fcs_option;

} l2cap_ertm_config_t;

// info regarding an actual channel
// note: l2cap_fixed_channel and l2cap_channel_t share commmon fields

typedef struct l2cap_fixed_channel {
    // linked list - assert: first field
    btstack_linked_item_t    item;
    
    // channel type
    l2cap_channel_type_t channel_type;

    // local cid, primary key for channel lookup
    uint16_t  local_cid;

    // packet handler
    btstack_packet_handler_t packet_handler;

    // send request
    uint8_t waiting_for_can_send_now;

    // -- end of shared prefix

} l2cap_fixed_channel_t;

typedef struct {
    // linked list - assert: first field
    btstack_linked_item_t    item;
    
    // channel type
    l2cap_channel_type_t channel_type;

    // local cid, primary key for channel lookup
    uint16_t  local_cid;

    // packet handler
    btstack_packet_handler_t packet_handler;

    // send request
    uint8_t   waiting_for_can_send_now;

    // -- end of shared prefix

    // timer
    btstack_timer_source_t rtx; // also used for ertx

    L2CAP_STATE state;
    uint16_t    state_var;

    // info
    hci_con_handle_t con_handle;

    bd_addr_t address;
    bd_addr_type_t address_type;
    
    uint8_t   remote_sig_id;    // used by other side, needed for delayed response
    uint8_t   local_sig_id;     // own signaling identifier
    
    uint16_t  remote_cid;
    
    uint16_t  local_mtu;
    uint16_t  remote_mtu;

    uint16_t  flush_timeout;    // default 0xffff

    uint16_t  psm;
    
    gap_security_level_t required_security_level;

    uint16_t   reason; // used in decline internal

    uint8_t   unknown_option; // used for ConfigResponse

    // Credit-Based Flow-Control mode

    // incoming SDU
    uint8_t * receive_sdu_buffer;
    uint16_t  receive_sdu_len;
    uint16_t  receive_sdu_pos;

#ifdef ENABLE_L2CAP_ENHANCED_CREDIT_BASED_FLOW_CONTROL_MODE
    uint8_t * renegotiate_sdu_buffer;
    uint16_t  renegotiate_mtu;
#endif

    // outgoing SDU
    const uint8_t * send_sdu_buffer;
    uint16_t   send_sdu_len;
    uint16_t   send_sdu_pos;

    // max PDU size
    uint16_t  local_mps;
    uint16_t  remote_mps;

    // credits for outgoing traffic
    uint16_t credits_outgoing;
    
    // number of packets remote will be granted
    uint16_t new_credits_incoming;

    // credits for incoming traffic
    uint16_t credits_incoming;

    // automatic credits incoming
    bool automatic_credits;

#ifdef ENABLE_L2CAP_ENHANCED_CREDIT_BASED_FLOW_CONTROL_MODE
    uint8_t cid_index;
    uint8_t num_cids;
#endif

#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE

    // l2cap channel mode: basic or enhanced retransmission mode
    l2cap_channel_mode_t mode;

    // retransmission timer
    btstack_timer_source_t retransmission_timer;

    // monitor timer
    btstack_timer_source_t monitor_timer;

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

    // Frame Chech Sequence (crc16) is present in both directions
    uint8_t fcs_option;

    // sender: max num of stored outgoing frames
    uint8_t num_tx_buffers;

    // sender: num stored outgoing frames
    uint8_t num_stored_tx_frames;

    // sender: number of unacknowledeged I-Frames - frames have been sent, but not acknowledged yet
    uint8_t unacked_frames;

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


    // receiver: max num out-of-order packets // tx_window
    uint8_t num_rx_buffers;

    // receiver: buffer index of to store packet with delta = 1
    uint8_t rx_store_index;

    // receiver: value of tx_seq in next expected i-frame
    uint8_t expected_tx_seq;

    // receiver: request transmission with tx_seq = req_seq and ack up to and including req_seq
    uint8_t req_seq;

    // receiver: local busy condition
    uint8_t local_busy;

    // receiver: send RR frame with optional final flag set - flag
    uint8_t send_supervisor_frame_receiver_ready;

    // receiver: send RR frame with poll bit set
    uint8_t send_supervisor_frame_receiver_ready_poll;

    // receiver: send RNR frame - flag
    uint8_t send_supervisor_frame_receiver_not_ready;

    // receiver: send REJ frame - flag
    uint8_t send_supervisor_frame_reject;

    // receiver: send SREJ frame - flag
    uint8_t send_supervisor_frame_selective_reject;

    // set final bit after poll packet with poll bit was received
    uint8_t set_final_bit_after_packet_with_poll_bit_set;

    // receiver: meta data for out-of-order frames
    l2cap_ertm_rx_packet_state_t * rx_packets_state;

    // sender: retransmission state
    l2cap_ertm_tx_packet_state_t * tx_packets_state;

    // receiver: reassemly pos
    uint16_t reassembly_pos;

    // receiver: reassemly sdu length
    uint16_t reassembly_sdu_length;

    // receiver: eassembly buffer
    uint8_t * reassembly_buffer;

    // receiver: num_rx_buffers of size local_mps
    uint8_t * rx_packets_data;

    // sender: num_tx_buffers of size local_mps
    uint8_t * tx_packets_data;

#endif    
} l2cap_channel_t;

// info regarding potential connections
typedef struct {
    // linked list - assert: first field
    btstack_linked_item_t    item;
    
    // service id
    uint16_t  psm;
    
    // max local mtu for basic mode, min remote mtu for enhanced credit-based flow-control mode
    uint16_t mtu;
    
    // internal connection
    btstack_packet_handler_t packet_handler;

    // required security level
    gap_security_level_t required_security_level;

    // requires authorization
    bool requires_authorization;

} l2cap_service_t;


typedef struct l2cap_signaling_response {
    hci_con_handle_t handle;
    uint8_t  sig_id;
    uint8_t  code;
    uint16_t cid;  // source cid for CONNECTION REQUEST
    uint16_t data; // infoType for INFORMATION REQUEST, result for CONNECTION REQUEST and COMMAND UNKNOWN
} l2cap_signaling_response_t;


void l2cap_register_fixed_channel(btstack_packet_handler_t packet_handler, uint16_t channel_id);
bool l2cap_can_send_fixed_channel_packet_now(hci_con_handle_t con_handle, uint16_t channel_id);
void l2cap_request_can_send_fix_channel_now_event(hci_con_handle_t con_handle, uint16_t channel_id);
uint8_t l2cap_send_connectionless(hci_con_handle_t con_handle, uint16_t cid, uint8_t *data, uint16_t len);
uint8_t l2cap_send_prepared_connectionless(hci_con_handle_t con_handle, uint16_t cid, uint16_t len);

// PTS Testing
int l2cap_send_echo_request(hci_con_handle_t con_handle, uint8_t *data, uint16_t len);
void l2cap_require_security_level_2_for_outgoing_sdp(void);

// Used by RFCOMM - similar to l2cap_can-send_packet_now but does not check if outgoing buffer is reserved
bool l2cap_can_send_prepared_packet_now(uint16_t local_cid);

/* API_START */

//
// PSM numbers from https://www.bluetooth.com/specifications/assigned-numbers/logical-link-control 
//
#define PSM_SDP           BLUETOOTH_PROTOCOL_SDP
#define PSM_RFCOMM        BLUETOOTH_PROTOCOL_RFCOMM
#define PSM_BNEP          BLUETOOTH_PROTOCOL_BNEP
// @TODO: scrape PSMs Bluetooth SIG site and put in bluetooth_psm.h or bluetooth_l2cap.h
#define PSM_HID_CONTROL   0x11
#define PSM_HID_INTERRUPT 0x13
#define PSM_ATT           0x1f
#define PSM_IPSP          0x23

/** 
 * @brief Set up L2CAP and register L2CAP with HCI layer.
 */
void l2cap_init(void);

/**
 * @brief Add event packet handler for LE Connection Parameter Update events
 */
void l2cap_add_event_handler(btstack_packet_callback_registration_t * callback_handler);

/**
 * @brief Remove event packet handler.
 */
void l2cap_remove_event_handler(btstack_packet_callback_registration_t * callback_handler);

/** 
 * @brief Get max MTU for Classic connections based on btstack configuration
 */
uint16_t l2cap_max_mtu(void);

/** 
 * @brief Get max MTU for LE connections based on btstack configuration
 */
uint16_t l2cap_max_le_mtu(void);

/**
* @brief Set the max MTU for LE connections, if not set l2cap_max_mtu() will be used.
*/
void l2cap_set_max_le_mtu(uint16_t max_mtu);

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
 * @brief Disconnects L2CAP channel with given identifier. 
 * @param local_cid
 * @return status ERROR_CODE_SUCCESS if successful or L2CAP_LOCAL_CID_DOES_NOT_EXIST
 */
uint8_t l2cap_disconnect(uint16_t local_cid);

/** 
 * @brief Queries the maximal transfer unit (MTU) for L2CAP channel with given identifier. 
 */
uint16_t l2cap_get_remote_mtu_for_local_cid(uint16_t local_cid);

/** 
 * @brief Sends L2CAP data packet to the channel with given identifier.
 * @note For channel in credit-based flow control mode, data needs to stay valid until .. event
 * @param local_cid
 * @param data to send
 * @param len of data
 * @return status
 */
uint8_t l2cap_send(uint16_t local_cid, const uint8_t *data, uint16_t len);

/** 
 * @brief Registers L2CAP service with given PSM and MTU, and assigns a packet handler. 
 * @param packet_handler
 * @param psm
 * @param mtu
 * @param security_level
 * @return status ERROR_CODE_SUCCESS if successful, otherwise L2CAP_SERVICE_ALREADY_REGISTERED or BTSTACK_MEMORY_ALLOC_FAILED
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
 * @brief Deny incoming L2CAP connection.
 */
void l2cap_decline_connection(uint16_t local_cid);

/** 
 * @brief Check if outgoing buffer is available and that there's space on the Bluetooth module
 * @return true if packet can be sent
 */
bool l2cap_can_send_packet_now(uint16_t local_cid);

/** 
 * @brief Request emission of L2CAP_EVENT_CAN_SEND_NOW as soon as possible
 * @note L2CAP_EVENT_CAN_SEND_NOW might be emitted during call to this function
 *       so packet handler should be ready to handle it
 * @param local_cid
 * @return status
 */
uint8_t l2cap_request_can_send_now_event(uint16_t local_cid);

/** 
 * @brief Reserve outgoing buffer
 * @note Only for L2CAP Basic Mode Channels
 * @note Must only be called after a 'can send now' check or event
 * @note Asserts if packet buffer is already reserved
 */
void l2cap_reserve_packet_buffer(void);

/** 
 * @brief Get outgoing buffer and prepare data.
 * @note Only for L2CAP Basic Mode Channels
 */
uint8_t *l2cap_get_outgoing_buffer(void);

/** 
 * @brief Send L2CAP packet prepared in outgoing buffer to channel
 * @note Only for L2CAP Basic Mode Channels
 */
uint8_t l2cap_send_prepared(uint16_t local_cid, uint16_t len);

/** 
 * @brief Release outgoing buffer (only needed if l2cap_send_prepared is not called)
 * @note Only for L2CAP Basic Mode Channels
 */
void l2cap_release_packet_buffer(void);

//
// Connection-Oriented Channels in Enhanced Retransmission Mode - ERTM
//

/**
 * @brief Creates L2CAP channel to the PSM of a remote device with baseband address using Enhanced Retransmission Mode.
 *        A new baseband connection will be initiated if necessary.
 * @param packet_handler
 * @param address
 * @param psm
 * @param ertm_config
 * @param buffer to store reassembled rx packet, out-of-order packets and unacknowledged outgoing packets with their tretransmission timers
 * @param size of buffer
 * @param local_cid
 * @return status
 */
uint8_t l2cap_ertm_create_channel(btstack_packet_handler_t packet_handler, bd_addr_t address, uint16_t psm,
                                  l2cap_ertm_config_t * ertm_contig, uint8_t * buffer, uint32_t size, uint16_t * out_local_cid);

/**
 * @brief Accepts incoming L2CAP connection for Enhanced Retransmission Mode
 * @param local_cid
 * @param ertm_config
 * @param buffer to store reassembled rx packet, out-of-order packets and unacknowledged outgoing packets with their tretransmission timers
 * @param size of buffer
 * @return status
 */
uint8_t l2cap_ertm_accept_connection(uint16_t local_cid, l2cap_ertm_config_t * ertm_contig, uint8_t * buffer, uint32_t size);

/**
 * @brief Deny incoming incoming L2CAP connection for Enhanced Retransmission Mode
 * @param local_cid
 * @return status
 */
uint8_t l2cap_ertm_decline_connection(uint16_t local_cid);

/**
 * @brief ERTM Set channel as busy.
 * @note Can be cleared by l2cap_ertm_set_ready
 * @param local_cid
 * @return status
 */
uint8_t l2cap_ertm_set_busy(uint16_t local_cid);

/**
 * @brief ERTM Set channel as ready
 * @note Used after l2cap_ertm_set_busy
 * @param local_cid
 * @return status
 */
uint8_t l2cap_ertm_set_ready(uint16_t local_cid);


//
// L2CAP Connection-Oriented Channels in LE Credit-Based Flow-Control Mode - CBM
//

/**
 * @brief Register L2CAP service in LE Credit-Based Flow-Control Mode
 * @note MTU and initial credits are specified in l2cap_cbm_accept_connection(..) call
 * @param packet_handler
 * @param psm
 * @param security_level
 */
uint8_t l2cap_cbm_register_service(btstack_packet_handler_t packet_handler, uint16_t psm, gap_security_level_t security_level);

/**
 * @brief Unregister L2CAP service in LE Credit-Based Flow-Control Mode
 * @param psm
 */

uint8_t l2cap_cbm_unregister_service(uint16_t psm);

/*
 * @brief Accept incoming connection LE Credit-Based Flow-Control Mode
 * @param local_cid             L2CAP Channel Identifier
 * @param receive_buffer        buffer used for reassembly of L2CAP LE Information Frames into service data unit (SDU) with given MTU
 * @param receive_buffer_size   buffer size equals MTU
 * @param initial_credits       Number of initial credits provided to peer or L2CAP_LE_AUTOMATIC_CREDITS to enable automatic credits
 */

uint8_t l2cap_cbm_accept_connection(uint16_t local_cid, uint8_t * receive_sdu_buffer, uint16_t mtu, uint16_t initial_credits);

/** 
 * @brief Deecline connection in LE Credit-Based Flow-Control Mode
 * @param local_cid             L2CAP Channel Identifier
 * @param result                result, see L2CAP_CBM_CONNECTION_RESULT_SUCCESS in bluetooth.h
 */

uint8_t l2cap_cbm_decline_connection(uint16_t local_cid, uint16_t result);

/**
 * @brief Create outgoing channel in LE Credit-Based Flow-Control Mode
 * @param packet_handler        Packet handler for this connection
 * @param con_handle            HCI Connection Handle, LE transport
 * @param psm                   Service PSM to connect to
 * @param receive_buffer        buffer used for reassembly of L2CAP LE Information Frames into service data unit (SDU) with given MTU
 * @param receive_buffer_size   buffer size equals MTU
 * @param initial_credits       Number of initial credits provided to peer or L2CAP_LE_AUTOMATIC_CREDITS to enable automatic credits
 * @param security_level        Minimum required security level
 * @param out_local_cid         L2CAP LE Channel Identifier is stored here
 */
uint8_t l2cap_cbm_create_channel(btstack_packet_handler_t packet_handler, hci_con_handle_t con_handle,
    uint16_t psm, uint8_t * receive_sdu_buffer, uint16_t mtu, uint16_t initial_credits, gap_security_level_t security_level,
    uint16_t * out_local_cid);

/**
 * @brief Provide credits for channel in LE Credit-Based Flow-Control Mode
 * @param local_cid             L2CAP Channel Identifier
 * @param credits               Number additional credits for peer
 */
uint8_t l2cap_cbm_provide_credits(uint16_t local_cid, uint16_t credits);

//
// L2CAP Connection-Oriented Channels in Enhanced Credit-Based Flow-Control Mode - ECBM
//

/**
 * @brief Register L2CAP service in Enhanced Credit-Based Flow-Control Mode
 * @note MTU and initial credits are specified in l2cap_enhanced_accept_connection(..) call
 * @param packet_handler
 * @param psm
 * @param min_remote_mtu
 * @param security_level
 * @oaram authorization_required
 * @return status
 */
uint8_t l2cap_ecbm_register_service(btstack_packet_handler_t packet_handler, uint16_t psm, uint16_t min_remote_mtu,
                                    gap_security_level_t security_level, bool authorization_required);

/**
 * @brief Unregister L2CAP service in Enhanced Credit-Based Flow-Control Mode
 * @param psm
 * @return status
 */

uint8_t l2cap_ecbm_unregister_service(uint16_t psm);

/**
 * @brief Set Minimal MPS for channel in Enhanced Credit-Based Flow-Control Mode
 * @param mps_min
 */
void l2cap_ecbm_mps_set_min(uint16_t mps_min);

/**
 * @brief Set Minimal MPS for channel in Enhanced Credit-Based Flow-Control Mode
 * @param mps_max
 */
void l2cap_ecbm_mps_set_max(uint16_t mps_max);

/**
 * @brief Create outgoing channel in Enhanced Credit-Based Flow-Control Mode
 * @note receive_buffer points to an array of receive buffers with num_channels elements
 * @note out_local_cid points to an array where CID is stored with num_channel elements
 * @param packet_handler        Packet handler for this connection
 * @param con_handle            HCI Connection Handle
 * @param security_level        Minimum required security level
 * @param psm                   Service PSM to connect to
 * @param num_channels          number of channels to create
 * @param initial_credits       Number of initial credits provided to peer per channel or L2CAP_LE_AUTOMATIC_CREDITS to enable automatic credits
 * @param receive_buffer_size   buffer size equals MTU
 * @param receive_buffers       Array of buffers used for reassembly of L2CAP Information Frames into service data unit (SDU) with given MTU
 * @param out_local_cids        Array of L2CAP Channel Identifiers is stored here on success
 * @return status
 */
uint8_t l2cap_ecbm_create_channels(btstack_packet_handler_t packet_handler, hci_con_handle_t con_handle,
                                       gap_security_level_t security_level,
                                       uint16_t psm, uint8_t num_channels, uint16_t initial_credits, uint16_t receive_buffer_size,
                                       uint8_t ** receive_buffers, uint16_t * out_local_cids);

/**
 * @brief  Accept incoming connection Enhanced Credit-Based Flow-Control Mode
 * @param local_cid            from L2CAP_EVENT_INCOMING_DATA_CONNECTION
 * @param num_channels
 * @param initial_credits      Number of initial credits provided to peer per channel or L2CAP_LE_AUTOMATIC_CREDITS to enable automatic credits
 * @param receive_buffer_size
 * @param receive_buffers      Array of buffers used for reassembly of L2CAP Information Frames into service data unit (SDU) with given MTU
 * @param out_local_cids       Array of L2CAP Channel Identifiers is stored here on success
 * @return status
 */
uint8_t l2cap_ecbm_accept_channels(uint16_t local_cid, uint8_t num_channels, uint16_t initial_credits,
                                            uint16_t receive_buffer_size, uint8_t ** receive_buffers, uint16_t * out_local_cids);
/**
 * @brief Decline connection in Enhanced Credit-Based Flow-Control Mode
 * @param local_cid           from L2CAP_EVENT_INCOMING_DATA_CONNECTION
 * @param result              See L2CAP_ECBM_CONNECTION_RESULT_ALL_SUCCESS in bluetooth.h
 * @return status
 */
uint8_t l2cap_ecbm_decline_channels(uint16_t local_cid, uint16_t result);

/**
 * @brief Provide credits for channel in Enhanced Credit-Based Flow-Control Mode
 * @param local_cid             L2CAP Channel Identifier
 * @param credits               Number additional credits for peer
 * @return status
 */
uint8_t l2cap_ecbm_provide_credits(uint16_t local_cid, uint16_t credits);

/**
 * @brief Request emission of L2CAP_EVENT_ECBM_CAN_SEND_NOW as soon as possible
 * @note L2CAP_EVENT_ECBM_CAN_SEND_NOW might be emitted during call to this function
 *       so packet handler should be ready to handle it
 * @param local_cid             L2CAP Channel Identifier
 * @return status
 */
uint8_t l2cap_ecbm_request_can_send_now_event(uint16_t local_cid);

/**
 * @brief Reconfigure MPS/MTU of local channels
 * @param num_cids
 * @param local_cids            array of local_cids to reconfigure
 * @param receive_buffer_size   buffer size equals MTU
 * @param receive_buffers       Array of buffers used for reassembly of L2CAP Information Frames into service data unit (SDU) with given MTU
 * @return status
 */
uint8_t l2cap_ecbm_reconfigure_channels(uint8_t num_cids, uint16_t * local_cids, int16_t receive_buffer_size, uint8_t ** receive_buffers);

/**
 * @brief Trigger pending connection responses after pairing completed
 * @note Must be called after receiving an SM_PAIRING_COMPLETE event, will be removed eventually
 * @param con_handle
 */
void l2cap_ecbm_trigger_pending_connection_responses(hci_con_handle_t con_handle);

/**
 * @brief De-Init L2CAP
 */
void l2cap_deinit(void);

/* API_END */


// @deprecated - please use l2cap_ertm_create_channel
uint8_t l2cap_create_ertm_channel(btstack_packet_handler_t packet_handler, bd_addr_t address, uint16_t psm,  l2cap_ertm_config_t * ertm_contig, uint8_t * buffer, uint32_t size, uint16_t * out_local_cid);

// @deprecated - please use l2cap_ertm_accept_connection
uint8_t l2cap_accept_ertm_connection(uint16_t local_cid, l2cap_ertm_config_t * ertm_contig, uint8_t * buffer, uint32_t size);

// @deprecated - please use l2cap_cbm_register_service
uint8_t l2cap_le_register_service(btstack_packet_handler_t packet_handler, uint16_t psm, gap_security_level_t security_level);

// @deprecated - please use l2cap_cbm_unregister_service
uint8_t l2cap_le_unregister_service(uint16_t psm);

// @deprecated - please use l2cap_cbm_accept_connection
uint8_t l2cap_le_accept_connection(uint16_t local_cid, uint8_t * receive_sdu_buffer, uint16_t mtu, uint16_t initial_credits);

// @deprecated - please use l2cap_cbm_decline_connection
uint8_t l2cap_le_decline_connection(uint16_t local_cid);

// @deprecated - please use l2cap_cbm_create_channel
uint8_t l2cap_le_create_channel(btstack_packet_handler_t packet_handler, hci_con_handle_t con_handle,
                                uint16_t psm, uint8_t * receive_sdu_buffer, uint16_t mtu, uint16_t initial_credits, gap_security_level_t security_level,
                                uint16_t * out_local_cid);

// @deprecated - please use l2cap_cbm_provide_credits
uint8_t l2cap_le_provide_credits(uint16_t local_cid, uint16_t credits);

// @deprecated - please use l2cap_can_send_now
bool l2cap_le_can_send_now(uint16_t local_cid);

// @deprecated - please use l2cap_request_can_send_now_event
uint8_t l2cap_le_request_can_send_now_event(uint16_t local_cid);

// @deprecated - please use l2cap_send
uint8_t l2cap_le_send_data(uint16_t local_cid, const uint8_t * data, uint16_t size);

// @deprecated - please use l2cap_disconnect
uint8_t l2cap_le_disconnect(uint16_t local_cid);


#if defined __cplusplus
}
#endif

#endif // L2CAP_H
