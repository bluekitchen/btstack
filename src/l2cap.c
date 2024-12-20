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

#define BTSTACK_FILE__ "l2cap.c"

/*
 *  l2cap.c
 *  Logical Link Control and Adaption Protocol (L2CAP)
 */

#include "l2cap.h"
#include "hci.h"
#include "hci_dump.h"
#include "bluetooth_sdp.h"
#include "bluetooth_psm.h"
#include "btstack_bool.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_memory.h"

#ifdef ENABLE_L2CAP_LE_CREDIT_BASED_FLOW_CONTROL_MODE
// TODO avoid dependency on higher layer: used to trigger pairing for outgoing connections
#include "ble/sm.h"
#endif

#include <stdarg.h>
#include <string.h>

/*
 * @brief L2CAP Supervisory function in S-Frames
 */
typedef enum {
    L2CAP_SUPERVISORY_FUNCTION_RR_RECEIVER_READY = 0,
    L2CAP_SUPERVISORY_FUNCTION_REJ_REJECT,
    L2CAP_SUPERVISORY_FUNCTION_RNR_RECEIVER_NOT_READY,
    L2CAP_SUPERVISORY_FUNCTION_SREJ_SELECTIVE_REJECT
} l2cap_supervisory_function_t;

/**
 * @brief L2CAP Information Types used in Information Request & Response
 */
typedef enum {
  L2CAP_INFO_TYPE_CONNECTIONLESS_MTU = 1,
  L2CAP_INFO_TYPE_EXTENDED_FEATURES_SUPPORTED,
  L2CAP_INFO_TYPE_FIXED_CHANNELS_SUPPORTED,
} l2cap_info_type_t;

/**
 * @brief L2CAP Configuration Option Types used in Configurateion Request & Response
 */
typedef enum {
  L2CAP_CONFIG_OPTION_TYPE_MAX_TRANSMISSION_UNIT = 1,
  L2CAP_CONFIG_OPTION_TYPE_FLUSH_TIMEOUT,
  L2CAP_CONFIG_OPTION_TYPE_QUALITY_OF_SERVICE,
  L2CAP_CONFIG_OPTION_TYPE_RETRANSMISSION_AND_FLOW_CONTROL, 
  L2CAP_CONFIG_OPTION_TYPE_FRAME_CHECK_SEQUENCE,
  L2CAP_CONFIG_OPTION_TYPE_EXTENDED_FLOW_SPECIFICATION,
  L2CAP_CONFIG_OPTION_TYPE_EXTENDED_WINDOW_SIZE,
} l2cap_config_option_type_t;


#define L2CAP_SIG_ID_INVALID 0

// size of HCI ACL + L2CAP Header for regular data packets (8)
#define COMPLETE_L2CAP_HEADER (HCI_ACL_HEADER_SIZE + L2CAP_HEADER_SIZE)

// L2CAP Configuration Result Codes
#define L2CAP_CONF_RESULT_SUCCESS                  0x0000
#define L2CAP_CONF_RESULT_UNACCEPTABLE_PARAMETERS  0x0001
#define L2CAP_CONF_RESULT_REJECT                   0x0002
#define L2CAP_CONF_RESULT_UNKNOWN_OPTIONS          0x0003
#define L2CAP_CONF_RESULT_PENDING                  0x0004
#define L2CAP_CONF_RESULT_FLOW_SPEC_REJECTED       0x0005

// L2CAP Reject Result Codes
#define L2CAP_REJ_CMD_UNKNOWN                      0x0000
#define L2CAP_REJ_MTU_EXCEEDED                     0x0001
#define L2CAP_REJ_INVALID_CID                      0x0002

// Response Timeout eXpired
#define L2CAP_RTX_TIMEOUT_MS   10000

// Extended Response Timeout eXpired
#define L2CAP_ERTX_TIMEOUT_MS 120000

// nr of buffered acl packets in outgoing queue to get max performance 
#define NR_BUFFERED_ACL_PACKETS 3

// used to cache l2cap rejects, echo, and informational requests
#define NR_PENDING_SIGNALING_RESPONSES 3

// nr of credits provided to remote if credits fall below watermark
#define L2CAP_CREDIT_BASED_FLOW_CONTROL_MODE_AUTOMATIC_CREDITS_WATERMARK 5
#define L2CAP_CREDIT_BASED_FLOW_CONTROL_MODE_AUTOMATIC_CREDITS_INCREMENT 5

// offsets for L2CAP SIGNALING COMMANDS
#define L2CAP_SIGNALING_COMMAND_CODE_OFFSET   0
#define L2CAP_SIGNALING_COMMAND_SIGID_OFFSET  1
#define L2CAP_SIGNALING_COMMAND_LENGTH_OFFSET 2
#define L2CAP_SIGNALING_COMMAND_DATA_OFFSET   4

#if defined(ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE) && !defined(ENABLE_CLASSIC)
#error "ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE depends on ENABLE_CLASSIC."
#error "Please remove ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE or add ENABLE_CLASSIC in btstack_config.h"
#endif


#ifdef ENABLE_LE_DATA_CHANNELS
#warning "ENABLE_LE_DATA_CHANNELS has been deprecated."
#warning "Please use ENABLE_L2CAP_LE_CREDIT_BASED_FLOW_CONTROL_MODE instead in btstack_config.h"
#define ENABLE_L2CAP_LE_CREDIT_BASED_FLOW_CONTROL_MODE
#endif

#if defined(ENABLE_L2CAP_LE_CREDIT_BASED_FLOW_CONTROL_MODE) || defined(ENABLE_L2CAP_ENHANCED_CREDIT_BASED_FLOW_CONTROL_MODE)
#define L2CAP_USES_CREDIT_BASED_CHANNELS
#endif

#if defined(L2CAP_USES_CREDIT_BASED_CHANNELS) || defined(ENABLE_CLASSIC)
#define L2CAP_USES_CHANNELS
#endif

// prototypes
static void l2cap_run(void);
static void l2cap_hci_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void l2cap_acl_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size );
static void l2cap_notify_channel_can_send(void);
static void l2cap_emit_can_send_now(btstack_packet_handler_t packet_handler, uint16_t channel);
static uint8_t  l2cap_next_sig_id(void);
static l2cap_fixed_channel_t * l2cap_fixed_channel_for_channel_id(uint16_t local_cid);
#ifdef ENABLE_CLASSIC
static void l2cap_handle_security_level_incoming_sufficient(l2cap_channel_t * channel);
static int l2cap_security_level_0_allowed_for_PSM(uint16_t psm);
static void l2cap_handle_remote_supported_features_received(l2cap_channel_t * channel);
static void l2cap_handle_connection_complete(hci_con_handle_t con_handle, l2cap_channel_t * channel);
static void l2cap_handle_information_request_complete(hci_connection_t * connection);
static inline l2cap_service_t * l2cap_get_service(uint16_t psm);
static void l2cap_emit_channel_opened(l2cap_channel_t *channel, uint8_t status);
static void l2cap_emit_channel_closed(l2cap_channel_t *channel);
static void l2cap_emit_incoming_connection(l2cap_channel_t *channel);
static int  l2cap_channel_ready_for_open(l2cap_channel_t *channel);
static uint8_t l2cap_classic_send(l2cap_channel_t * channel, const uint8_t *data, uint16_t len);
#endif
#ifdef ENABLE_L2CAP_LE_CREDIT_BASED_FLOW_CONTROL_MODE
static void l2cap_cbm_emit_channel_opened(l2cap_channel_t *channel, uint8_t status);
static void l2cap_cbm_emit_incoming_connection(l2cap_channel_t *channel);
static void l2cap_credit_based_notify_channel_can_send(l2cap_channel_t *channel);
static void l2cap_cbm_finalize_channel_close(l2cap_channel_t *channel);
static inline l2cap_service_t * l2cap_cbm_get_service(uint16_t le_psm);
#endif
#ifdef L2CAP_USES_CREDIT_BASED_CHANNELS
static uint8_t l2cap_credit_based_send_data(l2cap_channel_t * channel, const uint8_t * data, uint16_t size);
static void l2cap_credit_based_send_pdu(l2cap_channel_t *channel);
static void l2cap_credit_based_send_credits(l2cap_channel_t *channel);
static bool l2cap_credit_based_handle_credit_indication(hci_con_handle_t handle, const uint8_t * command, uint16_t len);
static void l2cap_credit_based_handle_pdu(l2cap_channel_t * l2cap_channel, const uint8_t * packet, uint16_t size);
#endif
#ifdef L2CAP_USES_CHANNELS
static uint16_t l2cap_next_local_cid(void);
static l2cap_channel_t * l2cap_get_channel_for_local_cid(uint16_t local_cid);
static void l2cap_emit_simple_event_with_cid(l2cap_channel_t * channel, uint8_t event_code);
static void l2cap_dispatch_to_channel(l2cap_channel_t *channel, uint8_t type, uint8_t * data, uint16_t size);
static l2cap_channel_t * l2cap_create_channel_entry(btstack_packet_handler_t packet_handler, l2cap_channel_type_t channel_type, bd_addr_t address, bd_addr_type_t address_type,
        uint16_t psm, uint16_t local_mtu, gap_security_level_t security_level);
static void l2cap_finalize_channel_close(l2cap_channel_t *channel);
static void l2cap_free_channel_entry(l2cap_channel_t * channel);
#endif
#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
static void l2cap_ertm_notify_channel_can_send(l2cap_channel_t * channel);
static void l2cap_ertm_monitor_timeout_callback(btstack_timer_source_t * ts);
static void l2cap_ertm_retransmission_timeout_callback(btstack_timer_source_t * ts);
#endif
#ifdef ENABLE_L2CAP_ENHANCED_CREDIT_BASED_FLOW_CONTROL_MODE
static void l2cap_ecbm_handle_security_level_incoming(l2cap_channel_t *channel);
static int l2cap_ecbm_signaling_handler_dispatch(hci_con_handle_t handle, uint16_t signaling_cid, uint8_t * command, uint8_t sig_id);
static void l2cap_run_trigger_callback(void * context);
#endif

// l2cap_fixed_channel_t entries
#ifdef ENABLE_BLE
static l2cap_fixed_channel_t l2cap_fixed_channel_le_att;
static l2cap_fixed_channel_t l2cap_fixed_channel_le_sm;
#endif
#ifdef ENABLE_CLASSIC
static l2cap_fixed_channel_t l2cap_fixed_channel_classic_connectionless;
static l2cap_fixed_channel_t l2cap_fixed_channel_classic_sm;
#endif

#ifdef ENABLE_CLASSIC
static btstack_linked_list_t l2cap_services;
static uint8_t l2cap_require_security_level2_for_outgoing_sdp;
static bd_addr_t l2cap_outgoing_classic_addr;
#endif

#ifdef ENABLE_L2CAP_LE_CREDIT_BASED_FLOW_CONTROL_MODE
static btstack_linked_list_t l2cap_le_services;
#endif

#ifdef ENABLE_L2CAP_ENHANCED_CREDIT_BASED_FLOW_CONTROL_MODE
static btstack_linked_list_t l2cap_enhanced_services;
static uint16_t l2cap_enhanced_mps_min;
static uint16_t l2cap_enhanced_mps_max;
static btstack_context_callback_registration_t l2cap_trigger_run_registration;
#endif

// single list of channels for connection-oriented channels (basic, ertm, cbm, ecbf) Classic Connectionless, ATT, and SM
static btstack_linked_list_t l2cap_channels;
#ifdef L2CAP_USES_CHANNELS
// next channel id for new connections
static uint16_t  l2cap_local_source_cid;
#endif
// next signaling sequence number
static uint8_t   l2cap_sig_seq_nr;

// used to cache l2cap rejects, echo, and informational requests
static l2cap_signaling_response_t l2cap_signaling_responses[NR_PENDING_SIGNALING_RESPONSES];
static int l2cap_signaling_responses_pending;
static btstack_packet_callback_registration_t l2cap_hci_event_callback_registration;

static bool l2cap_call_notify_channel_in_run;

#ifdef ENABLE_BLE
// only used for connection parameter update events
static uint16_t l2cap_le_custom_max_mtu;
#endif

/* callbacks for events */
static btstack_linked_list_t l2cap_event_handlers;

#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE

// enable for testing
// #define L2CAP_ERTM_SIMULATE_FCS_ERROR_INTERVAL 16

/*
 * CRC lookup table for generator polynom D^16 + D^15 + D^2 + 1
 */
static const uint16_t crc16_table[256] = {
    0x0000, 0xc0c1, 0xc181, 0x0140, 0xc301, 0x03c0, 0x0280, 0xc241, 0xc601, 0x06c0, 0x0780, 0xc741, 0x0500, 0xc5c1, 0xc481, 0x0440,
    0xcc01, 0x0cc0, 0x0d80, 0xcd41, 0x0f00, 0xcfc1, 0xce81, 0x0e40, 0x0a00, 0xcac1, 0xcb81, 0x0b40, 0xc901, 0x09c0, 0x0880, 0xc841,
    0xd801, 0x18c0, 0x1980, 0xd941, 0x1b00, 0xdbc1, 0xda81, 0x1a40, 0x1e00, 0xdec1, 0xdf81, 0x1f40, 0xdd01, 0x1dc0, 0x1c80, 0xdc41,
    0x1400, 0xd4c1, 0xd581, 0x1540, 0xd701, 0x17c0, 0x1680, 0xd641, 0xd201, 0x12c0, 0x1380, 0xd341, 0x1100, 0xd1c1, 0xd081, 0x1040,
    0xf001, 0x30c0, 0x3180, 0xf141, 0x3300, 0xf3c1, 0xf281, 0x3240, 0x3600, 0xf6c1, 0xf781, 0x3740, 0xf501, 0x35c0, 0x3480, 0xf441,
    0x3c00, 0xfcc1, 0xfd81, 0x3d40, 0xff01, 0x3fc0, 0x3e80, 0xfe41, 0xfa01, 0x3ac0, 0x3b80, 0xfb41, 0x3900, 0xf9c1, 0xf881, 0x3840,
    0x2800, 0xe8c1, 0xe981, 0x2940, 0xeb01, 0x2bc0, 0x2a80, 0xea41, 0xee01, 0x2ec0, 0x2f80, 0xef41, 0x2d00, 0xedc1, 0xec81, 0x2c40,
    0xe401, 0x24c0, 0x2580, 0xe541, 0x2700, 0xe7c1, 0xe681, 0x2640, 0x2200, 0xe2c1, 0xe381, 0x2340, 0xe101, 0x21c0, 0x2080, 0xe041,
    0xa001, 0x60c0, 0x6180, 0xa141, 0x6300, 0xa3c1, 0xa281, 0x6240, 0x6600, 0xa6c1, 0xa781, 0x6740, 0xa501, 0x65c0, 0x6480, 0xa441,
    0x6c00, 0xacc1, 0xad81, 0x6d40, 0xaf01, 0x6fc0, 0x6e80, 0xae41, 0xaa01, 0x6ac0, 0x6b80, 0xab41, 0x6900, 0xa9c1, 0xa881, 0x6840,
    0x7800, 0xb8c1, 0xb981, 0x7940, 0xbb01, 0x7bc0, 0x7a80, 0xba41, 0xbe01, 0x7ec0, 0x7f80, 0xbf41, 0x7d00, 0xbdc1, 0xbc81, 0x7c40,
    0xb401, 0x74c0, 0x7580, 0xb541, 0x7700, 0xb7c1, 0xb681, 0x7640, 0x7200, 0xb2c1, 0xb381, 0x7340, 0xb101, 0x71c0, 0x7080, 0xb041,
    0x5000, 0x90c1, 0x9181, 0x5140, 0x9301, 0x53c0, 0x5280, 0x9241, 0x9601, 0x56c0, 0x5780, 0x9741, 0x5500, 0x95c1, 0x9481, 0x5440,
    0x9c01, 0x5cc0, 0x5d80, 0x9d41, 0x5f00, 0x9fc1, 0x9e81, 0x5e40, 0x5a00, 0x9ac1, 0x9b81, 0x5b40, 0x9901, 0x59c0, 0x5880, 0x9841,
    0x8801, 0x48c0, 0x4980, 0x8941, 0x4b00, 0x8bc1, 0x8a81, 0x4a40, 0x4e00, 0x8ec1, 0x8f81, 0x4f40, 0x8d01, 0x4dc0, 0x4c80, 0x8c41,
    0x4400, 0x84c1, 0x8581, 0x4540, 0x8701, 0x47c0, 0x4680, 0x8641, 0x8201, 0x42c0, 0x4380, 0x8341, 0x4100, 0x81c1, 0x8081, 0x4040, 
};

static uint16_t crc16_calc(uint8_t * data, uint16_t len){
    uint16_t crc = 0;   // initial value = 0 
    while (len--){
        crc = (crc >> 8) ^ crc16_table[ (crc ^ ((uint16_t) *data++)) & 0x00FF ];
    }
    return crc;
}

static inline uint16_t l2cap_encanced_control_field_for_information_frame(uint8_t tx_seq, int final, uint8_t req_seq, l2cap_segmentation_and_reassembly_t sar){
    return (((uint16_t) sar) << 14) | (req_seq << 8) | (final << 7) | (tx_seq << 1) | 0; 
}

static inline uint16_t l2cap_encanced_control_field_for_supevisor_frame(l2cap_supervisory_function_t supervisory_function, int poll, int final, uint8_t req_seq){
    return (req_seq << 8) | (final << 7) | (poll << 4) | (((int) supervisory_function) << 2) | 1; 
}

static int l2cap_next_ertm_seq_nr(int seq_nr){
    return (seq_nr + 1) & 0x3f;
}

static bool l2cap_ertm_can_store_packet_now(l2cap_channel_t * channel){
    // get num free tx buffers
    int num_free_tx_buffers = channel->num_tx_buffers - channel->num_stored_tx_frames;
    // calculate num tx buffers for remote MTU
    int num_tx_buffers_for_max_remote_mtu;
    uint16_t effective_mps = btstack_min(channel->remote_mps, channel->local_mps);
    if (channel->remote_mtu <= effective_mps){
        // MTU fits into single packet
        num_tx_buffers_for_max_remote_mtu = 1;
    } else {
        // include SDU Length
        num_tx_buffers_for_max_remote_mtu = (channel->remote_mtu + 2 + (effective_mps - 1)) / effective_mps;
    }
    log_debug("num_free_tx_buffers %u, num_tx_buffers_for_max_remote_mtu %u", num_free_tx_buffers, num_tx_buffers_for_max_remote_mtu);
    return num_tx_buffers_for_max_remote_mtu <= num_free_tx_buffers;
}

static void l2cap_ertm_retransmit_unacknowleded_frames(l2cap_channel_t * l2cap_channel){
    log_info("Retransmit unacknowleged frames");
    l2cap_channel->unacked_frames = 0;;
    l2cap_channel->tx_send_index  = l2cap_channel->tx_read_index;
}

static void l2cap_ertm_next_tx_write_index(l2cap_channel_t * channel){
    channel->tx_write_index++;
    if (channel->tx_write_index < channel->num_tx_buffers) return;
    channel->tx_write_index = 0;
}

static void l2cap_ertm_start_monitor_timer(l2cap_channel_t * channel){
    log_info("Start Monitor timer");
    btstack_run_loop_remove_timer(&channel->monitor_timer);
    btstack_run_loop_set_timer_handler(&channel->monitor_timer, &l2cap_ertm_monitor_timeout_callback);
    btstack_run_loop_set_timer_context(&channel->monitor_timer, channel);
    btstack_run_loop_set_timer(&channel->monitor_timer, channel->local_monitor_timeout_ms);
    btstack_run_loop_add_timer(&channel->monitor_timer);
}

static void l2cap_ertm_stop_monitor_timer(l2cap_channel_t * channel){
    log_info("Stop Monitor timer");
    btstack_run_loop_remove_timer(&channel->monitor_timer);
}

static void l2cap_ertm_start_retransmission_timer(l2cap_channel_t * channel){
    log_info("Start Retransmission timer");
    btstack_run_loop_remove_timer(&channel->retransmission_timer);
    btstack_run_loop_set_timer_handler(&channel->retransmission_timer, &l2cap_ertm_retransmission_timeout_callback);
    btstack_run_loop_set_timer_context(&channel->retransmission_timer, channel);
    btstack_run_loop_set_timer(&channel->retransmission_timer, channel->local_retransmission_timeout_ms);
    btstack_run_loop_add_timer(&channel->retransmission_timer);
}

static void l2cap_ertm_stop_retransmission_timer(l2cap_channel_t * l2cap_channel){
    log_info("Stop Retransmission timer");
    btstack_run_loop_remove_timer(&l2cap_channel->retransmission_timer);
}    

static void l2cap_ertm_monitor_timeout_callback(btstack_timer_source_t * ts){
    log_info("Monitor timeout");
    l2cap_channel_t * l2cap_channel = (l2cap_channel_t *) btstack_run_loop_get_timer_context(ts);

    // TODO: we assume that it's the oldest packet
    l2cap_ertm_tx_packet_state_t * tx_state;
    tx_state = &l2cap_channel->tx_packets_state[l2cap_channel->tx_read_index];

    // check retry count
    if (tx_state->retry_count < l2cap_channel->remote_max_transmit){
        // increment retry count
        tx_state->retry_count++;

        // start retransmit
        l2cap_ertm_retransmit_unacknowleded_frames(l2cap_channel);

        // start monitor timer
        l2cap_ertm_start_monitor_timer(l2cap_channel);

        // send RR/P=1
        l2cap_channel->send_supervisor_frame_receiver_ready_poll = 1;
    } else {
        log_info("Monitor timer expired & retry count >= max transmit -> disconnect");
        l2cap_channel->state = L2CAP_STATE_WILL_SEND_DISCONNECT_REQUEST;
    }
    l2cap_run();
}

static void l2cap_ertm_retransmission_timeout_callback(btstack_timer_source_t * ts){
    log_info("Retransmission timeout");
    l2cap_channel_t * l2cap_channel = (l2cap_channel_t *) btstack_run_loop_get_timer_context(ts);
    
    // TODO: we assume that it's the oldest packet
    l2cap_ertm_tx_packet_state_t * tx_state;
    tx_state = &l2cap_channel->tx_packets_state[l2cap_channel->tx_read_index];

    // set retry count = 1
    tx_state->retry_count = 1;

    // start retransmit
    l2cap_ertm_retransmit_unacknowleded_frames(l2cap_channel);

    // start monitor timer
    l2cap_ertm_start_monitor_timer(l2cap_channel);
 
    // send RR/P=1
    l2cap_channel->send_supervisor_frame_receiver_ready_poll = 1;
    l2cap_run();
}

static int l2cap_ertm_send_information_frame(l2cap_channel_t * channel, int index, int final){
    l2cap_ertm_tx_packet_state_t * tx_state = &channel->tx_packets_state[index];
    hci_reserve_packet_buffer();
    uint8_t *acl_buffer = hci_get_outgoing_packet_buffer();
    uint16_t control = l2cap_encanced_control_field_for_information_frame(tx_state->tx_seq, final, channel->req_seq, tx_state->sar);
    log_info("I-Frame: control 0x%04x", control);
    little_endian_store_16(acl_buffer, 8, control);
    (void)memcpy(&acl_buffer[8 + 2],
                 &channel->tx_packets_data[index * channel->remote_mps],
                 tx_state->len);
    // (re-)start retransmission timer on 
    l2cap_ertm_start_retransmission_timer(channel);
    // send
    return l2cap_send_prepared(channel->local_cid, 2 + tx_state->len);
}

static void l2cap_ertm_store_fragment(l2cap_channel_t * channel, l2cap_segmentation_and_reassembly_t sar, uint16_t sdu_length, const uint8_t * data, uint16_t len){
    // get next index for storing packets
    int index = channel->tx_write_index;

    l2cap_ertm_tx_packet_state_t * tx_state = &channel->tx_packets_state[index];
    tx_state->tx_seq = channel->next_tx_seq;
    tx_state->sar = sar;
    tx_state->retry_count = 0;

    uint8_t * tx_packet = &channel->tx_packets_data[index * channel->remote_mps];
    log_debug("index %u, local mps %u, remote mps %u, packet tx %p, len %u", index, channel->local_mps, channel->remote_mps, tx_packet, len);
    int pos = 0;
    if (sar == L2CAP_SEGMENTATION_AND_REASSEMBLY_START_OF_L2CAP_SDU){
        little_endian_store_16(tx_packet, 0, sdu_length);
        pos += 2;
    }
    (void)memcpy(&tx_packet[pos], data, len);
    tx_state->len = pos + len;

    // update
    channel->num_stored_tx_frames++;
    channel->next_tx_seq = l2cap_next_ertm_seq_nr(channel->next_tx_seq);
    l2cap_ertm_next_tx_write_index(channel);

    log_info("l2cap_ertm_store_fragment: tx_read_index %u, tx_write_index %u, num stored %u", channel->tx_read_index, channel->tx_write_index, channel->num_stored_tx_frames);

}

static uint8_t l2cap_ertm_send(l2cap_channel_t * channel, const uint8_t * data, uint16_t len){
    if (len > channel->remote_mtu){
        log_error("l2cap_ertm_send cid 0x%02x, data length exceeds remote MTU.", channel->local_cid);
        return L2CAP_DATA_LEN_EXCEEDS_REMOTE_MTU;
    }

    if (!l2cap_ertm_can_store_packet_now(channel)){
        log_error("l2cap_ertm_send cid 0x%02x, fragment store full", channel->local_cid);
        return BTSTACK_ACL_BUFFERS_FULL;
    }

    // check if it needs to get fragmented
    uint16_t effective_mps = btstack_min(channel->remote_mps, channel->local_mps);
    if (len > effective_mps){
        // fragmentation needed.
        l2cap_segmentation_and_reassembly_t sar =  L2CAP_SEGMENTATION_AND_REASSEMBLY_START_OF_L2CAP_SDU;
        uint16_t chunk_len = 0;
        while (len){
            switch (sar){
                case L2CAP_SEGMENTATION_AND_REASSEMBLY_START_OF_L2CAP_SDU:
                    chunk_len = effective_mps - 2;    // sdu_length
                    l2cap_ertm_store_fragment(channel, sar, len, data, chunk_len);
                    sar = L2CAP_SEGMENTATION_AND_REASSEMBLY_CONTINUATION_OF_L2CAP_SDU;
                    break;
                case L2CAP_SEGMENTATION_AND_REASSEMBLY_CONTINUATION_OF_L2CAP_SDU:
                    chunk_len = effective_mps;
                    if (chunk_len >= len){
                        sar = L2CAP_SEGMENTATION_AND_REASSEMBLY_END_OF_L2CAP_SDU; 
                        chunk_len = len;                       
                    }
                    l2cap_ertm_store_fragment(channel, sar, len, data, chunk_len);
                    break;
                default:
                    btstack_unreachable();
                    break;
            }
            len  -= chunk_len;
            data += chunk_len;
        }

    } else {
        l2cap_ertm_store_fragment(channel, L2CAP_SEGMENTATION_AND_REASSEMBLY_UNSEGMENTED_L2CAP_SDU, 0, data, len);
    }

    // try to send
    l2cap_notify_channel_can_send();
    return ERROR_CODE_SUCCESS;
}

static uint16_t l2cap_setup_options_ertm_request(l2cap_channel_t * channel, uint8_t * config_options){
    int pos = 0;
    config_options[pos++] = L2CAP_CONFIG_OPTION_TYPE_RETRANSMISSION_AND_FLOW_CONTROL;
    config_options[pos++] = 9;      // length
    config_options[pos++] = (uint8_t) channel->mode;
    config_options[pos++] = channel->num_rx_buffers;    // == TxWindows size
    config_options[pos++] = channel->local_max_transmit;
    little_endian_store_16( config_options, pos, channel->local_retransmission_timeout_ms);
    pos += 2;
    little_endian_store_16( config_options, pos, channel->local_monitor_timeout_ms);
    pos += 2;
    little_endian_store_16( config_options, pos, channel->local_mps);
    pos += 2;
    //
    config_options[pos++] = L2CAP_CONFIG_OPTION_TYPE_MAX_TRANSMISSION_UNIT;
    config_options[pos++] = 2;     // length
    little_endian_store_16(config_options, pos, channel->local_mtu);
    pos += 2;

    // fcs_option = 2 <=> OMIT FCS Omit
    if (channel->fcs_option < 2){
        config_options[pos++] = L2CAP_CONFIG_OPTION_TYPE_FRAME_CHECK_SEQUENCE;
        config_options[pos++] = 1;     // length
        config_options[pos++] = channel->fcs_option;
    }
    return pos; // 11+4+3=18
}

static uint16_t l2cap_setup_options_ertm_response(l2cap_channel_t * channel, uint8_t * config_options){
    int pos = 0;
    config_options[pos++] = L2CAP_CONFIG_OPTION_TYPE_RETRANSMISSION_AND_FLOW_CONTROL;
    config_options[pos++] = 9;      // length
    config_options[pos++] = (uint8_t) channel->mode;
    // less or equal to remote tx window size
    config_options[pos++] = btstack_min(channel->num_tx_buffers, channel->remote_tx_window_size);
    // max transmit in response shall be ignored -> use sender values
    config_options[pos++] = channel->remote_max_transmit;
    // A value for the Retransmission time-out shall be sent in a positive Configuration Response
    // and indicates the value that will be used by the sender of the Configuration Response -> use our value
    little_endian_store_16( config_options, pos, channel->local_retransmission_timeout_ms);
    pos += 2;
    // A value for the Monitor time-out shall be sent in a positive Configuration Response
    // and indicates the value that will be used by the sender of the Configuration Response -> use our value
    little_endian_store_16( config_options, pos, channel->local_monitor_timeout_ms);
    pos += 2;
    // less or equal to remote mps
    uint16_t effective_mps = btstack_min(channel->remote_mps, channel->local_mps);
    little_endian_store_16( config_options, pos, effective_mps);
    pos += 2;
    //
    config_options[pos++] = L2CAP_CONFIG_OPTION_TYPE_MAX_TRANSMISSION_UNIT; // MTU
    config_options[pos++] = 2;     // length
    little_endian_store_16(config_options, pos, channel->remote_mtu);
    pos += 2;
    //
    config_options[pos++] = L2CAP_CONFIG_OPTION_TYPE_FRAME_CHECK_SEQUENCE;
    config_options[pos++] = 1;     // length
    config_options[pos++] = channel->fcs_option;
    return pos; // 11+4=15
}

static int l2cap_ertm_send_supervisor_frame(l2cap_channel_t * channel, uint16_t control){
    hci_reserve_packet_buffer();
    uint8_t *acl_buffer = hci_get_outgoing_packet_buffer();
    log_info("S-Frame: control 0x%04x", control);
    little_endian_store_16(acl_buffer, 8, control);
    return l2cap_send_prepared(channel->local_cid, 2);
}

static uint8_t l2cap_ertm_validate_local_config(l2cap_ertm_config_t * ertm_config){
    
    uint8_t result = ERROR_CODE_SUCCESS;
    if (ertm_config->max_transmit < 1){
        log_error("max_transmit must be >= 1");
        result = ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }
    if (ertm_config->retransmission_timeout_ms < 2000){
        log_error("retransmission_timeout_ms must be >= 2000 ms");
        result = ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }
    if (ertm_config->monitor_timeout_ms < 12000){
        log_error("monitor_timeout_ms must be >= 12000 ms");
        result = ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }
    if (ertm_config->local_mtu < 48){
        log_error("local_mtu must be >= 48");
        result = ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }
    if (ertm_config->num_rx_buffers < 1){
        log_error("num_rx_buffers must be >= 1");
        result = ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }
    if (ertm_config->num_tx_buffers < 1){
        log_error("num_rx_buffers must be >= 1");
        result = ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }
    return result;
}

static void l2cap_ertm_setup_buffers(l2cap_channel_t * channel, uint8_t * buffer, uint32_t size){
    btstack_assert( (((uintptr_t) buffer) & 0x0f) == 0);

    // setup state buffers - use void cast to avoid -Wcast-align warning
    uint32_t pos = 0;
    channel->rx_packets_state = (l2cap_ertm_rx_packet_state_t *) (void *) &buffer[pos];
    pos += channel->num_rx_buffers * sizeof(l2cap_ertm_rx_packet_state_t);
    channel->tx_packets_state = (l2cap_ertm_tx_packet_state_t *) (void *) &buffer[pos];
    pos += channel->num_tx_buffers * sizeof(l2cap_ertm_tx_packet_state_t);

    // setup reassembly buffer
    channel->reassembly_buffer = &buffer[pos];
    pos += channel->local_mtu;

    // setup rx buffers
    channel->rx_packets_data = &buffer[pos];
    pos += channel->num_rx_buffers * channel->local_mps;

    // setup tx buffers
    channel->tx_packets_data = &buffer[pos];
    pos += channel->num_rx_buffers * channel->remote_mps;

    btstack_assert(pos <= size);
    UNUSED(pos);
}

static void l2cap_ertm_configure_channel(l2cap_channel_t * channel, l2cap_ertm_config_t * ertm_config, uint8_t * buffer, uint32_t size){

    channel->mode  = L2CAP_CHANNEL_MODE_ENHANCED_RETRANSMISSION;
    channel->ertm_mandatory = ertm_config->ertm_mandatory;
    channel->local_max_transmit = ertm_config->max_transmit;
    channel->local_retransmission_timeout_ms = ertm_config->retransmission_timeout_ms;
    channel->local_monitor_timeout_ms = ertm_config->monitor_timeout_ms;
    channel->local_mtu = ertm_config->local_mtu;
    channel->num_rx_buffers = ertm_config->num_rx_buffers;
    channel->num_tx_buffers = ertm_config->num_tx_buffers;
    channel->fcs_option = ertm_config->fcs_option;

    // align buffer to 16-byte boundary to assert l2cap_ertm_rx_packet_state_t is aligned
    int bytes_till_alignment = 16 - (((uintptr_t) buffer) & 0x0f);
    buffer += bytes_till_alignment;
    size   -= bytes_till_alignment;

    // calculate space for rx and tx buffers
    uint32_t state_len = channel->num_rx_buffers * sizeof(l2cap_ertm_rx_packet_state_t) + channel->num_tx_buffers * sizeof(l2cap_ertm_tx_packet_state_t);
    uint32_t buffer_space = size - state_len - channel->local_mtu;

    // divide rest of data equally for initial config
    uint16_t mps = buffer_space / (ertm_config->num_rx_buffers + ertm_config->num_tx_buffers);
    channel->local_mps  = mps;
    channel->remote_mps = mps;
    l2cap_ertm_setup_buffers(channel, buffer, size);

    log_info("Local MPS: %u", channel->local_mps);
}

