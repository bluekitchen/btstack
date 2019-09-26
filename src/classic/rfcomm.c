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

#define BTSTACK_FILE__ "rfcomm.c"

/*
 *  rfcomm.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h> // memcpy
#include <stdint.h>

#include "bluetooth_sdp.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_memory.h"
#include "btstack_util.h"
#include "classic/core.h"
#include "classic/rfcomm.h"
#include "hci.h"
#include "hci_cmd.h"
#include "hci_dump.h"
#include "l2cap.h"

// workaround for missing PRIxPTR on mspgcc (16/20-bit MCU)
#ifndef PRIxPTR
#if defined(__MSP430X__)  &&  defined(__MSP430X_LARGE__)
#define PRIxPTR "lx"
#else
#define PRIxPTR "x"
#endif
#endif

// ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE_FOR_RFCOMM requires ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE_FOR_RFCOMM
#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE 
#define RFCOMM_USE_OUTGOING_BUFFER
#define RFCOMM_USE_ERTM
#else
#error "ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE_FOR_RFCOMM requires ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE. "
#error "Please disable ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE_FOR_RFCOMM, or, "
#error "enable ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE" 
#endif
#endif

#define RFCOMM_MULIPLEXER_TIMEOUT_MS 60000

#define RFCOMM_CREDITS 10

// FCS calc 
#define BT_RFCOMM_CODE_WORD         0xE0 // pol = x8+x2+x1+1
#define BT_RFCOMM_CRC_CHECK_LEN     3
#define BT_RFCOMM_UIHCRC_CHECK_LEN  2

// Control field values      bit no.       1 2 3 4 PF 6 7 8
#define BT_RFCOMM_SABM       0x3F       // 1 1 1 1  1 1 0 0
#define BT_RFCOMM_UA         0x73       // 1 1 0 0  1 1 1 0
#define BT_RFCOMM_DM         0x0F       // 1 1 1 1  0 0 0 0
#define BT_RFCOMM_DM_PF      0x1F       // 1 1 1 1  1 0 0 0
#define BT_RFCOMM_DISC       0x53       // 1 1 0 0  1 0 1 0
#define BT_RFCOMM_UIH        0xEF       // 1 1 1 1  0 1 1 1
#define BT_RFCOMM_UIH_PF     0xFF       // 1 1 1 1  0 1 1 1

// Multiplexer message types 
#define BT_RFCOMM_CLD_CMD    0xC3
#define BT_RFCOMM_FCON_CMD   0xA3
#define BT_RFCOMM_FCON_RSP   0xA1
#define BT_RFCOMM_FCOFF_CMD  0x63
#define BT_RFCOMM_FCOFF_RSP  0x61
#define BT_RFCOMM_MSC_CMD    0xE3
#define BT_RFCOMM_MSC_RSP    0xE1
#define BT_RFCOMM_NSC_RSP    0x11
#define BT_RFCOMM_PN_CMD     0x83
#define BT_RFCOMM_PN_RSP     0x81
#define BT_RFCOMM_RLS_CMD    0x53
#define BT_RFCOMM_RLS_RSP    0x51
#define BT_RFCOMM_RPN_CMD    0x93
#define BT_RFCOMM_RPN_RSP    0x91
#define BT_RFCOMM_TEST_CMD   0x23
#define BT_RFCOMM_TEST_RSP   0x21

typedef enum {
    CH_EVT_RCVD_SABM = 1,
    CH_EVT_RCVD_UA,
    CH_EVT_RCVD_PN,
    CH_EVT_RCVD_PN_RSP,
    CH_EVT_RCVD_DISC,
    CH_EVT_RCVD_DM,
    CH_EVT_RCVD_MSC_CMD,
    CH_EVT_RCVD_MSC_RSP,
    CH_EVT_RCVD_NSC_RSP,
    CH_EVT_RCVD_RLS_CMD,
    CH_EVT_RCVD_RLS_RSP,
    CH_EVT_RCVD_RPN_CMD,
    CH_EVT_RCVD_RPN_REQ,
    CH_EVT_RCVD_CREDITS,
    CH_EVT_MULTIPLEXER_READY,
    CH_EVT_READY_TO_SEND,
} RFCOMM_CHANNEL_EVENT;

typedef struct rfcomm_channel_event {
    RFCOMM_CHANNEL_EVENT type;
    uint16_t dummy; // force rfcomm_channel_event to be 2-byte aligned -> avoid -Wcast-align warning
} rfcomm_channel_event_t;

typedef struct rfcomm_channel_event_pn {
    rfcomm_channel_event_t super;
    uint16_t max_frame_size;
    uint8_t  priority;
    uint8_t  credits_outgoing;
} rfcomm_channel_event_pn_t;

typedef struct rfcomm_channel_event_rpn {
    rfcomm_channel_event_t super;
    rfcomm_rpn_data_t data;
} rfcomm_channel_event_rpn_t;

typedef struct rfcomm_channel_event_rls {
    rfcomm_channel_event_t super;
    uint8_t line_status;
} rfcomm_channel_event_rls_t;

typedef struct rfcomm_channel_event_msc {
    rfcomm_channel_event_t super;
    uint8_t modem_status;
} rfcomm_channel_event_msc_t;


// global rfcomm data
static uint16_t      rfcomm_client_cid_generator;  // used for client channel IDs

// linked lists for all
static btstack_linked_list_t rfcomm_multiplexers = NULL;
static btstack_linked_list_t rfcomm_channels = NULL;
static btstack_linked_list_t rfcomm_services = NULL;

static gap_security_level_t rfcomm_security_level;

#ifdef RFCOMM_USE_ERTM
static uint16_t rfcomm_ertm_id;
void (*rfcomm_ertm_request_callback)(rfcomm_ertm_request_t * request);
void (*rfcomm_ertm_released_callback)(uint16_t ertm_id);
#endif

#ifdef RFCOMM_USE_OUTGOING_BUFFER
static uint8_t outgoing_buffer[1030];
#endif

static int  rfcomm_channel_can_send(rfcomm_channel_t * channel);
static int  rfcomm_channel_ready_for_open(rfcomm_channel_t *channel);
static int rfcomm_channel_ready_to_send(rfcomm_channel_t * channel);
static void rfcomm_channel_state_machine_with_channel(rfcomm_channel_t *channel, const rfcomm_channel_event_t *event, int * out_channel_valid);
static void rfcomm_channel_state_machine_with_dlci(rfcomm_multiplexer_t * multiplexer, uint8_t dlci, const rfcomm_channel_event_t *event);
static void rfcomm_emit_can_send_now(rfcomm_channel_t *channel);
static int rfcomm_multiplexer_ready_to_send(rfcomm_multiplexer_t * multiplexer);
static void rfcomm_multiplexer_state_machine(rfcomm_multiplexer_t * multiplexer, RFCOMM_MULTIPLEXER_EVENT event);

// MARK: RFCOMM CLIENT EVENTS

static rfcomm_channel_t * rfcomm_channel_for_rfcomm_cid(uint16_t rfcomm_cid){
    btstack_linked_item_t *it;
    for (it = (btstack_linked_item_t *) rfcomm_channels; it ; it = it->next){
        rfcomm_channel_t * channel = ((rfcomm_channel_t *) it);
        if (channel->rfcomm_cid == rfcomm_cid) {
            return channel;
        };
    }
    return NULL;
}

static uint16_t rfcomm_next_client_cid(void){
    do {
        if (rfcomm_client_cid_generator == 0xffff) {
            // don't use 0 as channel id
            rfcomm_client_cid_generator = 1;
        } else {
            rfcomm_client_cid_generator++;
        }
    } while (rfcomm_channel_for_rfcomm_cid(rfcomm_client_cid_generator) != NULL);
    return rfcomm_client_cid_generator;
}

#ifdef RFCOMM_USE_ERTM
static rfcomm_multiplexer_t * rfcomm_multiplexer_for_ertm_id(uint16_t ertm_id) {
    btstack_linked_item_t *it;
    for (it = (btstack_linked_item_t *) rfcomm_multiplexers; it ; it = it->next){
        rfcomm_multiplexer_t * multiplexer = ((rfcomm_multiplexer_t *) it);
        if (multiplexer->ertm_id == ertm_id) {
            return multiplexer;
        };
    }
    return NULL;
}

static uint16_t rfcomm_next_ertm_id(void){
    do {
        if (rfcomm_ertm_id == 0xffff) {
            // don't use 0 as channel id
            rfcomm_ertm_id = 1;
        } else {
            rfcomm_ertm_id++;
        }
    } while (rfcomm_multiplexer_for_ertm_id(rfcomm_ertm_id) != NULL);
    return rfcomm_ertm_id;
}

#endif

// data: event (8), len(8), address(48), channel (8), rfcomm_cid (16)
static void rfcomm_emit_connection_request(rfcomm_channel_t *channel) {
    log_info("RFCOMM_EVENT_INCOMING_CONNECTION addr %s channel #%u cid 0x%02x",
             bd_addr_to_str(channel->multiplexer->remote_addr), channel->dlci>>1, channel->rfcomm_cid);
    uint8_t event[11];
    event[0] = RFCOMM_EVENT_INCOMING_CONNECTION;
    event[1] = sizeof(event) - 2;
    reverse_bd_addr(channel->multiplexer->remote_addr, &event[2]);
    event[8] = channel->dlci >> 1;
    little_endian_store_16(event, 9, channel->rfcomm_cid);
    hci_dump_packet(HCI_EVENT_PACKET, 0, event, sizeof(event));
	(channel->packet_handler)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

// API Change: BTstack-0.3.50x uses
// data: event(8), len(8), status (8), address (48), server channel(8), rfcomm_cid(16), max frame size(16)
// next Cydia release will use SVN version of this
// data: event(8), len(8), status (8), address (48), handle (16), server channel(8), rfcomm_cid(16), max frame size(16)
static void rfcomm_emit_channel_opened(rfcomm_channel_t *channel, uint8_t status) {
    log_info("RFCOMM_EVENT_CHANNEL_OPENED status 0x%x addr %s handle 0x%x channel #%u cid 0x%02x mtu %u",
             status, bd_addr_to_str(channel->multiplexer->remote_addr), channel->multiplexer->con_handle,
             channel->dlci>>1, channel->rfcomm_cid, channel->max_frame_size);
    uint8_t event[18];
    uint8_t pos = 0;
    event[pos++] = RFCOMM_EVENT_CHANNEL_OPENED;  // 0
    event[pos++] = sizeof(event) - 2;                   // 1
    event[pos++] = status;                              // 2
    reverse_bd_addr(channel->multiplexer->remote_addr, &event[pos]); pos += 6; // 3
    little_endian_store_16(event,  pos, channel->multiplexer->con_handle);   pos += 2; // 9
	event[pos++] = channel->dlci >> 1;                                      // 11
	little_endian_store_16(event, pos, channel->rfcomm_cid); pos += 2;                 // 12 - channel ID
	little_endian_store_16(event, pos, channel->max_frame_size); pos += 2;   // max frame size
    event[pos++] = channel->service ? 1 : 0;    // linked to service -> incoming
    hci_dump_packet(HCI_EVENT_PACKET, 0, event, sizeof(event));
	(channel->packet_handler)(HCI_EVENT_PACKET, 0, event, pos);

    // if channel opened successfully, also send can send now if possible
    if (status) return;
    if (rfcomm_channel_can_send(channel)){
        rfcomm_emit_can_send_now(channel);
    }
}

// data: event(8), len(8), rfcomm_cid(16)
static void rfcomm_emit_channel_closed(rfcomm_channel_t * channel) {
    log_info("RFCOMM_EVENT_CHANNEL_CLOSED cid 0x%02x", channel->rfcomm_cid);
    uint8_t event[4];
    event[0] = RFCOMM_EVENT_CHANNEL_CLOSED;
    event[1] = sizeof(event) - 2;
    little_endian_store_16(event, 2, channel->rfcomm_cid);
    hci_dump_packet(HCI_EVENT_PACKET, 0, event, sizeof(event));
	(channel->packet_handler)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void rfcomm_emit_remote_line_status(rfcomm_channel_t *channel, uint8_t line_status){
    log_info("RFCOMM_EVENT_REMOTE_LINE_STATUS cid 0x%02x c, line status 0x%x", channel->rfcomm_cid, line_status);
    uint8_t event[5];
    event[0] = RFCOMM_EVENT_REMOTE_LINE_STATUS;
    event[1] = sizeof(event) - 2;
    little_endian_store_16(event, 2, channel->rfcomm_cid);
    event[4] = line_status;
    hci_dump_packet( HCI_EVENT_PACKET, 0, event, sizeof(event));
    (channel->packet_handler)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void rfcomm_emit_port_configuration(rfcomm_channel_t *channel){
    // notify client about new settings
    uint8_t event[2+sizeof(rfcomm_rpn_data_t)];
    event[0] = RFCOMM_EVENT_PORT_CONFIGURATION;
    event[1] = sizeof(rfcomm_rpn_data_t);
    memcpy(&event[2], (uint8_t*) &channel->rpn_data, sizeof(rfcomm_rpn_data_t));
    hci_dump_packet( HCI_EVENT_PACKET, 0, event, sizeof(event));
    (channel->packet_handler)(HCI_EVENT_PACKET, channel->rfcomm_cid, event, sizeof(event));
}

static void rfcomm_emit_can_send_now(rfcomm_channel_t *channel) {
    log_debug("RFCOMM_EVENT_CHANNEL_CAN_SEND_NOW local_cid 0x%x", channel->rfcomm_cid);
    uint8_t event[4];
    event[0] = RFCOMM_EVENT_CAN_SEND_NOW;
    event[1] = sizeof(event) - 2;
    little_endian_store_16(event, 2, channel->rfcomm_cid);
    hci_dump_packet( HCI_EVENT_PACKET, 0, event, sizeof(event));
    (channel->packet_handler)(HCI_EVENT_PACKET, channel->rfcomm_cid, event, sizeof(event));
}

// MARK RFCOMM RPN DATA HELPER
static void rfcomm_rpn_data_set_defaults(rfcomm_rpn_data_t * rpn_data){
        rpn_data->baud_rate = RPN_BAUD_9600;  /* 9600 bps */
        rpn_data->flags = 0x03;               /* 8-n-1 */
        rpn_data->flow_control = 0;           /* no flow control */
        rpn_data->xon  = 0xd1;                /* XON */
        rpn_data->xoff = 0xd3;                /* XOFF */
        rpn_data->parameter_mask_0 = 0x7f;    /* parameter mask, all values set */
        rpn_data->parameter_mask_1 = 0x3f;    /* parameter mask, all values set */
}    

