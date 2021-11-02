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
 * @title RFCOMM
 *
 */

#ifndef RFCOMM_H
#define RFCOMM_H
 
#include "btstack_util.h"

#include <stdint.h>
#include "btstack_run_loop.h"
#include "gap.h"
#include "l2cap.h"

#if defined __cplusplus
extern "C" {
#endif
    
#define UNLIMITED_INCOMING_CREDITS 0xff

#define RFCOMM_TEST_DATA_MAX_LEN 4

#define RFCOMM_RLS_STATUS_INVALID 0xff


// private structs
typedef enum {
	RFCOMM_MULTIPLEXER_CLOSED = 1,
	RFCOMM_MULTIPLEXER_W4_CONNECT,  // outgoing
	RFCOMM_MULTIPLEXER_SEND_SABM_0,     //    "
	RFCOMM_MULTIPLEXER_W4_UA_0,     //    "
	RFCOMM_MULTIPLEXER_W4_SABM_0,   // incoming
    RFCOMM_MULTIPLEXER_SEND_UA_0,
	RFCOMM_MULTIPLEXER_OPEN,
    RFCOMM_MULTIPLEXER_SEND_UA_0_AND_DISC,
    RFCOMM_MULTIPLEXER_SHUTTING_DOWN,
} RFCOMM_MULTIPLEXER_STATE;

typedef enum {
    MULT_EV_READY_TO_SEND = 1,
} RFCOMM_MULTIPLEXER_EVENT;

typedef enum {
	RFCOMM_CHANNEL_CLOSED = 1,
	RFCOMM_CHANNEL_W4_MULTIPLEXER,
	RFCOMM_CHANNEL_SEND_UIH_PN,
    RFCOMM_CHANNEL_W4_PN_RSP,
	RFCOMM_CHANNEL_SEND_SABM_W4_UA,
	RFCOMM_CHANNEL_W4_UA,
    RFCOMM_CHANNEL_INCOMING_SETUP,
    RFCOMM_CHANNEL_DLC_SETUP,
	RFCOMM_CHANNEL_OPEN,
    RFCOMM_CHANNEL_SEND_UA_AFTER_DISC,
    RFCOMM_CHANNEL_SEND_DISC,
    RFCOMM_CHANNEL_W4_UA_AFTER_DISC,
    RFCOMM_CHANNEL_SEND_DM,
    RFCOMM_CHANNEL_EMIT_OPEN_FAILED_AND_DISCARD,
} RFCOMM_CHANNEL_STATE;


#define RFCOMM_CHANNEL_STATE_VAR_NONE              0
#define RFCOMM_CHANNEL_STATE_VAR_CLIENT_ACCEPTED   1 <<  0
#define RFCOMM_CHANNEL_STATE_VAR_RCVD_PN           1 <<  1
#define RFCOMM_CHANNEL_STATE_VAR_RCVD_SABM         1 <<  2
#define RFCOMM_CHANNEL_STATE_VAR_RCVD_MSC_CMD      1 <<  3
#define RFCOMM_CHANNEL_STATE_VAR_RCVD_MSC_RSP      1 <<  4
#define RFCOMM_CHANNEL_STATE_VAR_SEND_PN_RSP       1 <<  5
#define RFCOMM_CHANNEL_STATE_VAR_SEND_RPN_QUERY    1 <<  6
#define RFCOMM_CHANNEL_STATE_VAR_SEND_RPN_CONFIG   1 <<  7
#define RFCOMM_CHANNEL_STATE_VAR_SEND_RPN_RESPONSE 1 <<  8
#define RFCOMM_CHANNEL_STATE_VAR_EMIT_RPN_RESPONSE 1 <<  9
#define RFCOMM_CHANNEL_STATE_VAR_SEND_UA           1 << 10
#define RFCOMM_CHANNEL_STATE_VAR_SEND_MSC_CMD      1 << 11
#define RFCOMM_CHANNEL_STATE_VAR_SEND_MSC_RSP      1 << 12
#define RFCOMM_CHANNEL_STATE_VAR_SEND_CREDITS      1 << 13
#define RFCOMM_CHANNEL_STATE_VAR_SENT_MSC_CMD      1 << 14
#define RFCOMM_CHANNEL_STATE_VAR_SENT_MSC_RSP      1 << 15
#define RFCOMM_CHANNEL_STATE_VAR_SENT_CREDITS      1 << 16

typedef struct rfcomm_rpn_data {
    uint8_t baud_rate;
    uint8_t flags;
    uint8_t flow_control;
    uint8_t xon;
    uint8_t xoff;
    uint8_t parameter_mask_0;   // first byte
    uint8_t parameter_mask_1;   // second byte
} rfcomm_rpn_data_t;

// info regarding potential connections
typedef struct {
    // linked list - assert: first field
    btstack_linked_item_t    item;
	
    // packet handler
    btstack_packet_handler_t packet_handler;

    // server channel
    uint8_t server_channel;
    
    // incoming max frame size
    uint16_t max_frame_size;

    // use incoming flow control
    uint8_t incoming_flow_control;
    
    // initial incoming credits
    uint8_t incoming_initial_credits;
    
    
} rfcomm_service_t;

// info regarding multiplexer
// note: spec mandates single multiplexer per device combination
typedef struct {
    // linked list - assert: first field
    btstack_linked_item_t    item;
    
    btstack_timer_source_t   timer;
    int              timer_active;
    
	RFCOMM_MULTIPLEXER_STATE state;	
    
    uint16_t  l2cap_cid;
    
    uint8_t   fcon; // only send if fcon & 1, send rsp if fcon & 0x80

	bd_addr_t remote_addr;
    hci_con_handle_t con_handle;
    
	uint8_t   outgoing;
    
    // hack to deal with authentication failure only observed by remote side
    uint8_t at_least_one_connection;
    
    uint16_t max_frame_size;
    
    // send DM for DLCI != 0
    uint8_t send_dm_for_dlci;
    
    // non supported command, 0 if not set
    uint8_t nsc_command;

    // ertm id
    uint16_t ertm_id;

    // test data - limited to RFCOMM_TEST_DATA_MAX_LEN
    uint8_t test_data_len;
    uint8_t test_data[RFCOMM_TEST_DATA_MAX_LEN];

} rfcomm_multiplexer_t;

// info regarding an actual connection
typedef struct {

    // linked list - assert: first field
    btstack_linked_item_t    item;
	
    // packet handler
    btstack_packet_handler_t packet_handler;

    // server channel (see rfcomm_service_t) - NULL => outgoing channel
    rfcomm_service_t * service;

	// muliplexer for this channel
    rfcomm_multiplexer_t *multiplexer;
	
    // RFCOMM Channel ID
    uint16_t rfcomm_cid;
        
    // 
    uint8_t  dlci; 
    
    // credits for outgoing traffic
    uint8_t credits_outgoing;
    
    // number of packets remote will be granted
    uint8_t new_credits_incoming;

    // credits for incoming traffic
    uint8_t credits_incoming;
    
    // use incoming flow control
    uint8_t incoming_flow_control;
    
    // channel state
    RFCOMM_CHANNEL_STATE state;
    
    // state variables/flags
    uint32_t state_var;
    
    // priority set by incoming side in PN
    uint8_t pn_priority;
    
	// negotiated frame size
    uint16_t max_frame_size;
	
    // local rpn data
    rfcomm_rpn_data_t local_rpn_data;
    
    // remote rpn data
    rfcomm_rpn_data_t remote_rpn_data;

    // local line status. RFCOMM_RLS_STATUS_INVALID if not set
    // buffers local status for RLS CMD
    uint8_t local_line_status;

    // remote line status. RFCOMM_RLS_STATUS_INVALID if not set
    // send RLS RSP with status from the RLS CMD
    uint8_t remote_line_status;

    // local modem status.
    uint8_t local_modem_status;

    // remote modem status.
    uint8_t remote_modem_status;

    //
    uint8_t   waiting_for_can_send_now;
        
} rfcomm_channel_t;

// struct used in ERTM callback
typedef struct {
    // remote address
    bd_addr_t             addr;

    // ERTM ID - returned in RFCOMM_EVENT_ERTM_BUFFER_RELEASED.
    uint16_t              ertm_id;

    // ERTM Configuration - needs to stay valid indefinitely
    l2cap_ertm_config_t * ertm_config;

    // ERTM buffer
    uint8_t *             ertm_buffer;
    uint32_t              ertm_buffer_size;
} rfcomm_ertm_request_t;

/* API_START */

/** 
 * @brief Set up RFCOMM.
 */
void rfcomm_init(void);

/** 
 * @brief Set security level required for incoming connections, need to be called before registering services.
 * @deprecated use gap_set_security_level instead
 */
void rfcomm_set_required_security_level(gap_security_level_t security_level);

/* 
 * @brief Create RFCOMM connection to a given server channel on a remote deivce.
 * This channel will automatically provide enough credits to the remote side.
 * @param addr
 * @param server_channel
 * @param out_cid
 * @result status
 */
uint8_t rfcomm_create_channel(btstack_packet_handler_t packet_handler, bd_addr_t addr, uint8_t server_channel, uint16_t * out_cid);

/* 
 * @brief Create RFCOMM connection to a given server channel on a remote deivce.
 * This channel will use explicit credit management. During channel establishment, an initial  amount of credits is provided.
 * @param addr
 * @param server_channel
 * @param initial_credits
 * @param out_cid
 * @result status
 */
uint8_t rfcomm_create_channel_with_initial_credits(btstack_packet_handler_t packet_handler, bd_addr_t addr, uint8_t server_channel, uint8_t initial_credits, uint16_t * out_cid);

/** 
 * @brief Disconnects RFCOMM channel with given identifier.
 * @return status
 */
uint8_t rfcomm_disconnect(uint16_t rfcomm_cid);

/** 
 * @brief Registers RFCOMM service for a server channel and a maximum frame size, and assigns a packet handler.
 * This channel provides credits automatically to the remote side -> no flow control
 * @param packet handler for all channels of this service
 * @param channel 
 * @param max_frame_size
 * @return status ERROR_CODE_SUCCESS if successful, otherwise L2CAP_SERVICE_ALREADY_REGISTERED or BTSTACK_MEMORY_ALLOC_FAILED
 */
uint8_t rfcomm_register_service(btstack_packet_handler_t packet_handler, uint8_t channel, uint16_t max_frame_size);

/** 
 * @brief Registers RFCOMM service for a server channel and a maximum frame size, and assigns a packet handler. 
 * This channel will use explicit credit management. During channel establishment, an initial amount of credits is provided.
 * @param packet handler for all channels of this service
 * @param channel 
 * @param max_frame_size
 * @param initial_credits
 * @return status ERROR_CODE_SUCCESS if successful, otherwise L2CAP_SERVICE_ALREADY_REGISTERED or BTSTACK_MEMORY_ALLOC_FAILED
 */
uint8_t rfcomm_register_service_with_initial_credits(btstack_packet_handler_t packet_handler, uint8_t channel, uint16_t max_frame_size, uint8_t initial_credits);

/** 
 * @brief Unregister RFCOMM service.
 */
void rfcomm_unregister_service(uint8_t service_channel);

/** 
 * @brief Accepts incoming RFCOMM connection.
 * @return status
 */
uint8_t rfcomm_accept_connection(uint16_t rfcomm_cid);

/** 
 * @brief Deny incoming RFCOMM connection.
 * @return status
 */
uint8_t rfcomm_decline_connection(uint16_t rfcomm_cid);

/** 
 * @brief Grant more incoming credits to the remote side for the given RFCOMM channel identifier.
 * @return status
 */
uint8_t rfcomm_grant_credits(uint16_t rfcomm_cid, uint8_t credits);

/** 
 * @brief Checks if RFCOMM can send packet. 
 * @param rfcomm_cid
 * @result true if can send now
 */
bool rfcomm_can_send_packet_now(uint16_t rfcomm_cid);

/** 
 * @brief Request emission of RFCOMM_EVENT_CAN_SEND_NOW as soon as possible
 * @note RFCOMM_EVENT_CAN_SEND_NOW might be emitted during call to this function
 *       so packet handler should be ready to handle it
 * @param rfcomm_cid
 * @prarm status
 */
uint8_t rfcomm_request_can_send_now_event(uint16_t rfcomm_cid);

/** 
 * @brief Sends RFCOMM data packet to the RFCOMM channel with given identifier.
 * @param rfcomm_cid
 * @return status
 */
uint8_t rfcomm_send(uint16_t rfcomm_cid, uint8_t *data, uint16_t len);

/** 
 * @brief Sends Local Line Status, see LINE_STATUS_..
 * @param rfcomm_cid
 * @param line_status
 * @return status
 */
uint8_t rfcomm_send_local_line_status(uint16_t rfcomm_cid, uint8_t line_status);

/** 
 * @brief Send local modem status. see MODEM_STAUS_..
 * @param rfcomm_cid
 * @param modem_status
 * @return status
 */
uint8_t rfcomm_send_modem_status(uint16_t rfcomm_cid, uint8_t modem_status);

/** 
 * @brief Configure remote port 
 * @param rfcomm_cid
 * @param baud_rate
 * @param data_bits
 * @param stop_bits
 * @param parity
 * @param flow_control
 * @return status
 */
uint8_t rfcomm_send_port_configuration(uint16_t rfcomm_cid, rpn_baud_t baud_rate, rpn_data_bits_t data_bits, rpn_stop_bits_t stop_bits, rpn_parity_t parity, uint8_t flow_control);

/** 
 * @brief Query remote port 
 * @param rfcomm_cid
 * @return status
 */
uint8_t rfcomm_query_port_configuration(uint16_t rfcomm_cid);

/** 
 * @brief Query max frame size
 * @param rfcomm_cid
 * @return max frame size
 */
uint16_t rfcomm_get_max_frame_size(uint16_t rfcomm_cid);

/** 
 * @brief Reserve packet buffer to allow to create RFCOMM packet in place
 * @return true on success
 *
 * if (rfcomm_can_send_packet_now(cid)){
 *     rfcomm_reserve_packet_buffer();
 *     uint8_t * buffer = rfcomm_get_outgoing_buffer();
 *     uint16_t buffer_size = rfcomm_get_max_frame_size(cid);
 *     .. setup data in buffer with len ..
 *     rfcomm_send_prepared(cid, len)
 * }
 */
bool rfcomm_reserve_packet_buffer(void);

/**
 * @brief Get outgoing buffer
 * @return pointer to outgoing rfcomm buffer
 */
uint8_t * rfcomm_get_outgoing_buffer(void);

/**
 * @brief Send packet prepared in outgoing buffer
 * @note This releases the outgoing rfcomm buffer
 * @param rfcomm_cid
 * @param len
 * @return status
 */
uint8_t rfcomm_send_prepared(uint16_t rfcomm_cid, uint16_t len);

/**
 * @brief Release outgoing buffer in case rfcomm_send_prepared was not called
*/
void rfcomm_release_packet_buffer(void);

/**
 * @brief Enable L2CAP ERTM mode for RFCOMM. request callback is used to provide ERTM buffer. released callback returns buffer
 *
 * @note on request_callback, the app must set the ertm_config, buffer, size fields to enable ERTM for the current connection
 * @note if buffer is not set, BASIC mode will be used instead
 *
 * @note released_callback provides ertm_id from earlier request to match request and release
 *
 * @param request_callback
 * @param released_callback
 */
void rfcomm_enable_l2cap_ertm(void request_callback(rfcomm_ertm_request_t * request), void released_callback(uint16_t ertm_id));


// Event getters for RFCOMM_EVENT_PORT_CONFIGURATION

/**
 * @brief Get field rfcomm_cid from event RFCOMM_EVENT_PORT_CONFIGURATION
 * @param event packet
 * @return rfcomm_cid
 */
static inline uint16_t rfcomm_event_port_configuration_get_rfcomm_cid(const uint8_t * event){
    return little_endian_read_16(event, 2);
}

/**
 * @brief Get field local from event RFCOMM_EVENT_PORT_CONFIGURATION
 * @param event packet
 * @return remote - false for local port, true for remote port
 */
static inline bool rfcomm_event_port_configuration_get_remote(const uint8_t * event){
    return event[4] != 0;
}

/**
 * @brief Get field baud_rate from event RFCOMM_EVENT_PORT_CONFIGURATION
 * @param event packet
 * @return baud_rate
 */

static inline rpn_baud_t rfcomm_event_port_configuration_get_baud_rate(const uint8_t * event){
    return (rpn_baud_t) event[5];
}

/**
 * @brief Get field data_bits from event RFCOMM_EVENT_PORT_CONFIGURATION
 * @param event packet
 * @return data_bits
 */

static inline rpn_data_bits_t rfcomm_event_port_configuration_get_data_bits(const uint8_t * event){
    return (rpn_data_bits_t) (event[6] & 3);
}
/**
 * @brief Get field stop_bits from event RFCOMM_EVENT_PORT_CONFIGURATION
 * @param event packet
 * @return stop_bits
 */
static inline rpn_stop_bits_t rfcomm_event_port_configuration_get_stop_bits(const uint8_t * event){
    return (rpn_stop_bits_t) ((event[6] >> 2) & 1);
}

/**
 * @brief Get field parity from event RFCOMM_EVENT_PORT_CONFIGURATION
 * @param event packet
 * @return parity
 */
static inline rpn_parity_t rfcomm_event_port_configuration_get_parity(const uint8_t * event){
    return (rpn_parity_t) ((event[6] >> 3) & 7);
}

/**
 * @brief Get field flow_control from event RFCOMM_EVENT_PORT_CONFIGURATION
 * @param event packet
 * @return flow_control
 */

static inline uint8_t rfcomm_event_port_configuration_get_flow_control(const uint8_t * event){
    return event[7] & 0x3f;
}

/**
 * @brief Get field xon from event RFCOMM_EVENT_PORT_CONFIGURATION
 * @param event packet
 * @return xon
 */
static inline uint8_t rfcomm_event_port_configuration_get_xon(const uint8_t * event){
    return event[8];
}

/**
 * @brief Get field xoff from event RFCOMM_EVENT_PORT_CONFIGURATION
 * @param event packet
 * @return xoff
 */
static inline uint8_t rfcomm_event_port_configuration_get_xoff(const uint8_t * event){
    return event[9];
}

/**
 * @brief De-Init RFCOMM
 */
void rfcomm_deinit(void);

/* API_END */

#if defined __cplusplus
}
#endif

#endif // RFCOMM_H