uint8_t l2cap_ertm_create_channel(btstack_packet_handler_t packet_handler, bd_addr_t address, uint16_t psm,
    l2cap_ertm_config_t * ertm_config, uint8_t * buffer, uint32_t size, uint16_t * out_local_cid){

    log_info("l2cap_ertm_create_channel addr %s, psm 0x%x, local mtu %u", bd_addr_to_str(address), psm, ertm_config->local_mtu);

    // validate local config
    uint8_t result = l2cap_ertm_validate_local_config(ertm_config);
    if (result) return result;

    // determine security level based on psm
    const gap_security_level_t security_level = l2cap_security_level_0_allowed_for_PSM(psm) ? LEVEL_0 : gap_get_security_level();

    l2cap_channel_t * channel = l2cap_create_channel_entry(packet_handler, L2CAP_CHANNEL_TYPE_CLASSIC, address, BD_ADDR_TYPE_ACL, psm, ertm_config->local_mtu, security_level);
    if (!channel) {
        return BTSTACK_MEMORY_ALLOC_FAILED;
    }

    // configure ERTM
    l2cap_ertm_configure_channel(channel, ertm_config, buffer, size);

    // add to connections list
    btstack_linked_list_add_tail(&l2cap_channels, (btstack_linked_item_t *) channel);

    // store local_cid
    if (out_local_cid){
       *out_local_cid = channel->local_cid;
    }

    // check if hci connection is already usable
    hci_connection_t * conn = hci_connection_for_bd_addr_and_type(address, BD_ADDR_TYPE_ACL);
    if (conn){
        log_info("l2cap_create_channel, hci connection already exists");
        l2cap_handle_connection_complete(conn->con_handle, channel);
        // check if remote supported fearures are already received
        if (hci_remote_features_available(conn->con_handle)) {
            l2cap_handle_remote_supported_features_received(channel);
        } else {
            hci_remote_features_query(conn->con_handle);
        };
    }

    l2cap_run(); 

    return 0;     
}

static void l2cap_ertm_notify_channel_can_send(l2cap_channel_t * channel){
    if (l2cap_ertm_can_store_packet_now(channel)){
        channel->waiting_for_can_send_now = 0;
        l2cap_emit_can_send_now(channel->packet_handler, channel->local_cid);
    }
}

uint8_t l2cap_ertm_accept_connection(uint16_t local_cid, l2cap_ertm_config_t * ertm_config, uint8_t * buffer, uint32_t size){

    log_info("l2cap_ertm_accept_connection local_cid 0x%x", local_cid);
    l2cap_channel_t * channel = l2cap_get_channel_for_local_cid(local_cid);
    if (!channel) {
        log_error("l2cap_accept_connection called but local_cid 0x%x not found", local_cid);
        return L2CAP_LOCAL_CID_DOES_NOT_EXIST;
    }

    // validate local config
    uint8_t result = l2cap_ertm_validate_local_config(ertm_config);
    if (result) return result;

    // configure L2CAP ERTM
    l2cap_ertm_configure_channel(channel, ertm_config, buffer, size);

    // we need to know if ERTM is supported before sending a config response
    channel->state = L2CAP_STATE_WAIT_INCOMING_EXTENDED_FEATURES;
    hci_connection_t * connection = hci_connection_for_handle(channel->con_handle);
    btstack_assert(connection != NULL);
    switch (connection->l2cap_state.information_state){
        case L2CAP_INFORMATION_STATE_IDLE:
            connection->l2cap_state.information_state = L2CAP_INFORMATION_STATE_W2_SEND_EXTENDED_FEATURE_REQUEST;
            break;
        case L2CAP_INFORMATION_STATE_DONE:
            l2cap_handle_information_request_complete(connection);
            break;
        default:
            break;
    }

    l2cap_run();

    return ERROR_CODE_SUCCESS;
}

uint8_t l2cap_ertm_decline_connection(uint16_t local_cid){
    l2cap_decline_connection(local_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t l2cap_ertm_set_busy(uint16_t local_cid){
    l2cap_channel_t * channel = l2cap_get_channel_for_local_cid( local_cid);
    if (!channel) {
        log_error( "l2cap_decline_connection called but local_cid 0x%x not found", local_cid);
        return L2CAP_LOCAL_CID_DOES_NOT_EXIST;
    }
    if (!channel->local_busy){
        channel->local_busy = 1;
        channel->send_supervisor_frame_receiver_not_ready = 1;
        l2cap_run();
    }
    return ERROR_CODE_SUCCESS;
}

uint8_t l2cap_ertm_set_ready(uint16_t local_cid){
    l2cap_channel_t * channel = l2cap_get_channel_for_local_cid( local_cid);
    if (!channel) {
        log_error( "l2cap_decline_connection called but local_cid 0x%x not found", local_cid);
        return L2CAP_LOCAL_CID_DOES_NOT_EXIST;
    }
    if (channel->local_busy){
        channel->local_busy = 0;
        channel->send_supervisor_frame_receiver_ready_poll = 1;
        l2cap_run();
    }
    return ERROR_CODE_SUCCESS;
}

// Process-ReqSeq
static void l2cap_ertm_process_req_seq(l2cap_channel_t * l2cap_channel, uint8_t req_seq){
    int num_buffers_acked = 0;
    l2cap_ertm_tx_packet_state_t * tx_state;
    log_info("l2cap_ertm_process_req_seq: tx_read_index %u, tx_write_index %u, req_seq %u", l2cap_channel->tx_read_index, l2cap_channel->tx_write_index, req_seq);
    while (true){

        // no unack packets left
        if (l2cap_channel->unacked_frames == 0) {
            // stop retransmission timer
            l2cap_ertm_stop_retransmission_timer(l2cap_channel);
            break;
        }

        tx_state = &l2cap_channel->tx_packets_state[l2cap_channel->tx_read_index];
        // calc delta
        int delta = (req_seq - tx_state->tx_seq) & 0x03f;
        if (delta == 0) break;  // all packets acknowledged
        if (delta > l2cap_channel->remote_tx_window_size) break;   

        num_buffers_acked++;
        l2cap_channel->num_stored_tx_frames--;
        l2cap_channel->unacked_frames--;
        log_info("RR seq %u => packet with tx_seq %u done", req_seq, tx_state->tx_seq);

        l2cap_channel->tx_read_index++;
        if (l2cap_channel->tx_read_index >= l2cap_channel->num_rx_buffers){
            l2cap_channel->tx_read_index = 0;
        }
    }
    if (num_buffers_acked){
        log_info("num_buffers_acked %u", num_buffers_acked);
    l2cap_ertm_notify_channel_can_send(l2cap_channel);
}     
}     

static l2cap_ertm_tx_packet_state_t * l2cap_ertm_get_tx_state(l2cap_channel_t * l2cap_channel, uint8_t tx_seq){
    int i;
    for (i=0;i<l2cap_channel->num_tx_buffers;i++){
        l2cap_ertm_tx_packet_state_t * tx_state = &l2cap_channel->tx_packets_state[i];
        if (tx_state->tx_seq == tx_seq) return tx_state;
    }
    return NULL;
}

// @param delta number of frames in the future, >= 1
// @assumption size <= l2cap_channel->local_mps (checked in l2cap_acl_classic_handler)
static void l2cap_ertm_handle_out_of_sequence_sdu(l2cap_channel_t * l2cap_channel, l2cap_segmentation_and_reassembly_t sar, int delta, const uint8_t * payload, uint16_t size){
    log_info("Store SDU with delta %u", delta);
    // get rx state for packet to store
    int index = l2cap_channel->rx_store_index + delta - 1;
    if (index > l2cap_channel->num_rx_buffers){
        index -= l2cap_channel->num_rx_buffers;
    }
    log_info("Index of packet to store %u", index);
    l2cap_ertm_rx_packet_state_t * rx_state = &l2cap_channel->rx_packets_state[index];
    // check if buffer is free
    if (rx_state->valid){
        log_error("Packet buffer already used");
        return;
    }
    rx_state->valid = 1;
    rx_state->sar = sar;
    rx_state->len = size;
    uint8_t * rx_buffer = &l2cap_channel->rx_packets_data[index];
    (void)memcpy(rx_buffer, payload, size);
}

// @assumption size <= l2cap_channel->local_mps (checked in l2cap_acl_classic_handler)
static void l2cap_ertm_handle_in_sequence_sdu(l2cap_channel_t * l2cap_channel, l2cap_segmentation_and_reassembly_t sar, const uint8_t * payload, uint16_t size){
    uint16_t reassembly_sdu_length;
    switch (sar){
        case L2CAP_SEGMENTATION_AND_REASSEMBLY_UNSEGMENTED_L2CAP_SDU:
            // assert total packet size <= our mtu
            if (size > l2cap_channel->local_mtu) break;
            // packet complete -> disapatch
            l2cap_dispatch_to_channel(l2cap_channel, L2CAP_DATA_PACKET, (uint8_t*) payload, size);
            break;
        case L2CAP_SEGMENTATION_AND_REASSEMBLY_START_OF_L2CAP_SDU:
            // read SDU len
            reassembly_sdu_length = little_endian_read_16(payload, 0);
            payload += 2;
            size    -= 2;
            // assert reassembled size <= our mtu
            if (reassembly_sdu_length > l2cap_channel->local_mtu) break;
            // store start segment
            l2cap_channel->reassembly_sdu_length = reassembly_sdu_length;
            (void)memcpy(&l2cap_channel->reassembly_buffer[0], payload, size);
            l2cap_channel->reassembly_pos = size;
            break;
        case L2CAP_SEGMENTATION_AND_REASSEMBLY_CONTINUATION_OF_L2CAP_SDU:
            // assert size of reassembled data <= our mtu
            if (l2cap_channel->reassembly_pos + size > l2cap_channel->local_mtu) break;
            // store continuation segment
            (void)memcpy(&l2cap_channel->reassembly_buffer[l2cap_channel->reassembly_pos],
                         payload, size);
            l2cap_channel->reassembly_pos += size;
            break;
        case L2CAP_SEGMENTATION_AND_REASSEMBLY_END_OF_L2CAP_SDU:
            // assert size of reassembled data <= our mtu
            if (l2cap_channel->reassembly_pos + size > l2cap_channel->local_mtu) break;
            // store continuation segment
            (void)memcpy(&l2cap_channel->reassembly_buffer[l2cap_channel->reassembly_pos],
                         payload, size);
            l2cap_channel->reassembly_pos += size;
            // assert size of reassembled data matches announced sdu length
            if (l2cap_channel->reassembly_pos != l2cap_channel->reassembly_sdu_length) break;
            // packet complete -> disapatch
            l2cap_dispatch_to_channel(l2cap_channel, L2CAP_DATA_PACKET, l2cap_channel->reassembly_buffer, l2cap_channel->reassembly_pos);
            l2cap_channel->reassembly_pos = 0;    
            break; 
        default:
            btstack_assert(false);
            break;
    }
}

static void l2cap_ertm_channel_send_information_frame(l2cap_channel_t * channel){
    channel->unacked_frames++;
    int index = channel->tx_send_index;
    channel->tx_send_index++;
    if (channel->tx_send_index >= channel->num_tx_buffers){
        channel->tx_send_index = 0;
    }
    l2cap_ertm_send_information_frame(channel, index, 0);   // final = 0
}

#endif

#ifdef L2CAP_USES_CHANNELS
static uint16_t l2cap_next_local_cid(void){
    do {
        if (l2cap_local_source_cid == 0xfffeu) {
            l2cap_local_source_cid = 0x40;
        } else {
            l2cap_local_source_cid++;
        }
    } while (l2cap_get_channel_for_local_cid(l2cap_local_source_cid) != NULL);
    return l2cap_local_source_cid;
}
#endif

static uint8_t l2cap_next_sig_id(void){
    if (l2cap_sig_seq_nr == 0xffu) {
        l2cap_sig_seq_nr = 1;
    } else {
        l2cap_sig_seq_nr++;
    }
    return l2cap_sig_seq_nr;
}

void l2cap_init(void){
#ifdef L2CAP_USES_CHANNELS
    l2cap_local_source_cid  = 0x40;
#endif
    l2cap_sig_seq_nr  = 0xff;

#ifdef ENABLE_CLASSIC
    // Setup Connectionless Channel
    l2cap_fixed_channel_classic_connectionless.local_cid     = L2CAP_CID_CONNECTIONLESS_CHANNEL;
    l2cap_fixed_channel_classic_connectionless.channel_type  = L2CAP_CHANNEL_TYPE_CONNECTIONLESS;
    btstack_linked_list_add(&l2cap_channels, (btstack_linked_item_t *) &l2cap_fixed_channel_classic_connectionless);
#endif

#ifdef ENABLE_BLE
    // Setup fixed ATT Channel
    l2cap_fixed_channel_le_att.local_cid    = L2CAP_CID_ATTRIBUTE_PROTOCOL;
    l2cap_fixed_channel_le_att.channel_type = L2CAP_CHANNEL_TYPE_FIXED_LE;
    btstack_linked_list_add(&l2cap_channels, (btstack_linked_item_t *) &l2cap_fixed_channel_le_att);

    // Setup fixed SM Channel
    l2cap_fixed_channel_le_sm.local_cid     = L2CAP_CID_SECURITY_MANAGER_PROTOCOL;
    l2cap_fixed_channel_le_sm.channel_type  = L2CAP_CHANNEL_TYPE_FIXED_LE;
    btstack_linked_list_add(&l2cap_channels, (btstack_linked_item_t *) &l2cap_fixed_channel_le_sm);
#endif
#ifdef ENABLE_CLASSIC
    // Setup fixed SM Channel
    l2cap_fixed_channel_classic_sm.local_cid     = L2CAP_CID_BR_EDR_SECURITY_MANAGER;
    l2cap_fixed_channel_classic_sm.channel_type  = L2CAP_CHANNEL_TYPE_FIXED_CLASSIC;
    btstack_linked_list_add(&l2cap_channels, (btstack_linked_item_t *) &l2cap_fixed_channel_classic_sm);
#endif

#ifdef ENABLE_L2CAP_ENHANCED_CREDIT_BASED_FLOW_CONTROL_MODE
    l2cap_enhanced_mps_min = 0x0001;
    l2cap_enhanced_mps_max = 0xffff;
    l2cap_trigger_run_registration.callback = &l2cap_run_trigger_callback;
#endif

    //
    // register callback with HCI
    //
    l2cap_hci_event_callback_registration.callback = &l2cap_hci_event_handler;
    hci_add_event_handler(&l2cap_hci_event_callback_registration);

    hci_register_acl_packet_handler(&l2cap_acl_handler);

#ifndef ENABLE_EXPLICIT_CONNECTABLE_MODE_CONTROL
#ifdef ENABLE_CLASSIC
    gap_connectable_control(0); // no services yet
#endif
#endif
}

/**
 * @brief De-Init L2CAP
 */
void l2cap_deinit(void){
    l2cap_channels = NULL;
    l2cap_signaling_responses_pending = 0;
#ifdef ENABLE_CLASSIC
    l2cap_require_security_level2_for_outgoing_sdp = 0;
    (void)memset(&l2cap_fixed_channel_classic_connectionless, 0, sizeof(l2cap_fixed_channel_classic_connectionless));
    l2cap_services = NULL;
    (void)memset(l2cap_outgoing_classic_addr, 0, 6);
    (void)memset(&l2cap_fixed_channel_classic_sm, 0, sizeof(l2cap_fixed_channel_classic_sm));
#endif
#ifdef ENABLE_BLE
    l2cap_le_custom_max_mtu = 0;
    (void)memset(&l2cap_fixed_channel_le_att, 0, sizeof(l2cap_fixed_channel_le_att));
    (void)memset(&l2cap_fixed_channel_le_sm, 0, sizeof(l2cap_fixed_channel_le_sm));
#endif
#ifdef ENABLE_L2CAP_LE_CREDIT_BASED_FLOW_CONTROL_MODE
    l2cap_le_services = NULL;
#endif
#ifdef ENABLE_L2CAP_ENHANCED_CREDIT_BASED_FLOW_CONTROL_MODE
    l2cap_enhanced_services = NULL;
#endif
    l2cap_event_handlers = NULL;
}

void l2cap_add_event_handler(btstack_packet_callback_registration_t * callback_handler){
    btstack_linked_list_add_tail(&l2cap_event_handlers, (btstack_linked_item_t*) callback_handler);
}

void l2cap_remove_event_handler(btstack_packet_callback_registration_t * callback_handler){
    btstack_linked_list_remove(&l2cap_event_handlers, (btstack_linked_item_t*) callback_handler);
}

static void l2cap_emit_event(uint8_t *event, uint16_t size) {
    hci_dump_btstack_event( event, size);
    // dispatch to all event handlers
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &l2cap_event_handlers);
    while (btstack_linked_list_iterator_has_next(&it)){
        btstack_packet_callback_registration_t * entry = (btstack_packet_callback_registration_t*) btstack_linked_list_iterator_next(&it);
        entry->callback(HCI_EVENT_PACKET, 0, event, size);
    }
}

void l2cap_request_can_send_fix_channel_now_event(hci_con_handle_t con_handle, uint16_t channel_id){
    UNUSED(con_handle);  // ok: there is no con handle

    l2cap_fixed_channel_t * channel = l2cap_fixed_channel_for_channel_id(channel_id);
    if (!channel) return;
    channel->waiting_for_can_send_now = 1;
    l2cap_notify_channel_can_send();
}

bool l2cap_can_send_fixed_channel_packet_now(hci_con_handle_t con_handle, uint16_t channel_id){
    UNUSED(channel_id); // ok: only depends on Controller LE buffers

    return hci_can_send_acl_packet_now(con_handle);
}

uint8_t *l2cap_get_outgoing_buffer(void){
    return hci_get_outgoing_packet_buffer() + COMPLETE_L2CAP_HEADER; // 8 bytes
}

// only for L2CAP Basic Channels
void l2cap_reserve_packet_buffer(void){
    hci_reserve_packet_buffer();
}

// only for L2CAP Basic Channels
void l2cap_release_packet_buffer(void){
    hci_release_packet_buffer();
}

static void l2cap_setup_header(uint8_t * acl_buffer, hci_con_handle_t con_handle, uint8_t packet_boundary, uint16_t remote_cid, uint16_t len){
    // 0 - Connection handle : PB=pb : BC=00 
    little_endian_store_16(acl_buffer, 0u, con_handle | (packet_boundary << 12u) | (0u << 14u));
    // 2 - ACL length
    little_endian_store_16(acl_buffer, 2u,  len + 4u);
    // 4 - L2CAP packet length
    little_endian_store_16(acl_buffer, 4u,  len + 0u);
    // 6 - L2CAP channel DEST
    little_endian_store_16(acl_buffer, 6,  remote_cid);    
}

// assumption - only on LE connections
uint8_t l2cap_send_prepared_connectionless(hci_con_handle_t con_handle, uint16_t cid, uint16_t len){
    
    if (!hci_is_packet_buffer_reserved()){
        log_error("l2cap_send_prepared_connectionless called without reserving packet first");
        return BTSTACK_ACL_BUFFERS_FULL;
    }

    if (!hci_can_send_prepared_acl_packet_now(con_handle)){
        log_info("l2cap_send_prepared_connectionless handle 0x%02x, cid 0x%02x, cannot send", con_handle, cid);
        return BTSTACK_ACL_BUFFERS_FULL;
    }
    
    log_debug("l2cap_send_prepared_connectionless handle %u, cid 0x%02x", con_handle, cid);
    
    uint8_t *acl_buffer = hci_get_outgoing_packet_buffer();
    l2cap_setup_header(acl_buffer, con_handle, 0, cid, len);
    // send
    return hci_send_acl_packet_buffer(len+8u);
}

// assumption - only on LE connections
uint8_t l2cap_send_connectionless(hci_con_handle_t con_handle, uint16_t cid, uint8_t *data, uint16_t len){
    
    if (!hci_can_send_acl_packet_now(con_handle)){
        log_info("l2cap_send cid 0x%02x, cannot send", cid);
        return BTSTACK_ACL_BUFFERS_FULL;
    }
    
    hci_reserve_packet_buffer();
    uint8_t *acl_buffer = hci_get_outgoing_packet_buffer();
    
    (void)memcpy(&acl_buffer[8], data, len);
    
    return l2cap_send_prepared_connectionless(con_handle, cid, len);
}

static void l2cap_emit_can_send_now(btstack_packet_handler_t packet_handler, uint16_t channel) {
    log_debug("L2CAP_EVENT_CHANNEL_CAN_SEND_NOW local_cid 0x%x", channel);
    uint8_t event[4];
    event[0] = L2CAP_EVENT_CAN_SEND_NOW;
    event[1] = sizeof(event) - 2u;
    little_endian_store_16(event, 2, channel);
    hci_dump_btstack_event( event, sizeof(event));
    packet_handler(HCI_EVENT_PACKET, channel, event, sizeof(event));
}

#ifdef L2CAP_USES_CHANNELS
static void l2cap_dispatch_to_channel(l2cap_channel_t *channel, uint8_t type, uint8_t * data, uint16_t size){
    (* (channel->packet_handler))(type, channel->local_cid, data, size);
}

static void l2cap_emit_simple_event_with_cid(l2cap_channel_t * channel, uint8_t event_code){
    uint8_t event[4];
    event[0] = event_code;
    event[1] = sizeof(event) - 2u;
    little_endian_store_16(event, 2, channel->local_cid);
    hci_dump_btstack_event( event, sizeof(event));
    l2cap_dispatch_to_channel(channel, HCI_EVENT_PACKET, event, sizeof(event));
}
#endif

#ifdef ENABLE_CLASSIC
void l2cap_emit_channel_opened(l2cap_channel_t *channel, uint8_t status) {
    log_info("L2CAP_EVENT_CHANNEL_OPENED status 0x%x addr %s handle 0x%x psm 0x%x local_cid 0x%x remote_cid 0x%x local_mtu %u, remote_mtu %u, flush_timeout %u",
             status, bd_addr_to_str(channel->address), channel->con_handle, channel->psm,
             channel->local_cid, channel->remote_cid, channel->local_mtu, channel->remote_mtu, channel->flush_timeout);
    uint8_t event[26];
    event[0] = L2CAP_EVENT_CHANNEL_OPENED;
    event[1] = sizeof(event) - 2;
    event[2] = status;
    reverse_bd_addr(channel->address, &event[3]);
    little_endian_store_16(event,  9, channel->con_handle);
    little_endian_store_16(event, 11, channel->psm);
    little_endian_store_16(event, 13, channel->local_cid);
    little_endian_store_16(event, 15, channel->remote_cid);
    little_endian_store_16(event, 17, channel->local_mtu);
    little_endian_store_16(event, 19, channel->remote_mtu); 
    little_endian_store_16(event, 21, channel->flush_timeout); 
    event[23] = (channel->state_var & L2CAP_CHANNEL_STATE_VAR_INCOMING) ? 1 : 0;
#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
    log_info("ERTM mode %u, fcs enabled %u", channel->mode, channel->fcs_option);
    event[24] = channel->mode;
    event[25] = channel->fcs_option;

#else
    event[24] = L2CAP_CHANNEL_MODE_BASIC;
    event[25] = 0;
#endif
    hci_dump_btstack_event( event, sizeof(event));
    l2cap_dispatch_to_channel(channel, HCI_EVENT_PACKET, event, sizeof(event));
}

static void l2cap_emit_incoming_connection(l2cap_channel_t *channel) {
    log_info("L2CAP_EVENT_INCOMING_CONNECTION addr %s handle 0x%x psm 0x%x local_cid 0x%x remote_cid 0x%x",
             bd_addr_to_str(channel->address), channel->con_handle,  channel->psm, channel->local_cid, channel->remote_cid);
    uint8_t event[16];
    event[0] = L2CAP_EVENT_INCOMING_CONNECTION;
    event[1] = sizeof(event) - 2;
    reverse_bd_addr(channel->address, &event[2]);
    little_endian_store_16(event,  8, channel->con_handle);
    little_endian_store_16(event, 10, channel->psm);
    little_endian_store_16(event, 12, channel->local_cid);
    little_endian_store_16(event, 14, channel->remote_cid);
    hci_dump_btstack_event( event, sizeof(event));
    l2cap_dispatch_to_channel(channel, HCI_EVENT_PACKET, event, sizeof(event));
}

static void l2cap_handle_channel_open_failed(l2cap_channel_t * channel, uint8_t status){
#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
    // emit ertm buffer released, as it's not needed. if in basic mode, it was either not allocated or already released
    if (channel->mode == L2CAP_CHANNEL_MODE_ENHANCED_RETRANSMISSION){
        l2cap_emit_simple_event_with_cid(channel, L2CAP_EVENT_ERTM_BUFFER_RELEASED);
    }
#endif
    l2cap_emit_channel_opened(channel, status);
}

#endif

#ifdef L2CAP_USES_CHANNELS

static void l2cap_emit_channel_closed(l2cap_channel_t *channel) {
    log_info("L2CAP_EVENT_CHANNEL_CLOSED local_cid 0x%x", channel->local_cid);
    l2cap_emit_simple_event_with_cid(channel, L2CAP_EVENT_CHANNEL_CLOSED);
}

static void l2cap_handle_channel_closed(l2cap_channel_t * channel){
#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
    // emit ertm buffer released, as it's not needed anymore. if in basic mode, it was either not allocated or already released
    if (channel->mode == L2CAP_CHANNEL_MODE_ENHANCED_RETRANSMISSION){
        l2cap_emit_simple_event_with_cid(channel, L2CAP_EVENT_ERTM_BUFFER_RELEASED);
    }
#endif
    l2cap_emit_channel_closed(channel);
}
#endif

static l2cap_fixed_channel_t * l2cap_channel_item_by_cid(uint16_t cid){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, &l2cap_channels);
    while (btstack_linked_list_iterator_has_next(&it)){
        l2cap_fixed_channel_t * channel = (l2cap_fixed_channel_t*) btstack_linked_list_iterator_next(&it);
        if (channel->local_cid == cid) {
            return channel;
        }
    } 
    return NULL;
}

// used for fixed channels in LE (ATT/SM) and Classic (Connectionless Channel). CID < 0x04
static l2cap_fixed_channel_t * l2cap_fixed_channel_for_channel_id(uint16_t local_cid){
    if (local_cid >= 0x40u) return NULL;
    return (l2cap_fixed_channel_t*) l2cap_channel_item_by_cid(local_cid);
}

#ifdef L2CAP_USES_CHANNELS

static int l2cap_is_dynamic_channel_type(l2cap_channel_type_t channel_type) {
    switch (channel_type) {
        case L2CAP_CHANNEL_TYPE_CLASSIC:
        case L2CAP_CHANNEL_TYPE_CHANNEL_CBM:
        case L2CAP_CHANNEL_TYPE_CHANNEL_ECBM:
            return 1;
        default:
            return 0;
    }
}

static l2cap_channel_t * l2cap_get_channel_for_local_cid(uint16_t local_cid){
    if (local_cid < 0x40u) return NULL;
    return (l2cap_channel_t*) l2cap_channel_item_by_cid(local_cid);
}

static l2cap_channel_t * l2cap_get_channel_for_local_cid_and_handle(uint16_t local_cid, hci_con_handle_t con_handle){
    if (local_cid < 0x40u) return NULL;
    l2cap_channel_t * l2cap_channel = (l2cap_channel_t*) l2cap_channel_item_by_cid(local_cid);
    if (l2cap_channel == NULL)  return NULL;
    if (l2cap_channel->con_handle != con_handle) return NULL;
    return l2cap_channel;
}
#endif

#ifdef L2CAP_USES_CREDIT_BASED_CHANNELS
static l2cap_channel_t * l2cap_get_channel_for_remote_handle_and_cid(hci_con_handle_t con_handle, uint16_t remote_cid){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &l2cap_channels);
    while (btstack_linked_list_iterator_has_next(&it)){
        l2cap_fixed_channel_t * channel = (l2cap_fixed_channel_t*) btstack_linked_list_iterator_next(&it);
        if (!l2cap_is_dynamic_channel_type(channel->channel_type)) continue;
        l2cap_channel_t * dynamic_channel = (l2cap_channel_t *) channel;
        if (dynamic_channel->con_handle != con_handle) continue;
        if (dynamic_channel->remote_cid != remote_cid) continue;
        return dynamic_channel;
    }
    return NULL;
}
#endif

#ifdef L2CAP_USES_CHANNELS
uint8_t l2cap_request_can_send_now_event(uint16_t local_cid){
    l2cap_channel_t *channel = l2cap_get_channel_for_local_cid(local_cid);
    if (!channel) return L2CAP_LOCAL_CID_DOES_NOT_EXIST;
    channel->waiting_for_can_send_now = 1;
    switch (channel->channel_type){
        case L2CAP_CHANNEL_TYPE_CLASSIC:
#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
            if (channel->mode == L2CAP_CHANNEL_MODE_ENHANCED_RETRANSMISSION){
                l2cap_ertm_notify_channel_can_send(channel);
                return ERROR_CODE_SUCCESS;
            }
#endif
            l2cap_notify_channel_can_send();
            break;
#ifdef ENABLE_L2CAP_LE_CREDIT_BASED_FLOW_CONTROL_MODE
        case L2CAP_CHANNEL_TYPE_CHANNEL_CBM:
            l2cap_credit_based_notify_channel_can_send(channel);
            break;
#endif
#ifdef ENABLE_L2CAP_ENHANCED_CREDIT_BASED_FLOW_CONTROL_MODE
        case L2CAP_CHANNEL_TYPE_CHANNEL_ECBM:
            l2cap_credit_based_notify_channel_can_send(channel);
            break;
#endif
        default:
            break;
    }
    return ERROR_CODE_SUCCESS;
}

bool l2cap_can_send_packet_now(uint16_t local_cid){
    l2cap_channel_t *channel = l2cap_get_channel_for_local_cid(local_cid);
    if (!channel) return false;
    if (channel->state != L2CAP_STATE_OPEN) return false;
    switch (channel->channel_type){
        case L2CAP_CHANNEL_TYPE_CLASSIC:
#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
            if (channel->mode == L2CAP_CHANNEL_MODE_ENHANCED_RETRANSMISSION){
                return l2cap_ertm_can_store_packet_now(channel);
            }
#endif
            return hci_can_send_acl_packet_now(channel->con_handle);
#ifdef ENABLE_L2CAP_LE_CREDIT_BASED_FLOW_CONTROL_MODE
        case L2CAP_CHANNEL_TYPE_CHANNEL_CBM:
            return channel->send_sdu_buffer == NULL;
#endif
#ifdef ENABLE_L2CAP_ENHANCED_CREDIT_BASED_FLOW_CONTROL_MODE
        case L2CAP_CHANNEL_TYPE_CHANNEL_ECBM:
            return channel->send_sdu_buffer == NULL;
#endif
        default:
            return false;
    }
}

uint8_t l2cap_send(uint16_t local_cid, const uint8_t *data, uint16_t len){
    l2cap_channel_t * channel = l2cap_get_channel_for_local_cid(local_cid);
    if (!channel) {
        log_error("l2cap_send no channel for cid 0x%02x", local_cid);
        return L2CAP_LOCAL_CID_DOES_NOT_EXIST;
    }
    switch (channel->channel_type){
#ifdef ENABLE_CLASSIC
        case L2CAP_CHANNEL_TYPE_CLASSIC:
            return l2cap_classic_send(channel, data, len);
#endif
#ifdef ENABLE_L2CAP_LE_CREDIT_BASED_FLOW_CONTROL_MODE
        case L2CAP_CHANNEL_TYPE_CHANNEL_CBM:
            return l2cap_credit_based_send_data(channel, data, len);
#endif
#ifdef ENABLE_L2CAP_ENHANCED_CREDIT_BASED_FLOW_CONTROL_MODE
        case L2CAP_CHANNEL_TYPE_CHANNEL_ECBM:
            return l2cap_credit_based_send_data(channel, data, len);
#endif
        default:
            return ERROR_CODE_UNSPECIFIED_ERROR;
    }
}
#endif

#ifdef ENABLE_CLASSIC
bool l2cap_can_send_prepared_packet_now(uint16_t local_cid){
    l2cap_channel_t *channel = l2cap_get_channel_for_local_cid(local_cid);
    if (!channel) return false;
#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
    if (channel->mode == L2CAP_CHANNEL_MODE_ENHANCED_RETRANSMISSION){
        return false;
    }
#endif
    return hci_can_send_prepared_acl_packet_now(channel->con_handle);
}

#endif

#ifdef L2CAP_USES_CHANNELS
uint16_t l2cap_get_remote_mtu_for_local_cid(uint16_t local_cid){
    l2cap_channel_t * channel = l2cap_get_channel_for_local_cid(local_cid);
    if (channel) {
        return channel->remote_mtu;
    }
    return 0;
}
#endif

#ifdef ENABLE_CLASSIC
// RTX Timer only exist for dynamic channels
static l2cap_channel_t * l2cap_channel_for_rtx_timer(btstack_timer_source_t * ts){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, &l2cap_channels);
    while (btstack_linked_list_iterator_has_next(&it)){
        l2cap_channel_t * channel = (l2cap_channel_t *) btstack_linked_list_iterator_next(&it);
        if (!l2cap_is_dynamic_channel_type(channel->channel_type)) continue;
        if (&channel->rtx == ts) {
            return channel;
        }
    }
    return NULL;
}

static void l2cap_rtx_timeout(btstack_timer_source_t * ts){
    l2cap_channel_t * channel = l2cap_channel_for_rtx_timer(ts);
    if (!channel) return;

    log_info("l2cap_rtx_timeout for local cid 0x%02x", channel->local_cid);

    // "When terminating the channel, it is not necessary to send a L2CAP_DisconnectReq
    //  and enter WAIT_DISCONNECT state. Channels can be transitioned directly to the CLOSED state."
    // notify client
    l2cap_handle_channel_open_failed(channel, L2CAP_CONNECTION_RESPONSE_RESULT_RTX_TIMEOUT);

    // discard channel
    btstack_linked_list_remove(&l2cap_channels, (btstack_linked_item_t *) channel);
    l2cap_free_channel_entry(channel);
}

static uint8_t l2cap_classic_packet_boundary_flag(void){
    return (hci_non_flushable_packet_boundary_flag_supported() && (hci_automatic_flush_timeout() == 0)) ? 0x00 : 0x02;
}
#endif