static void rfcomm_rpn_data_update(rfcomm_rpn_data_t * dest, rfcomm_rpn_data_t * src){
    if (src->parameter_mask_0 & RPN_PARAM_MASK_0_BAUD){
        dest->baud_rate = src->baud_rate;        
    }
    if (src->parameter_mask_0 & RPN_PARAM_MASK_0_DATA_BITS){
        dest->flags = (dest->flags & 0xfc) | (src->flags & 0x03);
    }
    if (src->parameter_mask_0 & RPN_PARAM_MASK_0_STOP_BITS){
        dest->flags = (dest->flags & 0xfb) | (src->flags & 0x04);
    }
    if (src->parameter_mask_0 & RPN_PARAM_MASK_0_PARITY){
        dest->flags = (dest->flags & 0xf7) | (src->flags & 0x08);
    }
    if (src->parameter_mask_0 & RPN_PARAM_MASK_0_PARITY_TYPE){
        dest->flags = (dest->flags & 0xfc) | (src->flags & 0x30);
    }
    if (src->parameter_mask_0 & RPN_PARAM_MASK_0_XON_CHAR){
        dest->xon = src->xon;
    }
    if (src->parameter_mask_0 & RPN_PARAM_MASK_0_XOFF_CHAR){
        dest->xoff = src->xoff;
    }
    int i;
    for (i=0; i < 6 ; i++){
        uint8_t mask = 1 << i;
        if (src->parameter_mask_1 & mask){
            dest->flags = (dest->flags & ~mask) | (src->flags & mask); 
        }
    }
    // always copy parameter mask, too. informative for client, needed for response
    dest->parameter_mask_0 = src->parameter_mask_0;
    dest->parameter_mask_1 = src->parameter_mask_1;
}
// MARK: RFCOMM MULTIPLEXER HELPER

static uint16_t rfcomm_max_frame_size_for_l2cap_mtu(uint16_t l2cap_mtu){
    // Assume RFCOMM header without credits and 2 byte (14 bit) length field
    uint16_t max_frame_size = l2cap_mtu - 5;
    log_info("rfcomm_max_frame_size_for_l2cap_mtu:  %u -> %u", l2cap_mtu, max_frame_size);
    return max_frame_size;
}

static void rfcomm_multiplexer_initialize(rfcomm_multiplexer_t *multiplexer){
    multiplexer->state = RFCOMM_MULTIPLEXER_CLOSED;
    multiplexer->fcon = 1;
    multiplexer->send_dm_for_dlci = 0;
    multiplexer->max_frame_size = rfcomm_max_frame_size_for_l2cap_mtu(l2cap_max_mtu());
    multiplexer->test_data_len = 0;
    multiplexer->nsc_command = 0;
}

static rfcomm_multiplexer_t * rfcomm_multiplexer_create_for_addr(bd_addr_t addr){
    
    // alloc structure 
    rfcomm_multiplexer_t * multiplexer = btstack_memory_rfcomm_multiplexer_get();
    if (!multiplexer) return NULL;
    
    // fill in 
    rfcomm_multiplexer_initialize(multiplexer);
    bd_addr_copy(multiplexer->remote_addr, addr);

    // add to services list
    btstack_linked_list_add(&rfcomm_multiplexers, (btstack_linked_item_t *) multiplexer);
    
    return multiplexer;
}

static rfcomm_multiplexer_t * rfcomm_multiplexer_for_addr(bd_addr_t addr){
    btstack_linked_item_t *it;
    for (it = (btstack_linked_item_t *) rfcomm_multiplexers; it ; it = it->next){
        rfcomm_multiplexer_t * multiplexer = ((rfcomm_multiplexer_t *) it);
        // ignore multiplexer in shutdown
        if (multiplexer->state == RFCOMM_MULTIPLEXER_SHUTTING_DOWN) continue;
        if (bd_addr_cmp(addr, multiplexer->remote_addr) == 0) {
            return multiplexer;
        };
    }
    return NULL;
}

static rfcomm_multiplexer_t * rfcomm_multiplexer_for_l2cap_cid(uint16_t l2cap_cid) {
    btstack_linked_item_t *it;
    for (it = (btstack_linked_item_t *) rfcomm_multiplexers; it ; it = it->next){
        rfcomm_multiplexer_t * multiplexer = ((rfcomm_multiplexer_t *) it);
        if (multiplexer->l2cap_cid == l2cap_cid) {
            return multiplexer;
        };
    }
    return NULL;
}

static int rfcomm_multiplexer_has_channels(rfcomm_multiplexer_t * multiplexer){
    btstack_linked_item_t *it;
    for (it = (btstack_linked_item_t *) rfcomm_channels; it ; it = it->next){
        rfcomm_channel_t * channel = ((rfcomm_channel_t *) it);
        if (channel->multiplexer == multiplexer) {
            return 1;
        }
    }
    return 0;
}

// MARK: RFCOMM CHANNEL HELPER

static void rfcomm_dump_channels(void){
    btstack_linked_item_t * it;
    int channels = 0;
    for (it = (btstack_linked_item_t *) rfcomm_channels; it ; it = it->next){
        rfcomm_channel_t * channel = (rfcomm_channel_t *) it;
        log_info("Channel #%u: addr %p, state %u", channels, channel, channel->state);
        channels++;
    }
}

static void rfcomm_channel_initialize(rfcomm_channel_t *channel, rfcomm_multiplexer_t *multiplexer, 
                               rfcomm_service_t *service, uint8_t server_channel){
    
    // set defaults for port configuration (even for services)
    rfcomm_rpn_data_set_defaults(&channel->rpn_data);

    channel->state            = RFCOMM_CHANNEL_CLOSED;
    channel->state_var        = RFCOMM_CHANNEL_STATE_VAR_NONE;
    
    channel->multiplexer      = multiplexer;
    channel->rfcomm_cid       = rfcomm_next_client_cid();
    channel->max_frame_size   = multiplexer->max_frame_size;

    channel->credits_incoming = 0;
    channel->credits_outgoing = 0;

    // incoming flow control not active
    channel->new_credits_incoming  = RFCOMM_CREDITS;
    channel->incoming_flow_control = 0;

    channel->rls_line_status       = RFCOMM_RLS_STATUS_INVALID;

    channel->service = service;
	if (service) {
		// incoming connection
    	channel->dlci = (server_channel << 1) |  multiplexer->outgoing;
        if (channel->max_frame_size > service->max_frame_size) {
            channel->max_frame_size = service->max_frame_size;
        }
        channel->incoming_flow_control = service->incoming_flow_control;
        channel->new_credits_incoming  = service->incoming_initial_credits;
        channel->packet_handler        = service->packet_handler;
	} else {
		// outgoing connection
		channel->dlci = (server_channel << 1) | (multiplexer->outgoing ^ 1);
	}
}

// service == NULL -> outgoing channel
static rfcomm_channel_t * rfcomm_channel_create(rfcomm_multiplexer_t * multiplexer,
                                                rfcomm_service_t * service, uint8_t server_channel){

    log_info("rfcomm_channel_create for service %p, channel %u --- list of channels:", service, server_channel);
    rfcomm_dump_channels();

    // alloc structure 
    rfcomm_channel_t * channel = btstack_memory_rfcomm_channel_get();
    if (!channel) return NULL;
    
    // fill in 
    rfcomm_channel_initialize(channel, multiplexer, service, server_channel);
    
    // add to services list
    btstack_linked_list_add(&rfcomm_channels, (btstack_linked_item_t *) channel);
    
    return channel;
}

static void rfcomm_notify_channel_can_send(void){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &rfcomm_channels);
    while (btstack_linked_list_iterator_has_next(&it)){
        rfcomm_channel_t * channel = (rfcomm_channel_t *) btstack_linked_list_iterator_next(&it);
        if (!channel->waiting_for_can_send_now) continue; // didn't try to send yet
        if (!rfcomm_channel_can_send(channel)) continue;  // or cannot yet either

        channel->waiting_for_can_send_now = 0;
        rfcomm_emit_can_send_now(channel);
    }
}

static rfcomm_channel_t * rfcomm_channel_for_multiplexer_and_dlci(rfcomm_multiplexer_t * multiplexer, uint8_t dlci){
    btstack_linked_item_t *it;
    for (it = (btstack_linked_item_t *) rfcomm_channels; it ; it = it->next){
        rfcomm_channel_t * channel = ((rfcomm_channel_t *) it);
        if (channel->dlci == dlci && channel->multiplexer == multiplexer) {
            return channel;
        };
    }
    return NULL;
}

static rfcomm_service_t * rfcomm_service_for_channel(uint8_t server_channel){
    btstack_linked_item_t *it;
    for (it = (btstack_linked_item_t *) rfcomm_services; it ; it = it->next){
        rfcomm_service_t * service = ((rfcomm_service_t *) it);
        if ( service->server_channel == server_channel){
            return service;
        };
    }
    return NULL;
}

// MARK: RFCOMM SEND

/**
 * @param credits - only used for RFCOMM flow control in UIH wiht P/F = 1
 */
static int rfcomm_send_packet_for_multiplexer(rfcomm_multiplexer_t *multiplexer, uint8_t address, uint8_t control, uint8_t credits, uint8_t *data, uint16_t len){

    if (!l2cap_can_send_packet_now(multiplexer->l2cap_cid)) return BTSTACK_ACL_BUFFERS_FULL;
    
#ifdef RFCOMM_USE_OUTGOING_BUFFER
    uint8_t * rfcomm_out_buffer = outgoing_buffer;
#else
    l2cap_reserve_packet_buffer();
    uint8_t * rfcomm_out_buffer = l2cap_get_outgoing_buffer();
#endif

	uint16_t pos = 0;
	uint8_t crc_fields = 3;
	
	rfcomm_out_buffer[pos++] = address;
	rfcomm_out_buffer[pos++] = control;
	
	// length field can be 1 or 2 octets
	if (len < 128){
		rfcomm_out_buffer[pos++] = (len << 1)| 1;     // bits 0-6
	} else {
		rfcomm_out_buffer[pos++] = (len & 0x7f) << 1; // bits 0-6
		rfcomm_out_buffer[pos++] = len >> 7;          // bits 7-14
		crc_fields++;
	}

	// add credits for UIH frames when PF bit is set
	if (control == BT_RFCOMM_UIH_PF){
		rfcomm_out_buffer[pos++] = credits;
	}
	
	// copy actual data
	if (len) {
		memcpy(&rfcomm_out_buffer[pos], data, len);
		pos += len;
	}
	
	// UIH frames only calc FCS over address + control (5.1.1)
	if ((control & 0xef) == BT_RFCOMM_UIH){
		crc_fields = 2;
	}
	rfcomm_out_buffer[pos++] =  btstack_crc8_calc(rfcomm_out_buffer, crc_fields); // calc fcs

#ifdef RFCOMM_USE_OUTGOING_BUFFER
    int err = l2cap_send(multiplexer->l2cap_cid, rfcomm_out_buffer, pos);
#else
    int err = l2cap_send_prepared(multiplexer->l2cap_cid, pos);
#endif

    return err;
}

// simplified version of rfcomm_send_packet_for_multiplexer for prepared rfcomm packet (UIH, 2 byte len, no credits)
static int rfcomm_send_uih_prepared(rfcomm_multiplexer_t *multiplexer, uint8_t dlci, uint16_t len){

    uint8_t address = (1 << 0) | (multiplexer->outgoing << 1) | (dlci << 2); 
    uint8_t control = BT_RFCOMM_UIH;

#ifdef RFCOMM_USE_OUTGOING_BUFFER
    uint8_t * rfcomm_out_buffer = outgoing_buffer;
#else
    uint8_t * rfcomm_out_buffer = l2cap_get_outgoing_buffer();
#endif

    uint16_t pos = 0;
    rfcomm_out_buffer[pos++] = address;
    rfcomm_out_buffer[pos++] = control;
    rfcomm_out_buffer[pos++] = (len & 0x7f) << 1; // bits 0-6
    rfcomm_out_buffer[pos++] = len >> 7;          // bits 7-14

    // actual data is already in place
    pos += len;
    
    // UIH frames only calc FCS over address + control (5.1.1)
    rfcomm_out_buffer[pos++] =  btstack_crc8_calc(rfcomm_out_buffer, 2); // calc fcs
    
#ifdef RFCOMM_USE_OUTGOING_BUFFER
    int err = l2cap_send(multiplexer->l2cap_cid, rfcomm_out_buffer, pos);
#else
    int err = l2cap_send_prepared(multiplexer->l2cap_cid, pos);
#endif

    return err;
}

// C/R Flag in Address
// - terms: initiator = station that creates multiplexer with SABM
// - terms: responder = station that responds to multiplexer setup with UA
// "For SABM, UA, DM and DISC frames C/R bit is set according to Table 1 in GSM 07.10, section 5.2.1.2"
//    - command initiator = 1 /response responder = 1
//    - command responder = 0 /response initiator = 0
// "For UIH frames, the C/R bit is always set according to section 5.4.3.1 in GSM 07.10. 
//  This applies independently of what is contained wthin the UIH frames, either data or control messages."
//    - c/r = 1 for frames by initiating station, 0 = for frames by responding station

// C/R Flag in Message
// "In the message level, the C/R bit in the command type field is set as stated in section 5.4.6.2 in GSM 07.10." 
//   - If the C/R bit is set to 1 the message is a command
//   - if it is set to 0 the message is a response.

// temp/old messge construction

// new object oriented version
static int rfcomm_send_sabm(rfcomm_multiplexer_t *multiplexer, uint8_t dlci){
	uint8_t address = (1 << 0) | (multiplexer->outgoing << 1) | (dlci << 2);   // command
    return rfcomm_send_packet_for_multiplexer(multiplexer, address, BT_RFCOMM_SABM, 0, NULL, 0);
}

static int rfcomm_send_disc(rfcomm_multiplexer_t *multiplexer, uint8_t dlci){
	uint8_t address = (1 << 0) | (multiplexer->outgoing << 1) | (dlci << 2);  // command
    return rfcomm_send_packet_for_multiplexer(multiplexer, address, BT_RFCOMM_DISC, 0, NULL, 0);
}

static int rfcomm_send_ua(rfcomm_multiplexer_t *multiplexer, uint8_t dlci){
	uint8_t address = (1 << 0) | ((multiplexer->outgoing ^ 1) << 1) | (dlci << 2); // response
    return rfcomm_send_packet_for_multiplexer(multiplexer, address, BT_RFCOMM_UA, 0, NULL, 0);
}

static int rfcomm_send_dm_pf(rfcomm_multiplexer_t *multiplexer, uint8_t dlci){
	uint8_t address = (1 << 0) | ((multiplexer->outgoing ^ 1) << 1) | (dlci << 2); // response
    return rfcomm_send_packet_for_multiplexer(multiplexer, address, BT_RFCOMM_DM_PF, 0, NULL, 0);
}

static int rfcomm_send_uih_fc_rsp(rfcomm_multiplexer_t *multiplexer, uint8_t fcon) {
    uint8_t address = (1 << 0) | (multiplexer->outgoing<< 1);
    uint8_t payload[2]; 
    uint8_t pos = 0;
    payload[pos++] = fcon ? BT_RFCOMM_FCON_RSP : BT_RFCOMM_FCOFF_RSP;
    payload[pos++] = (0 << 1) | 1;  // len
    return rfcomm_send_packet_for_multiplexer(multiplexer, address, BT_RFCOMM_UIH, 0, (uint8_t *) payload, pos);
}