#ifdef L2CAP_USES_CHANNELS
static void l2cap_stop_rtx(l2cap_channel_t * channel){
    log_info("l2cap_stop_rtx for local cid 0x%02x", channel->local_cid);
    btstack_run_loop_remove_timer(&channel->rtx);
}
#endif

static uint8_t l2cap_send_signaling_packet(hci_con_handle_t handle, uint8_t pb_flags, uint16_t cid, L2CAP_SIGNALING_COMMANDS cmd, int identifier, va_list argptr){
    if (!hci_can_send_acl_packet_now(handle)){
        log_info("l2cap_send_classic_signaling_packet, cannot send");
        return BTSTACK_ACL_BUFFERS_FULL;
    }
    hci_reserve_packet_buffer();
    uint8_t *acl_buffer = hci_get_outgoing_packet_buffer();
    uint16_t len = l2cap_create_signaling_packet(acl_buffer, handle, pb_flags, cid, cmd, identifier, argptr);
    va_end(argptr);
    return hci_send_acl_packet_buffer(len);
}

#ifdef L2CAP_USES_CREDIT_BASED_CHANNELS
static int l2cap_send_general_signaling_packet(hci_con_handle_t handle, uint16_t signaling_cid, L2CAP_SIGNALING_COMMANDS cmd, int identifier, ...){
    va_list argptr;
    va_start(argptr, identifier);
    uint8_t pb_flags = 0x00;
#ifdef ENABLE_CLASSIC
    if (signaling_cid == L2CAP_CID_SIGNALING){
        pb_flags = l2cap_classic_packet_boundary_flag();
    }
#endif
uint8_t result = l2cap_send_signaling_packet(handle, pb_flags, signaling_cid, cmd, identifier, argptr);
    va_end(argptr);
    return result;
}
#endif

#ifdef ENABLE_CLASSIC

static void l2cap_start_rtx(l2cap_channel_t * channel){
    l2cap_stop_rtx(channel);
    log_info("l2cap_start_rtx for local cid 0x%02x", channel->local_cid);
    btstack_run_loop_set_timer_handler(&channel->rtx, l2cap_rtx_timeout);
    btstack_run_loop_set_timer(&channel->rtx, L2CAP_RTX_TIMEOUT_MS);
    btstack_run_loop_add_timer(&channel->rtx);
}

static void l2cap_start_ertx(l2cap_channel_t * channel){
    log_info("l2cap_start_ertx for local cid 0x%02x", channel->local_cid);
    l2cap_stop_rtx(channel);
    btstack_run_loop_set_timer_handler(&channel->rtx, l2cap_rtx_timeout);
    btstack_run_loop_set_timer(&channel->rtx, L2CAP_ERTX_TIMEOUT_MS);
    btstack_run_loop_add_timer(&channel->rtx);
}

void l2cap_require_security_level_2_for_outgoing_sdp(void){
    l2cap_require_security_level2_for_outgoing_sdp = 1;
}

static int l2cap_security_level_0_allowed_for_PSM(uint16_t psm){
    return (psm == BLUETOOTH_PSM_SDP) && (!l2cap_require_security_level2_for_outgoing_sdp);
}

static int l2cap_send_classic_signaling_packet(hci_con_handle_t handle, L2CAP_SIGNALING_COMMANDS cmd, int identifier, ...){
    va_list argptr;
    va_start(argptr, identifier);
    uint8_t pb_flag = l2cap_classic_packet_boundary_flag();
    uint8_t result = l2cap_send_signaling_packet(handle, pb_flag, L2CAP_CID_SIGNALING, cmd, identifier, argptr);
    va_end(argptr);
    return result;
}

// assumption - only on Classic connections
// cannot be used for L2CAP ERTM
uint8_t l2cap_send_prepared(uint16_t local_cid, uint16_t len){
    
    if (!hci_is_packet_buffer_reserved()){
        log_error("l2cap_send_prepared called without reserving packet first");
        return BTSTACK_ACL_BUFFERS_FULL;
    }

    l2cap_channel_t * channel = l2cap_get_channel_for_local_cid(local_cid);
    if (!channel) {
        log_error("l2cap_send_prepared no channel for cid 0x%02x", local_cid);
        return L2CAP_LOCAL_CID_DOES_NOT_EXIST;
    }

    if (!hci_can_send_prepared_acl_packet_now(channel->con_handle)){
        log_info("l2cap_send_prepared cid 0x%02x, cannot send", local_cid);
        return BTSTACK_ACL_BUFFERS_FULL;
    }
    
    log_debug("l2cap_send_prepared cid 0x%02x, handle %u, 1 credit used", local_cid, channel->con_handle);
    
    int fcs_size = 0;

#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
    if (channel->mode == L2CAP_CHANNEL_MODE_ENHANCED_RETRANSMISSION && channel->fcs_active){
        fcs_size = 2;
    }
#endif

    // set non-flushable packet boundary flag if supported on Controller
    uint8_t *acl_buffer = hci_get_outgoing_packet_buffer();
    uint8_t packet_boundary_flag = l2cap_classic_packet_boundary_flag();
    l2cap_setup_header(acl_buffer, channel->con_handle, packet_boundary_flag, channel->remote_cid, len + fcs_size);

#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
    if (fcs_size > 0){
        // calculate FCS over l2cap data
        uint16_t fcs = crc16_calc(acl_buffer + 4, 4 + len);
        log_info("I-Frame: fcs 0x%04x", fcs);
        little_endian_store_16(acl_buffer, 8 + len, fcs);
    }
#endif

    // send
    return hci_send_acl_packet_buffer(len+8+fcs_size);
}

// assumption - only on Classic connections
static uint8_t l2cap_classic_send(l2cap_channel_t * channel, const uint8_t *data, uint16_t len){

#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
    // send in ERTM
    if (channel->mode == L2CAP_CHANNEL_MODE_ENHANCED_RETRANSMISSION){
        return l2cap_ertm_send(channel, data, len);
    }
#endif

    if (len > channel->remote_mtu){
        log_error("l2cap_send cid 0x%02x, data length exceeds remote MTU.", channel->local_cid);
        return L2CAP_DATA_LEN_EXCEEDS_REMOTE_MTU;
    }

    if (!hci_can_send_acl_packet_now(channel->con_handle)){
        log_info("l2cap_send cid 0x%02x, cannot send", channel->local_cid);
        return BTSTACK_ACL_BUFFERS_FULL;
    }

    hci_reserve_packet_buffer();
    uint8_t *acl_buffer = hci_get_outgoing_packet_buffer();
    (void)memcpy(&acl_buffer[8], data, len);
    return l2cap_send_prepared(channel->local_cid, len);
}

int l2cap_send_echo_request(hci_con_handle_t con_handle, uint8_t *data, uint16_t len){
    return l2cap_send_classic_signaling_packet(con_handle, ECHO_REQUEST, l2cap_next_sig_id(), len, data);
}

static inline void channelStateVarSetFlag(l2cap_channel_t *channel, uint16_t flag){
    channel->state_var = channel->state_var | flag;
}

static inline void channelStateVarClearFlag(l2cap_channel_t *channel, uint16_t flag){
    channel->state_var = channel->state_var & ~flag;
}
#endif


#ifdef ENABLE_BLE
static int l2cap_send_le_signaling_packet(hci_con_handle_t handle, L2CAP_SIGNALING_COMMANDS cmd, int identifier, ...){
    va_list argptr;
    va_start(argptr, identifier);
    uint8_t pb_flags = 0x00;  // First non-automatically-flushable packet of a higher layer message
    uint8_t result = l2cap_send_signaling_packet(handle, pb_flags, L2CAP_CID_SIGNALING_LE, cmd, identifier, argptr);
    va_end(argptr);
    return result;
}
#endif

uint16_t l2cap_max_mtu(void){
    return HCI_ACL_PAYLOAD_SIZE - L2CAP_HEADER_SIZE;
}

#ifdef ENABLE_BLE
uint16_t l2cap_max_le_mtu(void){
    if (l2cap_le_custom_max_mtu != 0u) return l2cap_le_custom_max_mtu;
    return l2cap_max_mtu();
}

void l2cap_set_max_le_mtu(uint16_t max_mtu){
    if (max_mtu < l2cap_max_mtu()){
        l2cap_le_custom_max_mtu = max_mtu;
    }
}
#endif

#ifdef ENABLE_CLASSIC

static uint16_t l2cap_setup_options_mtu(uint8_t * config_options, uint16_t mtu){
    config_options[0] = L2CAP_CONFIG_OPTION_TYPE_MAX_TRANSMISSION_UNIT; // MTU
    config_options[1] = 2; // len param
    little_endian_store_16(config_options, 2, mtu);
    return 4;
}

#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
static int l2cap_ertm_mode(l2cap_channel_t * channel){
    hci_connection_t * connection = hci_connection_for_handle(channel->con_handle);
    return ((connection->l2cap_state.information_state == L2CAP_INFORMATION_STATE_DONE) 
        &&  (connection->l2cap_state.extended_feature_mask & 0x08));
}
#endif

static uint16_t l2cap_setup_options_request(l2cap_channel_t * channel, uint8_t * config_options){
#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
    // use ERTM options if supported by remote and channel ready to use it
    if (l2cap_ertm_mode(channel) && channel->mode == L2CAP_CHANNEL_MODE_ENHANCED_RETRANSMISSION){
        return l2cap_setup_options_ertm_request(channel, config_options);
    }
#endif
    uint16_t mtu = channel->local_mtu;
    return l2cap_setup_options_mtu(config_options, mtu);
}

static uint16_t l2cap_setup_options_mtu_response(l2cap_channel_t * channel, uint8_t * config_options){
    uint16_t mtu = btstack_min(channel->local_mtu, channel->remote_mtu);
    return l2cap_setup_options_mtu(config_options, mtu);
}

static uint32_t l2cap_extended_features_mask(void){
    // extended features request supported, features: fixed channels, unicast connectionless data reception
    uint32_t features = 0x280;
#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
    features |= 0x0028;
#endif
#ifdef ENABLE_L2CAP_ENHANCED_CREDIT_BASED_FLOW_CONTROL_MODE
    features |= 0x0400;
#endif
    return features;
}
#endif

//
#ifdef ENABLE_CLASSIC

// returns true if channel was finalized
static bool l2cap_run_for_classic_channel(l2cap_channel_t * channel){

#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
    uint8_t  config_options[18];
#else
    uint8_t  config_options[10];
#endif

    switch (channel->state){

        case L2CAP_STATE_WAIT_INCOMING_SECURITY_LEVEL_UPDATE:
        case L2CAP_STATE_WAIT_CLIENT_ACCEPT_OR_REJECT:
            if (!hci_can_send_acl_packet_now(channel->con_handle)) return false;
            if (channel->state_var & L2CAP_CHANNEL_STATE_VAR_SEND_CONN_RESP_PEND) {
                channelStateVarClearFlag(channel, L2CAP_CHANNEL_STATE_VAR_SEND_CONN_RESP_PEND);
                channelStateVarSetFlag(channel, L2CAP_CHANNEL_STATE_VAR_SENT_CONN_RESP_PEND);
                l2cap_send_classic_signaling_packet(channel->con_handle, CONNECTION_RESPONSE, channel->remote_sig_id,
                                                    channel->local_cid, channel->remote_cid, 1, 0);
            }
            break;

        case L2CAP_STATE_WILL_SEND_CREATE_CONNECTION:
            if (!hci_can_send_command_packet_now()) break;
            // send connection request - set state first
            channel->state = L2CAP_STATE_WAIT_CONNECTION_COMPLETE;
            // BD_ADDR, Packet_Type, Page_Scan_Repetition_Mode, Reserved, Clock_Offset, Allow_Role_Switch
            (void)memcpy(l2cap_outgoing_classic_addr, channel->address, 6);
            hci_send_cmd(&hci_create_connection, channel->address, hci_usable_acl_packet_types(), 0, 0, 0, hci_get_allow_role_switch());
            break;

        case L2CAP_STATE_WILL_SEND_CONNECTION_RESPONSE_DECLINE:
            if (!hci_can_send_acl_packet_now(channel->con_handle)) return false;
            channel->state = L2CAP_STATE_INVALID;
            l2cap_send_classic_signaling_packet(channel->con_handle, CONNECTION_RESPONSE, channel->remote_sig_id,
                                                channel->local_cid, channel->remote_cid, channel->reason, 0);
            // discard channel - l2cap_finialize_channel_close without sending l2cap close event
            btstack_linked_list_remove(&l2cap_channels, (btstack_linked_item_t *) channel);
            l2cap_free_channel_entry(channel);
            channel = NULL;
            break;

        case L2CAP_STATE_WILL_SEND_CONNECTION_RESPONSE_ACCEPT:
            if (!hci_can_send_acl_packet_now(channel->con_handle)) return false;
            channel->state = L2CAP_STATE_CONFIG;
            channelStateVarSetFlag(channel, L2CAP_CHANNEL_STATE_VAR_SEND_CONF_REQ);
            l2cap_send_classic_signaling_packet(channel->con_handle, CONNECTION_RESPONSE, channel->remote_sig_id,
                                                channel->local_cid, channel->remote_cid, 0, 0);
            break;

        case L2CAP_STATE_WILL_SEND_CONNECTION_REQUEST:
            if (!hci_can_send_acl_packet_now(channel->con_handle)) return false;
            // success, start l2cap handshake
            channel->local_sig_id = l2cap_next_sig_id();
            channel->state = L2CAP_STATE_WAIT_CONNECT_RSP;
            l2cap_send_classic_signaling_packet(channel->con_handle, CONNECTION_REQUEST, channel->local_sig_id,
                                                channel->psm, channel->local_cid);
            l2cap_start_rtx(channel);
            break;

        case L2CAP_STATE_CONFIG:
            if (!hci_can_send_acl_packet_now(channel->con_handle)) return false;
#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
            // fallback to basic mode if ERTM requested but not not supported by remote
            if (channel->mode == L2CAP_CHANNEL_MODE_ENHANCED_RETRANSMISSION){
                if (!l2cap_ertm_mode(channel)){
                    l2cap_emit_simple_event_with_cid(channel, L2CAP_EVENT_ERTM_BUFFER_RELEASED);
                    channel->mode = L2CAP_CHANNEL_MODE_BASIC;
                }
            }
#endif
            if (channel->state_var & L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP){
                uint16_t flags = 0;
                channelStateVarClearFlag(channel, L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP);
                if (channel->state_var & L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP_CONT) {
                    flags = 1;
                } else {
                    channelStateVarSetFlag(channel, L2CAP_CHANNEL_STATE_VAR_SENT_CONF_RSP);
                }
                if (channel->state_var & L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP_INVALID){
                    channelStateVarClearFlag(channel, L2CAP_CHANNEL_STATE_VAR_SENT_CONF_RSP);
                    l2cap_send_classic_signaling_packet(channel->con_handle, CONFIGURE_RESPONSE, channel->remote_sig_id,
                                                        channel->remote_cid, flags, L2CAP_CONF_RESULT_UNKNOWN_OPTIONS,
                                                        1, &channel->unknown_option);
#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
                } else if (channel->state_var & L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP_REJECTED){
                    channelStateVarClearFlag(channel, L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP_REJECTED);
                    channelStateVarClearFlag(channel, L2CAP_CHANNEL_STATE_VAR_SENT_CONF_RSP);
                    uint16_t options_size = l2cap_setup_options_ertm_response(channel, config_options);
                    l2cap_send_classic_signaling_packet(channel->con_handle, CONFIGURE_RESPONSE, channel->remote_sig_id,
                                                        channel->remote_cid, flags,
                                                        L2CAP_CONF_RESULT_UNACCEPTABLE_PARAMETERS, options_size,
                                                        &config_options);
                } else if (channel->state_var & L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP_ERTM){
                    channelStateVarClearFlag(channel, L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP_ERTM);
                    channelStateVarClearFlag(channel, L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP_MTU);
                    uint16_t options_size = l2cap_setup_options_ertm_response(channel, config_options);
                    l2cap_send_classic_signaling_packet(channel->con_handle, CONFIGURE_RESPONSE, channel->remote_sig_id,
                                                        channel->remote_cid, flags, L2CAP_CONF_RESULT_SUCCESS,
                                                        options_size, &config_options);
#endif
                } else if (channel->state_var & L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP_MTU){
                    channelStateVarClearFlag(channel,L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP_MTU);
                    uint16_t options_size = l2cap_setup_options_mtu_response(channel, config_options);
                    l2cap_send_classic_signaling_packet(channel->con_handle, CONFIGURE_RESPONSE, channel->remote_sig_id,
                                                        channel->remote_cid, flags, L2CAP_CONF_RESULT_SUCCESS,
                                                        options_size, &config_options);
                } else {
                    l2cap_send_classic_signaling_packet(channel->con_handle, CONFIGURE_RESPONSE, channel->remote_sig_id,
                                                        channel->remote_cid, flags, L2CAP_CONF_RESULT_SUCCESS, 0, NULL);
                }
                channelStateVarClearFlag(channel, L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP_CONT);
            }
            else if (channel->state_var & L2CAP_CHANNEL_STATE_VAR_SEND_CONF_REQ){
                channelStateVarClearFlag(channel, L2CAP_CHANNEL_STATE_VAR_SEND_CONF_REQ);
                channelStateVarSetFlag(channel, L2CAP_CHANNEL_STATE_VAR_SENT_CONF_REQ);
                channel->local_sig_id = l2cap_next_sig_id();
                uint16_t options_size = l2cap_setup_options_request(channel, config_options);
                l2cap_send_classic_signaling_packet(channel->con_handle, CONFIGURE_REQUEST, channel->local_sig_id,
                                                    channel->remote_cid, 0, options_size, &config_options);
                l2cap_start_rtx(channel);
            }
            if (l2cap_channel_ready_for_open(channel)){
                channel->state = L2CAP_STATE_OPEN;
                l2cap_emit_channel_opened(channel, 0);  // success
            }
            break;

        case L2CAP_STATE_WILL_SEND_DISCONNECT_RESPONSE:
            if (!hci_can_send_acl_packet_now(channel->con_handle)) return false;
            channel->state = L2CAP_STATE_INVALID;
            l2cap_send_classic_signaling_packet(channel->con_handle, DISCONNECTION_RESPONSE, channel->remote_sig_id,
                                                channel->local_cid, channel->remote_cid);
            // we don't start an RTX timer for a disconnect - there's no point in closing the channel if the other side doesn't respond :)
            l2cap_finalize_channel_close(channel);  // -- remove from list
            channel = NULL;
            break;

        case L2CAP_STATE_WILL_SEND_DISCONNECT_REQUEST:
            if (!hci_can_send_acl_packet_now(channel->con_handle)) return false;
            channel->local_sig_id = l2cap_next_sig_id();
            channel->state = L2CAP_STATE_WAIT_DISCONNECT;
            l2cap_send_classic_signaling_packet(channel->con_handle, DISCONNECTION_REQUEST, channel->local_sig_id,
                                                channel->remote_cid, channel->local_cid);
            break;
        default:
            break;
    }

    // handle channel finalize on L2CAP_STATE_WILL_SEND_DISCONNECT_RESPONSE and L2CAP_STATE_WILL_SEND_CONNECTION_RESPONSE_DECLINE
    return channel == NULL;
}

#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
static void l2cap_run_for_classic_channel_ertm(l2cap_channel_t * channel){

    // ERTM mode
    if (channel->mode != L2CAP_CHANNEL_MODE_ENHANCED_RETRANSMISSION) return;

    // check if we can still send
    if (channel->con_handle == HCI_CON_HANDLE_INVALID) return;
    if (!hci_can_send_acl_packet_now(channel->con_handle)) return;

    if (channel->send_supervisor_frame_receiver_ready){
        channel->send_supervisor_frame_receiver_ready = 0;
        log_info("Send S-Frame: RR %u, final %u", channel->req_seq, channel->set_final_bit_after_packet_with_poll_bit_set);
        uint16_t control = l2cap_encanced_control_field_for_supevisor_frame( L2CAP_SUPERVISORY_FUNCTION_RR_RECEIVER_READY, 0,  channel->set_final_bit_after_packet_with_poll_bit_set, channel->req_seq);
        channel->set_final_bit_after_packet_with_poll_bit_set = 0;
        l2cap_ertm_send_supervisor_frame(channel, control);
        return;
    }
    if (channel->send_supervisor_frame_receiver_ready_poll){
        channel->send_supervisor_frame_receiver_ready_poll = 0;
        log_info("Send S-Frame: RR %u with poll=1 ", channel->req_seq);
        uint16_t control = l2cap_encanced_control_field_for_supevisor_frame( L2CAP_SUPERVISORY_FUNCTION_RR_RECEIVER_READY, 1, 0, channel->req_seq);
        l2cap_ertm_send_supervisor_frame(channel, control);
        return;
    }
    if (channel->send_supervisor_frame_receiver_not_ready){
        channel->send_supervisor_frame_receiver_not_ready = 0;
        log_info("Send S-Frame: RNR %u", channel->req_seq);
        uint16_t control = l2cap_encanced_control_field_for_supevisor_frame( L2CAP_SUPERVISORY_FUNCTION_RNR_RECEIVER_NOT_READY, 0, 0, channel->req_seq);
        l2cap_ertm_send_supervisor_frame(channel, control);
        return;
    }
    if (channel->send_supervisor_frame_reject){
        channel->send_supervisor_frame_reject = 0;
        log_info("Send S-Frame: REJ %u", channel->req_seq);
        uint16_t control = l2cap_encanced_control_field_for_supevisor_frame( L2CAP_SUPERVISORY_FUNCTION_REJ_REJECT, 0, 0, channel->req_seq);
        l2cap_ertm_send_supervisor_frame(channel, control);
        return;
    }
    if (channel->send_supervisor_frame_selective_reject){
        channel->send_supervisor_frame_selective_reject = 0;
        log_info("Send S-Frame: SREJ %u", channel->expected_tx_seq);
        uint16_t control = l2cap_encanced_control_field_for_supevisor_frame( L2CAP_SUPERVISORY_FUNCTION_SREJ_SELECTIVE_REJECT, 0, channel->set_final_bit_after_packet_with_poll_bit_set, channel->expected_tx_seq);
        channel->set_final_bit_after_packet_with_poll_bit_set = 0;
        l2cap_ertm_send_supervisor_frame(channel, control);
        return;
    }

    if (channel->srej_active){
        int i;
        for (i=0;i<channel->num_tx_buffers;i++){
            l2cap_ertm_tx_packet_state_t * tx_state = &channel->tx_packets_state[i];
            if (tx_state->retransmission_requested) {
                tx_state->retransmission_requested = 0;
                uint8_t final = channel->set_final_bit_after_packet_with_poll_bit_set;
                channel->set_final_bit_after_packet_with_poll_bit_set = 0;
                l2cap_ertm_send_information_frame(channel, i, final);
                break;
            }
        }
        if (i == channel->num_tx_buffers){
            // no retransmission request found
            channel->srej_active = 0;
        } else {
            // packet was sent
            return;
        }
    }
}
#endif /* ERTM */
#endif /* Classic */

static void l2cap_run_signaling_response(void) {

    // check pending signaling responses
    while (l2cap_signaling_responses_pending){

        hci_con_handle_t handle = l2cap_signaling_responses[0].handle;

        if (!hci_can_send_acl_packet_now(handle)) break;

        uint8_t  sig_id        = l2cap_signaling_responses[0].sig_id;
        uint8_t  response_code = l2cap_signaling_responses[0].code;
        uint16_t result        = l2cap_signaling_responses[0].data;  // CONNECTION_REQUEST, COMMAND_REJECT, REJECT_SM_PAIRING, L2CAP_CREDIT_BASED_CONNECTION_REQUEST
        uint8_t  buffer[4];                                          // REJECT_SM_PAIRING, CONFIGURE_REQUEST
        uint16_t source_cid    = l2cap_signaling_responses[0].cid;   // CONNECTION_REQUEST, REJECT_SM_PAIRING, DISCONNECT_REQUEST, CONFIGURE_REQUEST, DISCONNECT_REQUEST
#ifdef ENABLE_CLASSIC
        uint16_t dest_cid      = l2cap_signaling_responses[0].data;  // DISCONNECT_REQUEST
        uint16_t info_type     = l2cap_signaling_responses[0].data;  // INFORMATION_REQUEST
#endif
#ifdef ENABLE_L2CAP_ENHANCED_CREDIT_BASED_FLOW_CONTROL_MODE
        uint8_t num_channels = l2cap_signaling_responses[0].cid >> 8;                 // L2CAP_CREDIT_BASED_CONNECTION_REQUEST
        uint16_t signaling_cid = (uint16_t) l2cap_signaling_responses[0].cid & 0xff;  // L2CAP_CREDIT_BASED_CONNECTION_REQUEST, L2CAP_CREDIT_BASED_CONNECTION_REQUEST
#endif

        // remove first item before sending (to avoid sending response mutliple times)
        l2cap_signaling_responses_pending--;
        int i;
        for (i=0; i < l2cap_signaling_responses_pending; i++){
            (void)memcpy(&l2cap_signaling_responses[i],
                         &l2cap_signaling_responses[i + 1],
                         sizeof(l2cap_signaling_response_t));
        }

        switch (response_code){
#ifdef ENABLE_CLASSIC
            case CONNECTION_REQUEST:
                l2cap_send_classic_signaling_packet(handle, CONNECTION_RESPONSE, sig_id, source_cid, 0, result, 0);
                break;
            case CONFIGURE_REQUEST:
                little_endian_store_16(buffer, 0, source_cid);
                little_endian_store_16(buffer, 2, 0);
                l2cap_send_classic_signaling_packet(handle, COMMAND_REJECT, sig_id, result, 4, buffer);
                break;
            case DISCONNECTION_RESPONSE:
                l2cap_send_classic_signaling_packet(handle, DISCONNECTION_RESPONSE, sig_id, dest_cid, source_cid);
                break;
            case ECHO_REQUEST:
                l2cap_send_classic_signaling_packet(handle, ECHO_RESPONSE, sig_id, 0, NULL);
                break;
            case INFORMATION_REQUEST:
                switch (info_type){
                    case L2CAP_INFO_TYPE_CONNECTIONLESS_MTU: {
                        uint16_t connectionless_mtu = hci_max_acl_data_packet_length();
                        l2cap_send_classic_signaling_packet(handle, INFORMATION_RESPONSE, sig_id, info_type, 0,
                                                            sizeof(connectionless_mtu), &connectionless_mtu);
                        }
                        break;
                    case L2CAP_INFO_TYPE_EXTENDED_FEATURES_SUPPORTED: {
                        uint32_t features = l2cap_extended_features_mask();
                        l2cap_send_classic_signaling_packet(handle, INFORMATION_RESPONSE, sig_id, info_type, 0,
                                                            sizeof(features), &features);
                        }
                        break;
                    case L2CAP_INFO_TYPE_FIXED_CHANNELS_SUPPORTED: {
                            uint8_t map[8];
                            memset(map, 0, 8);
                            // L2CAP Signaling Channel + Connectionless reception
                            map[0] = (1 << L2CAP_CID_SIGNALING) | (1 << L2CAP_CID_CONNECTIONLESS_CHANNEL);
#if defined (ENABLE_EXPLICIT_BR_EDR_SECURITY_MANAGER) || (defined(ENABLE_BLE) && defined(ENABLE_CROSS_TRANSPORT_KEY_DERIVATION))
                            // BR/EDR Security Manager (bit 7) if BR/EDR Secure Connections possible
                            if (gap_secure_connections_active()){
                                map[0] |= (1 << L2CAP_CID_BR_EDR_SECURITY_MANAGER);
                            }
#endif
                            l2cap_send_classic_signaling_packet(handle, INFORMATION_RESPONSE, sig_id, info_type, 0,
                                                            sizeof(map), &map);
                        }
                        break;
                    default:
                        // all other types are not supported
                        l2cap_send_classic_signaling_packet(handle, INFORMATION_RESPONSE, sig_id, info_type, 1, 0, NULL);
                        break;
                }
                break;
            case COMMAND_REJECT:
                l2cap_send_classic_signaling_packet(handle, COMMAND_REJECT, sig_id, result, 0, NULL);
                break;
#endif
#ifdef ENABLE_BLE
            case LE_CREDIT_BASED_CONNECTION_REQUEST:
                l2cap_send_le_signaling_packet(handle, LE_CREDIT_BASED_CONNECTION_RESPONSE, sig_id, 0, 0, 0, 0, result);
                break;
            case DISCONNECTION_REQUEST:
                // Invalid CID, local cid, remote cid
                little_endian_store_16(buffer, 0, result);
                little_endian_store_16(buffer, 2, source_cid);
                l2cap_send_le_signaling_packet(handle, COMMAND_REJECT, sig_id, 0x0002, 4, buffer);
                break;
            case COMMAND_REJECT_LE:
                l2cap_send_le_signaling_packet(handle, COMMAND_REJECT, sig_id, result, 0, NULL);
                break;
#endif
#ifdef ENABLE_L2CAP_ENHANCED_CREDIT_BASED_FLOW_CONTROL_MODE
            case L2CAP_CREDIT_BASED_CONNECTION_REQUEST: {
                // send variable size array or cids with each cid being zero
                uint16_t cids[6];
                (void) memset(cids, 0xff, sizeof(cids));
                (void) memset(cids, 0x00, num_channels * sizeof(uint16_t));
                l2cap_send_general_signaling_packet(handle, signaling_cid, L2CAP_CREDIT_BASED_CONNECTION_RESPONSE,
                                                    sig_id, 0, 0, 0, result, cids);
                break;
            }

            case L2CAP_CREDIT_BASED_RECONFIGURE_REQUEST:
                l2cap_send_general_signaling_packet(handle, signaling_cid, L2CAP_CREDIT_BASED_RECONFIGURE_RESPONSE,
                                                    sig_id, result);
                break;
#endif
            case SM_PAIRING_FAILED:
                buffer[0] = SM_CODE_PAIRING_FAILED;
                buffer[1] = (uint8_t) result;
                l2cap_send_connectionless(handle, source_cid, buffer, 2);
                break;
            default:
                // should not happen
                break;
        }
    }
}

#ifdef ENABLE_CLASSIC
static bool l2ap_run_information_requests(void){
    // send l2cap information request if requested
    btstack_linked_list_iterator_t it;
    hci_connections_get_iterator(&it);
    uint8_t info_type;
    l2cap_information_state_t new_state;
    while(btstack_linked_list_iterator_has_next(&it)){
        hci_connection_t * connection = (hci_connection_t *) btstack_linked_list_iterator_next(&it);
        switch (connection->l2cap_state.information_state){
            case L2CAP_INFORMATION_STATE_W2_SEND_EXTENDED_FEATURE_REQUEST:
                info_type = L2CAP_INFO_TYPE_EXTENDED_FEATURES_SUPPORTED;
                new_state = L2CAP_INFORMATION_STATE_W4_EXTENDED_FEATURE_RESPONSE;
                break;
            case L2CAP_INFORMATION_STATE_W2_SEND_FIXED_CHANNELS_REQUEST:
                info_type = L2CAP_INFO_TYPE_FIXED_CHANNELS_SUPPORTED;
                new_state = L2CAP_INFORMATION_STATE_W4_FIXED_CHANNELS_RESPONSE;
                break;
            default:
                continue;
        }
        if (!hci_can_send_acl_packet_now(connection->con_handle)) break;

        connection->l2cap_state.information_state = new_state;
        uint8_t sig_id = l2cap_next_sig_id();
        l2cap_send_classic_signaling_packet(connection->con_handle, INFORMATION_REQUEST, sig_id, info_type);
    }
    return false;
}
#endif

#ifdef ENABLE_L2CAP_LE_CREDIT_BASED_FLOW_CONTROL_MODE
// returns true if channel has been closed
static bool l2cap_cbm_run_channel(l2cap_channel_t * channel) {
    uint16_t mps;
    bool channel_closed = false;
    // log_info("l2cap_run: channel %p, state %u, var 0x%02x", channel, channel->state, channel->state_var);
    switch (channel->state){
        case L2CAP_STATE_WILL_SEND_LE_CONNECTION_REQUEST:
            channel->state = L2CAP_STATE_WAIT_LE_CONNECTION_RESPONSE;
            // le psm, source cid, mtu, mps, initial credits
            channel->local_sig_id = l2cap_next_sig_id();
            channel->credits_incoming =  channel->new_credits_incoming;
            channel->new_credits_incoming = 0;
            channel->local_mps = btstack_min(l2cap_max_le_mtu(), channel->local_mtu);
            l2cap_send_le_signaling_packet( channel->con_handle, LE_CREDIT_BASED_CONNECTION_REQUEST,
                                            channel->local_sig_id, channel->psm, channel->local_cid, channel->local_mtu,
                                            channel->local_mps, channel->credits_incoming);
            break;
        case L2CAP_STATE_WILL_SEND_LE_CONNECTION_RESPONSE_ACCEPT:
            // TODO: support larger MPS
            channel->state = L2CAP_STATE_OPEN;
            channel->credits_incoming =  channel->new_credits_incoming;
            channel->new_credits_incoming = 0;
            mps = btstack_min(l2cap_max_le_mtu(), channel->local_mtu);
            channel->local_mps = mps;
            l2cap_send_le_signaling_packet(channel->con_handle, LE_CREDIT_BASED_CONNECTION_RESPONSE, channel->remote_sig_id, channel->local_cid, channel->local_mtu, mps, channel->credits_incoming, 0);
            // notify client
            l2cap_cbm_emit_channel_opened(channel, ERROR_CODE_SUCCESS);
            break;
        case L2CAP_STATE_WILL_SEND_LE_CONNECTION_RESPONSE_DECLINE:
            channel->state = L2CAP_STATE_INVALID;
            l2cap_send_le_signaling_packet(channel->con_handle, LE_CREDIT_BASED_CONNECTION_RESPONSE, channel->remote_sig_id, 0, 0, 0, 0, channel->reason);
            channel_closed = true;
            break;
        case L2CAP_STATE_OPEN:
            if (channel->new_credits_incoming){
                l2cap_credit_based_send_credits(channel);
            }
            break;
        case L2CAP_STATE_WILL_SEND_DISCONNECT_REQUEST:
            channel->local_sig_id = l2cap_next_sig_id();
            channel->state = L2CAP_STATE_WAIT_DISCONNECT;
            l2cap_send_le_signaling_packet( channel->con_handle, DISCONNECTION_REQUEST, channel->local_sig_id, channel->remote_cid, channel->local_cid);
            break;
        case L2CAP_STATE_WILL_SEND_DISCONNECT_RESPONSE:
            channel->state = L2CAP_STATE_INVALID;
            l2cap_send_le_signaling_packet( channel->con_handle, DISCONNECTION_RESPONSE, channel->remote_sig_id, channel->local_cid, channel->remote_cid);
            l2cap_cbm_finalize_channel_close(channel);  // -- remove from list
            break;
        default:
            break;
    }
    return channel_closed;
}