// static int rfcomm_send_uih_test_cmd(rfcomm_multiplexer_t *multiplexer, uint8_t * data, uint16_t len) {
//     uint8_t address = (1 << 0) | (multiplexer->outgoing << 1);
//     uint8_t payload[2+len]; 
//     uint8_t pos = 0;
//     payload[pos++] = BT_RFCOMM_TEST_CMD;
//     payload[pos++] = (len + 1) << 1 | 1;  // len
//     memcpy(&payload[pos], data, len);
//     pos += len;
//     return rfcomm_send_packet_for_multiplexer(multiplexer, address, BT_RFCOMM_UIH, 0, (uint8_t *) payload, pos);
// }

static int rfcomm_send_uih_test_rsp(rfcomm_multiplexer_t *multiplexer, uint8_t * data, uint16_t len) {
    uint8_t address = (1 << 0) | (multiplexer->outgoing << 1);
    uint8_t payload[2+RFCOMM_TEST_DATA_MAX_LEN]; 
    uint8_t pos = 0;
    payload[pos++] = BT_RFCOMM_TEST_RSP;
    if (len > RFCOMM_TEST_DATA_MAX_LEN) {
        len = RFCOMM_TEST_DATA_MAX_LEN;
    }
    payload[pos++] = (len << 1) | 1;  // len
    memcpy(&payload[pos], data, len);
    pos += len;
    return rfcomm_send_packet_for_multiplexer(multiplexer, address, BT_RFCOMM_UIH, 0, (uint8_t *) payload, pos);
}

static int rfcomm_send_uih_msc_cmd(rfcomm_multiplexer_t *multiplexer, uint8_t dlci, uint8_t signals) {
	uint8_t address = (1 << 0) | (multiplexer->outgoing << 1);
	uint8_t payload[4]; 
	uint8_t pos = 0;
	payload[pos++] = BT_RFCOMM_MSC_CMD;
	payload[pos++] = (2 << 1) | 1;  // len
	payload[pos++] = (1 << 0) | (1 << 1) | (dlci << 2); // CMD => C/R = 1
	payload[pos++] = signals;
	return rfcomm_send_packet_for_multiplexer(multiplexer, address, BT_RFCOMM_UIH, 0, (uint8_t *) payload, pos);
}

static int rfcomm_send_uih_msc_rsp(rfcomm_multiplexer_t *multiplexer, uint8_t dlci, uint8_t signals) {
	uint8_t address = (1 << 0) | (multiplexer->outgoing<< 1);
	uint8_t payload[4]; 
	uint8_t pos = 0;
	payload[pos++] = BT_RFCOMM_MSC_RSP;
	payload[pos++] = (2 << 1) | 1;  // len
	payload[pos++] = (1 << 0) | (1 << 1) | (dlci << 2); // CMD => C/R = 1
	payload[pos++] = signals;
	return rfcomm_send_packet_for_multiplexer(multiplexer, address, BT_RFCOMM_UIH, 0, (uint8_t *) payload, pos);
}

static int rfcomm_send_uih_nsc_rsp(rfcomm_multiplexer_t *multiplexer, uint8_t command) {
    uint8_t address = (1 << 0) | (multiplexer->outgoing<< 1);
    uint8_t payload[3]; 
    uint8_t pos = 0;
    payload[pos++] = BT_RFCOMM_NSC_RSP;
    payload[pos++] = (1 << 1) | 1;  // len
    payload[pos++] = command;
    return rfcomm_send_packet_for_multiplexer(multiplexer, address, BT_RFCOMM_UIH, 0, (uint8_t *) payload, pos);
}

static int rfcomm_send_uih_pn_command(rfcomm_multiplexer_t *multiplexer, uint8_t dlci, uint16_t max_frame_size){
	uint8_t payload[10];
	uint8_t address = (1 << 0) | (multiplexer->outgoing << 1); 
	uint8_t pos = 0;
	payload[pos++] = BT_RFCOMM_PN_CMD;
	payload[pos++] = (8 << 1) | 1;  // len
	payload[pos++] = dlci;
	payload[pos++] = 0xf0; // pre-defined for Bluetooth, see 5.5.3 of TS 07.10 Adaption for RFCOMM
	payload[pos++] = 0; // priority
	payload[pos++] = 0; // max 60 seconds ack
	payload[pos++] = max_frame_size & 0xff; // max framesize low
	payload[pos++] = max_frame_size >> 8;   // max framesize high
	payload[pos++] = 0x00; // number of retransmissions
	payload[pos++] = 0x00; // (unused error recovery window) initial number of credits
	return rfcomm_send_packet_for_multiplexer(multiplexer, address, BT_RFCOMM_UIH, 0, (uint8_t *) payload, pos);
}

// "The response may not change the DLCI, the priority, the convergence layer, or the timer value." rfcomm_tutorial.pdf
static int rfcomm_send_uih_pn_response(rfcomm_multiplexer_t *multiplexer, uint8_t dlci,
                                       uint8_t priority, uint16_t max_frame_size){
	uint8_t payload[10];
	uint8_t address = (1 << 0) | (multiplexer->outgoing << 1); 
	uint8_t pos = 0;
	payload[pos++] = BT_RFCOMM_PN_RSP;
	payload[pos++] = (8 << 1) | 1;  // len
	payload[pos++] = dlci;
	payload[pos++] = 0xe0; // pre defined for Bluetooth, see 5.5.3 of TS 07.10 Adaption for RFCOMM
	payload[pos++] = priority; // priority
	payload[pos++] = 0; // max 60 seconds ack
	payload[pos++] = max_frame_size & 0xff; // max framesize low
	payload[pos++] = max_frame_size >> 8;   // max framesize high
	payload[pos++] = 0x00; // number of retransmissions
	payload[pos++] = 0x00; // (unused error recovery window) initial number of credits
	return rfcomm_send_packet_for_multiplexer(multiplexer, address, BT_RFCOMM_UIH, 0, (uint8_t *) payload, pos);
}

static int rfcomm_send_uih_rls_cmd(rfcomm_multiplexer_t *multiplexer, uint8_t dlci, uint8_t line_status) {
    uint8_t address = (1 << 0) | (multiplexer->outgoing << 1);
    uint8_t payload[4]; 
    uint8_t pos = 0;
    payload[pos++] = BT_RFCOMM_RLS_CMD;
    payload[pos++] = (2 << 1) | 1;  // len
    payload[pos++] = (1 << 0) | (1 << 1) | (dlci << 2); // CMD => C/R = 1
    payload[pos++] = line_status;
    return rfcomm_send_packet_for_multiplexer(multiplexer, address, BT_RFCOMM_UIH, 0, (uint8_t *) payload, pos);
}

static int rfcomm_send_uih_rls_rsp(rfcomm_multiplexer_t *multiplexer, uint8_t dlci, uint8_t line_status) {
    uint8_t address = (1 << 0) | (multiplexer->outgoing << 1);
    uint8_t payload[4]; 
    uint8_t pos = 0;
    payload[pos++] = BT_RFCOMM_RLS_RSP;
    payload[pos++] = (2 << 1) | 1;  // len
    payload[pos++] = (1 << 0) | (1 << 1) | (dlci << 2); // CMD => C/R = 1
    payload[pos++] = line_status;
    return rfcomm_send_packet_for_multiplexer(multiplexer, address, BT_RFCOMM_UIH, 0, (uint8_t *) payload, pos);
}

static int rfcomm_send_uih_rpn_cmd(rfcomm_multiplexer_t *multiplexer, uint8_t dlci, rfcomm_rpn_data_t *rpn_data) {
    uint8_t payload[10];
    uint8_t address = (1 << 0) | (multiplexer->outgoing << 1); 
    uint8_t pos = 0;
    payload[pos++] = BT_RFCOMM_RPN_CMD;
    payload[pos++] = (8 << 1) | 1;  // len
    payload[pos++] = (1 << 0) | (1 << 1) | (dlci << 2); // CMD => C/R = 1
    payload[pos++] = rpn_data->baud_rate;
    payload[pos++] = rpn_data->flags;
    payload[pos++] = rpn_data->flow_control;
    payload[pos++] = rpn_data->xon;
    payload[pos++] = rpn_data->xoff;
    payload[pos++] = rpn_data->parameter_mask_0;
    payload[pos++] = rpn_data->parameter_mask_1;
    return rfcomm_send_packet_for_multiplexer(multiplexer, address, BT_RFCOMM_UIH, 0, (uint8_t *) payload, pos);
}

static int rfcomm_send_uih_rpn_req(rfcomm_multiplexer_t *multiplexer, uint8_t dlci) {
    uint8_t payload[3];
    uint8_t address = (1 << 0) | (multiplexer->outgoing << 1); 
    uint8_t pos = 0;
    payload[pos++] = BT_RFCOMM_RPN_CMD;
    payload[pos++] = (1 << 1) | 1;  // len
    payload[pos++] = (1 << 0) | (1 << 1) | (dlci << 2); // CMD => C/R = 1
    return rfcomm_send_packet_for_multiplexer(multiplexer, address, BT_RFCOMM_UIH, 0, (uint8_t *) payload, pos);
}

static int rfcomm_send_uih_rpn_rsp(rfcomm_multiplexer_t *multiplexer, uint8_t dlci, rfcomm_rpn_data_t *rpn_data) {
	uint8_t payload[10];
	uint8_t address = (1 << 0) | (multiplexer->outgoing << 1); 
	uint8_t pos = 0;
	payload[pos++] = BT_RFCOMM_RPN_RSP;
	payload[pos++] = (8 << 1) | 1;  // len
	payload[pos++] = (1 << 0) | (1 << 1) | (dlci << 2); // CMD => C/R = 1
	payload[pos++] = rpn_data->baud_rate;
	payload[pos++] = rpn_data->flags;
	payload[pos++] = rpn_data->flow_control;
	payload[pos++] = rpn_data->xon;
	payload[pos++] = rpn_data->xoff;
	payload[pos++] = rpn_data->parameter_mask_0;
	payload[pos++] = rpn_data->parameter_mask_1;
	return rfcomm_send_packet_for_multiplexer(multiplexer, address, BT_RFCOMM_UIH, 0, (uint8_t *) payload, pos);
}

static void rfcomm_send_uih_credits(rfcomm_multiplexer_t *multiplexer, uint8_t dlci,  uint8_t credits){
    uint8_t address = (1 << 0) | (multiplexer->outgoing << 1) |  (dlci << 2); 
    rfcomm_send_packet_for_multiplexer(multiplexer, address, BT_RFCOMM_UIH_PF, credits, NULL, 0);
}

// depending on channel state emit channel opened with status or channel closed
static void rfcomm_channel_emit_final_event(rfcomm_channel_t * channel, uint8_t status){
    // emit appropriate events
    switch(channel->state){
        case RFCOMM_CHANNEL_OPEN:
        case RFCOMM_CHANNEL_W4_UA_AFTER_DISC:
            rfcomm_emit_channel_closed(channel);
            break;
        case RFCOMM_CHANNEL_SEND_UA_AFTER_DISC:
            // remote didn't wait until we send the UA disc
            // close event already emitted
            break;
        default:
            rfcomm_emit_channel_opened(channel, status); 
            break;
    }
}

// MARK: RFCOMM MULTIPLEXER
static void rfcomm_multiplexer_stop_timer(rfcomm_multiplexer_t * multiplexer){
    if (multiplexer->timer_active) {
        btstack_run_loop_remove_timer(&multiplexer->timer);
        multiplexer->timer_active = 0;
    }
}
static void rfcomm_multiplexer_free(rfcomm_multiplexer_t * multiplexer){
    btstack_linked_list_remove( &rfcomm_multiplexers, (btstack_linked_item_t *) multiplexer);
    btstack_memory_rfcomm_multiplexer_free(multiplexer);
}

static void rfcomm_multiplexer_finalize(rfcomm_multiplexer_t * multiplexer){
    // remove (potential) timer
    rfcomm_multiplexer_stop_timer(multiplexer);
    
    // close and remove all channels
    btstack_linked_item_t *it = (btstack_linked_item_t *) &rfcomm_channels;
    while (it->next){
        rfcomm_channel_t * channel = (rfcomm_channel_t *) it->next;
        if (channel->multiplexer == multiplexer) {
            // emit open with status or closed
            rfcomm_channel_emit_final_event(channel, RFCOMM_MULTIPLEXER_STOPPED);
            // remove from list
            it->next = it->next->next;
            // free channel struct
            btstack_memory_rfcomm_channel_free(channel);
        } else {
            it = it->next;
        }
    }
    
    // remove mutliplexer
    rfcomm_multiplexer_free(multiplexer);
}

static void rfcomm_multiplexer_timer_handler(btstack_timer_source_t *timer){
    rfcomm_multiplexer_t * multiplexer = (rfcomm_multiplexer_t*) btstack_run_loop_get_timer_context(timer);
    if (rfcomm_multiplexer_has_channels(multiplexer)) return;

    log_info("rfcomm_multiplexer_timer_handler timeout: shutting down multiplexer! (no channels)");
    uint16_t l2cap_cid = multiplexer->l2cap_cid;
    rfcomm_multiplexer_finalize(multiplexer);
    l2cap_disconnect(l2cap_cid, 0x13);
}

static void rfcomm_multiplexer_prepare_idle_timer(rfcomm_multiplexer_t * multiplexer){
    if (multiplexer->timer_active) {
        btstack_run_loop_remove_timer(&multiplexer->timer);
        multiplexer->timer_active = 0;
    }    
    if (rfcomm_multiplexer_has_channels(multiplexer)) return;

    // start idle timer for multiplexer timeout check as there are no rfcomm channels yet
    btstack_run_loop_set_timer(&multiplexer->timer, RFCOMM_MULIPLEXER_TIMEOUT_MS);
    btstack_run_loop_set_timer_handler(&multiplexer->timer, rfcomm_multiplexer_timer_handler);
    btstack_run_loop_set_timer_context(&multiplexer->timer, multiplexer);
    btstack_run_loop_add_timer(&multiplexer->timer);
    multiplexer->timer_active = 1;
}

static void rfcomm_multiplexer_opened(rfcomm_multiplexer_t *multiplexer){
    log_info("Multiplexer up and running");
    multiplexer->state = RFCOMM_MULTIPLEXER_OPEN;
    
    const rfcomm_channel_event_t event = { CH_EVT_MULTIPLEXER_READY, 0};
    
    // transition of channels that wait for multiplexer 
    btstack_linked_item_t *it;
    for (it = (btstack_linked_item_t *) rfcomm_channels; it ; it = it->next){
        rfcomm_channel_t * channel = ((rfcomm_channel_t *) it);
        if (channel->multiplexer != multiplexer) continue;
        int rfcomm_channel_valid = 1;
        rfcomm_channel_state_machine_with_channel(channel, &event, &rfcomm_channel_valid);
        if (rfcomm_channel_valid && rfcomm_channel_ready_to_send(channel)){
            l2cap_request_can_send_now_event(multiplexer->l2cap_cid);
        }
    }        
    rfcomm_multiplexer_prepare_idle_timer(multiplexer);

    // request can send now for multiplexer if ready
    if (rfcomm_multiplexer_ready_to_send(multiplexer)){
        l2cap_request_can_send_now_event(multiplexer->l2cap_cid);
    }
}

static void rfcomm_handle_can_send_now(uint16_t l2cap_cid){

    log_debug("rfcomm_handle_can_send_now enter: %u", l2cap_cid);

    btstack_linked_list_iterator_t it;
    int token_consumed = 0;

    // forward token to multiplexer
    btstack_linked_list_iterator_init(&it, &rfcomm_multiplexers);
    while (!token_consumed && btstack_linked_list_iterator_has_next(&it)){
        rfcomm_multiplexer_t * multiplexer = (rfcomm_multiplexer_t *) btstack_linked_list_iterator_next(&it);
        if (multiplexer->l2cap_cid != l2cap_cid) continue;
        if (rfcomm_multiplexer_ready_to_send(multiplexer)){
            log_debug("rfcomm_handle_can_send_now enter: multiplexer token");
            token_consumed = 1;
            rfcomm_multiplexer_state_machine(multiplexer, MULT_EV_READY_TO_SEND);
        }
    }

    // forward token to channel state machine
    btstack_linked_list_iterator_init(&it, &rfcomm_channels);
    while (!token_consumed && btstack_linked_list_iterator_has_next(&it)){
        rfcomm_channel_t * channel = (rfcomm_channel_t *) btstack_linked_list_iterator_next(&it);
        if (channel->multiplexer->l2cap_cid != l2cap_cid) continue;
        // channel state machine
        if (rfcomm_channel_ready_to_send(channel)){
            log_debug("rfcomm_handle_can_send_now enter: channel token");
            token_consumed = 1;
            const rfcomm_channel_event_t event = { CH_EVT_READY_TO_SEND, 0 };
            int rfcomm_channel_valid = 1;
            rfcomm_channel_state_machine_with_channel(channel, &event, &rfcomm_channel_valid);
        }
    }

    // forward token to client
    btstack_linked_list_iterator_init(&it, &rfcomm_channels);
    while (!token_consumed && btstack_linked_list_iterator_has_next(&it)){
        rfcomm_channel_t * channel = (rfcomm_channel_t *) btstack_linked_list_iterator_next(&it);
        if (channel->multiplexer->l2cap_cid != l2cap_cid) continue;
        // client waiting for can send now
        if (!channel->waiting_for_can_send_now)    continue;
        if ((channel->multiplexer->fcon & 1) == 0) continue;
        if (!channel->credits_outgoing){
            log_debug("rfcomm_handle_can_send_now waiting to send but no credits (ignore)");
            continue;
        }

        log_debug("rfcomm_handle_can_send_now enter: client token");
        token_consumed = 1;
        channel->waiting_for_can_send_now = 0;
        rfcomm_emit_can_send_now(channel);
    }

    // if token was consumed, request another one
    if (token_consumed) {
        l2cap_request_can_send_now_event(l2cap_cid);
    }

    log_debug("rfcomm_handle_can_send_now exit");
}

static void rfcomm_multiplexer_set_state_and_request_can_send_now_event(rfcomm_multiplexer_t * multiplexer, RFCOMM_MULTIPLEXER_STATE state){
    multiplexer->state = state;
    l2cap_request_can_send_now_event(multiplexer->l2cap_cid);
}

/**
 * @return handled packet
 */
static int rfcomm_hci_event_handler(uint8_t *packet, uint16_t size){

    UNUSED(size);   // ok: handling own l2cap events

    bd_addr_t event_addr;
    uint16_t  psm;
    uint16_t l2cap_cid;
    hci_con_handle_t con_handle;
    rfcomm_multiplexer_t *multiplexer = NULL;
    uint8_t status;
    
    switch (hci_event_packet_get_type(packet)) {
            
        // accept incoming rfcomm connection if no multiplexer exists yet
        case L2CAP_EVENT_INCOMING_CONNECTION:
            // data: event(8), len(8), address(48), handle (16),  psm (16), source cid(16) dest cid(16)
            reverse_bd_addr(&packet[2], event_addr);
            con_handle = little_endian_read_16(packet,  8);
            psm        = little_endian_read_16(packet, 10); 
            l2cap_cid  = little_endian_read_16(packet, 12); 

            if (psm != BLUETOOTH_PROTOCOL_RFCOMM) break;

            multiplexer = rfcomm_multiplexer_for_addr(event_addr);
            
            if (multiplexer) {
                log_info("INCOMING_CONNECTION (l2cap_cid 0x%02x) for BLUETOOTH_PROTOCOL_RFCOMM => decline - multiplexer already exists", l2cap_cid);
                l2cap_decline_connection(l2cap_cid);
                return 1;
            }
            
            // create and inititialize new multiplexer instance (incoming)
            multiplexer = rfcomm_multiplexer_create_for_addr(event_addr);
            if (!multiplexer){
                log_info("INCOMING_CONNECTION (l2cap_cid 0x%02x) for BLUETOOTH_PROTOCOL_RFCOMM => decline - no memory left", l2cap_cid);
                l2cap_decline_connection(l2cap_cid); 
                return 1;
            }
            
            multiplexer->con_handle = con_handle;
            multiplexer->l2cap_cid = l2cap_cid;
            // 
            multiplexer->state = RFCOMM_MULTIPLEXER_W4_SABM_0;
            log_info("L2CAP_EVENT_INCOMING_CONNECTION (l2cap_cid 0x%02x) for BLUETOOTH_PROTOCOL_RFCOMM => accept", l2cap_cid);

#ifdef RFCOMM_USE_ERTM
            // request
            rfcomm_ertm_request_t request;
            memset(&request, 0, sizeof(rfcomm_ertm_request_t));
            memcpy(request.addr, event_addr, 6);
            request.ertm_id = rfcomm_next_ertm_id();
            if (rfcomm_ertm_request_callback){
                (*rfcomm_ertm_request_callback)(&request);
            }
            if (request.ertm_config && request.ertm_buffer && request.ertm_buffer_size){
                multiplexer->ertm_id = request.ertm_id;
                l2cap_accept_ertm_connection(l2cap_cid, request.ertm_config, request.ertm_buffer, request.ertm_buffer_size);
                return 1;
            }
#endif

            l2cap_accept_connection(l2cap_cid);
            return 1;
            
        // l2cap connection opened -> store l2cap_cid, remote_addr
        case L2CAP_EVENT_CHANNEL_OPENED: 

            if (little_endian_read_16(packet, 11) != BLUETOOTH_PROTOCOL_RFCOMM) break;
            
            status = packet[2];
            log_info("L2CAP_EVENT_CHANNEL_OPENED for BLUETOOTH_PROTOCOL_RFCOMM, status %u", status);
            
            // get multiplexer for remote addr
            con_handle = little_endian_read_16(packet, 9);
            l2cap_cid = little_endian_read_16(packet, 13);
            reverse_bd_addr(&packet[3], event_addr);
            multiplexer = rfcomm_multiplexer_for_addr(event_addr);
            if (!multiplexer) {
                log_error("L2CAP_EVENT_CHANNEL_OPENED but no multiplexer prepared");
                return 1;
            }
            
            // on l2cap open error discard everything
            if (status){

                // remove (potential) timer
                rfcomm_multiplexer_stop_timer(multiplexer);

                // mark multiplexer as shutting down
                multiplexer->state = RFCOMM_MULTIPLEXER_SHUTTING_DOWN;

                // emit rfcomm_channel_opened with status and free channel
                // note: repeatedly go over list until full iteration causes no further change
                int done;
                do {
                    done = 1;
                    btstack_linked_item_t * it = (btstack_linked_item_t *) &rfcomm_channels;
                    while (it->next) {
                        rfcomm_channel_t * channel = (rfcomm_channel_t *) it->next;
                        if (channel->multiplexer == multiplexer){
                            done = 0;
                            rfcomm_emit_channel_opened(channel, status);
                            btstack_linked_list_remove(&rfcomm_channels, (btstack_linked_item_t *) channel);
                            btstack_memory_rfcomm_channel_free(channel);
                            break;
                        } else {
                            it = it->next;
                        }
                    }
                } while (!done);

                // free multiplexer
                rfcomm_multiplexer_free(multiplexer);
                return 1;
            }
            
            // following could be: rfcom_multiplexer_state_machein(..., EVENT_L2CAP_OPENED)

            // set max frame size based on l2cap MTU
            multiplexer->max_frame_size = rfcomm_max_frame_size_for_l2cap_mtu(little_endian_read_16(packet, 17));

            if (multiplexer->state == RFCOMM_MULTIPLEXER_W4_CONNECT) {
                log_info("L2CAP_EVENT_CHANNEL_OPENED: outgoing connection");
                // wrong remote addr
                if (bd_addr_cmp(event_addr, multiplexer->remote_addr)) break;
                multiplexer->l2cap_cid = l2cap_cid;
                multiplexer->con_handle = con_handle;
                // send SABM #0
                rfcomm_multiplexer_set_state_and_request_can_send_now_event(multiplexer, RFCOMM_MULTIPLEXER_SEND_SABM_0);

            }
            return 1;
            
            // l2cap disconnect -> state = RFCOMM_MULTIPLEXER_CLOSED;
        
        // Notify channel packet handler if they can send now
        case L2CAP_EVENT_CAN_SEND_NOW:
            l2cap_cid = l2cap_event_can_send_now_get_local_cid(packet);
            rfcomm_handle_can_send_now(l2cap_cid);
            return 1;
            
        case L2CAP_EVENT_CHANNEL_CLOSED:
            // data: event (8), len(8), channel (16)
            l2cap_cid = little_endian_read_16(packet, 2);
            multiplexer = rfcomm_multiplexer_for_l2cap_cid(l2cap_cid);
            log_info("L2CAP_EVENT_CHANNEL_CLOSED cid 0x%0x, mult %p", l2cap_cid, multiplexer);
            if (!multiplexer) break;
            log_info("L2CAP_EVENT_CHANNEL_CLOSED state %u", multiplexer->state);
            // no need to call l2cap_disconnect here, as it's already closed
            rfcomm_multiplexer_finalize(multiplexer);
            return 1;

#ifdef RFCOMM_USE_ERTM
        case L2CAP_EVENT_ERTM_BUFFER_RELEASED:
            l2cap_cid = l2cap_event_ertm_buffer_released_get_local_cid(packet);
            multiplexer = rfcomm_multiplexer_for_l2cap_cid(l2cap_cid);
            if (multiplexer) {
                log_info("buffer for ertm id %u released", multiplexer->ertm_id);
                if (rfcomm_ertm_released_callback){
                    (*rfcomm_ertm_released_callback)(multiplexer->ertm_id);
                }
            }
            break;
#endif

        default:
            break;
    }
    return 0;
}

static int rfcomm_multiplexer_l2cap_packet_handler(uint16_t channel, uint8_t *packet, uint16_t size){
    // get or create a multiplexer for a certain device
    rfcomm_multiplexer_t *multiplexer = rfcomm_multiplexer_for_l2cap_cid(channel);
    if (!multiplexer) return 0;
    
    uint16_t l2cap_cid = multiplexer->l2cap_cid;

	// but only care for multiplexer control channel
    uint8_t frame_dlci = packet[0] >> 2;
    if (frame_dlci) return 0;
    const uint8_t length_offset = (packet[2] & 1) ^ 1;  // to be used for pos >= 3
    const uint8_t credit_offset = ((packet[1] & BT_RFCOMM_UIH_PF) == BT_RFCOMM_UIH_PF) ? 1 : 0;   // credits for uih_pf frames
    const uint8_t payload_offset = 3 + length_offset + credit_offset;
    switch (packet[1]){
            
        case BT_RFCOMM_SABM:
            if (multiplexer->state == RFCOMM_MULTIPLEXER_W4_SABM_0){
                log_info("Received SABM #0");
                multiplexer->outgoing = 0;
                rfcomm_multiplexer_set_state_and_request_can_send_now_event(multiplexer, RFCOMM_MULTIPLEXER_SEND_UA_0);
                return 1;
            }
            break;
            
        case BT_RFCOMM_UA:
            if (multiplexer->state == RFCOMM_MULTIPLEXER_W4_UA_0) {
                // UA #0 -> send UA #0, state = RFCOMM_MULTIPLEXER_OPEN
                log_info("Received UA #0 ");
                rfcomm_multiplexer_opened(multiplexer);
                return 1;
            }
            break;
            
        case BT_RFCOMM_DISC:
            // DISC #0 -> send UA #0, close multiplexer
            log_info("Received DISC #0, (ougoing = %u)", multiplexer->outgoing);
            rfcomm_multiplexer_set_state_and_request_can_send_now_event(multiplexer, RFCOMM_MULTIPLEXER_SEND_UA_0_AND_DISC);
            return 1;
            
        case BT_RFCOMM_DM:
            // DM #0 - we shouldn't get this, just give up
            log_info("Received DM #0");
            log_info("-> Closing down multiplexer");
            rfcomm_multiplexer_finalize(multiplexer);
            l2cap_disconnect(l2cap_cid, 0x13);
            return 1;
            
        case BT_RFCOMM_UIH:
            if (packet[payload_offset] == BT_RFCOMM_CLD_CMD){
                // Multiplexer close down (CLD) -> close mutliplexer
                log_info("Received Multiplexer close down command");
                log_info("-> Closing down multiplexer");
                rfcomm_multiplexer_finalize(multiplexer);
                l2cap_disconnect(l2cap_cid, 0x13);
                return 1;
            }
            switch (packet[payload_offset]){
                case BT_RFCOMM_CLD_CMD:
                     // Multiplexer close down (CLD) -> close mutliplexer
                    log_info("Received Multiplexer close down command");
                    log_info("-> Closing down multiplexer");
                    rfcomm_multiplexer_finalize(multiplexer);
                    l2cap_disconnect(l2cap_cid, 0x13);
                    return 1;

                case BT_RFCOMM_FCON_CMD:
                    multiplexer->fcon = 0x81;
                    l2cap_request_can_send_now_event(multiplexer->l2cap_cid);
                    return 1;

                case BT_RFCOMM_FCOFF_CMD:
                    multiplexer->fcon = 0x80;
                    l2cap_request_can_send_now_event(multiplexer->l2cap_cid);
                    return 1;

                case BT_RFCOMM_TEST_CMD: {
                    log_info("Received test command");
                    int len = packet[payload_offset+1] >> 1; // length < 125
                    if (len > RFCOMM_TEST_DATA_MAX_LEN){
                        len = RFCOMM_TEST_DATA_MAX_LEN;
                    }
                    len = btstack_min(len, size - 1 - payload_offset);  // avoid information leak
                    multiplexer->test_data_len = len;
                    memcpy(multiplexer->test_data, &packet[payload_offset + 2], len);
                    l2cap_request_can_send_now_event(multiplexer->l2cap_cid);
                    return 1;
                }
                default:
                    break;
            }
            break;

        default:
            break;
            
    }
    return 0;
}