static void l2cap_cbm_run_channels(void){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &l2cap_channels);
    while (btstack_linked_list_iterator_has_next(&it)){
        l2cap_channel_t * channel = (l2cap_channel_t *) btstack_linked_list_iterator_next(&it);

        if (channel->channel_type != L2CAP_CHANNEL_TYPE_CHANNEL_CBM) continue;
        if (!hci_can_send_acl_packet_now(channel->con_handle)) continue;

        bool channel_closed = l2cap_cbm_run_channel(channel);
        if (channel_closed) {
            // discard channel - l2cap_finialize_channel_close without sending l2cap close event
            btstack_linked_list_iterator_remove(&it);
            l2cap_free_channel_entry(channel);
        }
    }
}

static inline uint8_t l2cap_cbm_status_for_result(uint16_t result) {
    switch (result) {
        case L2CAP_CBM_CONNECTION_RESULT_SUCCESS:
            return ERROR_CODE_SUCCESS;
        case L2CAP_CBM_CONNECTION_RESULT_SPSM_NOT_SUPPORTED:
            return L2CAP_CONNECTION_RESPONSE_RESULT_REFUSED_PSM;
        case L2CAP_CBM_CONNECTION_RESULT_NO_RESOURCES_AVAILABLE:
            return L2CAP_CONNECTION_RESPONSE_RESULT_REFUSED_RESOURCES;
        case L2CAP_CBM_CONNECTION_RESULT_INSUFFICIENT_AUTHENTICATION:
        case L2CAP_CBM_CONNECTION_RESULT_INSUFFICIENT_AUTHORIZATION:
        case L2CAP_CBM_CONNECTION_RESULT_ENCYRPTION_KEY_SIZE_TOO_SHORT:
        case L2CAP_CBM_CONNECTION_RESULT_INSUFFICIENT_ENCRYPTION:
            return L2CAP_CONNECTION_RESPONSE_RESULT_REFUSED_SECURITY;
        default:
            // invalid Source CID, Source CID already allocated, unacceptable parameters
            return L2CAP_CONNECTION_RESPONSE_UNKNOWN_ERROR;
    }
}

#endif

#ifdef ENABLE_L2CAP_ENHANCED_CREDIT_BASED_FLOW_CONTROL_MODE

static void l2cap_run_trigger_callback(void * context){
    UNUSED(context);
    l2cap_run();
}

static void l2cap_run_trigger(void){
    btstack_run_loop_execute_on_main_thread(&l2cap_trigger_run_registration);
}

// 11BH22222
static void l2cap_ecbm_emit_channel_opened(l2cap_channel_t *channel, uint8_t status) {
    log_info("opened ecbm channel status 0x%x addr_type %u addr %s handle 0x%x psm 0x%x local_cid 0x%x remote_cid 0x%x local_mtu %u, remote_mtu %u",
            status, channel->address_type, bd_addr_to_str(channel->address), channel->con_handle, channel->psm,
            channel->local_cid, channel->remote_cid, channel->local_mtu, channel->remote_mtu);
    uint8_t event[23];
    event[0] = L2CAP_EVENT_ECBM_CHANNEL_OPENED;
    event[1] = sizeof(event) - 2u;
    event[2] = status;
    event[3] = channel->address_type;
    reverse_bd_addr(channel->address, &event[4]);
    little_endian_store_16(event, 10, channel->con_handle);
    event[12] = (channel->state_var & L2CAP_CHANNEL_STATE_VAR_INCOMING) ? 1 : 0;
    little_endian_store_16(event, 13, channel->psm);
    little_endian_store_16(event, 15, channel->local_cid);
    little_endian_store_16(event, 17, channel->remote_cid);
    little_endian_store_16(event, 19, channel->local_mtu);
    little_endian_store_16(event, 21, channel->remote_mtu);
    hci_dump_btstack_event(event, sizeof(event));
    l2cap_dispatch_to_channel(channel, HCI_EVENT_PACKET, event, sizeof(event));
}

static void l2cap_ecbm_emit_reconfigure_complete(l2cap_channel_t *channel, uint16_t result) {
    // emit event
    uint8_t event[6];
    event[0] = L2CAP_EVENT_ECBM_RECONFIGURATION_COMPLETE;
    event[1] = sizeof(event) - 2;
    little_endian_store_16(event, 2, channel->local_cid);
    little_endian_store_16(event, 4, result);
    l2cap_dispatch_to_channel(channel, HCI_EVENT_PACKET, event, sizeof(event));
}

static void l2cap_ecbm_run_channels(void) {
    hci_con_handle_t con_handle = HCI_CON_HANDLE_INVALID;
    // num max channels + 1 for signaling pdu generator
    uint16_t cids[L2CAP_ECBM_MAX_CID_ARRAY_SIZE + 1];
    uint8_t num_cids = 0;
    uint8_t sig_id;
    uint16_t spsm;
    L2CAP_STATE matching_state;
    bool match_remote_sig_cid;
    uint8_t result = 0;
    uint16_t local_mtu;
    uint16_t initial_credits;
    uint16_t signaling_cid;
    L2CAP_STATE new_state;
    uint16_t local_mps;

    // pick first channel that needs to send a combined signaling pdu and setup collection via break
    // then collect all others that belong to the same pdu
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &l2cap_channels);
    while (btstack_linked_list_iterator_has_next(&it)) {
        l2cap_channel_t *channel = (l2cap_channel_t *) btstack_linked_list_iterator_next(&it);
        if (channel->channel_type != L2CAP_CHANNEL_TYPE_CHANNEL_ECBM) continue;
        if (con_handle == HCI_CON_HANDLE_INVALID) {
            switch (channel->state) {
                case L2CAP_STATE_WILL_SEND_ENHANCED_CONNECTION_REQUEST:
                    if (!hci_can_send_acl_packet_now(channel->con_handle)) continue;
                    local_mtu = channel->local_mtu;
                    local_mps = channel->local_mps;
                    spsm = channel->psm;
                    result = channel->reason;
                    initial_credits = channel->credits_incoming;
                    sig_id = channel->local_sig_id;
                    new_state = L2CAP_STATE_WAIT_ENHANCED_CONNECTION_RESPONSE;
                    match_remote_sig_cid = false;
                    break;
                case L2CAP_STATE_WILL_SEND_ENHANCED_CONNECTION_RESPONSE:
                    if (!hci_can_send_acl_packet_now(channel->con_handle)) continue;
                    local_mtu = channel->local_mtu;
                    local_mps = channel->local_mps;
                    initial_credits = channel->credits_incoming;
                    sig_id = channel->remote_sig_id;
                    new_state = L2CAP_STATE_OPEN;
                    match_remote_sig_cid = true;
                    break;
                case L2CAP_STATE_WILL_SEND_EHNANCED_RENEGOTIATION_REQUEST:
                    if (!hci_can_send_acl_packet_now(channel->con_handle)) continue;
                    sig_id = channel->local_sig_id;
                    local_mtu = channel->renegotiate_mtu;
                    local_mps = channel->local_mps;
                    new_state = L2CAP_STATE_WAIT_ENHANCED_RENEGOTIATION_RESPONSE;
                    match_remote_sig_cid = false;
                    break;
                case L2CAP_STATE_OPEN:
                    if (!hci_can_send_acl_packet_now(channel->con_handle)) continue;
                    if (channel->new_credits_incoming) {
                        l2cap_credit_based_send_credits(channel);
                    }
                    continue;
                case L2CAP_STATE_WILL_SEND_DISCONNECT_REQUEST:
                    if (!hci_can_send_acl_packet_now(channel->con_handle)) continue;
                    channel->local_sig_id = l2cap_next_sig_id();
                    channel->state = L2CAP_STATE_WAIT_DISCONNECT;
                    signaling_cid = channel->address_type == BD_ADDR_TYPE_ACL ? L2CAP_CID_SIGNALING : L2CAP_CID_SIGNALING_LE;
                    l2cap_send_general_signaling_packet(channel->con_handle, signaling_cid, DISCONNECTION_REQUEST,
                                                        channel->local_sig_id, channel->remote_cid, channel->local_cid);
                    continue;
                case L2CAP_STATE_WILL_SEND_DISCONNECT_RESPONSE:
                    if (!hci_can_send_acl_packet_now(channel->con_handle)) continue;
                    channel->state = L2CAP_STATE_INVALID;
                    signaling_cid = channel->address_type == BD_ADDR_TYPE_ACL ? L2CAP_CID_SIGNALING : L2CAP_CID_SIGNALING_LE;
                    l2cap_send_general_signaling_packet(channel->con_handle, signaling_cid, DISCONNECTION_RESPONSE,
                                                        channel->remote_sig_id, channel->local_cid,
                                                        channel->remote_cid);
                    l2cap_cbm_finalize_channel_close(channel);  // -- remove from list
                    continue;
                default:
                    continue;
            }

            // channel picked - setup cid array and collect info
            (void) memset(cids, 0xff, sizeof(cids));
            (void) memset(cids, 0, channel->num_cids * sizeof(uint16_t));
            matching_state = channel->state;
            con_handle = channel->con_handle;
            signaling_cid = channel->address_type == BD_ADDR_TYPE_ACL ? L2CAP_CID_SIGNALING : L2CAP_CID_SIGNALING_LE;
            num_cids = channel->num_cids;

        } else {
            // check if it matches first channel by state, con_handle, and signaling id
            if (matching_state != channel->state) continue;
            if (channel->con_handle != con_handle) continue;
            if (match_remote_sig_cid) {
                if (channel->remote_sig_id != sig_id) continue;
            } else {
                if (channel->local_sig_id != sig_id) continue;
            }
        }

        // add this cid
        cids[channel->cid_index] = channel->local_cid;

        // set new state
        channel->state = new_state;

        // handle open for L2CAP_STATE_WILL_SEND_ENHANCED_CONNECTION_RESPONSE
        if (matching_state == L2CAP_STATE_WILL_SEND_ENHANCED_CONNECTION_RESPONSE) {
            if (channel->reason == L2CAP_ECBM_CONNECTION_RESULT_ALL_SUCCESS) {
                l2cap_ecbm_emit_channel_opened(channel, ERROR_CODE_SUCCESS);
            } else {
                result = channel->reason;
                btstack_linked_list_iterator_remove(&it);
                btstack_memory_l2cap_channel_free(channel);
            }
        }
    }

    if (con_handle != HCI_CON_HANDLE_INVALID) {
        switch (matching_state) {
            case L2CAP_STATE_WILL_SEND_ENHANCED_CONNECTION_REQUEST:
                log_info("send combined connection request for %u cids", num_cids);
                l2cap_send_general_signaling_packet(con_handle, signaling_cid, L2CAP_CREDIT_BASED_CONNECTION_REQUEST,
                                                    sig_id, spsm, local_mtu, local_mps, initial_credits, cids);
                break;
            case L2CAP_STATE_WILL_SEND_ENHANCED_CONNECTION_RESPONSE:
                log_info("send combined connection response for %u cids", num_cids);
                l2cap_send_general_signaling_packet(con_handle, signaling_cid, L2CAP_CREDIT_BASED_CONNECTION_RESPONSE,
                                                    sig_id, local_mtu, local_mps, initial_credits, result, cids);
                break;
            case L2CAP_STATE_WILL_SEND_EHNANCED_RENEGOTIATION_REQUEST:
                log_info("send combined renegotiation request for %u cids", num_cids);
                l2cap_send_general_signaling_packet(con_handle, signaling_cid, L2CAP_CREDIT_BASED_RECONFIGURE_REQUEST,
                                                    sig_id, local_mtu, local_mps, cids);
                break;
            default:
                break;
        }
    }
}
#endif

// MARK: L2CAP_RUN
// process outstanding signaling tasks
static void l2cap_run(void){
    
    // log_info("l2cap_run: entered");
    l2cap_run_signaling_response();

#ifdef ENABLE_CLASSIC
    bool done = l2ap_run_information_requests();
    if (done) return;
#endif

#if defined(ENABLE_CLASSIC) || defined(ENABLE_BLE)
    btstack_linked_list_iterator_t it;
#endif

#ifdef ENABLE_CLASSIC
    btstack_linked_list_iterator_init(&it, &l2cap_channels);
    while (btstack_linked_list_iterator_has_next(&it)){

        l2cap_channel_t * channel = (l2cap_channel_t *) btstack_linked_list_iterator_next(&it);

        if (channel->channel_type != L2CAP_CHANNEL_TYPE_CLASSIC) continue;

        // log_info("l2cap_run: channel %p, state %u, var 0x%02x", channel, channel->state, channel->state_var);
        bool finalized = l2cap_run_for_classic_channel(channel);

        if (!finalized) {
#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
            l2cap_run_for_classic_channel_ertm(channel);
#endif
        }
    }
#endif

#ifdef ENABLE_L2CAP_LE_CREDIT_BASED_FLOW_CONTROL_MODE
    l2cap_cbm_run_channels();
#endif

#ifdef ENABLE_L2CAP_ENHANCED_CREDIT_BASED_FLOW_CONTROL_MODE
    l2cap_ecbm_run_channels();
#endif

#ifdef ENABLE_BLE
    // send l2cap con paramter update if necessary
    hci_connections_get_iterator(&it);
    while(btstack_linked_list_iterator_has_next(&it)){
        hci_connection_t * connection = (hci_connection_t *) btstack_linked_list_iterator_next(&it);
        if (!hci_is_le_connection_type(connection->address_type)) continue;
        if (!hci_can_send_acl_packet_now(connection->con_handle)) continue;
        switch (connection->le_con_parameter_update_state){
            case CON_PARAMETER_UPDATE_SEND_REQUEST:
                connection->le_con_parameter_update_state = CON_PARAMETER_UPDATE_NONE;
                l2cap_send_le_signaling_packet(connection->con_handle, CONNECTION_PARAMETER_UPDATE_REQUEST, l2cap_next_sig_id(),
                                               connection->le_conn_interval_min, connection->le_conn_interval_max, connection->le_conn_latency, connection->le_supervision_timeout);
                break;
            case CON_PARAMETER_UPDATE_SEND_RESPONSE:
                connection->le_con_parameter_update_state = CON_PARAMETER_UPDATE_CHANGE_HCI_CON_PARAMETERS;
                l2cap_send_le_signaling_packet(connection->con_handle, CONNECTION_PARAMETER_UPDATE_RESPONSE, connection->le_con_param_update_identifier, 0);
                break;
            case CON_PARAMETER_UPDATE_DENY:
                connection->le_con_parameter_update_state = CON_PARAMETER_UPDATE_NONE;
                l2cap_send_le_signaling_packet(connection->con_handle, CONNECTION_PARAMETER_UPDATE_RESPONSE, connection->le_con_param_update_identifier, 1);
                break;
            default:
                break;
        }
    }
#endif

    if (l2cap_call_notify_channel_in_run){
        l2cap_call_notify_channel_in_run = false;
        l2cap_notify_channel_can_send();
    }

    // log_info("l2cap_run: exit");
}

#ifdef ENABLE_CLASSIC
static void l2cap_ready_to_connect(l2cap_channel_t * channel){

#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
    // assumption: outgoing connection
    if (channel->mode == L2CAP_CHANNEL_MODE_ENHANCED_RETRANSMISSION){
        // ERTM requested: trigger information request if not already started then wait for response
        hci_connection_t * connection = hci_connection_for_handle(channel->con_handle);
        switch (connection->l2cap_state.information_state){
            case L2CAP_INFORMATION_STATE_DONE:
                break;
            case L2CAP_INFORMATION_STATE_IDLE:
                connection->l2cap_state.information_state = L2CAP_INFORMATION_STATE_W2_SEND_EXTENDED_FEATURE_REQUEST;
                /* fall through */
            default:
                channel->state = L2CAP_STATE_WAIT_OUTGOING_EXTENDED_FEATURES;
                return;
        }
    }
#endif

    // fine, go ahead
    channel->state = L2CAP_STATE_WILL_SEND_CONNECTION_REQUEST;
}

static void l2cap_handle_connection_complete(hci_con_handle_t con_handle, l2cap_channel_t * channel){
    if ((channel->state == L2CAP_STATE_WAIT_CONNECTION_COMPLETE) || (channel->state == L2CAP_STATE_WILL_SEND_CREATE_CONNECTION)) {
        log_info("connection complete con_handle %04x - for channel %p cid 0x%04x", (int) con_handle, channel, channel->local_cid);
        channel->con_handle = con_handle;
        // query remote features if pairing is required
        if (channel->required_security_level > LEVEL_0){
            channel->state = L2CAP_STATE_WAIT_REMOTE_SUPPORTED_FEATURES;
            hci_remote_features_query(con_handle);
        } else {
            l2cap_ready_to_connect(channel);
        }
    }
}

static void l2cap_handle_remote_supported_features_received(l2cap_channel_t * channel){
    if (channel->state != L2CAP_STATE_WAIT_REMOTE_SUPPORTED_FEATURES) return;

    bool security_required = channel->required_security_level > LEVEL_0;

    if ((channel->state_var & L2CAP_CHANNEL_STATE_VAR_INCOMING) != 0){

        // Core V5.2, Vol 3, Part C, 5.2.2.2 - Security Mode 4
        //   When a remote device attempts to access a service offered by a Bluetooth device that is in security mode 4
        //   and a sufficient link key exists and authentication has not been performed the local device shall authenticate
        //   the remote device and enable encryption after the channel establishment request is received but before a channel
        //   establishment confirmation (L2CAP_ConnectRsp with result code of 0x0000 or a higher-level channel establishment
        //   confirmation such as that of RFCOMM) is sent.

        //   If the remote device has indicated support for Secure Simple Pairing, a channel establishment request is
        //   received for a service other than SDP, and encryption has not yet been enabled, then the local device shall
        //   disconnect the ACL link with error code 0x05 - Authentication Failure.

        // => Disconnect if l2cap request received in mode 4 and ssp supported, non-sdp psm, not encrypted, no link key available
        if ((gap_get_security_mode() == GAP_SECURITY_MODE_4)
        && gap_ssp_supported_on_both_sides(channel->con_handle)
        && (channel->psm != PSM_SDP)
        && (gap_encryption_key_size(channel->con_handle) == 0)){
            hci_disconnect_security_block(channel->con_handle);
            return;
        }

        // incoming: assert security requirements
        channel->state = L2CAP_STATE_WAIT_INCOMING_SECURITY_LEVEL_UPDATE;
        if (channel->required_security_level <= gap_security_level(channel->con_handle)){
            l2cap_handle_security_level_incoming_sufficient(channel);
        } else {
            // send connection pending if not already done
            if ((channel->state_var & L2CAP_CHANNEL_STATE_VAR_SENT_CONN_RESP_PEND) == 0){
                channel->state_var |= L2CAP_CHANNEL_STATE_VAR_SEND_CONN_RESP_PEND;
            }
            gap_request_security_level(channel->con_handle, channel->required_security_level);
        }
    } else {
        // outgoing: we have been waiting for remote supported features
        if (security_required){
            // request security level
            channel->state = L2CAP_STATE_WAIT_OUTGOING_SECURITY_LEVEL_UPDATE;
            gap_request_security_level(channel->con_handle, channel->required_security_level);
        } else {
            l2cap_ready_to_connect(channel);
        }
    }
}
#endif

#ifdef L2CAP_USES_CHANNELS
static l2cap_channel_t * l2cap_create_channel_entry(btstack_packet_handler_t packet_handler, l2cap_channel_type_t channel_type, bd_addr_t address, bd_addr_type_t address_type, 
    uint16_t psm, uint16_t local_mtu, gap_security_level_t security_level){

    l2cap_channel_t * channel = btstack_memory_l2cap_channel_get();
    if (!channel) {
        return NULL;
    }
        
    // fill in 
    channel->packet_handler = packet_handler;
    channel->channel_type   = channel_type;
    bd_addr_copy(channel->address, address);
    channel->address_type = address_type;
    channel->psm = psm;
    channel->local_mtu  = local_mtu;
    channel->remote_mtu = L2CAP_DEFAULT_MTU;
    channel->required_security_level = security_level;

    // 
    channel->local_cid = l2cap_next_local_cid();
    channel->con_handle = HCI_CON_HANDLE_INVALID;

    // set initial state
    channel->state = L2CAP_STATE_WILL_SEND_CREATE_CONNECTION;
    channel->state_var = L2CAP_CHANNEL_STATE_VAR_NONE;
    channel->remote_sig_id = L2CAP_SIG_ID_INVALID;
    channel->local_sig_id = L2CAP_SIG_ID_INVALID;

    log_info("create channel %p, local_cid 0x%04x", (void*)channel, channel->local_cid);

    return channel;
}

static void l2cap_free_channel_entry(l2cap_channel_t * channel){
    log_info("free channel %p, local_cid 0x%04x", (void*)channel, channel->local_cid);
    // assert all timers are stopped
    l2cap_stop_rtx(channel);
#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
    l2cap_ertm_stop_retransmission_timer(channel);
    l2cap_ertm_stop_monitor_timer(channel);
#endif
    // free  memory
    btstack_memory_l2cap_channel_free(channel);
}
#endif

#ifdef ENABLE_CLASSIC

/** 
 * @brief Creates L2CAP channel to the PSM of a remote device with baseband address. A new baseband connection will be initiated if necessary.
 * @param packet_handler
 * @param address
 * @param psm
 * @param mtu
 * @param local_cid
 */

uint8_t l2cap_create_channel(btstack_packet_handler_t channel_packet_handler, bd_addr_t address, uint16_t psm, uint16_t mtu, uint16_t * out_local_cid){
    // limit MTU to the size of our outgoing HCI buffer
    uint16_t local_mtu = btstack_min(mtu, l2cap_max_mtu());

	// determine security level based on psm
	const gap_security_level_t security_level = l2cap_security_level_0_allowed_for_PSM(psm) ? LEVEL_0 : gap_get_security_level();
	log_info("create channel addr %s psm 0x%x mtu %u -> local mtu %u, sec level %u", bd_addr_to_str(address), psm, mtu, local_mtu, (int) security_level);

    l2cap_channel_t * channel = l2cap_create_channel_entry(channel_packet_handler, L2CAP_CHANNEL_TYPE_CLASSIC, address, BD_ADDR_TYPE_ACL, psm, local_mtu, security_level);
    if (!channel) {
        return BTSTACK_MEMORY_ALLOC_FAILED;
    }

#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
    channel->mode = L2CAP_CHANNEL_MODE_BASIC;
#endif    

    // add to connections list
    btstack_linked_list_add_tail(&l2cap_channels, (btstack_linked_item_t *) channel);

    // store local_cid
    if (out_local_cid){
       *out_local_cid = channel->local_cid;
    }

	// state: L2CAP_STATE_WILL_SEND_CREATE_CONNECTION

    // check if hci connection is already usable,
    hci_connection_t * conn = hci_connection_for_bd_addr_and_type(address, BD_ADDR_TYPE_ACL);
    if (conn && conn->state == OPEN){
    	// simulate connection complete
	    l2cap_handle_connection_complete(conn->con_handle, channel);

	    // state: L2CAP_STATE_WAIT_REMOTE_SUPPORTED_FEATURES or L2CAP_STATE_WILL_SEND_CONNECTION_REQUEST

        // simulate if remote supported features if requested and already received
        if ((channel->state == L2CAP_STATE_WAIT_REMOTE_SUPPORTED_FEATURES) && hci_remote_features_available(conn->con_handle)) {
        	// simulate remote features received
            l2cap_handle_remote_supported_features_received(channel);
        }
    }

    l2cap_run();

    return ERROR_CODE_SUCCESS;
}

static void l2cap_handle_connection_failed_for_addr(bd_addr_t address, uint8_t status){
    // mark all channels before emitting open events as these could trigger new connetion requests to the same device
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &l2cap_channels);
    while (btstack_linked_list_iterator_has_next(&it)){
        l2cap_channel_t * channel = (l2cap_channel_t *) btstack_linked_list_iterator_next(&it);
        if (!l2cap_is_dynamic_channel_type(channel->channel_type)) continue;
        if (bd_addr_cmp( channel->address, address) != 0) continue;
        // channel for this address found
        switch (channel->state){
            case L2CAP_STATE_WAIT_CONNECTION_COMPLETE:
            case L2CAP_STATE_WILL_SEND_CREATE_CONNECTION:
                channel->state = L2CAP_STATE_EMIT_OPEN_FAILED_AND_DISCARD;
                break;
            default:
                break;
        }
    }
    // emit and free marked entries. restart loop to deal with list changes
    int done = 0;
    while (!done) {
        done = 1;
        btstack_linked_list_iterator_init(&it, &l2cap_channels);
        while (btstack_linked_list_iterator_has_next(&it)){
            l2cap_channel_t * channel = (l2cap_channel_t *) btstack_linked_list_iterator_next(&it);
            if (!l2cap_is_dynamic_channel_type(channel->channel_type)) continue;
            if (channel->state == L2CAP_STATE_EMIT_OPEN_FAILED_AND_DISCARD){
                done = 0;
                // failure, forward error code
                l2cap_handle_channel_open_failed(channel, status);
                // discard channel
                btstack_linked_list_remove(&l2cap_channels, (btstack_linked_item_t *) channel);
                l2cap_free_channel_entry(channel);
                break;
            }
        }
    }

}

static void l2cap_handle_connection_success_for_addr(bd_addr_t address, hci_con_handle_t handle){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &l2cap_channels);
    while (btstack_linked_list_iterator_has_next(&it)){
        l2cap_channel_t * channel = (l2cap_channel_t *) btstack_linked_list_iterator_next(&it);
        if (!l2cap_is_dynamic_channel_type(channel->channel_type)) continue;
        if ( ! bd_addr_cmp( channel->address, address) ){
            l2cap_handle_connection_complete(handle, channel);
        }
    }

#ifdef ENABLE_L2CAP_INFORMATION_REQUESTS_ON_CONNECT
    // trigger query of extended features and fixed channels right away (instead of later)
    hci_connection_t * hci_connection = hci_connection_for_handle(handle);
    btstack_assert(hci_connection != NULL);
    hci_connection->l2cap_state.information_state = L2CAP_INFORMATION_STATE_W2_SEND_EXTENDED_FEATURE_REQUEST;
#endif

    // process
    l2cap_run();
}
#endif

static bool l2cap_channel_ready_to_send(l2cap_channel_t * channel){
    switch (channel->channel_type){
#ifdef ENABLE_CLASSIC
        case L2CAP_CHANNEL_TYPE_CLASSIC:
            if (channel->state != L2CAP_STATE_OPEN) return false;
#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
            // send if we have more data and remote windows isn't full yet
            if (channel->mode == L2CAP_CHANNEL_MODE_ENHANCED_RETRANSMISSION) {
                if (channel->unacked_frames >= btstack_min(channel->num_stored_tx_frames, channel->remote_tx_window_size)) return false;
                return hci_can_send_acl_packet_now(channel->con_handle);
            }
#endif
            if (!channel->waiting_for_can_send_now) return false;
            return hci_can_send_acl_packet_now(channel->con_handle);
        case L2CAP_CHANNEL_TYPE_FIXED_CLASSIC:
        case L2CAP_CHANNEL_TYPE_CONNECTIONLESS:
            if (!channel->waiting_for_can_send_now) return false;
            return hci_can_send_acl_classic_packet_now();
#endif
#ifdef ENABLE_BLE
        case L2CAP_CHANNEL_TYPE_FIXED_LE:
            if (!channel->waiting_for_can_send_now) return false;
            return hci_can_send_acl_le_packet_now();
#ifdef ENABLE_L2CAP_LE_CREDIT_BASED_FLOW_CONTROL_MODE
        case L2CAP_CHANNEL_TYPE_CHANNEL_CBM:
            if (channel->state != L2CAP_STATE_OPEN) return false;
            if (channel->send_sdu_buffer == NULL) return false;
            if (channel->credits_outgoing == 0u) return false;
            return hci_can_send_acl_packet_now(channel->con_handle);
#endif
#endif
#ifdef ENABLE_L2CAP_ENHANCED_CREDIT_BASED_FLOW_CONTROL_MODE
        case L2CAP_CHANNEL_TYPE_CHANNEL_ECBM:
            if (channel->state != L2CAP_STATE_OPEN) return false;
            if (channel->send_sdu_buffer == NULL) return false;
            if (channel->credits_outgoing == 0u) return false;
            return hci_can_send_acl_packet_now(channel->con_handle);
#endif
        default:
            return false;
    }
}

static void l2cap_channel_trigger_send(l2cap_channel_t * channel){
    switch (channel->channel_type){
#ifdef ENABLE_CLASSIC
        case L2CAP_CHANNEL_TYPE_CLASSIC:
#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
            if (channel->mode == L2CAP_CHANNEL_MODE_ENHANCED_RETRANSMISSION) {
                l2cap_ertm_channel_send_information_frame(channel);
                return;
            }
#endif
            channel->waiting_for_can_send_now = 0;
            l2cap_emit_can_send_now(channel->packet_handler, channel->local_cid);
            break;
        case L2CAP_CHANNEL_TYPE_CONNECTIONLESS:
        case L2CAP_CHANNEL_TYPE_FIXED_CLASSIC:
            channel->waiting_for_can_send_now = 0;
            l2cap_emit_can_send_now(channel->packet_handler, channel->local_cid);
            break;
#endif
#ifdef ENABLE_BLE
        case L2CAP_CHANNEL_TYPE_FIXED_LE:
            channel->waiting_for_can_send_now = 0;
            l2cap_emit_can_send_now(channel->packet_handler, channel->local_cid);
            break;
#endif
#ifdef ENABLE_L2CAP_LE_CREDIT_BASED_FLOW_CONTROL_MODE
        case L2CAP_CHANNEL_TYPE_CHANNEL_CBM:
            l2cap_credit_based_send_pdu(channel);
            break;
#endif
#ifdef ENABLE_L2CAP_ENHANCED_CREDIT_BASED_FLOW_CONTROL_MODE
        case L2CAP_CHANNEL_TYPE_CHANNEL_ECBM:
            l2cap_credit_based_send_pdu(channel);
            break;
#endif
        default:
            btstack_unreachable();
            break;
    }
}

static void l2cap_notify_channel_can_send(void){
    bool done = false;
    while (!done){
        done = true;
        btstack_linked_list_iterator_t it;
        btstack_linked_list_iterator_init(&it, &l2cap_channels);
        while (btstack_linked_list_iterator_has_next(&it)){
            l2cap_channel_t * channel = (l2cap_channel_t *) btstack_linked_list_iterator_next(&it);
            bool ready = l2cap_channel_ready_to_send(channel);
            if (!ready) continue;

            // requeue channel for fairness
            btstack_linked_list_remove(&l2cap_channels, (btstack_linked_item_t *) channel);
            btstack_linked_list_add_tail(&l2cap_channels, (btstack_linked_item_t *) channel);

            // trigger sending
            l2cap_channel_trigger_send(channel);

            // exit inner loop as we just broke the iterator, but try again
            done = false;
            break;
        }
    }
}

#ifdef L2CAP_USES_CHANNELS

uint8_t l2cap_disconnect(uint16_t local_cid){
    l2cap_channel_t * channel = l2cap_get_channel_for_local_cid(local_cid);
    if (!channel) {
        return L2CAP_LOCAL_CID_DOES_NOT_EXIST;
    }
    channel->state = L2CAP_STATE_WILL_SEND_DISCONNECT_REQUEST;
    l2cap_run();
    return ERROR_CODE_SUCCESS;
}

static int l2cap_send_open_failed_on_hci_disconnect(l2cap_channel_t * channel){
    // open cannot fail for for incoming connections
    if (channel->state_var & L2CAP_CHANNEL_STATE_VAR_INCOMING) return 0;

    // check state
    switch (channel->state){
        case L2CAP_STATE_WILL_SEND_CREATE_CONNECTION:
        case L2CAP_STATE_WAIT_CONNECTION_COMPLETE:
        case L2CAP_STATE_WAIT_REMOTE_SUPPORTED_FEATURES:
        case L2CAP_STATE_WAIT_OUTGOING_SECURITY_LEVEL_UPDATE:
        case L2CAP_STATE_WAIT_CLIENT_ACCEPT_OR_REJECT:
        case L2CAP_STATE_WAIT_OUTGOING_EXTENDED_FEATURES:
        case L2CAP_STATE_WAIT_CONNECT_RSP:
        case L2CAP_STATE_CONFIG:
        case L2CAP_STATE_WILL_SEND_CONNECTION_REQUEST:
        case L2CAP_STATE_WILL_SEND_LE_CONNECTION_REQUEST:
        case L2CAP_STATE_WAIT_LE_CONNECTION_RESPONSE:
        case L2CAP_STATE_EMIT_OPEN_FAILED_AND_DISCARD:
            return 1;

        case L2CAP_STATE_OPEN:
        case L2CAP_STATE_CLOSED:
        case L2CAP_STATE_WAIT_INCOMING_EXTENDED_FEATURES:
        case L2CAP_STATE_WAIT_DISCONNECT:
        case L2CAP_STATE_WILL_SEND_CONNECTION_RESPONSE_INSUFFICIENT_SECURITY:
        case L2CAP_STATE_WILL_SEND_CONNECTION_RESPONSE_DECLINE:
        case L2CAP_STATE_WILL_SEND_CONNECTION_RESPONSE_ACCEPT:
        case L2CAP_STATE_WILL_SEND_DISCONNECT_REQUEST:
        case L2CAP_STATE_WILL_SEND_DISCONNECT_RESPONSE:
        case L2CAP_STATE_WILL_SEND_LE_CONNECTION_RESPONSE_DECLINE:
        case L2CAP_STATE_WILL_SEND_LE_CONNECTION_RESPONSE_ACCEPT:
        case L2CAP_STATE_INVALID:
        case L2CAP_STATE_WAIT_INCOMING_SECURITY_LEVEL_UPDATE:
            return 0;

        default:
            // get a "warning" about new states
            btstack_assert(false);
            return 0;
    }
}
#endif