static int rfcomm_multiplexer_ready_to_send(rfcomm_multiplexer_t * multiplexer){
    if (multiplexer->send_dm_for_dlci) return 1;
    if (multiplexer->nsc_command) return 1;
    if (multiplexer->fcon & 0x80) return 1;
    switch (multiplexer->state){
        case RFCOMM_MULTIPLEXER_SEND_SABM_0:
        case RFCOMM_MULTIPLEXER_SEND_UA_0:
        case RFCOMM_MULTIPLEXER_SEND_UA_0_AND_DISC:
            return 1;
        case RFCOMM_MULTIPLEXER_OPEN:
            if (multiplexer->test_data_len) {
                return 1;
            }
            break;
        default:
            break;
    }
    return 0;
}

static void rfcomm_multiplexer_state_machine(rfcomm_multiplexer_t * multiplexer, RFCOMM_MULTIPLEXER_EVENT event){
    
    if (event != MULT_EV_READY_TO_SEND) return;

    uint16_t l2cap_cid = multiplexer->l2cap_cid;

    // process stored DM responses
    if (multiplexer->send_dm_for_dlci){
        uint8_t dlci = multiplexer->send_dm_for_dlci;
        multiplexer->send_dm_for_dlci = 0;
        rfcomm_send_dm_pf(multiplexer, dlci);
        return;
    }

    if (multiplexer->nsc_command){
        uint8_t command = multiplexer->nsc_command;
        multiplexer->nsc_command = 0;
        rfcomm_send_uih_nsc_rsp(multiplexer, command);
        return;
    }

    if (multiplexer->fcon & 0x80){
        multiplexer->fcon &= 0x01;
        rfcomm_send_uih_fc_rsp(multiplexer, multiplexer->fcon);

        if (multiplexer->fcon == 0) return;
        // trigger client to send again after sending FCon Response
        rfcomm_notify_channel_can_send();
        return;
    }

    switch (multiplexer->state) {
        case RFCOMM_MULTIPLEXER_SEND_SABM_0:
            log_info("Sending SABM #0 - (multi 0x%p)", multiplexer);
            multiplexer->state = RFCOMM_MULTIPLEXER_W4_UA_0;
            rfcomm_send_sabm(multiplexer, 0);
            break;
        case RFCOMM_MULTIPLEXER_SEND_UA_0:
            log_info("Sending UA #0");
            multiplexer->state = RFCOMM_MULTIPLEXER_OPEN;
            rfcomm_send_ua(multiplexer, 0);

            rfcomm_multiplexer_opened(multiplexer);
            break;
        case RFCOMM_MULTIPLEXER_SEND_UA_0_AND_DISC:
            log_info("Sending UA #0");
            log_info("Closing down multiplexer");
            multiplexer->state = RFCOMM_MULTIPLEXER_CLOSED;
            rfcomm_send_ua(multiplexer, 0);

            rfcomm_multiplexer_finalize(multiplexer);
            l2cap_disconnect(l2cap_cid, 0x13);
            break;
        case RFCOMM_MULTIPLEXER_OPEN:
            // respond to test command
            if (multiplexer->test_data_len){
                int len = multiplexer->test_data_len;
                log_info("Sending TEST Response with %u bytes", len);
                multiplexer->test_data_len = 0;
                rfcomm_send_uih_test_rsp(multiplexer, multiplexer->test_data, len);
                return;
            }
            break;
        default:
            break;
    }
}

// MARK: RFCOMM CHANNEL

static void rfcomm_channel_send_credits(rfcomm_channel_t *channel, uint8_t credits){
    channel->credits_incoming += credits;
    rfcomm_send_uih_credits(channel->multiplexer, channel->dlci, credits);
}

static int rfcomm_channel_can_send(rfcomm_channel_t * channel){
    if (!channel->credits_outgoing) return 0;
    if ((channel->multiplexer->fcon & 1) == 0) return 0;
    return l2cap_can_send_packet_now(channel->multiplexer->l2cap_cid);
}

static void rfcomm_channel_opened(rfcomm_channel_t *rfChannel){
    
    log_info("rfcomm_channel_opened!");
    
    rfChannel->state = RFCOMM_CHANNEL_OPEN;
    rfcomm_emit_channel_opened(rfChannel, 0);
    rfcomm_emit_port_configuration(rfChannel);

    // remove (potential) timer
    rfcomm_multiplexer_t *multiplexer = rfChannel->multiplexer;
    if (multiplexer->timer_active) {
        btstack_run_loop_remove_timer(&multiplexer->timer);
        multiplexer->timer_active = 0;
    }
    // hack for problem detecting authentication failure
    multiplexer->at_least_one_connection = 1;
    
    // request can send now if channel ready 
    if (rfcomm_channel_ready_to_send(rfChannel)){
        l2cap_request_can_send_now_event(multiplexer->l2cap_cid);
    }
}

static void rfcomm_channel_packet_handler_uih(rfcomm_multiplexer_t *multiplexer, uint8_t * packet, uint16_t size){
    const uint8_t frame_dlci = packet[0] >> 2;
    const uint8_t length_offset = (packet[2] & 1) ^ 1;  // to be used for pos >= 3
    const uint8_t credit_offset = ((packet[1] & BT_RFCOMM_UIH_PF) == BT_RFCOMM_UIH_PF) ? 1 : 0;   // credits for uih_pf frames
    const uint8_t payload_offset = 3 + length_offset + credit_offset;
    int request_can_send_now = 0;

    rfcomm_channel_t * channel = rfcomm_channel_for_multiplexer_and_dlci(multiplexer, frame_dlci);
    if (!channel) return;
    
    // handle new outgoing credits
    if (packet[1] == BT_RFCOMM_UIH_PF) {
        
        // add them
        uint16_t new_credits = packet[3+length_offset];
        channel->credits_outgoing += new_credits;
        log_info( "RFCOMM data UIH_PF, new credits channel 0x%02x: %u, now %u", channel->rfcomm_cid, new_credits, channel->credits_outgoing);

        // notify channel statemachine 
        rfcomm_channel_event_t channel_event = { CH_EVT_RCVD_CREDITS, 0 };
        log_debug("rfcomm_channel_state_machine_with_channel, waiting_for_can_send_now %u", channel->waiting_for_can_send_now);
        int rfcomm_channel_valid = 1;
        rfcomm_channel_state_machine_with_channel(channel, &channel_event, &rfcomm_channel_valid);
        if (rfcomm_channel_valid){
            if (rfcomm_channel_ready_to_send(channel) || channel->waiting_for_can_send_now){
                request_can_send_now = 1;
            }
        }        
    }
    
    // contains payload?
    if (size - 1 > payload_offset){

        // log_info( "RFCOMM data UIH_PF, size %u, channel %p", size-payload_offset-1, rfChannel->connection);

        // decrease incoming credit counter
        if (channel->credits_incoming > 0){
            channel->credits_incoming--;
        }
        
        // deliver payload
        (channel->packet_handler)(RFCOMM_DATA_PACKET, channel->rfcomm_cid,
                              &packet[payload_offset], size-payload_offset-1);
    }
    
    // automatically provide new credits to remote device, if no incoming flow control
    if (!channel->incoming_flow_control && channel->credits_incoming < 5){
        channel->new_credits_incoming = RFCOMM_CREDITS;
        request_can_send_now = 1;
    }    

    if (request_can_send_now){
        l2cap_request_can_send_now_event(multiplexer->l2cap_cid);
    }
}

static void rfcomm_channel_accept_pn(rfcomm_channel_t *channel, rfcomm_channel_event_pn_t *event){
    // priority of client request
    channel->pn_priority = event->priority;
    
    // new credits
    channel->credits_outgoing = event->credits_outgoing;
    
    // negotiate max frame size
    if (channel->max_frame_size > channel->multiplexer->max_frame_size) {
        channel->max_frame_size = channel->multiplexer->max_frame_size;
    }
    if (channel->max_frame_size > event->max_frame_size) {
        channel->max_frame_size = event->max_frame_size;
    }
    
}

static void rfcomm_channel_finalize(rfcomm_channel_t *channel){

    rfcomm_multiplexer_t *multiplexer = channel->multiplexer;

    // remove from list
    btstack_linked_list_remove( &rfcomm_channels, (btstack_linked_item_t *) channel);

    // free channel
    btstack_memory_rfcomm_channel_free(channel);
    
    // update multiplexer timeout after channel was removed from list
    rfcomm_multiplexer_prepare_idle_timer(multiplexer);
}

static void rfcomm_channel_state_machine_with_dlci(rfcomm_multiplexer_t * multiplexer, uint8_t dlci, const rfcomm_channel_event_t *event){

    // TODO: if client max frame size is smaller than RFCOMM_DEFAULT_SIZE, send PN

    
    // lookup existing channel
    rfcomm_channel_t * channel = rfcomm_channel_for_multiplexer_and_dlci(multiplexer, dlci);

    // log_info("rfcomm_channel_state_machine_with_dlci lookup dlci #%u = 0x%08x - event %u", dlci, (int) channel, event->type);

    if (channel) {
        int rfcomm_channel_valid = 1;
        rfcomm_channel_state_machine_with_channel(channel, event, &rfcomm_channel_valid);
        if (rfcomm_channel_valid && rfcomm_channel_ready_to_send(channel)){
            l2cap_request_can_send_now_event(multiplexer->l2cap_cid);
        }
        return;
    }
    
    // service registered?
    rfcomm_service_t * service = rfcomm_service_for_channel(dlci >> 1);
    // log_info("rfcomm_channel_state_machine_with_dlci service dlci #%u = 0x%08x", dlci, (int) service);
    if (!service) {
        // discard request by sending disconnected mode
        multiplexer->send_dm_for_dlci = dlci;
        l2cap_request_can_send_now_event(multiplexer->l2cap_cid);
        return;
    }

    // create channel for some events
    switch (event->type) {
        case CH_EVT_RCVD_SABM:
        case CH_EVT_RCVD_PN:
        case CH_EVT_RCVD_RPN_REQ:
        case CH_EVT_RCVD_RPN_CMD:
            // setup incoming channel
            channel = rfcomm_channel_create(multiplexer, service, dlci >> 1);
            if (!channel){
                // discard request by sending disconnected mode
                multiplexer->send_dm_for_dlci = dlci;
                l2cap_request_can_send_now_event(multiplexer->l2cap_cid);
            }
            break;
        default:
            break;
    }

    if (!channel) {
        // discard request by sending disconnected mode
        multiplexer->send_dm_for_dlci = dlci;
        l2cap_request_can_send_now_event(multiplexer->l2cap_cid);
        return;
    }

    int rfcomm_channel_valid = 1;
    rfcomm_channel_state_machine_with_channel(channel, event, &rfcomm_channel_valid);
    if (rfcomm_channel_valid && rfcomm_channel_ready_to_send(channel)){
        l2cap_request_can_send_now_event(multiplexer->l2cap_cid);
    }
}

static void rfcomm_channel_packet_handler(rfcomm_multiplexer_t * multiplexer,  uint8_t *packet, uint16_t size){

    UNUSED(size);   // ok: fixed format messages
        
    // rfcomm: (0) addr [76543 server channel] [2 direction: initiator uses 1] [1 C/R: CMD by initiator = 1] [0 EA=1]
    const uint8_t frame_dlci = packet[0] >> 2;
    uint8_t message_dlci; // used by commands in UIH(_PF) packets 
	uint8_t message_len;  //   "
    
    // rfcomm: (1) command/control
    // -- credits_offset = 1 if command == BT_RFCOMM_UIH_PF
    const uint8_t credit_offset = ((packet[1] & BT_RFCOMM_UIH_PF) == BT_RFCOMM_UIH_PF) ? 1 : 0;   // credits for uih_pf frames
    // rfcomm: (2) length. if bit 0 is cleared, 2 byte length is used. (little endian)
    const uint8_t length_offset = (packet[2] & 1) ^ 1;  // to be used for pos >= 3
    // rfcomm: (3+length_offset) credits if credits_offset == 1
    // rfcomm: (3+length_offest+credits_offset)
    const uint8_t payload_offset = 3 + length_offset + credit_offset;
    
    rfcomm_channel_event_t event;
    rfcomm_channel_event_pn_t event_pn;
    rfcomm_channel_event_rpn_t event_rpn;
    rfcomm_channel_event_msc_t event_msc;

    // switch by rfcomm message type
    switch(packet[1]) {
            
        case BT_RFCOMM_SABM:
            event.type = CH_EVT_RCVD_SABM;
            log_info("Received SABM #%u", frame_dlci);
            rfcomm_channel_state_machine_with_dlci(multiplexer, frame_dlci, &event);
            break;
            
        case BT_RFCOMM_UA:
            event.type = CH_EVT_RCVD_UA;
            log_info("Received UA #%u",frame_dlci);
            rfcomm_channel_state_machine_with_dlci(multiplexer, frame_dlci, &event);
            break;
            
        case BT_RFCOMM_DISC:
            event.type = CH_EVT_RCVD_DISC;
            rfcomm_channel_state_machine_with_dlci(multiplexer, frame_dlci, &event);
            break;
            
        case BT_RFCOMM_DM:
        case BT_RFCOMM_DM_PF:
            event.type = CH_EVT_RCVD_DM;
            rfcomm_channel_state_machine_with_dlci(multiplexer, frame_dlci, &event);
            break;
            
        case BT_RFCOMM_UIH_PF:
        case BT_RFCOMM_UIH:

            message_len  = packet[payload_offset+1] >> 1;

            switch (packet[payload_offset]) {
                case BT_RFCOMM_PN_CMD:
                    message_dlci = packet[payload_offset+2];
                    event_pn.super.type = CH_EVT_RCVD_PN;
                    event_pn.priority = packet[payload_offset+4];
                    event_pn.max_frame_size = little_endian_read_16(packet, payload_offset+6);
                    event_pn.credits_outgoing = packet[payload_offset+9];
                    log_info("Received UIH Parameter Negotiation Command for #%u, credits %u",
                        message_dlci, event_pn.credits_outgoing);
                    rfcomm_channel_state_machine_with_dlci(multiplexer, message_dlci, (rfcomm_channel_event_t*) &event_pn);
                    break;
                    
                case BT_RFCOMM_PN_RSP:
                    message_dlci = packet[payload_offset+2];
                    event_pn.super.type = CH_EVT_RCVD_PN_RSP;
                    event_pn.priority = packet[payload_offset+4];
                    event_pn.max_frame_size = little_endian_read_16(packet, payload_offset+6);
                    event_pn.credits_outgoing = packet[payload_offset+9];
                    log_info("Received UIH Parameter Negotiation Response max frame %u, credits %u",
                            event_pn.max_frame_size, event_pn.credits_outgoing);
                    rfcomm_channel_state_machine_with_dlci(multiplexer, message_dlci, (rfcomm_channel_event_t*) &event_pn);
                    break;
                    
                case BT_RFCOMM_MSC_CMD: 
                    message_dlci = packet[payload_offset+2] >> 2;
                    event_msc.super.type = CH_EVT_RCVD_MSC_CMD;
                    event_msc.modem_status = packet[payload_offset+3];
                    log_info("Received MSC CMD for #%u, ", message_dlci);
                    rfcomm_channel_state_machine_with_dlci(multiplexer, message_dlci, (rfcomm_channel_event_t*) &event_msc);
                    break;
                    
                case BT_RFCOMM_MSC_RSP:
                    message_dlci = packet[payload_offset+2] >> 2;
                    event.type = CH_EVT_RCVD_MSC_RSP;
                    log_info("Received MSC RSP for #%u", message_dlci);
                    rfcomm_channel_state_machine_with_dlci(multiplexer, message_dlci, &event);
                    break;
                    
                case BT_RFCOMM_RPN_CMD:
                    message_dlci = packet[payload_offset+2] >> 2;
                    switch (message_len){
                        case 1:
                            log_info("Received Remote Port Negotiation Request for #%u", message_dlci);
                            event.type = CH_EVT_RCVD_RPN_REQ;
                            rfcomm_channel_state_machine_with_dlci(multiplexer, message_dlci, &event);
                            break;
                        case 8:
                            log_info("Received Remote Port Negotiation Update for #%u", message_dlci);
                            event_rpn.super.type = CH_EVT_RCVD_RPN_CMD;
                            event_rpn.data = *(rfcomm_rpn_data_t*) &packet[payload_offset+3];
                            rfcomm_channel_state_machine_with_dlci(multiplexer, message_dlci, (rfcomm_channel_event_t*) &event_rpn);
                            break;
                        default:
                            break;
                    }
                    break;

                case BT_RFCOMM_RPN_RSP:
                    log_info("Received RPN response");
                    break;

                case BT_RFCOMM_RLS_CMD: {
                    log_info("Received RLS command");
                    message_dlci = packet[payload_offset+2] >> 2;
                    rfcomm_channel_event_rls_t event_rls;
                    event_rls.super.type = CH_EVT_RCVD_RLS_CMD;
                    event_rls.line_status = packet[payload_offset+3];
                    rfcomm_channel_state_machine_with_dlci(multiplexer, message_dlci, (rfcomm_channel_event_t*) &event_rls);
                    break;
                }

                case BT_RFCOMM_RLS_RSP:
                    log_info("Received RLS response");
                    break;

                // Following commands are handled by rfcomm_multiplexer_l2cap_packet_handler
                // case BT_RFCOMM_TEST_CMD: 
                // case BT_RFCOMM_FCOFF_CMD:
                // case BT_RFCOMM_FCON_CMD: 
                // everything else is an not supported command
                default: {
                    log_error("Received unknown UIH command packet - 0x%02x", packet[payload_offset]); 
                    multiplexer->nsc_command = packet[payload_offset];
                    break;
                }
            }
            break;
            
        default:
            log_error("Received unknown RFCOMM message type %x", packet[1]);
            break;
    }
    
    // trigger next action - example W4_PN_RSP: transition to SEND_SABM which only depends on "can send"
    if (rfcomm_multiplexer_ready_to_send(multiplexer)){
        l2cap_request_can_send_now_event(multiplexer->l2cap_cid);
    }
}