#ifdef ENABLE_CLASSIC
static void l2cap_handle_hci_disconnect_event(l2cap_channel_t * channel){
    if (l2cap_send_open_failed_on_hci_disconnect(channel)){
        l2cap_handle_channel_open_failed(channel, L2CAP_CONNECTION_BASEBAND_DISCONNECT);
    } else {
        l2cap_handle_channel_closed(channel);
    }
    l2cap_free_channel_entry(channel);
}
#endif

#ifdef ENABLE_L2CAP_LE_CREDIT_BASED_FLOW_CONTROL_MODE
static void l2cap_handle_hci_le_disconnect_event(l2cap_channel_t * channel){
    if (l2cap_send_open_failed_on_hci_disconnect(channel)){
        l2cap_cbm_emit_channel_opened(channel, L2CAP_CONNECTION_BASEBAND_DISCONNECT);
    } else {
        l2cap_emit_channel_closed(channel);
    }
    l2cap_free_channel_entry(channel);
}
#endif

#ifdef ENABLE_CLASSIC
static void l2cap_check_classic_timeout(hci_con_handle_t handle){
    if (gap_get_connection_type(handle) != GAP_CONNECTION_ACL) {
        return;
    }
    if (hci_authentication_active_for_handle(handle)) {
        return;
    }
    bool hci_con_used = false;
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &l2cap_channels);
    while (btstack_linked_list_iterator_has_next(&it)){
        l2cap_channel_t * channel = (l2cap_channel_t *) btstack_linked_list_iterator_next(&it);
        if (!l2cap_is_dynamic_channel_type(channel->channel_type)) continue;
        if (channel->con_handle != handle) continue;
        hci_con_used = true;
        break;
    }
    if (hci_con_used) {
        return;
    }
    if (!hci_can_send_command_packet_now()) {
        return;
    }
    hci_send_cmd(&hci_disconnect, handle, 0x13); // remote closed connection
}

static void l2cap_handle_features_complete(hci_con_handle_t handle){
    if (hci_remote_features_available(handle) == false){
        return;
    }
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &l2cap_channels);
    while (btstack_linked_list_iterator_has_next(&it)){
        l2cap_channel_t * channel = (l2cap_channel_t *) btstack_linked_list_iterator_next(&it);
        if (!l2cap_is_dynamic_channel_type(channel->channel_type)) continue;
        if (channel->con_handle != handle) continue;
        log_info("remote supported features, channel %p, cid %04x - state %u", channel, channel->local_cid, channel->state);
        l2cap_handle_remote_supported_features_received(channel);
    }
}

static void l2cap_handle_security_level_incoming_sufficient(l2cap_channel_t * channel){
    channel->state = L2CAP_STATE_WAIT_CLIENT_ACCEPT_OR_REJECT;
    l2cap_emit_incoming_connection(channel);
}

static void l2cap_handle_security_level(hci_con_handle_t handle, gap_security_level_t actual_level){
    log_info("security level update for handle 0x%04x", handle);

    // trigger l2cap information requests
    if (actual_level > LEVEL_0){
        hci_connection_t * hci_connection = hci_connection_for_handle(handle);
        btstack_assert(hci_connection != NULL);
        if (hci_connection->l2cap_state.information_state == L2CAP_INFORMATION_STATE_IDLE){
            hci_connection->l2cap_state.information_state = L2CAP_INFORMATION_STATE_W2_SEND_EXTENDED_FEATURE_REQUEST;
        }
    }

    bool done = false;
    while (!done){
        done = true;
        btstack_linked_list_iterator_t it;
        btstack_linked_list_iterator_init(&it, &l2cap_channels);
        while (btstack_linked_list_iterator_has_next(&it)){
            l2cap_channel_t * channel = (l2cap_channel_t *) btstack_linked_list_iterator_next(&it);
            if (!l2cap_is_dynamic_channel_type(channel->channel_type)) continue;
            if (channel->con_handle != handle) continue;

            gap_security_level_t required_level = channel->required_security_level;

            log_info("channel %p, cid %04x - state %u: actual %u >= required %u?", channel, channel->local_cid, channel->state, actual_level, required_level);

            switch (channel->state){
                case L2CAP_STATE_WAIT_INCOMING_SECURITY_LEVEL_UPDATE:
                    if (actual_level >= required_level){
                        l2cap_handle_security_level_incoming_sufficient(channel);
                    } else {
                        channel->reason = L2CAP_CONNECTION_RESULT_SECURITY_BLOCK;
                        channel->state = L2CAP_STATE_WILL_SEND_CONNECTION_RESPONSE_DECLINE;
                    }
                    break;

                case L2CAP_STATE_WAIT_OUTGOING_SECURITY_LEVEL_UPDATE:
                    if (actual_level >= required_level){
                        l2cap_ready_to_connect(channel);
                    } else {
                        // security level insufficient, report error and free channel
                        l2cap_handle_channel_open_failed(channel, L2CAP_CONNECTION_RESPONSE_RESULT_REFUSED_SECURITY);
                        btstack_linked_list_remove(&l2cap_channels, (btstack_linked_item_t  *) channel);
                        l2cap_free_channel_entry(channel);
                    }
                    break;

                default:
                    break;
            }
        }
    }
}
#endif

#ifdef L2CAP_USES_CHANNELS
static void l2cap_handle_disconnection_complete(hci_con_handle_t handle){
    // collect channels to close
    btstack_linked_list_t channels_to_close = NULL;
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &l2cap_channels);
    while (btstack_linked_list_iterator_has_next(&it)) {
        l2cap_channel_t *channel = (l2cap_channel_t *) btstack_linked_list_iterator_next(&it);
        if (!l2cap_is_dynamic_channel_type(channel->channel_type)) continue;
        if (channel->con_handle != handle) continue;
        btstack_linked_list_iterator_remove(&it);
        btstack_linked_list_add(&channels_to_close, (btstack_linked_item_t *) channel);
    }
    // send l2cap open failed or closed events for all channels on this handle and free them
    btstack_linked_list_iterator_init(&it, &channels_to_close);
    while (btstack_linked_list_iterator_has_next(&it)) {
        l2cap_channel_t *channel = (l2cap_channel_t *) btstack_linked_list_iterator_next(&it);
        btstack_linked_list_iterator_remove(&it);
        switch(channel->channel_type){
#ifdef ENABLE_CLASSIC
            case L2CAP_CHANNEL_TYPE_CLASSIC:
                l2cap_handle_hci_disconnect_event(channel);
                break;
#endif
#ifdef ENABLE_L2CAP_LE_CREDIT_BASED_FLOW_CONTROL_MODE
            case L2CAP_CHANNEL_TYPE_CHANNEL_CBM:
                l2cap_handle_hci_le_disconnect_event(channel);
                break;
#endif
#ifdef ENABLE_L2CAP_ENHANCED_CREDIT_BASED_FLOW_CONTROL_MODE
            case L2CAP_CHANNEL_TYPE_CHANNEL_ECBM:
                switch (channel->state) {
                    case L2CAP_STATE_WILL_SEND_ENHANCED_CONNECTION_REQUEST:
                    case L2CAP_STATE_WAIT_ENHANCED_CONNECTION_RESPONSE:
                        // emit open failed if disconnected before connection complete
                        l2cap_ecbm_emit_channel_opened(channel, L2CAP_CONNECTION_BASEBAND_DISCONNECT);
                        break;
                    case L2CAP_STATE_WILL_SEND_EHNANCED_RENEGOTIATION_REQUEST:
                    case L2CAP_STATE_WAIT_ENHANCED_RENEGOTIATION_RESPONSE:
                        // emit reconfigure failure - result = 0xffff
                        l2cap_ecbm_emit_reconfigure_complete(channel, 0xffff);
                        break;
                    case L2CAP_STATE_WAIT_INCOMING_SECURITY_LEVEL_UPDATE:
                        // no incoming event has been sent to higher layer, no need to follow up
                        break;
                    default:
                        l2cap_emit_simple_event_with_cid(channel, L2CAP_EVENT_CHANNEL_CLOSED);
                        break;
                }
                l2cap_free_channel_entry(channel);
                break;
#endif
            default:
                break;
        }
    }
}
#endif

static void l2cap_hci_event_handler(uint8_t packet_type, uint16_t cid, uint8_t *packet, uint16_t size){

    UNUSED(packet_type); // ok: registered with hci_event_callback_registration
    UNUSED(cid);         // ok: there is no channel
    UNUSED(size);        // ok: fixed format events read from HCI buffer
    
#ifdef ENABLE_CLASSIC
    bd_addr_t address;
    gap_security_level_t security_level;
#endif
#ifdef L2CAP_USES_CHANNELS
    hci_con_handle_t handle;
#endif

    switch(hci_event_packet_get_type(packet)){
            
        // Notify channel packet handler if they can send now
        case HCI_EVENT_TRANSPORT_PACKET_SENT:
        case HCI_EVENT_NUMBER_OF_COMPLETED_PACKETS:
        case BTSTACK_EVENT_NR_CONNECTIONS_CHANGED:
#ifdef ENABLE_TESTING_SUPPORT
        case HCI_EVENT_NOP:
#endif
            l2cap_call_notify_channel_in_run = true;
            break;

        case HCI_EVENT_COMMAND_STATUS:
#ifdef ENABLE_CLASSIC
            // check command status for create connection for errors
            if (hci_event_command_status_get_command_opcode(packet) == HCI_OPCODE_HCI_CREATE_CONNECTION){
                // cache outgoing address and reset
                (void)memcpy(address, l2cap_outgoing_classic_addr, 6);
                memset(l2cap_outgoing_classic_addr, 0, 6);
                // error => outgoing connection failed
                uint8_t status = hci_event_command_status_get_status(packet);
                if (status){
                    l2cap_handle_connection_failed_for_addr(address, status);
                }
            }
#endif
            l2cap_run();    // try sending signaling packets first
            break;

#ifdef ENABLE_CLASSIC
        // handle connection complete events
        case HCI_EVENT_CONNECTION_COMPLETE:
            reverse_bd_addr(&packet[5], address);
            if (packet[2] == 0){
                handle = little_endian_read_16(packet, 3);
                l2cap_handle_connection_success_for_addr(address, handle);
            } else {
                l2cap_handle_connection_failed_for_addr(address, packet[2]);
            }
            break;

        // handle successful create connection cancel command
        case HCI_EVENT_COMMAND_COMPLETE:
            if (hci_event_command_complete_get_command_opcode(packet) == HCI_OPCODE_HCI_CREATE_CONNECTION_CANCEL) {
                if (packet[5] == 0){
                    reverse_bd_addr(&packet[6], address);
                    // CONNECTION TERMINATED BY LOCAL HOST (0X16)
                    l2cap_handle_connection_failed_for_addr(address, 0x16);
                }
            }
            l2cap_run();    // try sending signaling packets first
            break;
#endif
            
#ifdef L2CAP_USES_CHANNELS
        // handle disconnection complete events
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            handle = little_endian_read_16(packet, 3);
            l2cap_handle_disconnection_complete(handle);
            break;
#endif

        // HCI Connection Timeouts
#ifdef ENABLE_CLASSIC
        case L2CAP_EVENT_TIMEOUT_CHECK:
            handle = little_endian_read_16(packet, 2);
            l2cap_check_classic_timeout(handle);
            break;

        case HCI_EVENT_READ_REMOTE_SUPPORTED_FEATURES_COMPLETE:
        case HCI_EVENT_READ_REMOTE_EXTENDED_FEATURES_COMPLETE:
            handle = little_endian_read_16(packet, 3);
            l2cap_handle_features_complete(handle);
            break;

        case GAP_EVENT_SECURITY_LEVEL:
            handle = little_endian_read_16(packet, 2);
            security_level = (gap_security_level_t) packet[4];
            l2cap_handle_security_level(handle, security_level);
            break;
#endif
        default:
            break;
    }
    
    l2cap_run();
}

static void l2cap_register_signaling_response(hci_con_handle_t handle, uint8_t code, uint8_t sig_id, uint16_t cid, uint16_t data){
    // Vol 3, Part A, 4.3: "The DCID and SCID fields shall be ignored when the result field indicates the connection was refused."
    if (l2cap_signaling_responses_pending < NR_PENDING_SIGNALING_RESPONSES) {
        l2cap_signaling_responses[l2cap_signaling_responses_pending].handle = handle;
        l2cap_signaling_responses[l2cap_signaling_responses_pending].code = code;
        l2cap_signaling_responses[l2cap_signaling_responses_pending].sig_id = sig_id;
        l2cap_signaling_responses[l2cap_signaling_responses_pending].cid = cid;
        l2cap_signaling_responses[l2cap_signaling_responses_pending].data = data;
        l2cap_signaling_responses_pending++;
        l2cap_run();
    }
}

#ifdef ENABLE_CLASSIC
static void l2cap_handle_disconnect_request(l2cap_channel_t *channel, uint8_t identifier){
    switch (channel->state){
        case L2CAP_STATE_CONFIG:
        case L2CAP_STATE_OPEN:
        case L2CAP_STATE_WILL_SEND_DISCONNECT_REQUEST:
        case L2CAP_STATE_WAIT_DISCONNECT:
            break;
        default:
            // ignore in other states
            return;
    }

    channel->remote_sig_id = identifier;
    channel->state = L2CAP_STATE_WILL_SEND_DISCONNECT_RESPONSE;
    l2cap_run();
}

static void l2cap_handle_connection_request(hci_con_handle_t handle, uint8_t sig_id, uint16_t psm, uint16_t source_cid){
    
    // log_info("l2cap_handle_connection_request for handle %u, psm %u cid 0x%02x", handle, psm, source_cid);
    l2cap_service_t *service = l2cap_get_service(psm);
    if (!service) {
        l2cap_register_signaling_response(handle, CONNECTION_REQUEST, sig_id, source_cid, L2CAP_CONNECTION_RESULT_PSM_NOT_SUPPORTED);
        return;
    }
    
    hci_connection_t * hci_connection = hci_connection_for_handle( handle );
    if (!hci_connection) {
        // 
        log_error("no hci_connection for handle %u", handle);
        return;
    }

    // if SC only mode is active and service requires encryption, reject connection if SC not active or use security level
    gap_security_level_t required_level = service->required_security_level;
    if (gap_get_secure_connections_only_mode() && (required_level != LEVEL_0)){
        if (gap_secure_connection(handle)){
            required_level = LEVEL_4;
        } else {
            l2cap_register_signaling_response(handle, CONNECTION_REQUEST, sig_id, source_cid, L2CAP_CONNECTION_RESULT_SECURITY_BLOCK);
            return;
        }
    }

    // alloc structure
    l2cap_channel_t * channel = l2cap_create_channel_entry(service->packet_handler, L2CAP_CHANNEL_TYPE_CLASSIC, hci_connection->address, BD_ADDR_TYPE_ACL,
    psm, service->mtu, required_level);
    if (!channel){
        l2cap_register_signaling_response(handle, CONNECTION_REQUEST, sig_id, source_cid, L2CAP_CONNECTION_RESULT_NO_RESOURCES_AVAILABLE);
        return;
    }

    channel->con_handle = handle;
    channel->remote_cid = source_cid;
    channel->remote_sig_id = sig_id;

    // limit local mtu to max acl packet length - l2cap header
    if (channel->local_mtu > l2cap_max_mtu()) {
        channel->local_mtu = l2cap_max_mtu();
    }
    
    // set initial state
    channel->state =     L2CAP_STATE_WAIT_REMOTE_SUPPORTED_FEATURES;
    channel->state_var = L2CAP_CHANNEL_STATE_VAR_INCOMING;

    // add to connections list
    btstack_linked_list_add_tail(&l2cap_channels, (btstack_linked_item_t *) channel);

    //
    if (required_level > LEVEL_0){
        // send conn resp pending if remote supported features have not been received yet
        if (hci_remote_features_available(handle)) {
            l2cap_handle_remote_supported_features_received(channel);
        } else {
            channel->state_var |= L2CAP_CHANNEL_STATE_VAR_SEND_CONN_RESP_PEND;
            hci_remote_features_query(handle);
        }
    } else {
        l2cap_handle_security_level_incoming_sufficient(channel);
    }
}

void l2cap_accept_connection(uint16_t local_cid){
    log_info("L2CAP_ACCEPT_CONNECTION local_cid 0x%x", local_cid);
    l2cap_channel_t * channel = l2cap_get_channel_for_local_cid(local_cid);
    if (!channel) {
        log_error("accept called but local_cid 0x%x not found", local_cid);
        return;
    }

#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
    // configure L2CAP Basic mode
    channel->mode  = L2CAP_CHANNEL_MODE_BASIC;
#endif

    channel->state = L2CAP_STATE_WILL_SEND_CONNECTION_RESPONSE_ACCEPT;

    // process
    l2cap_run();
}

void l2cap_decline_connection(uint16_t local_cid){
    log_info("decline local_cid 0x%x", local_cid);
    l2cap_channel_t * channel = l2cap_get_channel_for_local_cid( local_cid);
    if (!channel) {
        log_error( "l2cap_decline_connection called but local_cid 0x%x not found", local_cid);
        return;
    }
    channel->state  = L2CAP_STATE_WILL_SEND_CONNECTION_RESPONSE_DECLINE;
    channel->reason = L2CAP_CONNECTION_RESULT_NO_RESOURCES_AVAILABLE;
    l2cap_run();
}

// @pre command len is valid, see check in l2cap_signaling_handler_channel
static void l2cap_signaling_handle_configure_request(l2cap_channel_t *channel, uint8_t *command){

#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
    uint8_t use_fcs = 1;
#endif

    channel->remote_sig_id = command[L2CAP_SIGNALING_COMMAND_SIGID_OFFSET];

    uint16_t flags = little_endian_read_16(command, 6);
    if (flags & 1) {
        channelStateVarSetFlag(channel, L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP_CONT);
    }

    // accept the other's configuration options
    uint16_t end_pos = 4 + little_endian_read_16(command, L2CAP_SIGNALING_COMMAND_LENGTH_OFFSET);
    uint16_t pos     = 8;
    while (pos < end_pos){
        uint8_t option_hint = command[pos] >> 7;
        uint8_t option_type = command[pos] & 0x7f;
        // log_info("l2cap cid %u, hint %u, type %u", channel->local_cid, option_hint, option_type);
        pos++;
        uint8_t length = command[pos++];
        // MTU { type(8): 1, len(8):2, MTU(16) }
        if ((option_type == L2CAP_CONFIG_OPTION_TYPE_MAX_TRANSMISSION_UNIT) && (length == 2)){
            channel->remote_mtu = little_endian_read_16(command, pos);
            log_info("Remote MTU %u", channel->remote_mtu);
            if (channel->remote_mtu > l2cap_max_mtu()){
                log_info("Remote MTU %u larger than outgoing buffer, only using MTU = %u", channel->remote_mtu, l2cap_max_mtu());
                channel->remote_mtu = l2cap_max_mtu();
            }
            channelStateVarSetFlag(channel, L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP_MTU);
        }
        // Flush timeout { type(8):2, len(8): 2, Flush Timeout(16)}
        if ((option_type == L2CAP_CONFIG_OPTION_TYPE_FLUSH_TIMEOUT) && (length == 2)){
            channel->flush_timeout = little_endian_read_16(command, pos);
            log_info("Flush timeout: %u ms", channel->flush_timeout);
        }

#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
        // Retransmission and Flow Control Option
        if (option_type == L2CAP_CONFIG_OPTION_TYPE_RETRANSMISSION_AND_FLOW_CONTROL && length == 9){
            l2cap_channel_mode_t mode = (l2cap_channel_mode_t) command[pos];
            switch(channel->mode){
                case L2CAP_CHANNEL_MODE_ENHANCED_RETRANSMISSION:
                    // Store remote config
                    channel->remote_tx_window_size = command[pos+1];
                    channel->remote_max_transmit   = command[pos+2];
                    channel->remote_retransmission_timeout_ms = little_endian_read_16(command, pos + 3);
                    channel->remote_monitor_timeout_ms = little_endian_read_16(command, pos + 5);
                    {
                        uint16_t remote_mps = little_endian_read_16(command, pos + 7);
                        // optimize our tx buffer configuration based on actual remote mps if remote mps is smaller than planned
                        if (remote_mps < channel->remote_mps){
                            // get current tx storage
                            uint16_t num_bytes_per_tx_buffer_before = sizeof(l2cap_ertm_tx_packet_state_t) + channel->remote_mps;
                            uint16_t tx_storage = channel->num_tx_buffers * num_bytes_per_tx_buffer_before;

                            channel->remote_mps = remote_mps;
                            uint16_t num_bytes_per_tx_buffer_now = sizeof(l2cap_ertm_tx_packet_state_t) + channel->remote_mps;
                            channel->num_tx_buffers = tx_storage / num_bytes_per_tx_buffer_now;
                            uint32_t total_storage = (sizeof(l2cap_ertm_rx_packet_state_t) + channel->local_mps) * channel->num_rx_buffers + tx_storage + channel->local_mtu;
                            l2cap_ertm_setup_buffers(channel, (uint8_t *) channel->rx_packets_state, total_storage);
                        }
                        // limit remote mtu by our tx buffers. Include 2 bytes SDU Length
                        uint16_t effective_mtu = channel->remote_mps * channel->num_tx_buffers - 2;
                        channel->remote_mtu    = btstack_min( effective_mtu, channel->remote_mtu);
                    }
                    log_info("FC&C config: tx window: %u, max transmit %u, retrans timeout %u, monitor timeout %u, mps %u",
                        channel->remote_tx_window_size,
                        channel->remote_max_transmit,
                        channel->remote_retransmission_timeout_ms,
                        channel->remote_monitor_timeout_ms,
                        channel->remote_mps);
                    // If ERTM mandatory, but remote doens't offer ERTM -> disconnect
                    if (channel->ertm_mandatory && mode != L2CAP_CHANNEL_MODE_ENHANCED_RETRANSMISSION){
                        channel->state = L2CAP_STATE_WILL_SEND_DISCONNECT_REQUEST;
                    } else {
                        channelStateVarSetFlag(channel, L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP_ERTM);
                    }
                    break;
                case L2CAP_CHANNEL_MODE_BASIC:
                    switch (mode){
                        case L2CAP_CHANNEL_MODE_ENHANCED_RETRANSMISSION:
                            // remote asks for ERTM, but we want basic mode. disconnect if this happens a second time
                            if (channel->state_var & L2CAP_CHANNEL_STATE_VAR_BASIC_FALLBACK_TRIED){
                                channel->state = L2CAP_STATE_WILL_SEND_DISCONNECT_REQUEST;
                            }
                            channelStateVarSetFlag(channel, L2CAP_CHANNEL_STATE_VAR_BASIC_FALLBACK_TRIED);
                            channelStateVarSetFlag(channel, L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP_REJECTED);
                            break;
                        default: // case L2CAP_CHANNEL_MODE_BASIC:
                            // TODO store and evaluate configuration
                            channelStateVarSetFlag(channel, L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP_ERTM);
                            break;
                    }
                    break;
                default:
                    break;
            }
        }
        if (option_type == L2CAP_CONFIG_OPTION_TYPE_FRAME_CHECK_SEQUENCE && length == 1){
            use_fcs = command[pos];
        }        
#endif        
        // check for unknown options
        if ((option_hint == 0) && ((option_type < L2CAP_CONFIG_OPTION_TYPE_MAX_TRANSMISSION_UNIT) || (option_type > L2CAP_CONFIG_OPTION_TYPE_EXTENDED_WINDOW_SIZE))){
            log_info("l2cap cid %u, unknown option 0x%02x", channel->local_cid, option_type);
            channel->unknown_option = option_type;
            channelStateVarSetFlag(channel, L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP_INVALID);
        }
        pos += length;
    }

#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
        // "FCS" has precedence over "No FCS"
        uint8_t update = channel->fcs_option || use_fcs;
        log_info("local fcs: %u, remote fcs: %u -> %u", channel->fcs_option, use_fcs, update);
        channel->fcs_active = update;
        // If ERTM mandatory, but remote didn't send Retransmission and Flowcontrol options -> disconnect
        if (((channel->state_var & L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP_ERTM) == 0) & (channel->ertm_mandatory)){
            channel->state = L2CAP_STATE_WILL_SEND_DISCONNECT_REQUEST;
        }
#endif
}

// @pre command len is valid, see check in l2cap_signaling_handler_channel
static void l2cap_signaling_handle_configure_response(l2cap_channel_t *channel, uint16_t result, uint8_t *command){
    log_info("l2cap_signaling_handle_configure_response");
#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
    uint16_t end_pos = 4 + little_endian_read_16(command, L2CAP_SIGNALING_COMMAND_LENGTH_OFFSET);
    uint16_t pos     = 10;
    while (pos < end_pos){
        uint8_t option_hint = command[pos] >> 7;
        uint8_t option_type = command[pos] & 0x7f;
        // log_info("l2cap cid %u, hint %u, type %u", channel->local_cid, option_hint, option_type);
        pos++;
        uint8_t length = command[pos++];

        // Retransmission and Flow Control Option
        if (option_type == L2CAP_CONFIG_OPTION_TYPE_RETRANSMISSION_AND_FLOW_CONTROL && length == 9){
            switch (channel->mode){
                case L2CAP_CHANNEL_MODE_ENHANCED_RETRANSMISSION:
                    if (channel->ertm_mandatory){
                        // ??
                    } else {
                        // On 'Reject - Unacceptable Parameters' to our optional ERTM request, fall back to BASIC mode
                        if (result == L2CAP_CONF_RESULT_UNACCEPTABLE_PARAMETERS){
                            l2cap_emit_simple_event_with_cid(channel, L2CAP_EVENT_ERTM_BUFFER_RELEASED);
                            channel->mode = L2CAP_CHANNEL_MODE_BASIC;
                        }
                    }
                    break;
                case L2CAP_CHANNEL_MODE_BASIC:
                    if (result == L2CAP_CONF_RESULT_UNACCEPTABLE_PARAMETERS){
                        // On 'Reject - Unacceptable Parameters' to our Basic mode request, disconnect
                        channel->state = L2CAP_STATE_WILL_SEND_DISCONNECT_REQUEST;
                    }
                    break;
                default:
                    break;
            }
        }

        // check for unknown options
        if (option_hint == 0 && (option_type < L2CAP_CONFIG_OPTION_TYPE_MAX_TRANSMISSION_UNIT || option_type > L2CAP_CONFIG_OPTION_TYPE_EXTENDED_WINDOW_SIZE)){
            log_info("l2cap cid %u, unknown option 0x%02x", channel->local_cid, option_type);
            channel->unknown_option = option_type;
            channelStateVarSetFlag(channel, L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP_INVALID);
        }

        pos += length;
    }
#else
    UNUSED(channel);  // ok: no code
    UNUSED(result);   // ok: no code
    UNUSED(command);  // ok: no code
#endif        
}

static int l2cap_channel_ready_for_open(l2cap_channel_t *channel){
    // log_info("l2cap_channel_ready_for_open 0x%02x", channel->state_var);
    if ((channel->state_var & L2CAP_CHANNEL_STATE_VAR_RCVD_CONF_RSP) == 0) return 0;
    if ((channel->state_var & L2CAP_CHANNEL_STATE_VAR_SENT_CONF_RSP) == 0) return 0;
    // addition check that fixes re-entrance issue causing l2cap event channel opened twice
    if (channel->state == L2CAP_STATE_OPEN) return 0;
    return 1;
}


// @pre command len is valid, see check in l2cap_signaling_handler_dispatch
static void l2cap_signaling_handler_channel(l2cap_channel_t *channel, uint8_t *command){

    uint8_t  code       = command[L2CAP_SIGNALING_COMMAND_CODE_OFFSET];
    uint8_t  identifier = command[L2CAP_SIGNALING_COMMAND_SIGID_OFFSET];
    uint16_t cmd_len    = little_endian_read_16(command, L2CAP_SIGNALING_COMMAND_LENGTH_OFFSET);
    uint16_t result = 0;
    
    log_info("L2CAP signaling handler code %u, state %u", code, channel->state);
    
    // handle DISCONNECT REQUESTS seperately
    if (code == DISCONNECTION_REQUEST){
        l2cap_handle_disconnect_request(channel, identifier);
        return;
    }
    
    // @STATEMACHINE(l2cap)
    switch (channel->state) {
            
        case L2CAP_STATE_WAIT_CONNECT_RSP:
            switch (code){
                case CONNECTION_RESPONSE:
                    if (cmd_len < 8){
                        // command imcomplete
                        l2cap_register_signaling_response(channel->con_handle, COMMAND_REJECT, identifier, 0, L2CAP_REJ_CMD_UNKNOWN);
                        break;
                    }
                    l2cap_stop_rtx(channel);
                    result = little_endian_read_16 (command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET+4);
                    switch (result) {
                        case 0:
                            // successful connection
                            channel->remote_cid = little_endian_read_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET);
                            channel->state = L2CAP_STATE_CONFIG;
                            channelStateVarSetFlag(channel, L2CAP_CHANNEL_STATE_VAR_SEND_CONF_REQ);
                            break;
                        case 1:
                            // connection pending. get some coffee, but start the ERTX
                            l2cap_start_ertx(channel);
                            break;
                        default:
                            // channel closed
                            channel->state = L2CAP_STATE_CLOSED;
                            // map l2cap connection response result to BTstack status enumeration
                            l2cap_handle_channel_open_failed(channel, L2CAP_CONNECTION_RESPONSE_RESULT_SUCCESSFUL + result);
                            
                            // drop link key if security block
                            if ((L2CAP_CONNECTION_RESPONSE_RESULT_SUCCESSFUL + result) == L2CAP_CONNECTION_RESPONSE_RESULT_REFUSED_SECURITY){
                                gap_drop_link_key_for_bd_addr(channel->address);
                            }
                            
                            // discard channel
                            btstack_linked_list_remove(&l2cap_channels, (btstack_linked_item_t *) channel);
                            l2cap_free_channel_entry(channel);
                            break;
                    }
                    break;
                    
                default:
                    //@TODO: implement other signaling packets
                    break;
            }
            break;

        case L2CAP_STATE_CONFIG:
            switch (code) {
                case CONFIGURE_REQUEST:
                    if (cmd_len < 4){
                        // command incomplete
                        l2cap_register_signaling_response(channel->con_handle, COMMAND_REJECT, identifier, 0, L2CAP_REJ_CMD_UNKNOWN);
                        break;
                    }
                    channelStateVarSetFlag(channel, L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP);
                    l2cap_signaling_handle_configure_request(channel, command);
                    if (!(channel->state_var & L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP_CONT)){
                        // only done if continuation not set
                        channelStateVarSetFlag(channel, L2CAP_CHANNEL_STATE_VAR_RCVD_CONF_REQ);
                    }
                    break;
                case CONFIGURE_RESPONSE:
                    if (cmd_len < 6){
                        // command incomplete
                        l2cap_register_signaling_response(channel->con_handle, COMMAND_REJECT, identifier, 0, L2CAP_REJ_CMD_UNKNOWN);
                        break;
                    }
                    result = little_endian_read_16 (command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET+4);
                    l2cap_stop_rtx(channel);
                    l2cap_signaling_handle_configure_response(channel, result, command);
                    switch (result){
                        case 0: // success
                            channelStateVarSetFlag(channel, L2CAP_CHANNEL_STATE_VAR_RCVD_CONF_RSP);
                            break;
                        case 4: // pending
                            l2cap_start_ertx(channel);
                            break;
                        default:
#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
                            if (channel->mode == L2CAP_CHANNEL_MODE_ENHANCED_RETRANSMISSION && channel->ertm_mandatory){
                                // remote does not offer ertm but it's required
                                channel->state = L2CAP_STATE_WILL_SEND_DISCONNECT_REQUEST;
                                break;
                            } 
#endif                     
                            // retry on negative result
                            channelStateVarSetFlag(channel, L2CAP_CHANNEL_STATE_VAR_SEND_CONF_REQ);
                            break;
                    }
                    break;
                default:
                    break;
            }
            if (l2cap_channel_ready_for_open(channel)){

#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
                // assert that packet can be stored in fragment buffers in ertm
                if (channel->mode == L2CAP_CHANNEL_MODE_ENHANCED_RETRANSMISSION){
                    uint16_t effective_mps = btstack_min(channel->remote_mps, channel->local_mps);
                    uint16_t usable_mtu = channel->num_tx_buffers == 1 ? effective_mps : channel->num_tx_buffers * effective_mps - 2;
                    if (usable_mtu < channel->remote_mtu){
                        log_info("Remote MTU %u > max storable ERTM packet, only using MTU = %u", channel->remote_mtu, usable_mtu);
                        channel->remote_mtu = usable_mtu;
                    }
                }
#endif
                // for open:
                channel->state = L2CAP_STATE_OPEN;
                l2cap_emit_channel_opened(channel, 0);
            }
            break;
            
        case L2CAP_STATE_WAIT_DISCONNECT:
            switch (code) {
                case DISCONNECTION_RESPONSE:
                    l2cap_finalize_channel_close(channel);
                    break;
                default:
                    //@TODO: implement other signaling packets
                    break;
            }
            break;
            
        case L2CAP_STATE_CLOSED:
            // @TODO handle incoming requests
            break;
            
        case L2CAP_STATE_OPEN:
            //@TODO: implement other signaling packets, e.g. re-configure
            break;
        default:
            break;
    }
    // log_info("new state %u", channel->state);
}

#ifdef ENABLE_CLASSIC
static void l2cap_handle_information_request_complete(hci_connection_t * connection){

    connection->l2cap_state.information_state = L2CAP_INFORMATION_STATE_DONE;

    // emit event
    uint8_t event[8];
    event[0] = L2CAP_EVENT_INFORMATION_RESPONSE;
    event[1] = sizeof(event) - 2;
    little_endian_store_16(event, 2, connection->con_handle);
    little_endian_store_16(event, 4, connection->l2cap_state.extended_feature_mask);
    little_endian_store_16(event, 6, connection->l2cap_state.fixed_channels_supported);
    l2cap_emit_event(event, sizeof(event));

    // trigger connection request
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &l2cap_channels);
    while (btstack_linked_list_iterator_has_next(&it)){
        l2cap_channel_t * channel = (l2cap_channel_t *) btstack_linked_list_iterator_next(&it);
        if (!l2cap_is_dynamic_channel_type(channel->channel_type)) continue;
        if (channel->con_handle != connection->con_handle) continue;

        // incoming connection: information request was triggered after user has accepted connection,
        // now: verify channel configuration, esp. if ertm will be mandatory
        if (channel->state == L2CAP_STATE_WAIT_INCOMING_EXTENDED_FEATURES){
            // default: continue
            channel->state = L2CAP_STATE_WILL_SEND_CONNECTION_RESPONSE_ACCEPT;
#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
            if ((channel->mode == L2CAP_CHANNEL_MODE_ENHANCED_RETRANSMISSION) && ((connection->l2cap_state.extended_feature_mask & 0x08) == 0)){
                // ERTM not possible, select basic mode and release buffer
                channel->mode = L2CAP_CHANNEL_MODE_BASIC;
                l2cap_emit_simple_event_with_cid(channel, L2CAP_EVENT_ERTM_BUFFER_RELEASED);

                // bail if ERTM is mandatory
                if (channel->ertm_mandatory){
                    // We chose 'no resources available' for "ERTM mandatory but you don't even know ERTM exists"
                    log_info("ERTM mandatory -> reject connection");
                    channel->state  = L2CAP_STATE_WILL_SEND_CONNECTION_RESPONSE_DECLINE;
                    channel->reason = L2CAP_CONNECTION_RESULT_NO_RESOURCES_AVAILABLE;
                }  else {
                    log_info("ERTM not supported by remote -> use Basic mode");
                }
            }
#endif
            continue;
        }

        // outgoing connection: information request is triggered before connection request
        if (channel->state == L2CAP_STATE_WAIT_OUTGOING_EXTENDED_FEATURES){

#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
            // if ERTM was requested, but is not listed in extended feature mask:
            if ((channel->mode == L2CAP_CHANNEL_MODE_ENHANCED_RETRANSMISSION) && ((connection->l2cap_state.extended_feature_mask & 0x08) == 0)){

                if (channel->ertm_mandatory){
                    // bail if ERTM is mandatory
                    channel->state = L2CAP_STATE_CLOSED;
                    // map l2cap connection response result to BTstack status enumeration
                    l2cap_handle_channel_open_failed(channel, L2CAP_CONNECTION_RESPONSE_RESULT_ERTM_NOT_SUPPORTED);
                    // discard channel
                    btstack_linked_list_remove(&l2cap_channels, (btstack_linked_item_t *) channel);
                    l2cap_free_channel_entry(channel);
                    continue;

                } else {
                    // fallback to Basic mode
                    l2cap_emit_simple_event_with_cid(channel, L2CAP_EVENT_ERTM_BUFFER_RELEASED);
                    channel->mode = L2CAP_CHANNEL_MODE_BASIC;
                }
            }
#endif

            // respond to connection request
            channel->state = L2CAP_STATE_WILL_SEND_CONNECTION_REQUEST;
            continue;
        }
    }
}
#endif

// @pre command len is valid, see check in l2cap_acl_classic_handler
static void l2cap_signaling_handler_dispatch(hci_con_handle_t handle, uint8_t * command){

    hci_connection_t * connection;
    btstack_linked_list_iterator_t it;

    // get code, signal identifier and command len
    uint8_t code     = command[L2CAP_SIGNALING_COMMAND_CODE_OFFSET];
    uint8_t sig_id   = command[L2CAP_SIGNALING_COMMAND_SIGID_OFFSET];
    uint16_t cmd_len = little_endian_read_16(command, L2CAP_SIGNALING_COMMAND_LENGTH_OFFSET);

    // general commands without an assigned channel
    switch(code) {
            
        case CONNECTION_REQUEST:
            if (cmd_len == 4){
                uint16_t psm =        little_endian_read_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET);
                uint16_t source_cid = little_endian_read_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET+2);
                l2cap_handle_connection_request(handle, sig_id, psm, source_cid);
            } else {
                l2cap_register_signaling_response(handle, COMMAND_REJECT, sig_id, 0, L2CAP_REJ_CMD_UNKNOWN);
            }
            return;
            
        case ECHO_REQUEST:
            l2cap_register_signaling_response(handle, code, sig_id, 0, 0);
            return;
            
        case INFORMATION_REQUEST:
            if (cmd_len == 2) {
                uint16_t info_type = little_endian_read_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET);
                l2cap_register_signaling_response(handle, code, sig_id, 0, info_type);
                return;
            }
            break;

        case INFORMATION_RESPONSE:
            connection = hci_connection_for_handle(handle);
            if (!connection) return;
            switch (connection->l2cap_state.information_state){
                case L2CAP_INFORMATION_STATE_W4_EXTENDED_FEATURE_RESPONSE:
                    // get extended features from response if valid
                    if (cmd_len >= 6) {
                        uint16_t info_type = little_endian_read_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET);
                        uint16_t result    = little_endian_read_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET+2);
                        if (result == 0 && info_type == L2CAP_INFO_TYPE_EXTENDED_FEATURES_SUPPORTED) {
                            connection->l2cap_state.extended_feature_mask = little_endian_read_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET+4);
                        }
                        log_info("extended features mask 0x%02x", connection->l2cap_state.extended_feature_mask);
                        if ((connection->l2cap_state.extended_feature_mask & 0x80) != 0){
                            connection->l2cap_state.information_state = L2CAP_INFORMATION_STATE_W2_SEND_FIXED_CHANNELS_REQUEST;
                        } else {
                            // information request complete
                            l2cap_handle_information_request_complete(connection);
                        }
                    }
                    break;
                case L2CAP_INFORMATION_STATE_W4_FIXED_CHANNELS_RESPONSE:
                    if (cmd_len >= 12) {
                        uint16_t info_type = little_endian_read_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET);
                        uint16_t result    = little_endian_read_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET+2);
                        if (result == 0 && info_type == L2CAP_INFO_TYPE_FIXED_CHANNELS_SUPPORTED) {
                            connection->l2cap_state.fixed_channels_supported = little_endian_read_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET + 4);
                        }
                        log_info("fixed channels mask 0x%02x", connection->l2cap_state.fixed_channels_supported);
                        // information request complete
                        l2cap_handle_information_request_complete(connection);
                    }
                    break;
                default:
                    break;
            }
            return;

#ifdef ENABLE_L2CAP_ENHANCED_CREDIT_BASED_FLOW_CONTROL_MODE
        case L2CAP_CREDIT_BASED_CONNECTION_REQUEST:
        case L2CAP_CREDIT_BASED_CONNECTION_RESPONSE:
        case L2CAP_CREDIT_BASED_RECONFIGURE_REQUEST:
        case L2CAP_CREDIT_BASED_RECONFIGURE_RESPONSE:
            l2cap_ecbm_signaling_handler_dispatch(handle, L2CAP_CID_SIGNALING, command, sig_id);
            return;
        case L2CAP_FLOW_CONTROL_CREDIT_INDICATION:
            // return if valid
            if (l2cap_credit_based_handle_credit_indication(handle, command, cmd_len)) return;
            break;

        case COMMAND_REJECT:
            btstack_linked_list_iterator_init(&it, &l2cap_channels);
            while (btstack_linked_list_iterator_has_next(&it)) {
                l2cap_channel_t *channel = (l2cap_channel_t *) btstack_linked_list_iterator_next(&it);
                if (!l2cap_is_dynamic_channel_type(channel->channel_type)) continue;
                if (channel->con_handle != handle) continue;
                if (channel->local_sig_id != sig_id) continue;
                if (channel->state != L2CAP_STATE_WAIT_ENHANCED_CONNECTION_RESPONSE) continue;

                // open failed
                channel->state = L2CAP_STATE_CLOSED;
                l2cap_ecbm_emit_channel_opened(channel,
                                               ERROR_CODE_CONNECTION_REJECTED_DUE_TO_LIMITED_RESOURCES);
                // drop failed channel
                btstack_linked_list_iterator_remove(&it);
                l2cap_free_channel_entry(channel);
            }
            break;
#endif

        default:
            break;
    }
    
    if (cmd_len >= 2){
        // Get potential destination CID
        uint16_t dest_cid = little_endian_read_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET);

        // Find channel for this sig_id and connection handle
        btstack_linked_list_iterator_init(&it, &l2cap_channels);
        while (btstack_linked_list_iterator_has_next(&it)){
            l2cap_channel_t * channel = (l2cap_channel_t *) btstack_linked_list_iterator_next(&it);
            if (!l2cap_is_dynamic_channel_type(channel->channel_type)) continue;
            if (channel->con_handle != handle) continue;
            if (code & 1) {
                // match odd commands (responses) by previous signaling identifier
                if (channel->local_sig_id == sig_id) {
                    l2cap_signaling_handler_channel(channel, command);
                    return;
                }
            } else {
                // match even commands (requests) by local channel id
                if (channel->local_cid == dest_cid) {
                    l2cap_signaling_handler_channel(channel, command);
                    return;
                }
            }
        }
    }

    // If dynamic channel cannot be found, either never set-up or already finalized, assume state CLOSED
    // Handle events as described in Core 5.4, Vol 3. Host, 6.1.1 CLOSED state
    switch (code){
        case CONNECTION_RESPONSE:
        case CONFIGURE_RESPONSE:
        case DISCONNECTION_RESPONSE:
            // Ignore request
            return;
        case DISCONNECTION_REQUEST:
            if (cmd_len == 4){
                // send disconnect response for received dest and source cids
                uint16_t dest_cid = little_endian_read_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET);
                uint16_t source_cid = little_endian_read_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET+2);
                l2cap_register_signaling_response(handle, DISCONNECTION_RESPONSE, sig_id, source_cid, dest_cid);
                return;
            }
            break;
        case CONFIGURE_REQUEST:
            if (cmd_len >= 2){
                // send command reject with reason invalid cid
                uint16_t dest_cid = little_endian_read_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET);
                l2cap_register_signaling_response(handle, CONFIGURE_REQUEST, sig_id, dest_cid, L2CAP_REJ_INVALID_CID);
                return;
            }
            break;
        default:
            break;
    }
    // otherwise send command reject with reason unknown command
    l2cap_register_signaling_response(handle, COMMAND_REJECT, sig_id, 0, L2CAP_REJ_CMD_UNKNOWN);
}
#endif

#ifdef L2CAP_USES_CHANNELS
static l2cap_service_t * l2cap_get_service_internal(btstack_linked_list_t * services, uint16_t psm){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, services);
    while (btstack_linked_list_iterator_has_next(&it)){
        l2cap_service_t * service = (l2cap_service_t *) btstack_linked_list_iterator_next(&it);
        if ( service->psm == psm){
            return service;
        };
    }
    return NULL;
}
#endif

#ifdef ENABLE_L2CAP_ENHANCED_CREDIT_BASED_FLOW_CONTROL_MODE

static uint8_t l2cap_ecbm_setup_channels(btstack_linked_list_t * channels,
                                         btstack_packet_handler_t packet_handler, uint8_t num_channels,
                                         hci_connection_t * connection, uint16_t psm, uint16_t mtu, gap_security_level_t security_level){
    uint8_t i;
    uint8_t status = ERROR_CODE_SUCCESS;
    for (i=0;i<num_channels;i++){
        l2cap_channel_t * channel = l2cap_create_channel_entry(packet_handler, L2CAP_CHANNEL_TYPE_CHANNEL_ECBM, connection->address, connection->address_type, psm, mtu, security_level);
        if (!channel) {
            status = BTSTACK_MEMORY_ALLOC_FAILED;
            break;
        }
        channel->con_handle = connection->con_handle;
        btstack_linked_list_add_tail(channels, (btstack_linked_item_t *) channel);
    }

    // free channels if not all allocated
    if (status != ERROR_CODE_SUCCESS){
        while (true) {
            l2cap_channel_t * channel = (l2cap_channel_t *) btstack_linked_list_pop(channels);
            if (channel == NULL) break;
            l2cap_free_channel_entry(channel);
        }
    }

    return status;
}

static inline l2cap_service_t * l2cap_ecbm_get_service(uint16_t spsm){
    return l2cap_get_service_internal(&l2cap_enhanced_services, spsm);
}

static inline uint8_t l2cap_ecbm_status_for_result(uint16_t result) {
    switch (result) {
        case L2CAP_ECBM_CONNECTION_RESULT_ALL_SUCCESS:
            return ERROR_CODE_SUCCESS;
        case L2CAP_ECBM_CONNECTION_RESULT_ALL_REFUSED_SPSM_NOT_SUPPORTED:
            return L2CAP_CONNECTION_RESPONSE_RESULT_REFUSED_PSM;
        case L2CAP_ECBM_CONNECTION_RESULT_SOME_REFUSED_INSUFFICIENT_RESOURCES_AVAILABLE:
            return L2CAP_CONNECTION_RESPONSE_RESULT_REFUSED_RESOURCES;
        case L2CAP_ECBM_CONNECTION_RESULT_ALL_REFUSED_INSUFFICIENT_AUTHENTICATION:
        case L2CAP_ECBM_CONNECTION_RESULT_ALL_REFUSED_INSUFFICIENT_AUTHORIZATION:
        case L2CAP_ECBM_CONNECTION_RESULT_ALL_REFUSED_ENCYRPTION_KEY_SIZE_TOO_SHORT:
        case L2CAP_ECBM_CONNECTION_RESULT_ALL_REFUSED_INSUFFICIENT_ENCRYPTION:
            return L2CAP_CONNECTION_RESPONSE_RESULT_REFUSED_SECURITY;
        default:
            // invalid Source CID, Source CID already allocated, unacceptable parameters
            return L2CAP_CONNECTION_RESPONSE_UNKNOWN_ERROR;
    }
}

static void
l2cap_ecbm_emit_incoming_connection(l2cap_channel_t *channel, uint8_t num_channels) {
    uint8_t event[16];
    event[0] = L2CAP_EVENT_ECBM_INCOMING_CONNECTION;
    event[1] = sizeof(event) - 2;
    event[2] = channel->address_type;
    reverse_bd_addr(channel->address, &event[3]);
    little_endian_store_16(event, 9, channel->con_handle);
    little_endian_store_16(event, 11, channel->psm);
    event[13] = num_channels;
    little_endian_store_16(event, 14, channel->local_cid);
    hci_dump_btstack_event(event, sizeof(event));
    (*channel->packet_handler)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static uint8_t
l2cap_ecbm_security_status_for_connection_request(hci_con_handle_t handle, gap_security_level_t required_security_level,
                                                  bool requires_authorization) {
    // check security in increasing error priority
    uint8_t security_status = L2CAP_ECBM_CONNECTION_RESULT_ALL_SUCCESS;

    // security: check encryption
    if (required_security_level >= LEVEL_2) {
        if (gap_encryption_key_size(handle) < 16) {
            security_status = L2CAP_ECBM_CONNECTION_RESULT_ALL_REFUSED_ENCYRPTION_KEY_SIZE_TOO_SHORT;
        }
        if (gap_encryption_key_size(handle) == 0){
            security_status = L2CAP_ECBM_CONNECTION_RESULT_ALL_REFUSED_INSUFFICIENT_ENCRYPTION;
        }
    }

    // security: check authentication
    if (required_security_level >= LEVEL_3) {
        if (!gap_authenticated(handle)) {
            security_status = requires_authorization ?
                    L2CAP_ECBM_CONNECTION_RESULT_ALL_REFUSED_INSUFFICIENT_AUTHORIZATION :
                    L2CAP_ECBM_CONNECTION_RESULT_ALL_REFUSED_INSUFFICIENT_AUTHENTICATION;
        }
    }
    return security_status;
}

static void l2cap_ecbm_handle_security_level_incoming(l2cap_channel_t * channel){
    // count number of l2cap_channels in state L2CAP_STATE_WAIT_INCOMING_SECURITY_LEVEL_UPDATE with same remote_sig_id
    uint8_t sig_id = channel->remote_sig_id;
    hci_con_handle_t con_handle = channel->con_handle;

    uint8_t security_status = l2cap_ecbm_security_status_for_connection_request(channel->con_handle,
                                                                                channel->required_security_level,
                                                                                false);
    bool security_sufficient = security_status == L2CAP_ECBM_CONNECTION_RESULT_ALL_SUCCESS;

    uint8_t num_channels = 0;
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &l2cap_channels);
    while (btstack_linked_list_iterator_has_next(&it)) {
        l2cap_channel_t *channel = (l2cap_channel_t *) btstack_linked_list_iterator_next(&it);
        if (!l2cap_is_dynamic_channel_type(channel->channel_type)) continue;
        if (channel->con_handle != con_handle) continue;
        if (channel->remote_sig_id != sig_id) continue;
        if (channel->state != L2CAP_STATE_WAIT_INCOMING_SECURITY_LEVEL_UPDATE) continue;
        num_channels++;

        if (security_sufficient){
            channel->state = L2CAP_STATE_WAIT_CLIENT_ACCEPT_OR_REJECT;
        } else {
            btstack_linked_list_iterator_remove(&it);
            btstack_memory_l2cap_channel_free(channel);
        }
    }

    if (security_sufficient){
        l2cap_ecbm_emit_incoming_connection(channel, num_channels);
    } else {
        // combine signaling cid and number channels for l2cap_register_signaling_response
        uint16_t signaling_cid = L2CAP_CID_SIGNALING;
        uint16_t num_channels_and_signaling_cid = (num_channels << 8) | signaling_cid;
        l2cap_register_signaling_response(con_handle, L2CAP_CREDIT_BASED_CONNECTION_REQUEST, sig_id,
                                          num_channels_and_signaling_cid, security_status);
    }
}

void l2cap_ecbm_trigger_pending_connection_responses(hci_con_handle_t con_handle){
    bool done = false;
    while (!done) {
        done = true;
        btstack_linked_list_iterator_t it;
        btstack_linked_list_iterator_init(&it, &l2cap_channels);
        while (btstack_linked_list_iterator_has_next(&it)) {
            l2cap_channel_t *channel = (l2cap_channel_t *) btstack_linked_list_iterator_next(&it);
            if (channel->channel_type != L2CAP_CHANNEL_TYPE_CHANNEL_ECBM) continue;
            if (channel->con_handle != con_handle) continue;
            if  (channel->state == L2CAP_STATE_WAIT_INCOMING_SECURITY_LEVEL_UPDATE) {
                l2cap_ecbm_handle_security_level_incoming(channel);
                done = false;
            };
        }
    }
}

static int l2cap_ecbm_signaling_handler_dispatch(hci_con_handle_t handle, uint16_t signaling_cid, uint8_t *command,
                                                 uint8_t sig_id) {

    hci_connection_t *connection;
    btstack_linked_list_iterator_t it;
    l2cap_service_t *service;
    uint16_t spsm;
    uint8_t num_channels;
    uint16_t num_channels_and_signaling_cid;
    uint8_t i;
    uint16_t new_mtu;
    uint16_t new_mps;
    uint16_t initial_credits;
    uint16_t result;
    uint8_t status;

    uint8_t code = command[L2CAP_SIGNALING_COMMAND_CODE_OFFSET];
    uint16_t len = little_endian_read_16(command, L2CAP_SIGNALING_COMMAND_LENGTH_OFFSET);
    log_info("enhanced dispatch: command 0x%02x, sig id %u, len %u", code, sig_id, len);

    switch (code) {
        case L2CAP_CREDIT_BASED_CONNECTION_REQUEST:
            // check size
            if (len < 10u) return 0u;

            // get hci connection, bail if not found (must not happen)
            connection = hci_connection_for_handle(handle);
            btstack_assert(connection != NULL);

            // get num channels to establish
            num_channels = (len - 8) / sizeof(uint16_t);

            // combine signaling cid and number channels for l2cap_register_signaling_response
            num_channels_and_signaling_cid = (num_channels << 8) | signaling_cid;

            // check if service registered
            spsm = little_endian_read_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET);
            service = l2cap_ecbm_get_service(spsm);

            if (service) {

                uint16_t remote_mtu = little_endian_read_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET + 2);
                uint16_t remote_mps = little_endian_read_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET + 4);
                uint16_t credits_outgoing = little_endian_read_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET + 6);

                // param: check remote mtu
                if (service->mtu > remote_mtu) {
                    l2cap_register_signaling_response(handle, L2CAP_CREDIT_BASED_CONNECTION_REQUEST, sig_id,
                                                      num_channels_and_signaling_cid, L2CAP_ECBM_CONNECTION_RESULT_ALL_REFUSED_INVALID_PARAMETERS);
                    return 1;
                }

                // check if authentication is required and possible
                bool send_pending = false;
                uint8_t security_status = l2cap_ecbm_security_status_for_connection_request(handle,
                                                                                            service->required_security_level,
                                                                                            service->requires_authorization);
                if (security_status != L2CAP_ECBM_CONNECTION_RESULT_ALL_SUCCESS) {
                    if (gap_get_bondable_mode() != 0) {
                        // if possible, send pending and continue
                        send_pending = true;
                    } else {
                        // otherwise, send refused and abort
                        l2cap_register_signaling_response(handle, L2CAP_CREDIT_BASED_CONNECTION_REQUEST, sig_id,
                                                          num_channels_and_signaling_cid, security_status);
                        return 1;
                    }
                }

                // report the last result code != 0
                result = 0;
                // store one of the local channels for the event
                l2cap_channel_t * a_channel = NULL;
                for (i = 0; i < num_channels; i++) {

                    // check source cids
                    uint16_t source_cid = little_endian_read_16(command, 12 + (i * 2));
                    if (source_cid < 0x40u) {
                        result = L2CAP_ECBM_CONNECTION_RESULT_SOME_REFUSED_INVALID_SOURCE_CID;
                        continue;
                    }

                    // go through list of channels for this ACL connection and check if we get a match
                    btstack_linked_list_iterator_init(&it, &l2cap_channels);
                    bool source_cid_allocated = false;
                    while (btstack_linked_list_iterator_has_next(&it)) {
                        l2cap_channel_t *channel = (l2cap_channel_t *) btstack_linked_list_iterator_next(&it);
                        if (!l2cap_is_dynamic_channel_type(channel->channel_type)) continue;
                        if (channel->con_handle != handle) continue;
                        if (channel->remote_cid != source_cid) continue;
                        source_cid_allocated = true;
                        break;
                    }
                    if (source_cid_allocated) {
                        result = 0x00a;
                        continue;
                    }

                    // setup channel
                    l2cap_channel_t *channel = l2cap_create_channel_entry(service->packet_handler,
                                                                          L2CAP_CHANNEL_TYPE_CHANNEL_ECBM,
                                                                          connection->address,
                                                                          connection->address_type, spsm,
                                                                          service->mtu,
                                                                          service->required_security_level);

                    if (channel == NULL) {
                        // Some connections refused – insufficient resources available
                        result = 0x004;
                        continue;
                    }

                    // setup state
                    channel->state = L2CAP_STATE_WAIT_INCOMING_SECURITY_LEVEL_UPDATE;
                    channel->state_var |= L2CAP_CHANNEL_STATE_VAR_INCOMING;
                    channel->con_handle = connection->con_handle;
                    channel->remote_sig_id = sig_id;
                    channel->remote_cid = source_cid;
                    channel->remote_mtu = remote_mtu;
                    channel->remote_mps = remote_mps;
                    channel->credits_outgoing = credits_outgoing;
                    channel->cid_index = i;
                    channel->num_cids = num_channels;

                    // if more than one error, we can report any of them
                    channel->reason = result;

                    btstack_linked_list_add_tail(&l2cap_channels, (btstack_linked_item_t *) channel);

                    a_channel = channel;
                }

                // if no channels have been created, all have been refused, and we can respond right away
                if (a_channel == NULL) {
                    l2cap_register_signaling_response(handle, L2CAP_CREDIT_BASED_CONNECTION_REQUEST, sig_id,
                                                      num_channels_and_signaling_cid, result);
                    return 1;
                }

                // if security is pending, send intermediate response, otherwise, ask user
                if (send_pending){
                    l2cap_register_signaling_response(handle, L2CAP_CREDIT_BASED_CONNECTION_REQUEST, sig_id,
                                                      num_channels_and_signaling_cid, L2CAP_ECBM_CONNECTION_RESULT_ALL_PENDING_AUTHENTICATION);
                } else {
                    // if security is ok but authorization is required, send intermediate response and ask user
                    if (service->requires_authorization){
                        l2cap_register_signaling_response(handle, L2CAP_CREDIT_BASED_CONNECTION_REQUEST, sig_id,
                                                          num_channels_and_signaling_cid, L2CAP_ECBM_CONNECTION_RESULT_ALL_PENDING_AUTHORIZATION);
                    }
                    l2cap_ecbm_handle_security_level_incoming(a_channel);
                }

            } else {
                l2cap_register_signaling_response(handle, L2CAP_CREDIT_BASED_CONNECTION_REQUEST, sig_id,
                                                  num_channels_and_signaling_cid, L2CAP_ECBM_CONNECTION_RESULT_ALL_REFUSED_SPSM_NOT_SUPPORTED);
            }
            return 1;

        case L2CAP_CREDIT_BASED_CONNECTION_RESPONSE:
            // check size
            if (len < 10u) return 0u;

            // get hci connection, ignore if not found
            connection = hci_connection_for_handle(handle);
            if (connection == NULL) return 0;

            // get channel config: mtu, mps, initial credits, result
            new_mtu = little_endian_read_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET + 0);
            new_mps = little_endian_read_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET + 2);
            initial_credits = little_endian_read_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET + 4);
            result = little_endian_read_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET + 6);
            status = l2cap_ecbm_status_for_result(result);

            // get num channels to modify
            num_channels = (len - 8) / sizeof(uint16_t);
            btstack_linked_list_iterator_init(&it, &l2cap_channels);
            while (btstack_linked_list_iterator_has_next(&it)) {
                uint8_t channel_status = status;
                l2cap_channel_t *channel = (l2cap_channel_t *) btstack_linked_list_iterator_next(&it);
                if (!l2cap_is_dynamic_channel_type(channel->channel_type)) continue;
                if (channel->con_handle != handle) continue;
                if (channel->local_sig_id != sig_id) continue;
                if (channel->state != L2CAP_STATE_WAIT_ENHANCED_CONNECTION_RESPONSE) continue;
                if (channel->cid_index < num_channels) {
                    uint16_t remote_cid = little_endian_read_16(command, 12 + channel->cid_index * sizeof(uint16_t));
                    if (remote_cid != 0) {
                        // check for duplicate remote CIDs
                        l2cap_channel_t * original_channel = l2cap_get_channel_for_remote_handle_and_cid(handle, remote_cid);
                        if (original_channel == NULL){
                            channel->state = L2CAP_STATE_OPEN;
                            channel->remote_cid = remote_cid;
                            channel->remote_mtu = new_mtu;
                            channel->remote_mps = new_mps;
                            channel->credits_outgoing = initial_credits;
                            l2cap_ecbm_emit_channel_opened(channel, ERROR_CODE_SUCCESS);
                            continue;
                        }
                        // close original channel
                        original_channel->state = L2CAP_STATE_WILL_SEND_DISCONNECT_REQUEST;
                        channel_status = ERROR_CODE_ACL_CONNECTION_ALREADY_EXISTS;
                    }
                }
                // open failed
                l2cap_ecbm_emit_channel_opened(channel, channel_status);
                // drop failed channel
                btstack_linked_list_iterator_remove(&it);
                btstack_memory_l2cap_channel_free(channel);
            }
            return 1;

        case L2CAP_CREDIT_BASED_RECONFIGURE_REQUEST:
            // check minimal size
            if (len < 6) return 0u;

            // get hci connection, bail if not found (must not happen)
            connection = hci_connection_for_handle(handle);
            btstack_assert(connection != NULL);

            // get num channels to modify
            num_channels = (len - 4) / sizeof(uint16_t);

            // get new mtu and mps
            new_mtu = little_endian_read_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET + 0);
            new_mps = little_endian_read_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET + 2);

            // validate request
            result = 0;
            for (i = 0; i < num_channels; i++) {
                // check cid
                uint16_t remote_cid = little_endian_read_16(command, 8 + (i * sizeof(uint16_t)));
                l2cap_channel_t *channel = l2cap_get_channel_for_remote_handle_and_cid(handle, remote_cid);
                if (channel == NULL) {
                    result = L2CAP_ECBM_RECONFIGURE_FAILED_DESTINATION_CID_INVALID;
                    break;
                }
                // check MTU is not reduced
                if (channel->remote_mtu > new_mtu) {
                    result = L2CAP_ECBM_RECONFIGURE_FAILED_MTU_REDUCTION_NOT_ALLOWED;
                    break;
                }
                // check MPS reduction
                if ((num_channels > 1) && (channel->remote_mps > new_mps)) {
                    result = L2CAP_ECBM_RECONFIGURE_FAILED_MPS_REDUCTION_MULTIPLE_CHANNELS;
                    break;
                }
                // check MPS valid
                if (new_mps < l2cap_enhanced_mps_min) {
                    result = L2CAP_ECBM_RECONFIGURE_FAILED_UNACCEPTABLE_PARAMETERS;
                    break;
                }
            }

            // send reject
            if (result != 0) {
                l2cap_register_signaling_response(handle, L2CAP_CREDIT_BASED_RECONFIGURE_REQUEST, sig_id, signaling_cid,
                                                  result);
                return 1;
            }

            // update channels
            for (i = 0; i < num_channels; i++) {
                uint16_t remote_cid = little_endian_read_16(command, 8 + (i * sizeof(uint16_t)));
                l2cap_channel_t *channel = l2cap_get_channel_for_remote_handle_and_cid(handle, remote_cid);
                channel->remote_mps = new_mps;
                channel->remote_mtu = new_mtu;
                // emit event
                uint8_t event[8];
                event[0] = L2CAP_EVENT_ECBM_RECONFIGURED;
                event[1] = sizeof(event) - 2;
                little_endian_store_16(event, 2, channel->local_cid);
                little_endian_store_16(event, 4, new_mtu);
                little_endian_store_16(event, 6, new_mps);
                l2cap_dispatch_to_channel(channel, HCI_EVENT_PACKET, event, sizeof(event));
            }

            // send accept
            l2cap_register_signaling_response(handle, L2CAP_CREDIT_BASED_RECONFIGURE_REQUEST, sig_id, signaling_cid, 0);
            return 1;

        case L2CAP_CREDIT_BASED_RECONFIGURE_RESPONSE:
            // check size
            if (len < 2u) return 0u;

            result = little_endian_read_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET + 0);
            btstack_linked_list_iterator_init(&it, &l2cap_channels);
            while (btstack_linked_list_iterator_has_next(&it)) {
                l2cap_channel_t *channel = (l2cap_channel_t *) btstack_linked_list_iterator_next(&it);
                if (!l2cap_is_dynamic_channel_type(channel->channel_type)) continue;
                if (channel->con_handle != handle) continue;
                if (channel->local_sig_id != sig_id) continue;
                if (channel->state != L2CAP_STATE_WAIT_ENHANCED_RENEGOTIATION_RESPONSE) continue;
                // complete request
                if (result == 0) {
                    channel->receive_sdu_buffer = channel->renegotiate_sdu_buffer;
                    channel->local_mtu = channel->renegotiate_mtu;
                }
                channel->state = L2CAP_STATE_OPEN;
                // emit event
                l2cap_ecbm_emit_reconfigure_complete(channel, result);
            }
            break;

        default:
            btstack_unreachable();
            break;
    }
    return 0;
}

#endif

#ifdef ENABLE_BLE

static void l2cap_emit_connection_parameter_update_response(hci_con_handle_t con_handle, uint16_t result){
    uint8_t event[6];
    event[0] = L2CAP_EVENT_CONNECTION_PARAMETER_UPDATE_RESPONSE;
    event[1] = 4;
    little_endian_store_16(event, 2, con_handle);
    little_endian_store_16(event, 4, result);
    l2cap_emit_event(event, sizeof(event));
}