static void rfcomm_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    
    if (packet_type == HCI_EVENT_PACKET){
        rfcomm_hci_event_handler(packet, size);
        return;
    }

    // we only handle l2cap packets for:
    if (packet_type != L2CAP_DATA_PACKET) return;

    //  - multiplexer itself
    int handled = rfcomm_multiplexer_l2cap_packet_handler(channel, packet, size);

    if (handled) return;
    
    // - channel over open mutliplexer
    rfcomm_multiplexer_t * multiplexer = rfcomm_multiplexer_for_l2cap_cid(channel);
    if (!multiplexer || multiplexer->state != RFCOMM_MULTIPLEXER_OPEN) return;
    
    // channel data ?
    // rfcomm: (0) addr [76543 server channel] [2 direction: initiator uses 1] [1 C/R: CMD by initiator = 1] [0 EA=1]
    const uint8_t frame_dlci = packet[0] >> 2;
	
    if (frame_dlci && (packet[1] == BT_RFCOMM_UIH || packet[1] == BT_RFCOMM_UIH_PF)) {
        rfcomm_channel_packet_handler_uih(multiplexer, packet, size);
        return;
    }
     
    rfcomm_channel_packet_handler(multiplexer, packet, size);
}

static int rfcomm_channel_ready_for_open(rfcomm_channel_t *channel){
    // note: exchanging MSC isn't neccessary to consider channel open
    // note: having outgoing credits is also not necessary to consider channel open
    // log_info("rfcomm_channel_ready_for_open state %u, flags needed %04x, current %04x, rf credits %u, l2cap credits %u ", channel->state, RFCOMM_CHANNEL_STATE_VAR_RCVD_MSC_RSP|RFCOMM_CHANNEL_STATE_VAR_SENT_MSC_RSP|RFCOMM_CHANNEL_STATE_VAR_SENT_CREDITS, channel->state_var, channel->credits_outgoing, channel->multiplexer->l2cap_credits);
    // if ((channel->state_var & RFCOMM_CHANNEL_STATE_VAR_SENT_MSC_RSP) == 0) return 0;
    // if (channel->credits_outgoing == 0) return 0;
    log_info("rfcomm_channel_ready_for_open state %u, flags needed %04x, current %04x, rf credits %u",
         channel->state, RFCOMM_CHANNEL_STATE_VAR_RCVD_MSC_RSP, channel->state_var, channel->credits_outgoing);
    if ((channel->state_var & RFCOMM_CHANNEL_STATE_VAR_RCVD_MSC_RSP) == 0) return 0;
    if ((channel->state_var & RFCOMM_CHANNEL_STATE_VAR_SENT_CREDITS) == 0) return 0;
    
    return 1;
}

static int rfcomm_channel_ready_for_incoming_dlc_setup(rfcomm_channel_t * channel){
    log_info("rfcomm_channel_ready_for_incoming_dlc_setup state var %04x", channel->state_var);
    // Client accept and SABM/UA is required, PN RSP is needed if PN was received
    if ((channel->state_var & RFCOMM_CHANNEL_STATE_VAR_CLIENT_ACCEPTED) == 0) return 0;
    if ((channel->state_var & RFCOMM_CHANNEL_STATE_VAR_RCVD_SABM      ) == 0) return 0;
    if ((channel->state_var & RFCOMM_CHANNEL_STATE_VAR_SEND_UA        ) != 0) return 0;
    if ((channel->state_var & RFCOMM_CHANNEL_STATE_VAR_SEND_PN_RSP    ) != 0) return 0;
    return 1;
}            

inline static void rfcomm_channel_state_add(rfcomm_channel_t *channel, RFCOMM_CHANNEL_STATE_VAR event){
    channel->state_var = (RFCOMM_CHANNEL_STATE_VAR) (channel->state_var | event);    
}
inline static void rfcomm_channel_state_remove(rfcomm_channel_t *channel, RFCOMM_CHANNEL_STATE_VAR event){
    channel->state_var = (RFCOMM_CHANNEL_STATE_VAR) (channel->state_var & ~event);    
}

static int rfcomm_channel_ready_to_send(rfcomm_channel_t * channel){
    switch (channel->state){
        case RFCOMM_CHANNEL_SEND_UIH_PN:
            log_debug("ch-ready: state %u", channel->state);
            return 1;
        case RFCOMM_CHANNEL_SEND_SABM_W4_UA:
            log_debug("ch-ready: state %u", channel->state);
            return 1;
        case RFCOMM_CHANNEL_SEND_UA_AFTER_DISC:
            log_debug("ch-ready: state %u", channel->state);
            return 1;
        case RFCOMM_CHANNEL_SEND_DISC:
            log_debug("ch-ready: state %u", channel->state);
            return 1;
        case RFCOMM_CHANNEL_SEND_DM:
            log_debug("ch-ready: state %u", channel->state);
            return 1;
        case RFCOMM_CHANNEL_OPEN:
            if (channel->new_credits_incoming) { 
                log_debug("ch-ready: channel open & new_credits_incoming") ; 
                return 1;
            }
            break;
        case RFCOMM_CHANNEL_DLC_SETUP:
            if (channel->state_var & (
                RFCOMM_CHANNEL_STATE_VAR_SEND_MSC_CMD  |
                RFCOMM_CHANNEL_STATE_VAR_SEND_CREDITS
             )) {
                log_debug("ch-ready: channel dlc setup & send msc cmd or send credits") ; 
                return 1;
            }
            break;

        default:
            break;
    }
    
    if (channel->state_var & (
        RFCOMM_CHANNEL_STATE_VAR_SEND_PN_RSP   |
        RFCOMM_CHANNEL_STATE_VAR_SEND_RPN_INFO |
        RFCOMM_CHANNEL_STATE_VAR_SEND_RPN_RSP  |
        RFCOMM_CHANNEL_STATE_VAR_SEND_UA       |
        RFCOMM_CHANNEL_STATE_VAR_SEND_MSC_RSP
                             )){
        log_debug("ch-ready: state %x, state var %x", channel->state, channel->state_var);
        return 1;
    }
    
    if (channel->rls_line_status != RFCOMM_RLS_STATUS_INVALID) {
        log_debug("ch-ready: rls_line_status");
        return 1;
    }

    return 0;
}


static void rfcomm_channel_state_machine_with_channel(rfcomm_channel_t *channel, const rfcomm_channel_event_t *event, int * out_channel_valid){
    
    // log_info("rfcomm_channel_state_machine_with_channel: state %u, state_var %04x, event %u", channel->state, channel->state_var ,event->type);

    // channel != NULL -> channel valid
    *out_channel_valid = 1;

    rfcomm_multiplexer_t *multiplexer = channel->multiplexer;
    
    // TODO: integrate in common switch
    if (event->type == CH_EVT_RCVD_DISC){
        rfcomm_emit_channel_closed(channel);
        channel->state = RFCOMM_CHANNEL_SEND_UA_AFTER_DISC;
        return;
    }
    
    // TODO: integrate in common switch
    if (event->type == CH_EVT_RCVD_DM){
        log_info("Received DM message for #%u", channel->dlci);
        log_info("-> Closing channel locally for #%u", channel->dlci);
        rfcomm_channel_emit_final_event(channel, ERROR_CODE_CONNECTION_REJECTED_DUE_TO_LIMITED_RESOURCES);
        rfcomm_channel_finalize(channel);
        *out_channel_valid = 0;
        return;
    }
    
    // remote port negotiation command - just accept everything for now
    //
    // "The RPN command can be used before a new DLC is opened and should be used whenever the port settings change."
    // "The RPN command is specified as optional in TS 07.10, but it is mandatory to recognize and respond to it in RFCOMM. 
    //   (Although the handling of individual settings are implementation-dependent.)"
    //
    
    // TODO: integrate in common switch
    if (event->type == CH_EVT_RCVD_RPN_CMD){
        // control port parameters
        rfcomm_channel_event_rpn_t *event_rpn = (rfcomm_channel_event_rpn_t*) event;
        rfcomm_rpn_data_update(&channel->rpn_data, &event_rpn->data);
        rfcomm_channel_state_add(channel, RFCOMM_CHANNEL_STATE_VAR_SEND_RPN_RSP);
        // notify client about new settings
        rfcomm_emit_port_configuration(channel);
        return;
    }

    // TODO: integrate in common switch
    if (event->type == CH_EVT_RCVD_RPN_REQ){
        // no values got accepted (no values have beens sent)
        channel->rpn_data.parameter_mask_0 = 0x00;
        channel->rpn_data.parameter_mask_1 = 0x00;
        rfcomm_channel_state_add(channel, RFCOMM_CHANNEL_STATE_VAR_SEND_RPN_RSP);
        return;
    }
    
    if (event->type == CH_EVT_RCVD_RLS_CMD){ 
        rfcomm_channel_event_rls_t * event_rls = (rfcomm_channel_event_rls_t*) event;
        channel->rls_line_status = event_rls->line_status & 0x0f;
        log_info("CH_EVT_RCVD_RLS_CMD setting line status to 0x%0x", channel->rls_line_status);
        rfcomm_emit_remote_line_status(channel, event_rls->line_status);
        return;
    }

    // TODO: integrate in common switch
    if (event->type == CH_EVT_READY_TO_SEND){
        if (channel->state_var & RFCOMM_CHANNEL_STATE_VAR_SEND_RPN_RSP){
            log_info("Sending Remote Port Negotiation RSP for #%u", channel->dlci);
            rfcomm_channel_state_remove(channel, RFCOMM_CHANNEL_STATE_VAR_SEND_RPN_RSP);
            rfcomm_send_uih_rpn_rsp(multiplexer, channel->dlci, &channel->rpn_data);
            return;
        }
        if (channel->state_var & RFCOMM_CHANNEL_STATE_VAR_SEND_MSC_RSP){
            log_info("Sending MSC RSP for #%u", channel->dlci);
            rfcomm_channel_state_remove(channel, RFCOMM_CHANNEL_STATE_VAR_SEND_MSC_RSP);
            rfcomm_channel_state_add(channel, RFCOMM_CHANNEL_STATE_VAR_SENT_MSC_RSP);
            rfcomm_send_uih_msc_rsp(multiplexer, channel->dlci, 0x8d);  // ea=1,fc=0,rtc=1,rtr=1,ic=0,dv=1
            return;
        }
        if (channel->rls_line_status != RFCOMM_RLS_STATUS_INVALID){
            log_info("Sending RLS RSP 0x%0x", channel->rls_line_status);
            uint8_t line_status = channel->rls_line_status;
            channel->rls_line_status = RFCOMM_RLS_STATUS_INVALID;
            rfcomm_send_uih_rls_rsp(multiplexer, channel->dlci, line_status);
            return;
        }
    }
    
    // emit MSC status to app
    if (event->type == CH_EVT_RCVD_MSC_CMD){
        // notify client about new settings
        rfcomm_channel_event_msc_t *event_msc = (rfcomm_channel_event_msc_t*) event;
        uint8_t modem_status_event[2+1];
        modem_status_event[0] = RFCOMM_EVENT_REMOTE_MODEM_STATUS;
        modem_status_event[1] = 1;
        modem_status_event[2] = event_msc->modem_status;
        (channel->packet_handler)(HCI_EVENT_PACKET, channel->rfcomm_cid, (uint8_t*)&modem_status_event, sizeof(modem_status_event));
        // no return, MSC_CMD will be handled by state machine below
    }

    rfcomm_channel_event_pn_t * event_pn = (rfcomm_channel_event_pn_t*) event;
    
    switch (channel->state) {
        case RFCOMM_CHANNEL_CLOSED:
            switch (event->type){
                case CH_EVT_RCVD_SABM:
                    log_info("-> Inform app");
                    rfcomm_channel_state_add(channel, RFCOMM_CHANNEL_STATE_VAR_RCVD_SABM);
                    channel->state = RFCOMM_CHANNEL_INCOMING_SETUP;
                    rfcomm_emit_connection_request(channel);
                    break;
                case CH_EVT_RCVD_PN:
                    rfcomm_channel_accept_pn(channel, event_pn);
                    log_info("-> Inform app");
                    rfcomm_channel_state_add(channel, RFCOMM_CHANNEL_STATE_VAR_RCVD_PN);
                    channel->state = RFCOMM_CHANNEL_INCOMING_SETUP;
                    rfcomm_emit_connection_request(channel);
                    break;
                default:
                    break;
            }
            break;

        case RFCOMM_CHANNEL_INCOMING_SETUP:
            switch (event->type){
                case CH_EVT_RCVD_SABM:
                    rfcomm_channel_state_add(channel, RFCOMM_CHANNEL_STATE_VAR_RCVD_SABM);
                    if (channel->state_var & RFCOMM_CHANNEL_STATE_VAR_CLIENT_ACCEPTED) {
                        rfcomm_channel_state_add(channel, RFCOMM_CHANNEL_STATE_VAR_SEND_UA);
                    }
                    break;
                case CH_EVT_RCVD_PN:
                    rfcomm_channel_accept_pn(channel, event_pn);
                    rfcomm_channel_state_add(channel, RFCOMM_CHANNEL_STATE_VAR_RCVD_PN);
                    if (channel->state_var & RFCOMM_CHANNEL_STATE_VAR_CLIENT_ACCEPTED) {
                        rfcomm_channel_state_add(channel, RFCOMM_CHANNEL_STATE_VAR_SEND_PN_RSP);
                    }
                    break;
                case CH_EVT_READY_TO_SEND:
                    // if / else if is used to check for state transition after sending
                    if (channel->state_var & RFCOMM_CHANNEL_STATE_VAR_SEND_PN_RSP){
                        log_info("Sending UIH Parameter Negotiation Respond for #%u", channel->dlci);
                        rfcomm_channel_state_remove(channel, RFCOMM_CHANNEL_STATE_VAR_SEND_PN_RSP);
                        rfcomm_send_uih_pn_response(multiplexer, channel->dlci, channel->pn_priority, channel->max_frame_size);
                    } else if (channel->state_var & RFCOMM_CHANNEL_STATE_VAR_SEND_UA){
                        log_info("Sending UA #%u", channel->dlci);
                        rfcomm_channel_state_remove(channel, RFCOMM_CHANNEL_STATE_VAR_SEND_UA);
                        rfcomm_send_ua(multiplexer, channel->dlci);
                    }
                    if (rfcomm_channel_ready_for_incoming_dlc_setup(channel)){
                        log_info("Incomping setup done, requesting send MSC CMD and send Credits");
                        rfcomm_channel_state_add(channel, RFCOMM_CHANNEL_STATE_VAR_SEND_MSC_CMD);
                        rfcomm_channel_state_add(channel, RFCOMM_CHANNEL_STATE_VAR_SEND_CREDITS);
                        channel->state = RFCOMM_CHANNEL_DLC_SETUP;
                     }
                    break;
                default:
                    break;
            }
            break;
            
        case RFCOMM_CHANNEL_W4_MULTIPLEXER:
            switch (event->type) {
                case CH_EVT_MULTIPLEXER_READY:
                    log_info("Muliplexer opened, sending UIH PN next");
                    channel->state = RFCOMM_CHANNEL_SEND_UIH_PN;
                    break;
                default:
                    break;
            }
            break;
            
        case RFCOMM_CHANNEL_SEND_UIH_PN:
            switch (event->type) {
                case CH_EVT_READY_TO_SEND:
                    // update mtu
                    channel->max_frame_size = btstack_min(multiplexer->max_frame_size, channel->max_frame_size);
                    log_info("Sending UIH Parameter Negotiation Command for #%u (channel 0x%p) mtu %u", channel->dlci, channel, channel->max_frame_size );
                    channel->state = RFCOMM_CHANNEL_W4_PN_RSP;
                    rfcomm_send_uih_pn_command(multiplexer, channel->dlci, channel->max_frame_size);
                    break;
                default:
                    break;
            }
            break;
            
        case RFCOMM_CHANNEL_W4_PN_RSP:
            switch (event->type){
                case CH_EVT_RCVD_PN_RSP:
                    // update max frame size
                    if (channel->max_frame_size > event_pn->max_frame_size) {
                        channel->max_frame_size = event_pn->max_frame_size;
                    }
                    // new credits
                    channel->credits_outgoing = event_pn->credits_outgoing;
                    channel->state = RFCOMM_CHANNEL_SEND_SABM_W4_UA;
                    break;
                default:
                    break;
            }
            break;

        case RFCOMM_CHANNEL_SEND_SABM_W4_UA:
            switch (event->type) {
                case CH_EVT_READY_TO_SEND:
                    log_info("Sending SABM #%u", channel->dlci);
                    channel->state = RFCOMM_CHANNEL_W4_UA;
                    rfcomm_send_sabm(multiplexer, channel->dlci);
                    break;
                default:
                    break;
            }
            break;
            
        case RFCOMM_CHANNEL_W4_UA:
            switch (event->type){
                case CH_EVT_RCVD_UA:
                    channel->state = RFCOMM_CHANNEL_DLC_SETUP;
                    rfcomm_channel_state_add(channel, RFCOMM_CHANNEL_STATE_VAR_SEND_MSC_CMD);
                    rfcomm_channel_state_add(channel, RFCOMM_CHANNEL_STATE_VAR_SEND_CREDITS);
                    break;
                default:
                    break;
            }
            break;

        case RFCOMM_CHANNEL_DLC_SETUP:
            switch (event->type){
                case CH_EVT_RCVD_MSC_CMD:
                    rfcomm_channel_state_add(channel, RFCOMM_CHANNEL_STATE_VAR_RCVD_MSC_CMD);
                    rfcomm_channel_state_add(channel, RFCOMM_CHANNEL_STATE_VAR_SEND_MSC_RSP);
                    break;
                case CH_EVT_RCVD_MSC_RSP:
                    rfcomm_channel_state_add(channel, RFCOMM_CHANNEL_STATE_VAR_RCVD_MSC_RSP);
                    break;
                    
                case CH_EVT_READY_TO_SEND:
                    if (channel->state_var & RFCOMM_CHANNEL_STATE_VAR_SEND_MSC_CMD){
                        log_info("Sending MSC CMD for #%u", channel->dlci);
                        rfcomm_channel_state_remove(channel, RFCOMM_CHANNEL_STATE_VAR_SEND_MSC_CMD);
                        rfcomm_channel_state_add(channel, RFCOMM_CHANNEL_STATE_VAR_SENT_MSC_CMD);
                        rfcomm_send_uih_msc_cmd(multiplexer, channel->dlci , 0x8d);  // ea=1,fc=0,rtc=1,rtr=1,ic=0,dv=1
                        break;
                    }
                    if (channel->state_var & RFCOMM_CHANNEL_STATE_VAR_SEND_CREDITS){
                        log_info("Providing credits for #%u", channel->dlci);
                        rfcomm_channel_state_remove(channel, RFCOMM_CHANNEL_STATE_VAR_SEND_CREDITS);
                        rfcomm_channel_state_add(channel, RFCOMM_CHANNEL_STATE_VAR_SENT_CREDITS);
                        
                        if (channel->new_credits_incoming) {
                            uint8_t new_credits = channel->new_credits_incoming;
                            channel->new_credits_incoming = 0;
                            rfcomm_channel_send_credits(channel, new_credits);
                        }
                        break;

                    }
                    break;
                default:
                    break;
            }
            // finally done?
            if (rfcomm_channel_ready_for_open(channel)){
                channel->state = RFCOMM_CHANNEL_OPEN;
                rfcomm_channel_opened(channel);
            }
            break;
        
        case RFCOMM_CHANNEL_OPEN:
            switch (event->type){
                case CH_EVT_RCVD_MSC_CMD:
                    rfcomm_channel_state_add(channel, RFCOMM_CHANNEL_STATE_VAR_SEND_MSC_RSP);
                    break;
                case CH_EVT_READY_TO_SEND:
                    if (channel->new_credits_incoming) {
                        uint8_t new_credits = channel->new_credits_incoming;
                        channel->new_credits_incoming = 0;
                        rfcomm_channel_send_credits(channel, new_credits);
                        break;
                    }
                    break;
                case CH_EVT_RCVD_CREDITS:
                    rfcomm_notify_channel_can_send();
                    break;
                default:
                    break;
            }
            break;
            
        case RFCOMM_CHANNEL_SEND_DM:
            switch (event->type) {
                case CH_EVT_READY_TO_SEND:
                    log_info("Sending DM_PF for #%u", channel->dlci);
                    // don't emit channel closed - channel was never open
                    channel->state = RFCOMM_CHANNEL_CLOSED;
                    rfcomm_send_dm_pf(multiplexer, channel->dlci);
                    rfcomm_channel_finalize(channel);
                    *out_channel_valid = 0;
                    break;
                default:
                    break;
            }
            break;
            
        case RFCOMM_CHANNEL_SEND_DISC:
            switch (event->type) {
                case CH_EVT_READY_TO_SEND:
                    channel->state = RFCOMM_CHANNEL_W4_UA_AFTER_DISC;
                    rfcomm_send_disc(multiplexer, channel->dlci);
                    break;
                default:
                    break;
            }
            break;

        case RFCOMM_CHANNEL_W4_UA_AFTER_DISC:
            switch (event->type){
                case CH_EVT_RCVD_UA:
                    channel->state = RFCOMM_CHANNEL_CLOSED;
                    rfcomm_emit_channel_closed(channel);
                    rfcomm_channel_finalize(channel);
                    *out_channel_valid = 0;
                    break;
                default:
                    break;
            }
            break;
                        
        case RFCOMM_CHANNEL_SEND_UA_AFTER_DISC:
            switch (event->type) {
                case CH_EVT_READY_TO_SEND:
                    log_info("Sending UA after DISC for #%u", channel->dlci);
                    channel->state = RFCOMM_CHANNEL_CLOSED;
                    rfcomm_send_ua(multiplexer, channel->dlci);
                    rfcomm_channel_finalize(channel);
                    *out_channel_valid = 0;
                    break;
                default:
                    break;
            }
            break;
            
        default:
            break;
    }
}

// MARK: RFCOMM BTstack API

void rfcomm_init(void){
    rfcomm_client_cid_generator = 0;
    rfcomm_multiplexers = NULL;
    rfcomm_services     = NULL;
    rfcomm_channels     = NULL;
    rfcomm_security_level = LEVEL_2;
}

void rfcomm_set_required_security_level(gap_security_level_t security_level){
    rfcomm_security_level = security_level;
}

int rfcomm_can_send_packet_now(uint16_t rfcomm_cid){
    rfcomm_channel_t * channel = rfcomm_channel_for_rfcomm_cid(rfcomm_cid);
    if (!channel){
        log_error("rfcomm_send cid 0x%02x doesn't exist!", rfcomm_cid);
        return 0;
    }
    return rfcomm_channel_can_send(channel);
}

void rfcomm_request_can_send_now_event(uint16_t rfcomm_cid){
    rfcomm_channel_t * channel = rfcomm_channel_for_rfcomm_cid(rfcomm_cid);
    if (!channel){
        log_error("rfcomm_send cid 0x%02x doesn't exist!", rfcomm_cid);
        return;
    }
    channel->waiting_for_can_send_now = 1;
    l2cap_request_can_send_now_event(channel->multiplexer->l2cap_cid);
}

static int rfcomm_assert_send_valid(rfcomm_channel_t * channel , uint16_t len){
    if (len > channel->max_frame_size){
        log_error("rfcomm_send cid 0x%02x, rfcomm data lenght exceeds MTU!", channel->rfcomm_cid);
        return RFCOMM_DATA_LEN_EXCEEDS_MTU;
    }

#ifdef RFCOMM_USE_OUTGOING_BUFFER
    if (len > rfcomm_max_frame_size_for_l2cap_mtu(sizeof(outgoing_buffer))){
        log_error("rfcomm_send cid 0x%02x, length exceeds outgoing rfcomm_out_buffer", channel->rfcomm_cid);
        return RFCOMM_DATA_LEN_EXCEEDS_MTU;
    }
#endif

    if (!channel->credits_outgoing){
        log_info("rfcomm_send cid 0x%02x, no rfcomm outgoing credits!", channel->rfcomm_cid);
        return RFCOMM_NO_OUTGOING_CREDITS;
    }
    
    if ((channel->multiplexer->fcon & 1) == 0){
        log_info("rfcomm_send cid 0x%02x, aggregate flow off!", channel->rfcomm_cid);
        return RFCOMM_AGGREGATE_FLOW_OFF;
    }
    return 0;    
}

uint16_t rfcomm_get_max_frame_size(uint16_t rfcomm_cid){
    rfcomm_channel_t * channel = rfcomm_channel_for_rfcomm_cid(rfcomm_cid);
    if (!channel){
        log_error("rfcomm_get_max_frame_size cid 0x%02x doesn't exist!", rfcomm_cid);
        return 0;
    }
    return channel->max_frame_size;
}

// pre: rfcomm_can_send_packet_now(rfcomm_cid) == true
int rfcomm_reserve_packet_buffer(void){
#ifdef RFCOMM_USE_OUTGOING_BUFFER
    log_error("rfcomm_reserve_packet_buffer should not get called with ERTM");
    return 0;
#else
    return l2cap_reserve_packet_buffer();
#endif
}

void rfcomm_release_packet_buffer(void){
#ifdef RFCOMM_USE_OUTGOING_BUFFER
    log_error("rfcomm_release_packet_buffer should not get called with ERTM");
#else
    l2cap_release_packet_buffer();
#endif
}

uint8_t * rfcomm_get_outgoing_buffer(void){
#ifdef RFCOMM_USE_OUTGOING_BUFFER
    uint8_t * rfcomm_out_buffer = outgoing_buffer;
#else
    uint8_t * rfcomm_out_buffer = l2cap_get_outgoing_buffer();
#endif
    // address + control + length (16) + no credit field
    return &rfcomm_out_buffer[4];
}