// @return valid
static int l2cap_le_signaling_handler_dispatch(hci_con_handle_t handle, uint8_t * command, uint8_t sig_id){
    hci_connection_t * connection;
    uint16_t result;
    uint8_t  event[12];

#ifdef L2CAP_USES_CREDIT_BASED_CHANNELS
    l2cap_channel_t * channel;
    btstack_linked_list_iterator_t it;
#endif
#ifdef ENABLE_L2CAP_LE_CREDIT_BASED_FLOW_CONTROL_MODE
    uint16_t local_cid;
    uint16_t le_psm;
    l2cap_service_t * service;
    uint16_t source_cid;
#endif

    uint8_t code   = command[L2CAP_SIGNALING_COMMAND_CODE_OFFSET];
    uint16_t len   = little_endian_read_16(command, L2CAP_SIGNALING_COMMAND_LENGTH_OFFSET);
    log_info("le dispatch: command 0x%02x, sig id %u, len %u", code, sig_id, len);

    switch (code){

        case CONNECTION_PARAMETER_UPDATE_REQUEST:
            // check size
            if (len < 8u) return 0u;
            connection = hci_connection_for_handle(handle);
            if (connection != NULL){
                if (connection->role != HCI_ROLE_MASTER){
                    // reject command without notifying upper layer when not in master role
                    return 0;
                }
                le_connection_parameter_range_t existing_range;
                gap_get_connection_parameter_range(&existing_range);
                uint16_t le_conn_interval_min   = little_endian_read_16(command,L2CAP_SIGNALING_COMMAND_DATA_OFFSET);
                uint16_t le_conn_interval_max   = little_endian_read_16(command,L2CAP_SIGNALING_COMMAND_DATA_OFFSET+2);
                uint16_t le_conn_latency        = little_endian_read_16(command,L2CAP_SIGNALING_COMMAND_DATA_OFFSET+4);
                uint16_t le_supervision_timeout = little_endian_read_16(command,L2CAP_SIGNALING_COMMAND_DATA_OFFSET+6);

                int update_parameter = gap_connection_parameter_range_included(&existing_range, le_conn_interval_min, le_conn_interval_max, le_conn_latency, le_supervision_timeout);
                if (update_parameter){
                    connection->le_con_parameter_update_state = CON_PARAMETER_UPDATE_SEND_RESPONSE;
                    connection->le_conn_interval_min = le_conn_interval_min;
                    connection->le_conn_interval_max = le_conn_interval_max;
                    connection->le_conn_latency = le_conn_latency;
                    connection->le_supervision_timeout = le_supervision_timeout;
                } else {
                    connection->le_con_parameter_update_state = CON_PARAMETER_UPDATE_DENY;
                }
                connection->le_con_param_update_identifier = sig_id;
            }

            event[0] = L2CAP_EVENT_CONNECTION_PARAMETER_UPDATE_REQUEST;
            event[1] = 10;
            little_endian_store_16(event, 2, handle);
            (void)memcpy(&event[4], &command[4], 8);
            l2cap_emit_event(event, sizeof(event));
            break;

        case CONNECTION_PARAMETER_UPDATE_RESPONSE:
            // check size
            if (len < 2u) return 0u;
            result = little_endian_read_16(command, 4);
            l2cap_emit_connection_parameter_update_response(handle, result);
            break;

#ifdef ENABLE_L2CAP_ENHANCED_CREDIT_BASED_FLOW_CONTROL_MODE
        case L2CAP_CREDIT_BASED_CONNECTION_REQUEST:
        case L2CAP_CREDIT_BASED_CONNECTION_RESPONSE:
        case L2CAP_CREDIT_BASED_RECONFIGURE_REQUEST:
        case L2CAP_CREDIT_BASED_RECONFIGURE_RESPONSE:
            return l2cap_ecbm_signaling_handler_dispatch(handle, L2CAP_CID_SIGNALING_LE, command, sig_id);
#endif

#ifdef L2CAP_USES_CREDIT_BASED_CHANNELS
        case COMMAND_REJECT:
            btstack_linked_list_iterator_init(&it, &l2cap_channels);
            while (btstack_linked_list_iterator_has_next(&it)){
                channel = (l2cap_channel_t *) btstack_linked_list_iterator_next(&it);
                if (!l2cap_is_dynamic_channel_type(channel->channel_type)) continue;
                if (channel->con_handle   != handle) continue;
                if (channel->local_sig_id != sig_id) continue;
#ifdef ENABLE_L2CAP_LE_CREDIT_BASED_FLOW_CONTROL_MODE
                // if received while waiting for le connection response, assume legacy device
                if (channel->state == L2CAP_STATE_WAIT_LE_CONNECTION_RESPONSE){
                    channel->state = L2CAP_STATE_CLOSED;
                    // no official value for this, use: Connection refused – LE_PSM not supported
                    l2cap_cbm_emit_channel_opened(channel, L2CAP_CBM_CONNECTION_RESULT_SPSM_NOT_SUPPORTED);

                    // discard channel
                    btstack_linked_list_remove(&l2cap_channels, (btstack_linked_item_t *) channel);
                    l2cap_free_channel_entry(channel);
                    continue;
                }
#endif
#ifdef ENABLE_L2CAP_ENHANCED_CREDIT_BASED_FLOW_CONTROL_MODE
                // if received while waiting for le connection response, assume legacy device
                if (channel->state == L2CAP_STATE_WAIT_ENHANCED_CONNECTION_RESPONSE){
                    channel->state = L2CAP_STATE_CLOSED;
                    // treat as SPSM not supported
                    l2cap_ecbm_emit_channel_opened(channel, L2CAP_CONNECTION_RESPONSE_RESULT_REFUSED_PSM);

                    // discard channel
                    btstack_linked_list_remove(&l2cap_channels, (btstack_linked_item_t *) channel);
                    l2cap_free_channel_entry(channel);
                    continue;
                }
#endif
            }
            break;
#endif

#ifdef ENABLE_L2CAP_LE_CREDIT_BASED_FLOW_CONTROL_MODE
        case LE_CREDIT_BASED_CONNECTION_REQUEST:
            // check size
            if (len < 10u) return 0u;

            // get hci connection, bail if not found (must not happen)
            connection = hci_connection_for_handle(handle);
            if (!connection) return 0;

            // check if service registered
            le_psm  = little_endian_read_16(command, 4);
            service = l2cap_cbm_get_service(le_psm);
            source_cid = little_endian_read_16(command, 6);
                
            if (service){
                if (source_cid < 0x40u){
                    l2cap_register_signaling_response(handle, LE_CREDIT_BASED_CONNECTION_REQUEST, sig_id, source_cid, L2CAP_CBM_CONNECTION_RESULT_INVALID_SOURCE_CID);
                    return 1;
                }

                // go through list of channels for this ACL connection and check if we get a match
                btstack_linked_list_iterator_init(&it, &l2cap_channels);
                while (btstack_linked_list_iterator_has_next(&it)){
                    l2cap_channel_t * a_channel = (l2cap_channel_t *) btstack_linked_list_iterator_next(&it);
                    if (!l2cap_is_dynamic_channel_type(a_channel->channel_type)) continue;
                    if (a_channel->con_handle != handle) continue;
                    if (a_channel->remote_cid != source_cid) continue;
                    l2cap_register_signaling_response(handle, LE_CREDIT_BASED_CONNECTION_REQUEST, sig_id, source_cid, L2CAP_CBM_CONNECTION_RESULT_SOURCE_CID_ALREADY_ALLOCATED);
                    return 1;
                }

                // security: check authorization
                if (service->required_security_level >= LEVEL_4){
                    if (gap_authorization_state(handle) != AUTHORIZATION_GRANTED){
                        l2cap_register_signaling_response(handle, LE_CREDIT_BASED_CONNECTION_REQUEST, sig_id, source_cid, L2CAP_CBM_CONNECTION_RESULT_INSUFFICIENT_AUTHORIZATION);
                        return 1;
                    }
                }

                // security: check authentication
                if (service->required_security_level >= LEVEL_3){
                    if (!gap_authenticated(handle)){
                        l2cap_register_signaling_response(handle, LE_CREDIT_BASED_CONNECTION_REQUEST, sig_id, source_cid, L2CAP_CBM_CONNECTION_RESULT_INSUFFICIENT_AUTHENTICATION);
                        return 1;
                    }
                }

                // security: check encryption
                if (service->required_security_level >= LEVEL_2){
                    if (gap_encryption_key_size(handle) == 0){
                        l2cap_register_signaling_response(handle, LE_CREDIT_BASED_CONNECTION_REQUEST, sig_id, source_cid, L2CAP_CBM_CONNECTION_RESULT_INSUFFICIENT_ENCRYPTION);
                        return 1;
                    }
                    // anything less than 16 byte key size is insufficient
                    if (gap_encryption_key_size(handle) < 16){
                        l2cap_register_signaling_response(handle, LE_CREDIT_BASED_CONNECTION_REQUEST, sig_id, source_cid, L2CAP_CBM_CONNECTION_RESULT_ENCYRPTION_KEY_SIZE_TOO_SHORT);
                        return 1;
                    }
                }

                // allocate channel
                channel = l2cap_create_channel_entry(service->packet_handler, L2CAP_CHANNEL_TYPE_CHANNEL_CBM, connection->address,
                                                     connection->address_type, le_psm, service->mtu, service->required_security_level);
                if (!channel){
                    l2cap_register_signaling_response(handle, LE_CREDIT_BASED_CONNECTION_REQUEST, sig_id, source_cid, L2CAP_CBM_CONNECTION_RESULT_NO_RESOURCES_AVAILABLE);
                    return 1;
                }

                channel->con_handle = handle;
                channel->remote_cid = source_cid;
                channel->remote_sig_id = sig_id; 
                channel->remote_mtu = little_endian_read_16(command, 8);
                channel->remote_mps = little_endian_read_16(command, 10);
                channel->credits_outgoing = little_endian_read_16(command, 12);

                // set initial state
                channel->state      = L2CAP_STATE_WAIT_CLIENT_ACCEPT_OR_REJECT;
                channel->state_var |= L2CAP_CHANNEL_STATE_VAR_INCOMING;

                // add to connections list
                btstack_linked_list_add_tail(&l2cap_channels, (btstack_linked_item_t *) channel);

                // post connection request event
                l2cap_cbm_emit_incoming_connection(channel);

            } else {
                l2cap_register_signaling_response(handle, LE_CREDIT_BASED_CONNECTION_REQUEST, sig_id, source_cid, L2CAP_CBM_CONNECTION_RESULT_SPSM_NOT_SUPPORTED);
            }
            break;

        case LE_CREDIT_BASED_CONNECTION_RESPONSE:
            // check size
            if (len < 10u) return 0u;

            // Find channel for this sig_id and connection handle
            channel = NULL;
            btstack_linked_list_iterator_init(&it, &l2cap_channels);
            while (btstack_linked_list_iterator_has_next(&it)){
                l2cap_channel_t * a_channel = (l2cap_channel_t *) btstack_linked_list_iterator_next(&it);
                if (!l2cap_is_dynamic_channel_type(a_channel->channel_type)) continue;
                if (a_channel->con_handle   != handle) continue;
                if (a_channel->local_sig_id != sig_id) continue;
                channel = a_channel;
                break; 
            }
            if (!channel) break;

            // cid + 0
            result = little_endian_read_16 (command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET+8);
            if (result){
                channel->state = L2CAP_STATE_CLOSED;
                uint8_t status = l2cap_cbm_status_for_result(result);
                l2cap_cbm_emit_channel_opened(channel, status);
                                
                // discard channel
                btstack_linked_list_remove(&l2cap_channels, (btstack_linked_item_t *) channel);
                l2cap_free_channel_entry(channel);
                break;
            }

            // success
            channel->remote_cid = little_endian_read_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET + 0);
            channel->remote_mtu = little_endian_read_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET + 2);
            channel->remote_mps = little_endian_read_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET + 4);
            channel->credits_outgoing = little_endian_read_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET + 6);
            channel->state = L2CAP_STATE_OPEN;
            l2cap_cbm_emit_channel_opened(channel, ERROR_CODE_SUCCESS);
            break;
#endif

#ifdef L2CAP_USES_CREDIT_BASED_CHANNELS
        case L2CAP_FLOW_CONTROL_CREDIT_INDICATION:
            return l2cap_credit_based_handle_credit_indication(handle, command, len) ? 1 : 0;

        case DISCONNECTION_REQUEST:
            // check size
            if (len < 4u) return 0u;

            // find channel
            local_cid = little_endian_read_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET + 0);
            channel = l2cap_get_channel_for_local_cid_and_handle(local_cid, handle);
            if (!channel) {
                l2cap_register_signaling_response(handle, DISCONNECTION_REQUEST, sig_id,
                                                  little_endian_read_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET + 2), local_cid);
                break;
            }
            channel->remote_sig_id = sig_id;
            channel->state = L2CAP_STATE_WILL_SEND_DISCONNECT_RESPONSE;
            break;

        case DISCONNECTION_RESPONSE:
            // check size
            if (len < 4u) return 0u;

            // find channel
            local_cid = little_endian_read_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET + 0);
            channel = l2cap_get_channel_for_local_cid_and_handle(local_cid, handle);
            if (!channel) break;
            if (channel->local_sig_id == sig_id) {
                if (channel->state == L2CAP_STATE_WAIT_DISCONNECT){
                    l2cap_finalize_channel_close(channel);
                }
            }
            break;
#endif

        default:
            // command unknown -> reject command
            return 0;
    }
    return 1;
}
#endif

#ifdef ENABLE_CLASSIC
static void l2cap_acl_classic_handler_for_channel(l2cap_channel_t * l2cap_channel, uint8_t * packet, uint16_t size){

    // forward data only in OPEN state
    if (l2cap_channel->state != L2CAP_STATE_OPEN) return;

#ifdef L2CAP_USES_CREDIT_BASED_CHANNELS
    if (l2cap_channel->channel_type == L2CAP_CHANNEL_TYPE_CHANNEL_ECBM){
        l2cap_credit_based_handle_pdu(l2cap_channel, packet, size);
        return;
    }
#endif

#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
    if (l2cap_channel->mode == L2CAP_CHANNEL_MODE_ENHANCED_RETRANSMISSION){

        int fcs_size = l2cap_channel->fcs_active ? 2 : 0;

        // assert control + FCS fields are inside
        if (size < COMPLETE_L2CAP_HEADER+2+fcs_size) return;

        if (fcs_size > 0){
            // verify FCS (required if one side requested it)
            uint16_t fcs_calculated = crc16_calc(&packet[4], size - (4+2));
            uint16_t fcs_packet     = little_endian_read_16(packet, size-2);

#ifdef L2CAP_ERTM_SIMULATE_FCS_ERROR_INTERVAL
            // simulate fcs error
            static int counter = 0;
            if (++counter == L2CAP_ERTM_SIMULATE_FCS_ERROR_INTERVAL) {
                log_info("Simulate fcs error");
                fcs_calculated++;
                counter = 0;
            }
#endif

            if (fcs_calculated == fcs_packet){
                log_info("Packet FCS 0x%04x verified", fcs_packet);
            } else {
                log_error("FCS mismatch! Packet 0x%04x, calculated 0x%04x", fcs_packet, fcs_calculated);
                // ERTM State Machine in Bluetooth Spec does not handle 'I-Frame with invalid FCS'
                return;
            }
        }

        // switch on packet type
        uint16_t control = little_endian_read_16(packet, COMPLETE_L2CAP_HEADER);
        uint8_t  req_seq = (control >> 8) & 0x3f;
        int final = (control >> 7) & 0x01;
        if (control & 1){
            // S-Frame
            int poll  = (control >> 4) & 0x01;
            l2cap_supervisory_function_t s = (l2cap_supervisory_function_t) ((control >> 2) & 0x03);
            log_info("Control: 0x%04x => Supervisory function %u, ReqSeq %02u", control, (int) s, req_seq);
            l2cap_ertm_tx_packet_state_t * tx_state;
            switch (s){
                case L2CAP_SUPERVISORY_FUNCTION_RR_RECEIVER_READY:
                    log_info("L2CAP_SUPERVISORY_FUNCTION_RR_RECEIVER_READY");
                    l2cap_ertm_process_req_seq(l2cap_channel, req_seq);
                    if (poll && final){
                        // S-frames shall not be transmitted with both the F-bit and the P-bit set to 1 at the same time.
                        log_error("P=F=1 in S-Frame");
                        break;
                    }
                    if (poll){
                        // check if we did request selective retransmission before <==> we have stored SDU segments
                        int i;
                        int num_stored_out_of_order_packets = 0;
                        for (i=0;i<l2cap_channel->num_rx_buffers;i++){
                            int index = l2cap_channel->rx_store_index + i;
                            if (index >= l2cap_channel->num_rx_buffers){
                                index -= l2cap_channel->num_rx_buffers;
                            }
                            l2cap_ertm_rx_packet_state_t * rx_state = &l2cap_channel->rx_packets_state[index];
                            if (!rx_state->valid) continue;
                            num_stored_out_of_order_packets++;
                        }
                        if (num_stored_out_of_order_packets){
                            l2cap_channel->send_supervisor_frame_selective_reject = 1;
                        } else {
                            l2cap_channel->send_supervisor_frame_receiver_ready   = 1;
                        }
                        l2cap_channel->set_final_bit_after_packet_with_poll_bit_set = 1;
                    }
                    if (final){
                        // Stop-MonitorTimer
                        l2cap_ertm_stop_monitor_timer(l2cap_channel);
                        // If UnackedFrames > 0 then Start-RetransTimer
                        if (l2cap_channel->unacked_frames){
                            l2cap_ertm_start_retransmission_timer(l2cap_channel);
                        }
                        // final bit set <- response to RR with poll bit set. All not acknowledged packets need to be retransmitted
                        l2cap_ertm_retransmit_unacknowleded_frames(l2cap_channel);
                    }
                    break;
                case L2CAP_SUPERVISORY_FUNCTION_REJ_REJECT:
                    log_info("L2CAP_SUPERVISORY_FUNCTION_REJ_REJECT");
                    l2cap_ertm_process_req_seq(l2cap_channel, req_seq);
                    // restart transmittion from last unacknowledted packet (earlier packets already freed in l2cap_ertm_process_req_seq)
                    l2cap_ertm_retransmit_unacknowleded_frames(l2cap_channel);
                    break;
                case L2CAP_SUPERVISORY_FUNCTION_RNR_RECEIVER_NOT_READY:
                    log_error("L2CAP_SUPERVISORY_FUNCTION_RNR_RECEIVER_NOT_READY");
                    break;
                case L2CAP_SUPERVISORY_FUNCTION_SREJ_SELECTIVE_REJECT:
                    log_info("L2CAP_SUPERVISORY_FUNCTION_SREJ_SELECTIVE_REJECT");
                    if (poll){
                        l2cap_ertm_process_req_seq(l2cap_channel, req_seq);
                    }
                    // find requested i-frame
                    tx_state = l2cap_ertm_get_tx_state(l2cap_channel, req_seq);
                    if (tx_state){
                        log_info("Retransmission for tx_seq %u requested", req_seq);
                        l2cap_channel->set_final_bit_after_packet_with_poll_bit_set = poll;
                        tx_state->retransmission_requested = 1;
                        l2cap_channel->srej_active = 1;
                    }
                    break;
                default:
                    break;
            }
        } else {
            // I-Frame
            // get control
            l2cap_segmentation_and_reassembly_t sar = (l2cap_segmentation_and_reassembly_t) (control >> 14);
            uint8_t tx_seq = (control >> 1) & 0x3f;
            log_info("Control: 0x%04x => SAR %u, ReqSeq %02u, R?, TxSeq %02u", control, (int) sar, req_seq, tx_seq);
            log_info("SAR: pos %u", l2cap_channel->reassembly_pos);
            log_info("State: expected_tx_seq %02u, req_seq %02u", l2cap_channel->expected_tx_seq, l2cap_channel->req_seq);
            l2cap_ertm_process_req_seq(l2cap_channel, req_seq);
            if (final){
                // final bit set <- response to RR with poll bit set. All not acknowledged packets need to be retransmitted
                l2cap_ertm_retransmit_unacknowleded_frames(l2cap_channel);
            }

            // get SDU
            const uint8_t * payload_data = &packet[COMPLETE_L2CAP_HEADER+2];
            uint16_t        payload_len  = size-(COMPLETE_L2CAP_HEADER+2+fcs_size);

            // assert SDU size is smaller or equal to our buffers
            uint16_t max_payload_size = 0;
            switch (sar){
                case L2CAP_SEGMENTATION_AND_REASSEMBLY_UNSEGMENTED_L2CAP_SDU:
                case L2CAP_SEGMENTATION_AND_REASSEMBLY_START_OF_L2CAP_SDU:
                    // SDU Length + MPS
                    max_payload_size = l2cap_channel->local_mps + 2;
                    break;
                case L2CAP_SEGMENTATION_AND_REASSEMBLY_CONTINUATION_OF_L2CAP_SDU:
                case L2CAP_SEGMENTATION_AND_REASSEMBLY_END_OF_L2CAP_SDU:
                    max_payload_size = l2cap_channel->local_mps;
                    break;
                default:
                    btstack_assert(false);
                    break;
            }
            if (payload_len > max_payload_size){
                log_info("payload len %u > max payload %u -> drop packet", payload_len, max_payload_size);
                return;
            }

            // check ordering
            if (l2cap_channel->expected_tx_seq == tx_seq){
                log_info("Received expected frame with TxSeq == ExpectedTxSeq == %02u", tx_seq);
                l2cap_channel->expected_tx_seq = l2cap_next_ertm_seq_nr(l2cap_channel->expected_tx_seq);
                l2cap_channel->req_seq         = l2cap_channel->expected_tx_seq;

                // process SDU
                l2cap_ertm_handle_in_sequence_sdu(l2cap_channel, sar, payload_data, payload_len);

                // process stored segments
                while (true){
                    int index = l2cap_channel->rx_store_index;
                    l2cap_ertm_rx_packet_state_t * rx_state = &l2cap_channel->rx_packets_state[index];
                    if (!rx_state->valid) break;

                    log_info("Processing stored frame with TxSeq == ExpectedTxSeq == %02u", l2cap_channel->expected_tx_seq);
                    l2cap_channel->expected_tx_seq = l2cap_next_ertm_seq_nr(l2cap_channel->expected_tx_seq);
                    l2cap_channel->req_seq         = l2cap_channel->expected_tx_seq;

                    rx_state->valid = 0;
                    l2cap_ertm_handle_in_sequence_sdu(l2cap_channel, rx_state->sar, &l2cap_channel->rx_packets_data[index], rx_state->len);

                    // update rx store index
                    index++;
                    if (index >= l2cap_channel->num_rx_buffers){
                        index = 0;
                    }
                    l2cap_channel->rx_store_index = index;
                }

                //
                l2cap_channel->send_supervisor_frame_receiver_ready = 1;

            } else {
                int delta = (tx_seq - l2cap_channel->expected_tx_seq) & 0x3f;
                if (delta < 2){
                    // store segment
                    l2cap_ertm_handle_out_of_sequence_sdu(l2cap_channel, sar, delta, payload_data, payload_len);

                    log_info("Received unexpected frame TxSeq %u but expected %u -> send S-SREJ", tx_seq, l2cap_channel->expected_tx_seq);
                    l2cap_channel->send_supervisor_frame_selective_reject = 1;
                } else {
                    log_info("Received unexpected frame TxSeq %u but expected %u -> send S-REJ", tx_seq, l2cap_channel->expected_tx_seq);
                    l2cap_channel->send_supervisor_frame_reject = 1;
                }
            }
        }
        return;
    }
#endif

    // check size
    uint16_t payload_size = size - COMPLETE_L2CAP_HEADER;
    if (l2cap_channel->local_mtu < payload_size) return;

    l2cap_dispatch_to_channel(l2cap_channel, L2CAP_DATA_PACKET, &packet[COMPLETE_L2CAP_HEADER], payload_size);
}
#endif

static void l2cap_acl_classic_handler(hci_con_handle_t handle, uint8_t *packet, uint16_t size){
#ifdef ENABLE_CLASSIC
    l2cap_channel_t * l2cap_channel;
    l2cap_fixed_channel_t * l2cap_fixed_channel;

    uint16_t channel_id = READ_L2CAP_CHANNEL_ID(packet);
    uint8_t  broadcast_flag = READ_ACL_FLAGS(packet) >> 2;
    switch (channel_id) {
            
        case L2CAP_CID_SIGNALING: {
            if (broadcast_flag != 0) break;
            uint32_t command_offset = 8;
            while ((command_offset + L2CAP_SIGNALING_COMMAND_DATA_OFFSET) <= size) {
                // assert signaling command is fully inside packet
                uint16_t data_len = little_endian_read_16(packet, command_offset + L2CAP_SIGNALING_COMMAND_LENGTH_OFFSET);
                uint32_t next_command_offset = command_offset + L2CAP_SIGNALING_COMMAND_DATA_OFFSET + data_len;
                if (next_command_offset > size){
                    log_error("signaling command incomplete -> drop");
                    break;
                }
                // handle signaling command
                l2cap_signaling_handler_dispatch(handle, &packet[command_offset]);
                // go to next command
                command_offset = next_command_offset;
            }
            // handle incomplete packet
            if (command_offset < size) {
                log_error("signaling command incomplete -> reject");
                l2cap_register_signaling_response(handle, COMMAND_REJECT, 0, 0, L2CAP_REJ_CMD_UNKNOWN);
            }
            break;
        }

        case L2CAP_CID_CONNECTIONLESS_CHANNEL:
            if (broadcast_flag == 0) break;
            l2cap_fixed_channel = l2cap_fixed_channel_for_channel_id(L2CAP_CID_CONNECTIONLESS_CHANNEL);
            if (!l2cap_fixed_channel) break;
            if (!l2cap_fixed_channel->packet_handler) break;
            (*l2cap_fixed_channel->packet_handler)(UCD_DATA_PACKET, handle, &packet[COMPLETE_L2CAP_HEADER], size-COMPLETE_L2CAP_HEADER);
            break;

#ifdef ENABLE_BLE
        case L2CAP_CID_BR_EDR_SECURITY_MANAGER:
            l2cap_fixed_channel = l2cap_fixed_channel_for_channel_id(L2CAP_CID_BR_EDR_SECURITY_MANAGER);
            if ((l2cap_fixed_channel == NULL) || (l2cap_fixed_channel->packet_handler == NULL)){
                // Pairing Failed
                l2cap_register_signaling_response(handle, (uint8_t) SM_PAIRING_FAILED, 0, L2CAP_CID_BR_EDR_SECURITY_MANAGER, SM_REASON_PAIRING_NOT_SUPPORTED);
            } else {
                (*l2cap_fixed_channel->packet_handler)(SM_DATA_PACKET, handle, &packet[COMPLETE_L2CAP_HEADER], size-COMPLETE_L2CAP_HEADER);
            }
            break;
#endif

        default:
            if (broadcast_flag != 0) break;
            // Find channel for this channel_id and connection handle
            l2cap_channel = l2cap_get_channel_for_local_cid_and_handle(channel_id, handle);
            if (l2cap_channel != NULL){
                l2cap_acl_classic_handler_for_channel(l2cap_channel, packet, size);
            }
            break;
    }
#else
    UNUSED(handle); // ok: no code
    UNUSED(packet); // ok: no code
    UNUSED(size);   // ok: no code
#endif
}

static void l2cap_acl_le_handler(hci_con_handle_t handle, uint8_t *packet, uint16_t size){
#ifdef ENABLE_BLE

    l2cap_fixed_channel_t * l2cap_fixed_channel;

#ifdef ENABLE_L2CAP_LE_CREDIT_BASED_FLOW_CONTROL_MODE
    l2cap_channel_t * l2cap_channel;
#endif
    uint16_t channel_id = READ_L2CAP_CHANNEL_ID(packet);
    switch (channel_id) {

        case L2CAP_CID_SIGNALING_LE: {
            uint8_t sig_id = packet[COMPLETE_L2CAP_HEADER + 1];
            uint16_t len = little_endian_read_16(packet, COMPLETE_L2CAP_HEADER + 2);
            if ((COMPLETE_L2CAP_HEADER + 4u + len) > size) break;
            int      valid  = l2cap_le_signaling_handler_dispatch(handle, &packet[COMPLETE_L2CAP_HEADER], sig_id);
            if (!valid){
                l2cap_register_signaling_response(handle, COMMAND_REJECT_LE, sig_id, 0, L2CAP_REJ_CMD_UNKNOWN);
            }
            break;
        }

        case L2CAP_CID_ATTRIBUTE_PROTOCOL:
            l2cap_fixed_channel = l2cap_fixed_channel_for_channel_id(L2CAP_CID_ATTRIBUTE_PROTOCOL);
            if (!l2cap_fixed_channel) break;
            if (!l2cap_fixed_channel->packet_handler) break;
            (*l2cap_fixed_channel->packet_handler)(ATT_DATA_PACKET, handle, &packet[COMPLETE_L2CAP_HEADER], size-COMPLETE_L2CAP_HEADER);
            break;

        case L2CAP_CID_SECURITY_MANAGER_PROTOCOL:
            l2cap_fixed_channel = l2cap_fixed_channel_for_channel_id(L2CAP_CID_SECURITY_MANAGER_PROTOCOL);
            if ((l2cap_fixed_channel == NULL) || (l2cap_fixed_channel->packet_handler == NULL)){
                // Pairing Failed
                l2cap_register_signaling_response(handle, (uint8_t) SM_PAIRING_FAILED, 0, L2CAP_CID_SECURITY_MANAGER_PROTOCOL, SM_REASON_PAIRING_NOT_SUPPORTED);
            } else {
                (*l2cap_fixed_channel->packet_handler)(SM_DATA_PACKET, handle, &packet[COMPLETE_L2CAP_HEADER], size-COMPLETE_L2CAP_HEADER);
            }
            break;

        default:

#ifdef ENABLE_L2CAP_LE_CREDIT_BASED_FLOW_CONTROL_MODE
            l2cap_channel = l2cap_get_channel_for_local_cid_and_handle(channel_id, handle);
            if (l2cap_channel != NULL) {
                l2cap_credit_based_handle_pdu(l2cap_channel, packet, size);
            }
#endif
            break;
    }
#else
    UNUSED(handle); // ok: no code
    UNUSED(packet); // ok: no code
    UNUSED(size);   // ok: no code
#endif
}

static void l2cap_acl_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type);    // ok: registered with hci_register_acl_packet_handler
    UNUSED(channel);        // ok: there is no channel

    // Assert full L2CAP header present
    if (size < COMPLETE_L2CAP_HEADER) return;

    // Dispatch to Classic or LE handler (SCO packets are not dispatched to L2CAP)
    hci_con_handle_t handle = READ_ACL_CONNECTION_HANDLE(packet);
    hci_connection_t *conn = hci_connection_for_handle(handle);
    if (!conn) return;
    if (conn->address_type == BD_ADDR_TYPE_ACL){
        l2cap_acl_classic_handler(handle, packet, size);
    } else {
        l2cap_acl_le_handler(handle, packet, size);
    }

    l2cap_run();
}

// Bluetooth 4.0 - allows to register handler for Attribute Protocol and Security Manager Protocol
void l2cap_register_fixed_channel(btstack_packet_handler_t packet_handler, uint16_t channel_id) {
    l2cap_fixed_channel_t * channel = l2cap_fixed_channel_for_channel_id(channel_id);
    if (!channel) return;
    channel->packet_handler = packet_handler;
}

#ifdef L2CAP_USES_CHANNELS
// finalize closed channel - l2cap_handle_disconnect_request & DISCONNECTION_RESPONSE
void l2cap_finalize_channel_close(l2cap_channel_t * channel){
    channel->state = L2CAP_STATE_CLOSED;
    l2cap_handle_channel_closed(channel);
    // discard channel
    btstack_linked_list_remove(&l2cap_channels, (btstack_linked_item_t *) channel);
    l2cap_free_channel_entry(channel);
}
#endif

#ifdef ENABLE_CLASSIC
static inline l2cap_service_t * l2cap_get_service(uint16_t psm){
    return l2cap_get_service_internal(&l2cap_services, psm);
}

static void l2cap_update_minimal_security_level(void){
    // update minimal service security level
    gap_security_level_t minimal_level = LEVEL_1;
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &l2cap_services);
    while (btstack_linked_list_iterator_has_next(&it)){
        l2cap_service_t * service = (l2cap_service_t *) btstack_linked_list_iterator_next(&it);
        if (service->required_security_level > minimal_level){
            minimal_level = service->required_security_level;
        };
    }
    gap_set_minimal_service_security_level(minimal_level);
}

uint8_t l2cap_register_service(btstack_packet_handler_t service_packet_handler, uint16_t psm, uint16_t mtu, gap_security_level_t security_level){
    
    log_info("L2CAP_REGISTER_SERVICE psm 0x%x mtu %u", psm, mtu);
    
    // check for alread registered psm 
    l2cap_service_t *service = l2cap_get_service(psm);
    if (service) {
        log_error("register: PSM %u already registered", psm);
        return L2CAP_SERVICE_ALREADY_REGISTERED;
    }
    
    // alloc structure
    service = btstack_memory_l2cap_service_get();
    if (!service) {
        log_error("register: no memory for l2cap_service_t");
        return BTSTACK_MEMORY_ALLOC_FAILED;
    }
    
    // fill in 
    service->psm = psm;
    service->mtu = mtu;
    service->packet_handler = service_packet_handler;
    service->required_security_level = security_level;

    // add to services list
    btstack_linked_list_add(&l2cap_services, (btstack_linked_item_t *) service);

    l2cap_update_minimal_security_level();

#ifndef ENABLE_EXPLICIT_CONNECTABLE_MODE_CONTROL
    // enable page scan
    gap_connectable_control(1);
#endif


    return ERROR_CODE_SUCCESS;
}

uint8_t l2cap_unregister_service(uint16_t psm){
    
    log_info("unregister psm 0x%x", psm);

    l2cap_service_t *service = l2cap_get_service(psm);
    if (!service) return L2CAP_SERVICE_DOES_NOT_EXIST;
    btstack_linked_list_remove(&l2cap_services, (btstack_linked_item_t *) service);
    btstack_memory_l2cap_service_free(service);

    l2cap_update_minimal_security_level();

#ifndef ENABLE_EXPLICIT_CONNECTABLE_MODE_CONTROL
    // disable page scan when no services registered
    if (btstack_linked_list_empty(&l2cap_services)) {
        gap_connectable_control(0);
    }
#endif

    return ERROR_CODE_SUCCESS;
}
#endif


#ifdef L2CAP_USES_CREDIT_BASED_CHANNELS

static void l2cap_credit_based_send_pdu(l2cap_channel_t *channel) {
    btstack_assert(channel != NULL);
    btstack_assert(channel->send_sdu_buffer != NULL);
    btstack_assert(channel->credits_outgoing > 0);

    // send part of SDU
    hci_reserve_packet_buffer();
    uint8_t *acl_buffer = hci_get_outgoing_packet_buffer();
    uint8_t *l2cap_payload = acl_buffer + 8;
    uint16_t pos = 0;
    if (!channel->send_sdu_pos) {
        // store SDU len
        channel->send_sdu_pos += 2u;
        little_endian_store_16(l2cap_payload, pos, channel->send_sdu_len);
        pos += 2u;
    }
    uint16_t payload_size = btstack_min(channel->send_sdu_len + 2u - channel->send_sdu_pos, channel->remote_mps - pos);
    log_info("len %u, pos %u => payload %u, credits %u", channel->send_sdu_len, channel->send_sdu_pos, payload_size,
             channel->credits_outgoing);
    (void) memcpy(&l2cap_payload[pos],
                  &channel->send_sdu_buffer[channel->send_sdu_pos - 2u],
                  payload_size); // -2 for virtual SDU len
    pos += payload_size;
    channel->send_sdu_pos += payload_size;
    l2cap_setup_header(acl_buffer, channel->con_handle, 0, channel->remote_cid, pos);

    channel->credits_outgoing--;

    // update state (mark SDU as done) before calling hci_send_acl_packet_buffer (trigger l2cap_le_send_pdu again)
    bool done = channel->send_sdu_pos >= (channel->send_sdu_len + 2u);
    if (done) {
        channel->send_sdu_buffer = NULL;
    }

    hci_send_acl_packet_buffer(8u + pos);

    if (done) {
        // send done event
        l2cap_emit_simple_event_with_cid(channel, L2CAP_EVENT_PACKET_SENT);
        // inform about can send now
        l2cap_credit_based_notify_channel_can_send(channel);
    }
}

static uint8_t l2cap_credit_based_send_data(l2cap_channel_t * channel, const uint8_t * data, uint16_t size){

    if (size > channel->remote_mtu){
        log_error("l2cap send, cid 0x%02x, data length exceeds remote MTU.", channel->local_cid);
        return L2CAP_DATA_LEN_EXCEEDS_REMOTE_MTU;
    }

    if (channel->send_sdu_buffer){
        log_info("l2cap send, cid 0x%02x, cannot send", channel->local_cid);
        return BTSTACK_ACL_BUFFERS_FULL;
    }

    channel->send_sdu_buffer = data;
    channel->send_sdu_len    = size;
    channel->send_sdu_pos    = 0;

    l2cap_notify_channel_can_send();
    return ERROR_CODE_SUCCESS;
}

static uint8_t l2cap_credit_based_provide_credits(uint16_t local_cid, uint16_t credits){
    l2cap_channel_t * channel = l2cap_get_channel_for_local_cid(local_cid);
    if (!channel) {
        log_error("le credits no channel for cid 0x%02x", local_cid);
        return L2CAP_LOCAL_CID_DOES_NOT_EXIST;
    }

    // check state
    if (channel->state != L2CAP_STATE_OPEN){
        log_error("le credits but channel 0x%02x not open yet", local_cid);
    }

    // ignore if set to automatic credits
    if (channel->automatic_credits) return ERROR_CODE_SUCCESS;

    // assert incoming credits + credits <= 0xffff
    uint32_t total_credits = channel->credits_incoming;
    total_credits += channel->new_credits_incoming;
    total_credits += credits;
    if (total_credits > 0xffffu){
        log_error("le credits overrun: current %u, scheduled %u, additional %u", channel->credits_incoming,
                  channel->new_credits_incoming, credits);
    }

    // set credits_granted
    channel->new_credits_incoming += credits;

    // go
    l2cap_run();
    return ERROR_CODE_SUCCESS;
}

static void l2cap_credit_based_send_credits(l2cap_channel_t *channel) {
    log_info("l2cap: sending %u credits", channel->new_credits_incoming);
    channel->local_sig_id = l2cap_next_sig_id();
    uint16_t new_credits = channel->new_credits_incoming;
    channel->new_credits_incoming = 0;
    channel->credits_incoming += new_credits;
    uint16_t signaling_cid = channel->address_type == BD_ADDR_TYPE_ACL ? L2CAP_CID_SIGNALING : L2CAP_CID_SIGNALING_LE;
    l2cap_send_general_signaling_packet(channel->con_handle, signaling_cid, L2CAP_FLOW_CONTROL_CREDIT_INDICATION, channel->local_sig_id, channel->local_cid, new_credits);
}

// @return valid
static bool l2cap_credit_based_handle_credit_indication(hci_con_handle_t handle, const uint8_t * command, uint16_t len){
    // check size
    if (len < 4u) return false;

    // find channel
    uint16_t remote_cid = little_endian_read_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET + 0);
    l2cap_channel_t * channel = l2cap_get_channel_for_remote_handle_and_cid(handle, remote_cid);
    if (!channel) {
        log_error("credit: no channel for handle 0x%04x/cid 0x%02x", handle, remote_cid);
        return true;
    }
    uint16_t new_credits = little_endian_read_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET + 2);
    uint16_t credits_before = channel->credits_outgoing;
    channel->credits_outgoing += new_credits;
    // check for credit overrun
    if (credits_before > channel->credits_outgoing) {
        log_error("credit: new credits caused overrrun for cid 0x%02x, disconnecting", channel->local_cid);
        channel->state = L2CAP_STATE_WILL_SEND_DISCONNECT_REQUEST;
        return true;
    }
    log_info("credit: %u credits for 0x%02x, now %u", new_credits, channel->local_cid, channel->credits_outgoing);
    l2cap_call_notify_channel_in_run = true;
    return true;
}

static void l2cap_credit_based_handle_pdu(l2cap_channel_t * l2cap_channel, const uint8_t * packet, uint16_t size){
    // ignore empty packets
    if (size == COMPLETE_L2CAP_HEADER) return;

    // credit counting
    if (l2cap_channel->credits_incoming == 0u){
        log_info("(e)CBM: packet received but no incoming credits");
        l2cap_channel->state = L2CAP_STATE_WILL_SEND_DISCONNECT_REQUEST;
        return;
    }
    l2cap_channel->credits_incoming--;

    // automatic credits
    if ((l2cap_channel->credits_incoming < L2CAP_CREDIT_BASED_FLOW_CONTROL_MODE_AUTOMATIC_CREDITS_WATERMARK) && l2cap_channel->automatic_credits){
        l2cap_channel->new_credits_incoming = L2CAP_CREDIT_BASED_FLOW_CONTROL_MODE_AUTOMATIC_CREDITS_INCREMENT;
    }

    // first fragment
    uint16_t pos = 0;
    if (!l2cap_channel->receive_sdu_len){
        if (size < (COMPLETE_L2CAP_HEADER + 2)) return;
        uint16_t sdu_len = little_endian_read_16(packet, COMPLETE_L2CAP_HEADER);
        if (sdu_len > l2cap_channel->local_mtu) {
            log_info("(e)CBM: packet received larger than MTU");
            l2cap_channel->state = L2CAP_STATE_WILL_SEND_DISCONNECT_REQUEST;
            return;
        }
        l2cap_channel->receive_sdu_len = sdu_len;
        l2cap_channel->receive_sdu_pos = 0;
        pos  += 2u;
        size -= 2u;
    }

    uint16_t fragment_size   = size-COMPLETE_L2CAP_HEADER;

    // check fragment_size
    if (fragment_size > l2cap_channel->local_mps) {
        log_info("(e)CBM: fragment larger than local MPS");
        l2cap_channel->state = L2CAP_STATE_WILL_SEND_DISCONNECT_REQUEST;
        return;
    }

    // check sdu overrun
    if ((l2cap_channel->receive_sdu_pos + fragment_size) > l2cap_channel->receive_sdu_len){
        log_info("(e)CBM: fragments larger than SDU");
        l2cap_channel->state = L2CAP_STATE_WILL_SEND_DISCONNECT_REQUEST;
        return;
    }

    (void)memcpy(&l2cap_channel->receive_sdu_buffer[l2cap_channel->receive_sdu_pos],
                 &packet[COMPLETE_L2CAP_HEADER + pos],
                 fragment_size);
    l2cap_channel->receive_sdu_pos += fragment_size;

    // done?
    log_debug("le packet pos %u, len %u", l2cap_channel->receive_sdu_pos, l2cap_channel->receive_sdu_len);
    if (l2cap_channel->receive_sdu_pos >= l2cap_channel->receive_sdu_len){
        l2cap_dispatch_to_channel(l2cap_channel, L2CAP_DATA_PACKET, l2cap_channel->receive_sdu_buffer, l2cap_channel->receive_sdu_len);
        l2cap_channel->receive_sdu_len = 0;
    }
}

static void l2cap_credit_based_notify_channel_can_send(l2cap_channel_t *channel){
    if (!channel->waiting_for_can_send_now) return;
    if (channel->send_sdu_buffer) return;
    channel->waiting_for_can_send_now = 0;
    log_debug("le can send now, local_cid 0x%x", channel->local_cid);
    l2cap_emit_simple_event_with_cid(channel, L2CAP_EVENT_CAN_SEND_NOW);
}

static uint16_t l2cap_credit_based_available_credits(uint16_t local_cid){
    l2cap_channel_t * channel = l2cap_get_channel_for_local_cid(local_cid);
    if (channel != NULL) {
        return channel->credits_outgoing;
    }
    return 0;
}

#endif

#ifdef ENABLE_L2CAP_LE_CREDIT_BASED_FLOW_CONTROL_MODE
// 1BH2222
static void l2cap_cbm_emit_incoming_connection(l2cap_channel_t *channel) {
    log_info("le incoming addr_type %u, addr %s handle 0x%x psm 0x%x local_cid 0x%x remote_cid 0x%x, remote_mtu %u",
             channel->address_type, bd_addr_to_str(channel->address), channel->con_handle,  channel->psm, channel->local_cid, channel->remote_cid, channel->remote_mtu);
    uint8_t event[19];
    event[0] = L2CAP_EVENT_CBM_INCOMING_CONNECTION;
    event[1] = sizeof(event) - 2u;
    event[2] = channel->address_type;
    reverse_bd_addr(channel->address, &event[3]);
    little_endian_store_16(event,  9, channel->con_handle);
    little_endian_store_16(event, 11, channel->psm);
    little_endian_store_16(event, 13, channel->local_cid);
    little_endian_store_16(event, 15, channel->remote_cid);
    little_endian_store_16(event, 17, channel->remote_mtu);
    hci_dump_btstack_event( event, sizeof(event));
    l2cap_dispatch_to_channel(channel, HCI_EVENT_PACKET, event, sizeof(event));
}
// 11BH22222
static void l2cap_cbm_emit_channel_opened(l2cap_channel_t *channel, uint8_t status) {
    log_info("opened le channel status 0x%x addr_type %u addr %s handle 0x%x psm 0x%x local_cid 0x%x remote_cid 0x%x local_mtu %u, remote_mtu %u",
             status, channel->address_type, bd_addr_to_str(channel->address), channel->con_handle, channel->psm,
             channel->local_cid, channel->remote_cid, channel->local_mtu, channel->remote_mtu);
    uint8_t event[23];
    event[0] = L2CAP_EVENT_CBM_CHANNEL_OPENED;
    event[1] = sizeof(event) - 2u;
    event[2] = status;
    event[3] = channel->address_type;
    reverse_bd_addr(channel->address, &event[4]);
    little_endian_store_16(event, 10, channel->con_handle);
    event[12] = (channel->state_var & L2CAP_CHANNEL_STATE_VAR_INCOMING) ? 1 : 0;
    little_endian_store_16(event, 13, channel->psm);
    little_endian_store_16(event, 15, channel->local_cid);
    little_endian_store_16(event, 17, channel->remote_cid);
    little_endian_store_16(event, 19, channel->local_mtu);
    little_endian_store_16(event, 21, channel->remote_mtu); 
    hci_dump_btstack_event(  event, sizeof(event));
    l2cap_dispatch_to_channel(channel, HCI_EVENT_PACKET, event, sizeof(event));
}

// finalize closed channel - l2cap_handle_disconnect_request & DISCONNECTION_RESPONSE
static void l2cap_cbm_finalize_channel_close(l2cap_channel_t * channel){
    channel->state = L2CAP_STATE_CLOSED;
    l2cap_emit_simple_event_with_cid(channel, L2CAP_EVENT_CHANNEL_CLOSED);
    // discard channel
    btstack_linked_list_remove(&l2cap_channels, (btstack_linked_item_t *) channel);
    l2cap_free_channel_entry(channel);
}

static inline l2cap_service_t * l2cap_cbm_get_service(uint16_t le_psm){
    return l2cap_get_service_internal(&l2cap_le_services, le_psm);
}

uint8_t l2cap_cbm_register_service(btstack_packet_handler_t packet_handler, uint16_t psm, gap_security_level_t security_level){
    
    log_info("l2cap_cbm_register_service psm 0x%x", psm);
    
    // check for alread registered psm 
    l2cap_service_t *service = l2cap_cbm_get_service(psm);
    if (service) {
        return L2CAP_SERVICE_ALREADY_REGISTERED;
    }
    
    // alloc structure
    service = btstack_memory_l2cap_service_get();
    if (!service) {
        log_error("register: no memory for l2cap_service_t");
        return BTSTACK_MEMORY_ALLOC_FAILED;
    }
    
    // fill in 
    service->psm = psm;
    service->mtu = 0;
    service->packet_handler = packet_handler;
    service->required_security_level = security_level;
    service->requires_authorization = false;

    // add to services list
    btstack_linked_list_add(&l2cap_le_services, (btstack_linked_item_t *) service);
    
    // done
    return ERROR_CODE_SUCCESS;
}

uint8_t l2cap_cbm_unregister_service(uint16_t psm) {
    log_info("l2cap_cbm_unregister_service psm 0x%x", psm);
    l2cap_service_t *service = l2cap_cbm_get_service(psm);
    if (!service) return L2CAP_SERVICE_DOES_NOT_EXIST;

    btstack_linked_list_remove(&l2cap_le_services, (btstack_linked_item_t *) service);
    btstack_memory_l2cap_service_free(service);
    return ERROR_CODE_SUCCESS;
}

uint8_t l2cap_cbm_accept_connection(uint16_t local_cid, uint8_t * receive_sdu_buffer, uint16_t mtu, uint16_t initial_credits){
    // get channel
    l2cap_channel_t * channel = l2cap_get_channel_for_local_cid(local_cid);
    if (!channel) return L2CAP_LOCAL_CID_DOES_NOT_EXIST;

    // validate state
    if (channel->state != L2CAP_STATE_WAIT_CLIENT_ACCEPT_OR_REJECT){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    // set state accept connection
    channel->state = L2CAP_STATE_WILL_SEND_LE_CONNECTION_RESPONSE_ACCEPT;
    channel->receive_sdu_buffer = receive_sdu_buffer;
    channel->local_mtu = mtu;
    channel->new_credits_incoming = initial_credits;
    channel->automatic_credits  = initial_credits == L2CAP_LE_AUTOMATIC_CREDITS;

    // go
    l2cap_run();
    return ERROR_CODE_SUCCESS;
}

uint8_t l2cap_cbm_decline_connection(uint16_t local_cid, uint16_t result) {
    // get channel
    l2cap_channel_t * channel = l2cap_get_channel_for_local_cid(local_cid);
    if (!channel) return L2CAP_LOCAL_CID_DOES_NOT_EXIST;

    // validate state
    if (channel->state != L2CAP_STATE_WAIT_CLIENT_ACCEPT_OR_REJECT){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    // set state decline connection
    channel->state  = L2CAP_STATE_WILL_SEND_LE_CONNECTION_RESPONSE_DECLINE;
    channel->reason = result;
    l2cap_run();
    return ERROR_CODE_SUCCESS;
}

static gap_security_level_t l2cap_cbm_security_level_for_connection(hci_con_handle_t con_handle){
    uint8_t encryption_key_size = gap_encryption_key_size(con_handle);
    if (encryption_key_size == 0) return LEVEL_0;

    bool authenticated = gap_authenticated(con_handle);
    if (!authenticated) return LEVEL_2;

    return encryption_key_size == 16 ? LEVEL_4 : LEVEL_3;
}

// used to handle pairing complete after triggering to increase
static void l2cap_cbm_sm_packet_handler(uint8_t packet_type, uint16_t channel_nr, uint8_t *packet, uint16_t size) {
    UNUSED(channel_nr);
    UNUSED(size);
    UNUSED(packet_type);
    btstack_assert(packet_type == HCI_EVENT_PACKET);
    if (hci_event_packet_get_type(packet) != SM_EVENT_PAIRING_COMPLETE) return;
    hci_con_handle_t con_handle = sm_event_pairing_complete_get_handle(packet);
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &l2cap_channels);
    while (btstack_linked_list_iterator_has_next(&it)) {
        l2cap_channel_t *channel = (l2cap_channel_t *) btstack_linked_list_iterator_next(&it);
        if (!l2cap_is_dynamic_channel_type(channel->channel_type)) continue;
        if (channel->con_handle != con_handle) continue;
        if (channel->state != L2CAP_STATE_WAIT_OUTGOING_SECURITY_LEVEL_UPDATE) continue;

        // found channel, check security level
        if (l2cap_cbm_security_level_for_connection(con_handle) < channel->required_security_level){
            // pairing failed or wasn't good enough, inform user
            l2cap_cbm_emit_channel_opened(channel, ERROR_CODE_INSUFFICIENT_SECURITY);
            // discard channel
            btstack_linked_list_remove(&l2cap_channels, (btstack_linked_item_t *) channel);
            l2cap_free_channel_entry(channel);
        } else {
            // send conn request now
            channel->state = L2CAP_STATE_WILL_SEND_LE_CONNECTION_REQUEST;
            l2cap_run();
        }
    }
}

uint8_t l2cap_cbm_create_channel(btstack_packet_handler_t packet_handler, hci_con_handle_t con_handle,
    uint16_t psm, uint8_t * receive_sdu_buffer, uint16_t mtu, uint16_t initial_credits, gap_security_level_t security_level,
    uint16_t * out_local_cid) {

    static btstack_packet_callback_registration_t sm_event_callback_registration;
    static bool sm_callback_registered = false;

    log_info("create, handle 0x%04x psm 0x%x mtu %u", con_handle, psm, mtu);

    hci_connection_t * connection = hci_connection_for_handle(con_handle);
    if (!connection) {
        log_error("no hci_connection for handle 0x%04x", con_handle);
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    l2cap_channel_t * channel = l2cap_create_channel_entry(packet_handler, L2CAP_CHANNEL_TYPE_CHANNEL_CBM, connection->address, connection->address_type, psm, mtu, security_level);
    if (!channel) {
        return BTSTACK_MEMORY_ALLOC_FAILED;
    }
    log_info("created %p", (void*)channel);

    // store local_cid
    if (out_local_cid){
       *out_local_cid = channel->local_cid;
    }

    // setup channel entry
    channel->con_handle = con_handle;
    channel->receive_sdu_buffer = receive_sdu_buffer;
    channel->new_credits_incoming = initial_credits;
    channel->automatic_credits    = initial_credits == L2CAP_LE_AUTOMATIC_CREDITS;

    // add to connections list
    btstack_linked_list_add_tail(&l2cap_channels, (btstack_linked_item_t *) channel);

    // check security level
    if (l2cap_cbm_security_level_for_connection(con_handle) < channel->required_security_level){
        if (!sm_callback_registered){
            sm_callback_registered = true;
            // lazy registration for SM events
            sm_event_callback_registration.callback = &l2cap_cbm_sm_packet_handler;
            sm_add_event_handler(&sm_event_callback_registration);
        }

        // start pairing
        channel->state = L2CAP_STATE_WAIT_OUTGOING_SECURITY_LEVEL_UPDATE;
        sm_request_pairing(con_handle);
    } else {
        // send conn request right away
        channel->state = L2CAP_STATE_WILL_SEND_LE_CONNECTION_REQUEST;
        l2cap_run();
    }

    return ERROR_CODE_SUCCESS;
}

uint8_t l2cap_cbm_provide_credits(uint16_t local_cid, uint16_t credits){
    return l2cap_credit_based_provide_credits(local_cid, credits);
}

uint16_t l2cap_cbm_available_credits(uint16_t local_cid){
    return l2cap_credit_based_available_credits(local_cid);
}
#endif

#ifdef ENABLE_L2CAP_ENHANCED_CREDIT_BASED_FLOW_CONTROL_MODE

uint8_t l2cap_ecbm_register_service(btstack_packet_handler_t packet_handler, uint16_t psm, uint16_t min_remote_mtu,
                                    gap_security_level_t security_level, bool authorization_required) {

    // check for already registered psm
    l2cap_service_t *service = l2cap_ecbm_get_service(psm);
    if (service) {
        return L2CAP_SERVICE_ALREADY_REGISTERED;
    }

    // alloc structure
    service = btstack_memory_l2cap_service_get();
    if (!service) {
        log_error("register: no memory for l2cap_service_t");
        return BTSTACK_MEMORY_ALLOC_FAILED;
    }

    // fill in
    service->psm = psm;
    service->mtu = min_remote_mtu;
    service->packet_handler = packet_handler;
    service->required_security_level = security_level;
    service->requires_authorization = authorization_required;

    // add to services list
    btstack_linked_list_add(&l2cap_enhanced_services, (btstack_linked_item_t *) service);

    // done
    return ERROR_CODE_SUCCESS;
}

uint8_t l2cap_ecbm_unregister_service(uint16_t psm) {
    l2cap_service_t *service = l2cap_ecbm_get_service(psm);
    if (!service) return L2CAP_SERVICE_DOES_NOT_EXIST;

    btstack_linked_list_remove(&l2cap_enhanced_services, (btstack_linked_item_t *) service);
    btstack_memory_l2cap_service_free(service);
    return ERROR_CODE_SUCCESS;
}

void l2cap_ecbm_mps_set_min(uint16_t mps_min){
    l2cap_enhanced_mps_min = mps_min;
}

void l2cap_ecbm_mps_set_max(uint16_t mps_max){
    l2cap_enhanced_mps_max = mps_max;
}

uint8_t l2cap_ecbm_create_channels(btstack_packet_handler_t packet_handler, hci_con_handle_t con_handle,
                                       gap_security_level_t security_level,
                                       uint16_t psm, uint8_t num_channels, uint16_t initial_credits, uint16_t mtu,
                                       uint8_t ** receive_sdu_buffers, uint16_t * out_local_cid){

    log_info("create enhanced, handle 0x%04x psm 0x%x mtu %u", con_handle, psm, mtu);

    hci_connection_t * connection = hci_connection_for_handle(con_handle);
    if (!connection) {
        log_error("no hci_connection for handle 0x%04x", con_handle);
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    // setup all channels
    btstack_linked_list_t channels = NULL;
    uint8_t status = l2cap_ecbm_setup_channels(&channels, packet_handler, num_channels, connection, psm, mtu,
                                               security_level);
    uint16_t local_mps = btstack_min(l2cap_enhanced_mps_max, btstack_min(l2cap_max_le_mtu(), mtu));

    // add to connections list and set state + local_sig_id
    l2cap_channel_t * channel;
    uint8_t i = 0;
    uint8_t local_sig_id = l2cap_next_sig_id();
    while (true) {
        channel = (l2cap_channel_t *) btstack_linked_list_pop(&channels);
        if (channel == NULL) break;
        channel->state              = L2CAP_STATE_WILL_SEND_ENHANCED_CONNECTION_REQUEST;
        channel->local_sig_id       = local_sig_id;
        channel->local_mps = local_mps;
        channel->cid_index = i;
        channel->num_cids = num_channels;
        channel->credits_incoming   = initial_credits;
        channel->automatic_credits  = initial_credits == L2CAP_LE_AUTOMATIC_CREDITS;
        channel->receive_sdu_buffer = receive_sdu_buffers[i];
        // store local_cid
        if (out_local_cid){
            out_local_cid[i] = channel->local_cid;
        }
        btstack_linked_list_add_tail(&l2cap_channels, (btstack_linked_item_t *) channel);
        i++;
    }

#if 0
    // check security level
    if (l2cap_le_security_level_for_connection(con_handle) < channel->required_security_level){
        if (!sm_callback_registered){
            sm_callback_registered = true;
            // lazy registration for SM events
            sm_event_callback_registration.callback = &l2cap_sm_packet_handler;
            sm_add_event_handler(&sm_event_callback_registration);
        }

        // start pairing
        channel->state = L2CAP_STATE_WAIT_OUTGOING_SECURITY_LEVEL_UPDATE;
        sm_request_pairing(con_handle);
    } else {
        // send conn request right away
        channel->state = L2CAP_STATE_WILL_SEND_LE_CONNECTION_REQUEST;
        l2cap_run();
    }
#endif

    l2cap_run();

    return status;
}

uint8_t l2cap_ecbm_accept_channels(uint16_t local_cid, uint8_t num_channels, uint16_t initial_credits,
                                            uint16_t receive_buffer_size, uint8_t ** receive_buffers, uint16_t * out_local_cids){

    l2cap_channel_t * channel = l2cap_get_channel_for_local_cid(local_cid);
    if (!channel) {

        return L2CAP_LOCAL_CID_DOES_NOT_EXIST;
    }

    uint16_t local_mps = btstack_min(l2cap_enhanced_mps_max, btstack_min(l2cap_max_le_mtu(), receive_buffer_size));

    //
    hci_con_handle_t  con_handle    = channel->con_handle;
    uint8_t           local_sig_id  = channel->local_sig_id;
    uint8_t           channel_index = 0;
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &l2cap_channels);
    while (btstack_linked_list_iterator_has_next(&it)) {
        channel = (l2cap_channel_t *) btstack_linked_list_iterator_next(&it);
        if (!l2cap_is_dynamic_channel_type(channel->channel_type)) continue;
        if (channel->con_handle != con_handle) continue;
        if (channel->local_sig_id != local_sig_id) continue;
        if (channel->state != L2CAP_STATE_WAIT_CLIENT_ACCEPT_OR_REJECT) continue;

        if (channel_index < num_channels){
            // assign buffer and cid
            out_local_cids[channel_index] = channel->local_cid;
            channel->receive_sdu_buffer = receive_buffers[channel_index];
            channel->local_mtu = receive_buffer_size;
            channel->local_mps = local_mps;
            channel->credits_incoming   = initial_credits;
            channel->automatic_credits  = initial_credits == L2CAP_LE_AUTOMATIC_CREDITS;
            channel_index++;
        } else {
            // clear local cid for response packet
            channel->local_cid = 0;
            channel->reason = L2CAP_ECBM_CONNECTION_RESULT_SOME_REFUSED_INSUFFICIENT_RESOURCES_AVAILABLE;
        }
        // update state
        channel->state = L2CAP_STATE_WILL_SEND_ENHANCED_CONNECTION_RESPONSE;
    }
    l2cap_run_trigger();
    return ERROR_CODE_SUCCESS;
}

uint8_t l2cap_ecbm_decline_channels(uint16_t local_cid, uint16_t result){
    l2cap_channel_t * channel = l2cap_get_channel_for_local_cid(local_cid);
    if (!channel) {
        return L2CAP_LOCAL_CID_DOES_NOT_EXIST;
    }
    //
    hci_con_handle_t  con_handle    = channel->con_handle;
    uint8_t           local_sig_id  = channel->local_sig_id;
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &l2cap_channels);
    while (btstack_linked_list_iterator_has_next(&it)) {
        channel = (l2cap_channel_t *) btstack_linked_list_iterator_next(&it);
        if (!l2cap_is_dynamic_channel_type(channel->channel_type)) continue;
        if (channel->con_handle != con_handle) continue;
        if (channel->local_sig_id != local_sig_id) continue;
        if (channel->state != L2CAP_STATE_WAIT_CLIENT_ACCEPT_OR_REJECT) continue;

        // prepare response
        channel->local_cid = 0;
        channel->reason = result;
        channel->state = L2CAP_STATE_WILL_SEND_ENHANCED_CONNECTION_RESPONSE;
    }
    l2cap_run_trigger();
    return ERROR_CODE_SUCCESS;
}

uint8_t l2cap_ecbm_reconfigure_channels(uint8_t num_cids, uint16_t * local_cids, int16_t receive_buffer_size, uint8_t ** receive_buffers){
    btstack_assert(receive_buffers != NULL);
    btstack_assert(local_cids != NULL);

    if (num_cids > L2CAP_ECBM_MAX_CID_ARRAY_SIZE){
        return ERROR_CODE_UNACCEPTABLE_CONNECTION_PARAMETERS;
    }

    // check if all cids exist and have the same con handle
    uint8_t i;
    hci_con_handle_t con_handle = HCI_CON_HANDLE_INVALID;
    for (i = 0 ; i < num_cids ; i++){
        l2cap_channel_t * channel = l2cap_get_channel_for_local_cid(local_cids[i]);
        if (!channel) {
            return L2CAP_LOCAL_CID_DOES_NOT_EXIST;
        }
        if (channel->state != L2CAP_STATE_OPEN){
            return ERROR_CODE_COMMAND_DISALLOWED;
        }
        if (con_handle == HCI_CON_HANDLE_INVALID){
            con_handle = channel->con_handle;
        } else if (con_handle != channel->con_handle){
            return ERROR_CODE_UNACCEPTABLE_CONNECTION_PARAMETERS;
        }
    }
    // set renegotiation data and state
    uint8_t sig_id = l2cap_next_sig_id();
    for (i = 0 ; i < num_cids ; i++){
        l2cap_channel_t * channel = l2cap_get_channel_for_local_cid(local_cids[i]);
        channel->cid_index = i;
        channel->num_cids = num_cids;
        channel->local_sig_id = sig_id;
        channel->renegotiate_mtu = receive_buffer_size;
        channel->renegotiate_sdu_buffer = receive_buffers[i];
        channel->state = L2CAP_STATE_WILL_SEND_EHNANCED_RENEGOTIATION_REQUEST;
    }


    l2cap_run();
    return ERROR_CODE_SUCCESS;
}

uint8_t l2cap_ecbm_provide_credits(uint16_t local_cid, uint16_t credits){
    return l2cap_credit_based_provide_credits(local_cid, credits);
}

uint16_t l2cap_ecbm_available_credits(uint16_t local_cid){
    return l2cap_credit_based_available_credits(local_cid);
}
#endif

#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
// @deprecated - please use l2cap_ertm_create_channel
uint8_t l2cap_create_ertm_channel(btstack_packet_handler_t packet_handler, bd_addr_t address, uint16_t psm,
                                  l2cap_ertm_config_t * ertm_contig, uint8_t * buffer, uint32_t size, uint16_t * out_local_cid){
    log_error("deprecated - please use l2cap_ertm_create_channel");
    return l2cap_ertm_create_channel(packet_handler, address, psm, ertm_contig, buffer, size, out_local_cid);
};

// @deprecated - please use l2cap_ertm_accept_connection
uint8_t l2cap_accept_ertm_connection(uint16_t local_cid, l2cap_ertm_config_t * ertm_contig, uint8_t * buffer, uint32_t size){
    log_error("deprecated - please use l2cap_ertm_accept_connection");
    return l2cap_ertm_accept_connection(local_cid, ertm_contig, buffer, size);
}
#endif

#ifdef ENABLE_L2CAP_LE_CREDIT_BASED_FLOW_CONTROL_MODE
// @deprecated - please use l2cap_cbm_register_service
uint8_t l2cap_le_register_service(btstack_packet_handler_t packet_handler, uint16_t psm, gap_security_level_t security_level){
    log_error("deprecated - please use l2cap_cbm_register_service");
    return l2cap_cbm_register_service(packet_handler, psm, security_level);
}

// @deprecated - please use l2cap_cbm_unregister_service
uint8_t l2cap_le_unregister_service(uint16_t psm){
    log_error("deprecated - please use l2cap_cbm_unregister_service");
    return l2cap_cbm_unregister_service(psm);
}

// @deprecated - please use l2cap_cbm_accept_connection
uint8_t l2cap_le_accept_connection(uint16_t local_cid, uint8_t * receive_sdu_buffer, uint16_t mtu, uint16_t initial_credits){
    log_error("deprecated - please use l2cap_cbm_accept_connection");
    return l2cap_cbm_accept_connection(local_cid, receive_sdu_buffer, mtu, initial_credits);
}

// @deprecated - please use l2cap_cbm_decline_connection
uint8_t l2cap_le_decline_connection(uint16_t local_cid){
    log_error("deprecated - please use l2cap_cbm_decline_connection");
    return l2cap_cbm_decline_connection(local_cid, L2CAP_CBM_CONNECTION_RESULT_NO_RESOURCES_AVAILABLE);
}

// @deprecated - please use l2cap_cbm_create_channel
uint8_t l2cap_le_create_channel(btstack_packet_handler_t packet_handler, hci_con_handle_t con_handle,
                                uint16_t psm, uint8_t * receive_sdu_buffer, uint16_t mtu, uint16_t initial_credits, gap_security_level_t security_level,
                                uint16_t * out_local_cid){
    log_error("deprecated - please use l2cap_cbm_create_channel");
    return l2cap_cbm_create_channel(packet_handler, con_handle, psm, receive_sdu_buffer, mtu, initial_credits, security_level, out_local_cid);
}

// @deprecated - please use l2cap_cbm_provide_credits
uint8_t l2cap_le_provide_credits(uint16_t local_cid, uint16_t credits){
    log_error("deprecated - please use l2cap_cbm_provide_credits");
    return l2cap_cbm_provide_credits(local_cid, credits);
}

// @deprecated - please use l2cap_can_send_packet_now
bool l2cap_le_can_send_now(uint16_t local_cid){
    log_error("deprecated - please use l2cap_can_send_packet_now");
    return l2cap_can_send_packet_now(local_cid);
}

// @deprecated - please use l2cap_request_can_send_now_event
uint8_t l2cap_le_request_can_send_now_event(uint16_t local_cid){
    log_error("deprecated - please use l2cap_request_can_send_now_event");
    return l2cap_request_can_send_now_event(local_cid);
}

// @deprecated - please use l2cap_cbm_send_data
uint8_t l2cap_le_send_data(uint16_t local_cid, const uint8_t * data, uint16_t size){
    log_error("deprecated - please use l2cap_cbm_send_data");
    return l2cap_send(local_cid, data, size);
}

// @deprecated - please use l2cap_disconnect
uint8_t l2cap_le_disconnect(uint16_t local_cid){
    log_error("deprecated - please use l2cap_disconnect");
    return l2cap_disconnect(local_cid);
}
#endif

#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
static void fuzz_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
}
void l2cap_setup_test_channels_fuzz(void) {
    bd_addr_t address;
    l2cap_channel_t * channel;

    // 0x41 1setup classic basic
    channel = l2cap_create_channel_entry(fuzz_packet_handler, L2CAP_CHANNEL_TYPE_CLASSIC, address,
        BD_ADDR_TYPE_ACL, 0x01, 100, LEVEL_4);
    btstack_linked_list_add_tail(&l2cap_channels, (btstack_linked_item_t *) channel);

    // 0x42 setup le cbm
    channel = l2cap_create_channel_entry(fuzz_packet_handler, L2CAP_CHANNEL_TYPE_CHANNEL_CBM, address,
        BD_ADDR_TYPE_LE_PUBLIC, 0x03, 100, LEVEL_4);
    btstack_linked_list_add_tail(&l2cap_channels, (btstack_linked_item_t *) channel);

    // 0x43 setup le ecbm
    channel = l2cap_create_channel_entry(fuzz_packet_handler, L2CAP_CHANNEL_TYPE_CHANNEL_ECBM,
        address, BD_ADDR_TYPE_LE_PUBLIC, 0x05, 100, LEVEL_4);
    btstack_linked_list_add_tail(&l2cap_channels, (btstack_linked_item_t *) channel);
}

void l2cap_free_channels_fuzz(void){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &l2cap_channels);
    while (btstack_linked_list_iterator_has_next(&it)){
        l2cap_channel_t * channel = (l2cap_channel_t*) btstack_linked_list_iterator_next(&it);
        bool fixed_channel = false;
        switch (channel->channel_type) {
            case L2CAP_CHANNEL_TYPE_FIXED_LE:
            case L2CAP_CHANNEL_TYPE_FIXED_CLASSIC:
            case L2CAP_CHANNEL_TYPE_CONNECTIONLESS:
                fixed_channel = true;
                break;
            default:
                break;
        }
        if (fixed_channel == false) {
            btstack_linked_list_iterator_remove(&it);
            btstack_memory_l2cap_channel_free(channel);
        }
    }
}

l2cap_channel_t * l2cap_get_dynamic_channel_fuzz(void){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &l2cap_channels);
    while (btstack_linked_list_iterator_has_next(&it)){
        l2cap_channel_t * channel = (l2cap_channel_t*) btstack_linked_list_iterator_next(&it);
        switch (channel->channel_type) {
            case L2CAP_CHANNEL_TYPE_CLASSIC:
            case L2CAP_CHANNEL_TYPE_CHANNEL_CBM:
            case L2CAP_CHANNEL_TYPE_CHANNEL_ECBM:
                return channel;
            default:
                break;
        }
    }
    return NULL;
}
#endif