int rfcomm_send_prepared(uint16_t rfcomm_cid, uint16_t len){
    rfcomm_channel_t * channel = rfcomm_channel_for_rfcomm_cid(rfcomm_cid);
    if (!channel){
        log_error("rfcomm_send_prepared cid 0x%02x doesn't exist!", rfcomm_cid);
        return 0;
    }

    int err = rfcomm_assert_send_valid(channel, len);
    if (err) return err;

#ifdef RFCOMM_USE_OUTGOING_BUFFER
    if (!l2cap_can_send_packet_now(channel->multiplexer->l2cap_cid)){
        log_error("rfcomm_send_prepared: l2cap cannot send now");
        return BTSTACK_ACL_BUFFERS_FULL;
    }
#else
    if (!l2cap_can_send_prepared_packet_now(channel->multiplexer->l2cap_cid)){
        log_error("rfcomm_send_prepared: l2cap cannot send now");
        return BTSTACK_ACL_BUFFERS_FULL;
    }
#endif

    // send might cause l2cap to emit new credits, update counters first
    if (len){
        channel->credits_outgoing--;
    } else {
        log_info("sending empty RFCOMM packet for cid %02x", rfcomm_cid);
    }
        
    int result = rfcomm_send_uih_prepared(channel->multiplexer, channel->dlci, len);
    
    if (result != 0) {
        if (len) {
            channel->credits_outgoing++;
        }
        log_error("rfcomm_send_prepared: error %d", result);
        return result;
    }
    
    return result;
}

int rfcomm_send(uint16_t rfcomm_cid, uint8_t *data, uint16_t len){
    rfcomm_channel_t * channel = rfcomm_channel_for_rfcomm_cid(rfcomm_cid);
    if (!channel){
        log_error("cid 0x%02x doesn't exist!", rfcomm_cid);
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    int err = rfcomm_assert_send_valid(channel, len);
    if (err) return err;
    if (!l2cap_can_send_packet_now(channel->multiplexer->l2cap_cid)){
        log_error("rfcomm_send_internal: l2cap cannot send now");
        return BTSTACK_ACL_BUFFERS_FULL;
    }

#ifdef RFCOMM_USE_OUTGOING_BUFFER
#else
    rfcomm_reserve_packet_buffer();
#endif
    uint8_t * rfcomm_payload = rfcomm_get_outgoing_buffer();

    memcpy(rfcomm_payload, data, len);
    err = rfcomm_send_prepared(rfcomm_cid, len);    

#ifdef RFCOMM_USE_OUTGOING_BUFFER
#else
    if (err){
        rfcomm_release_packet_buffer();
    }
#endif

    return err;
}

// Sends Local Lnie Status, see LINE_STATUS_..
int rfcomm_send_local_line_status(uint16_t rfcomm_cid, uint8_t line_status){
    rfcomm_channel_t * channel = rfcomm_channel_for_rfcomm_cid(rfcomm_cid);
    if (!channel){
        log_error("rfcomm_send_local_line_status cid 0x%02x doesn't exist!", rfcomm_cid);
        return 0;
    }
    return rfcomm_send_uih_rls_cmd(channel->multiplexer, channel->dlci, line_status);
}

// Sned local modem status. see MODEM_STAUS_..
int rfcomm_send_modem_status(uint16_t rfcomm_cid, uint8_t modem_status){
    rfcomm_channel_t * channel = rfcomm_channel_for_rfcomm_cid(rfcomm_cid);
    if (!channel){
        log_error("rfcomm_send_modem_status cid 0x%02x doesn't exist!", rfcomm_cid);
        return 0;
    }
    return rfcomm_send_uih_msc_cmd(channel->multiplexer, channel->dlci, modem_status);
}

// Configure remote port 
int rfcomm_send_port_configuration(uint16_t rfcomm_cid, rpn_baud_t baud_rate, rpn_data_bits_t data_bits, rpn_stop_bits_t stop_bits, rpn_parity_t parity, rpn_flow_control_t flow_control){
    rfcomm_channel_t * channel = rfcomm_channel_for_rfcomm_cid(rfcomm_cid);
    if (!channel){
        log_error("rfcomm_send_port_configuration cid 0x%02x doesn't exist!", rfcomm_cid);
        return 0;
    }
    rfcomm_rpn_data_t rpn_data; 
    rpn_data.baud_rate = baud_rate;
    rpn_data.flags = data_bits | (stop_bits << 2) | (parity << 3);
    rpn_data.flow_control = flow_control;
    rpn_data.xon = 0;
    rpn_data.xoff = 0;
    rpn_data.parameter_mask_0 = 0x1f;   // all but xon/xoff
    rpn_data.parameter_mask_1 = 0x3f;   // all flow control options
    return rfcomm_send_uih_rpn_cmd(channel->multiplexer, channel->dlci, &rpn_data);
}

// Query remote port 
int rfcomm_query_port_configuration(uint16_t rfcomm_cid){
    rfcomm_channel_t * channel = rfcomm_channel_for_rfcomm_cid(rfcomm_cid);
    if (!channel){
        log_error("rfcomm_query_port_configuration cid 0x%02x doesn't exist!", rfcomm_cid);
        return 0;
    }
    return rfcomm_send_uih_rpn_req(channel->multiplexer, channel->dlci);
}


static uint8_t rfcomm_channel_create_internal(btstack_packet_handler_t packet_handler, bd_addr_t addr, uint8_t server_channel, uint8_t incoming_flow_control, uint8_t initial_credits, uint16_t * out_rfcomm_cid){
    log_info("RFCOMM_CREATE_CHANNEL addr %s channel #%u init credits %u",  bd_addr_to_str(addr), server_channel, initial_credits);
    
    // create new multiplexer if necessary
    uint8_t status = 0;
    uint8_t dlci = 0;
    int new_multiplexer = 0;
    rfcomm_channel_t * channel = NULL;
    rfcomm_multiplexer_t * multiplexer = rfcomm_multiplexer_for_addr(addr);
    if (!multiplexer) {
        multiplexer = rfcomm_multiplexer_create_for_addr(addr);
        if (!multiplexer){
            status = BTSTACK_MEMORY_ALLOC_FAILED;
            goto fail;
        }
        multiplexer->outgoing = 1;
        multiplexer->state = RFCOMM_MULTIPLEXER_W4_CONNECT;
        new_multiplexer = 1;
    }
    
    // check if channel for this remote service already exists
    dlci = (server_channel << 1) | (multiplexer->outgoing ^ 1);
    channel = rfcomm_channel_for_multiplexer_and_dlci(multiplexer, dlci);
    if (channel){
        status = RFCOMM_CHANNEL_ALREADY_REGISTERED;
        goto fail;
    }

    // prepare channel
    channel = rfcomm_channel_create(multiplexer, NULL, server_channel);
    if (!channel){
        status = BTSTACK_MEMORY_ALLOC_FAILED;
        goto fail;
    }

    // rfcomm_cid is already assigned by rfcomm_channel_create
    channel->incoming_flow_control = incoming_flow_control;
    channel->new_credits_incoming  = initial_credits;
    channel->packet_handler = packet_handler;
    
    // return rfcomm_cid
    if (out_rfcomm_cid){
        *out_rfcomm_cid = channel->rfcomm_cid;
    }

    // start multiplexer setup
    if (multiplexer->state != RFCOMM_MULTIPLEXER_OPEN) {
        channel->state = RFCOMM_CHANNEL_W4_MULTIPLEXER;
        uint16_t l2cap_cid = 0;
#ifdef RFCOMM_USE_ERTM
        // request 
        rfcomm_ertm_request_t request;
        memset(&request, 0, sizeof(rfcomm_ertm_request_t));
        memcpy(request.addr, addr, 6);
        request.ertm_id = rfcomm_next_ertm_id();
        if (rfcomm_ertm_request_callback){
            (*rfcomm_ertm_request_callback)(&request);
        }
        if (request.ertm_config && request.ertm_buffer && request.ertm_buffer_size){
            multiplexer->ertm_id = request.ertm_id;
            status = l2cap_create_ertm_channel(rfcomm_packet_handler, addr, BLUETOOTH_PROTOCOL_RFCOMM, 
                        request.ertm_config, request.ertm_buffer, request.ertm_buffer_size, &l2cap_cid);
        }
        else
#endif
        {
            status = l2cap_create_channel(rfcomm_packet_handler, addr, BLUETOOTH_PROTOCOL_RFCOMM, l2cap_max_mtu(), &l2cap_cid);
        }
        if (status) goto fail;
        multiplexer->l2cap_cid = l2cap_cid;
        return 0;
    }
    
    channel->state = RFCOMM_CHANNEL_SEND_UIH_PN;
    
    // start connecting, if multiplexer is already up and running
    l2cap_request_can_send_now_event(multiplexer->l2cap_cid);
    return 0;

fail:
    if (new_multiplexer) btstack_memory_rfcomm_multiplexer_free(multiplexer);
    if (channel)         btstack_memory_rfcomm_channel_free(channel);
    return status;
}

uint8_t rfcomm_create_channel_with_initial_credits(btstack_packet_handler_t packet_handler, bd_addr_t addr, uint8_t server_channel, uint8_t initial_credits, uint16_t * out_rfcomm_cid){
    return rfcomm_channel_create_internal(packet_handler, addr, server_channel, 1, initial_credits, out_rfcomm_cid);
}

uint8_t rfcomm_create_channel(btstack_packet_handler_t packet_handler, bd_addr_t addr, uint8_t server_channel, uint16_t * out_rfcomm_cid){
    return rfcomm_channel_create_internal(packet_handler, addr, server_channel, 0, RFCOMM_CREDITS, out_rfcomm_cid);
}

void rfcomm_disconnect(uint16_t rfcomm_cid){
    log_info("RFCOMM_DISCONNECT cid 0x%02x", rfcomm_cid);
    rfcomm_channel_t * channel = rfcomm_channel_for_rfcomm_cid(rfcomm_cid);
    if (!channel) return;

    channel->state = RFCOMM_CHANNEL_SEND_DISC;
    l2cap_request_can_send_now_event(channel->multiplexer->l2cap_cid);
}

static uint8_t rfcomm_register_service_internal(btstack_packet_handler_t packet_handler, 
    uint8_t channel, uint16_t max_frame_size, uint8_t incoming_flow_control, uint8_t initial_credits){

    log_info("RFCOMM_REGISTER_SERVICE channel #%u mtu %u flow_control %u credits %u",
             channel, max_frame_size, incoming_flow_control, initial_credits);

    // check if already registered
    rfcomm_service_t * service = rfcomm_service_for_channel(channel);
    if (service){
        return RFCOMM_CHANNEL_ALREADY_REGISTERED;
    }
    
    // alloc structure
    service = btstack_memory_rfcomm_service_get();
    if (!service) {
        return BTSTACK_MEMORY_ALLOC_FAILED;
    }
    
    // register with l2cap if not registered before, max MTU
    if (btstack_linked_list_empty(&rfcomm_services)){
        l2cap_register_service(rfcomm_packet_handler, BLUETOOTH_PROTOCOL_RFCOMM, 0xffff, rfcomm_security_level);
    }
    
    // fill in 
    service->packet_handler = packet_handler;
    service->server_channel = channel;
    service->max_frame_size = max_frame_size;
    service->incoming_flow_control = incoming_flow_control;
    service->incoming_initial_credits = initial_credits;
    
    // add to services list
    btstack_linked_list_add(&rfcomm_services, (btstack_linked_item_t *) service);
    
    return 0;
}

uint8_t rfcomm_register_service_with_initial_credits(btstack_packet_handler_t packet_handler, 
    uint8_t channel, uint16_t max_frame_size, uint8_t initial_credits){

    return rfcomm_register_service_internal(packet_handler, channel, max_frame_size, 1, initial_credits);
}

uint8_t rfcomm_register_service(btstack_packet_handler_t packet_handler, uint8_t channel, 
    uint16_t max_frame_size){
    
    return rfcomm_register_service_internal(packet_handler, channel, max_frame_size, 0,RFCOMM_CREDITS);
}

void rfcomm_unregister_service(uint8_t service_channel){
    log_info("RFCOMM_UNREGISTER_SERVICE #%u", service_channel);
    rfcomm_service_t *service = rfcomm_service_for_channel(service_channel);
    if (!service) return;
    btstack_linked_list_remove(&rfcomm_services, (btstack_linked_item_t *) service);
    btstack_memory_rfcomm_service_free(service);
    
    // unregister if no services active
    if (btstack_linked_list_empty(&rfcomm_services)){
        // bt_send_cmd(&l2cap_unregister_service, BLUETOOTH_PROTOCOL_RFCOMM);
        l2cap_unregister_service(BLUETOOTH_PROTOCOL_RFCOMM);
    }
}

void rfcomm_accept_connection(uint16_t rfcomm_cid){
    log_info("RFCOMM_ACCEPT_CONNECTION cid 0x%02x", rfcomm_cid);
    rfcomm_channel_t * channel = rfcomm_channel_for_rfcomm_cid(rfcomm_cid);
    if (!channel) return;
    switch (channel->state) {
        case RFCOMM_CHANNEL_INCOMING_SETUP:
            rfcomm_channel_state_add(channel, RFCOMM_CHANNEL_STATE_VAR_CLIENT_ACCEPTED);
            if (channel->state_var & RFCOMM_CHANNEL_STATE_VAR_RCVD_PN){
                rfcomm_channel_state_add(channel, RFCOMM_CHANNEL_STATE_VAR_SEND_PN_RSP);
                l2cap_request_can_send_now_event(channel->multiplexer->l2cap_cid);
            }
            if (channel->state_var & RFCOMM_CHANNEL_STATE_VAR_RCVD_SABM){
                rfcomm_channel_state_add(channel, RFCOMM_CHANNEL_STATE_VAR_SEND_UA);
                l2cap_request_can_send_now_event(channel->multiplexer->l2cap_cid);
            }
            // at least one of { PN RSP, UA } needs to be sent
            // state transistion incoming setup -> dlc setup happens in rfcomm_run after these have been sent
            break;
        default:
            break;
    }

}

void rfcomm_decline_connection(uint16_t rfcomm_cid){
    log_info("RFCOMM_DECLINE_CONNECTION cid 0x%02x", rfcomm_cid);
    rfcomm_channel_t * channel = rfcomm_channel_for_rfcomm_cid(rfcomm_cid);
    if (!channel) return;
    switch (channel->state) {
        case RFCOMM_CHANNEL_INCOMING_SETUP:
            channel->state = RFCOMM_CHANNEL_SEND_DM;
            l2cap_request_can_send_now_event(channel->multiplexer->l2cap_cid);
            break;
        default:
            break;
    }
}

void rfcomm_grant_credits(uint16_t rfcomm_cid, uint8_t credits){
    log_info("RFCOMM_GRANT_CREDITS cid 0x%02x credits %u", rfcomm_cid, credits);
    rfcomm_channel_t * channel = rfcomm_channel_for_rfcomm_cid(rfcomm_cid);
    if (!channel) return;
    if (!channel->incoming_flow_control) return;
    channel->new_credits_incoming += credits;

    // process
    l2cap_request_can_send_now_event(channel->multiplexer->l2cap_cid);
}

#ifdef RFCOMM_USE_ERTM
void rfcomm_enable_l2cap_ertm(void request_callback(rfcomm_ertm_request_t * request), void released_callback(uint16_t ertm_id)){
    rfcomm_ertm_request_callback  = request_callback;
    rfcomm_ertm_released_callback = released_callback;
}
#endif
