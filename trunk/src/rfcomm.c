/*
 * Copyright (C) 2010 by Matthias Ringwald
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
 *
 * THIS SOFTWARE IS PROVIDED BY MATTHIAS RINGWALD AND CONTRIBUTORS
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
 */

/*
 *  rfcomm.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h> // memcpy

#include <btstack/btstack.h>
#include <btstack/hci_cmds.h>
#include <btstack/utils.h>

#include <btstack/utils.h>
#include "hci.h"
#include "hci_dump.h"
#include "debug.h"
#include "rfcomm.h"

// Control field values      bit no.       1 2 3 4 PF 6 7 8
#define BT_RFCOMM_SABM       0x3F       // 1 1 1 1  1 1 0 0
#define BT_RFCOMM_UA         0x73       // 1 1 0 0  1 1 1 0
#define BT_RFCOMM_DM         0x0F       // 1 1 1 1  0 0 0 0
#define BT_RFCOMM_DM_PF      0x1F		// 1 1 1 1  1 0 0 0
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

#define RFCOMM_L2CAP_MTU 1021
#define RFCOMM_MULIPLEXER_TIMEOUT_MS 60000

// FCS calc 
#define BT_RFCOMM_CODE_WORD         0xE0 // pol = x8+x2+x1+1
#define BT_RFCOMM_CRC_CHECK_LEN     3
#define BT_RFCOMM_UIHCRC_CHECK_LEN  2

#include "l2cap.h"

// private structs
typedef enum {
	RFCOMM_MULTIPLEXER_CLOSED = 1,
	RFCOMM_MULTIPLEXER_W4_CONNECT,  // outgoing
	RFCOMM_MULTIPLEXER_W4_UA_0,     //    "
	RFCOMM_MULTIPLEXER_W4_SABM_0,   // incoming
	RFCOMM_MULTIPLEXER_OPEN,
} RFCOMM_MULTIPLEXER_STATE;

typedef enum {
	RFCOMM_CHANNEL_CLOSED = 1,
    RFCOMM_CHANNEL_SEND_DM,
	RFCOMM_CHANNEL_W4_MULTIPLEXER,
    RFCOMM_CHANNEL_W4_CLIENT_AFTER_SABM,   // received SABM
    RFCOMM_CHANNEL_SEND_UA,
    RFCOMM_CHANNEL_W4_CLIENT_AFTER_PN_CMD, // received PN_CMD
	RFCOMM_CHANNEL_W4_PN_BEFORE_OPEN,
	RFCOMM_CHANNEL_W4_PN_AFTER_OPEN,
    RFCOMM_CHANNEL_W4_PN_RSP,
	RFCOMM_CHANNEL_SEND_PN_RSP_W4_SABM_OR_PN_CMD,
	RFCOMM_CHANNEL_W4_SABM_OR_PN_CMD,
	RFCOMM_CHANNEL_SEND_SABM_W4_UA,
	RFCOMM_CHANNEL_W4_UA,
    RFCOMM_CHANNEL_SEND_MSC_CMD_W4_MSC_CMD_OR_MSC_RSP,
	RFCOMM_CHANNEL_W4_MSC_CMD_OR_MSC_RSP,   // outgoing, sent MSC_CMD
	RFCOMM_CHANNEL_SEND_MSC_RSP_MSC_CMD_W4_CREDITS,
	RFCOMM_CHANNEL_W4_MSC_CMD,
	RFCOMM_CHANNEL_SEND_MSC_RSP_W4_MSC_RSP,
	RFCOMM_CHANNEL_W4_MSC_RSP,
	RFCOMM_CHANNEL_W4_CREDITS,
    RFCOMM_CHANNEL_SEND_MSC_CMD_SEND_CREDITS,
    RFCOMM_CHANNEL_SEND_CREDITS,
	RFCOMM_CHANNEL_OPEN
} RFCOMM_CHANNEL_STATE;

// info regarding potential connections
typedef struct {
    // linked list - assert: first field
    linked_item_t    item;
	
    // server channel
    uint8_t server_channel;
    
    // incoming max frame size
    uint16_t max_frame_size;
    
    // client connection
    void *connection;    
    
    // internal connection
    btstack_packet_handler_t packet_handler;
    
} rfcomm_service_t;

// info regarding multiplexer
// note: spec mandates single multplexer per device combination
typedef struct {
    // linked list - assert: first field
    linked_item_t    item;

    timer_source_t   timer;
    int              timer_active;
    
	RFCOMM_MULTIPLEXER_STATE state;	
    
    uint16_t  l2cap_cid;
    uint8_t   l2cap_credits;
    
	bd_addr_t remote_addr;
    hci_con_handle_t con_handle;
    
	uint8_t   outgoing;
    
    // hack to deal with authentication failure only observed by remote side
    uint8_t   at_least_one_connection;
    
    uint16_t max_frame_size;

} rfcomm_multiplexer_t;

// info regarding an actual coneection
typedef struct {
    // linked list - assert: first field
    linked_item_t    item;
	
    RFCOMM_CHANNEL_STATE state;
	rfcomm_multiplexer_t *multiplexer;
	uint16_t rfcomm_cid;
    uint8_t  outgoing;
    uint8_t  dlci; 
    
    // credits for outgoing traffic
    uint8_t credits_outgoing;
    
    // number of packets granted to client
    uint8_t packets_granted;

    // credits for incoming traffic
    uint8_t credits_incoming;
    
    // priority set by incoming side in PN
    uint8_t pn_priority;
    
	// negotiated frame size
    uint16_t max_frame_size;
	
	// server channel (see rfcomm_service_t) - NULL => outgoing channel
	rfcomm_service_t * service;
    
    // internal connection
    btstack_packet_handler_t packet_handler;

    // client connection
    void * connection;
    
} rfcomm_channel_t;

// global rfcomm data
static uint16_t      rfcomm_client_cid_generator;  // used for client channel IDs

// linked lists for all
static linked_list_t rfcomm_multiplexers = NULL;
static linked_list_t rfcomm_channels = NULL;
static linked_list_t rfcomm_services = NULL;

// used to assemble rfcomm packets
#define RFCOMM_MAX_PAYLOAD (HCI_ACL_3DH5_SIZE-HCI_ACL_DATA_PKT_HDR)
uint8_t rfcomm_out_buffer[RFCOMM_MAX_PAYLOAD];

static void (*app_packet_handler)(void * connection, uint8_t packet_type,
                                  uint16_t channel, uint8_t *packet, uint16_t size);

static void rfcomm_run(void);

static void rfcomm_multiplexer_initialize(rfcomm_multiplexer_t *multiplexer){
    bzero(multiplexer, sizeof(rfcomm_multiplexer_t));
    multiplexer->state = RFCOMM_MULTIPLEXER_CLOSED;
    multiplexer->l2cap_credits = 0;
    // - Max RFCOMM header has 6 bytes (P/F bit is set, payload length >= 128)
    // - therefore, we set RFCOMM max frame size <= Local L2CAP MTU - 6
    multiplexer->max_frame_size = RFCOMM_L2CAP_MTU - 6; // max
}

static rfcomm_multiplexer_t * rfcomm_multiplexer_create_for_addr(bd_addr_t *addr){
    
    // alloc structure 
    rfcomm_multiplexer_t * multiplexer = malloc(sizeof(rfcomm_multiplexer_t));
    if (!multiplexer) return NULL;
    
    // fill in 
    rfcomm_multiplexer_initialize(multiplexer);
    BD_ADDR_COPY(&multiplexer->remote_addr, addr);

    // add to services list
    linked_list_add(&rfcomm_multiplexers, (linked_item_t *) multiplexer);
    
    return multiplexer;
}

static rfcomm_multiplexer_t * rfcomm_multiplexer_for_addr(bd_addr_t *addr){
    linked_item_t *it;
    for (it = (linked_item_t *) rfcomm_multiplexers; it ; it = it->next){
        rfcomm_multiplexer_t * multiplexer = ((rfcomm_multiplexer_t *) it);
        if (BD_ADDR_CMP(addr, multiplexer->remote_addr) == 0) {
            return multiplexer;
        };
    }
    return NULL;
}

static rfcomm_multiplexer_t * rfcomm_multiplexer_for_l2cap_cid(uint16_t l2cap_cid) {
    linked_item_t *it;
    for (it = (linked_item_t *) rfcomm_multiplexers; it ; it = it->next){
        rfcomm_multiplexer_t * multiplexer = ((rfcomm_multiplexer_t *) it);
        if (multiplexer->l2cap_cid == l2cap_cid) {
            return multiplexer;
        };
    }
    return NULL;
}

static int rfcomm_multiplexer_has_channels(rfcomm_multiplexer_t * multiplexer){
    linked_item_t *it;
    for (it = (linked_item_t *) rfcomm_channels; it ; it = it->next){
        rfcomm_channel_t * channel = ((rfcomm_channel_t *) it);
        if (channel->multiplexer == multiplexer) {
            return 1;
        }
    }
    return 0;
}

static void rfcomm_dump_channels(void){
    linked_item_t * it;
    int channels = 0;
    for (it = (linked_item_t *) rfcomm_channels; it ; it = it->next){
        rfcomm_channel_t * channel = (rfcomm_channel_t *) it;
        log_dbg("Channel #%u: addr %p, state %u\n", channels++, channel, channel->state);
    }        
}

static void rfcomm_channel_initialize(rfcomm_channel_t *channel, rfcomm_multiplexer_t *multiplexer, 
                               rfcomm_service_t *service, uint8_t server_channel){
    
    // don't use 0 as channel id
    if (rfcomm_client_cid_generator == 0) ++rfcomm_client_cid_generator;
    
    // setup channel
    bzero(channel, sizeof(rfcomm_channel_t));
    channel->multiplexer      = multiplexer;
    channel->service          = service;
    channel->rfcomm_cid       = rfcomm_client_cid_generator++;
    channel->max_frame_size   = multiplexer->max_frame_size;

    channel->credits_incoming = 0;
    channel->credits_outgoing = 0;
    channel->packets_granted  = 0;
	if (service) {
		// incoming connection
		channel->outgoing = 0;
		channel->dlci = (server_channel << 1) |  multiplexer->outgoing;
	} else {
		// outgoing connection
		channel->outgoing = 1;
		channel->dlci = (server_channel << 1) | (multiplexer->outgoing ^ 1);
	}
}

// service == NULL -> outgoing channel
static rfcomm_channel_t * rfcomm_channel_create(rfcomm_multiplexer_t * multiplexer,
                                                rfcomm_service_t * service, uint8_t server_channel){

    log_dbg("rfcomm_channel_create for service %p, channel %u --- begin\n", service, server_channel);
    rfcomm_dump_channels();

    // alloc structure 
    rfcomm_channel_t * channel = malloc(sizeof(rfcomm_channel_t));
    if (!channel) return NULL;
    
    // fill in 
    rfcomm_channel_initialize(channel, multiplexer, service, server_channel);
    
    // add to services list
    linked_list_add(&rfcomm_channels, (linked_item_t *) channel);
    
    return channel;
}

static rfcomm_channel_t * rfcomm_channel_for_rfcomm_cid(uint16_t rfcomm_cid){
    linked_item_t *it;
    for (it = (linked_item_t *) rfcomm_channels; it ; it = it->next){
        rfcomm_channel_t * channel = ((rfcomm_channel_t *) it);
        if (channel->rfcomm_cid == rfcomm_cid) {
            return channel;
        };
    }
    return NULL;
}

static rfcomm_channel_t * rfcomm_channel_for_multiplexer_and_dlci(rfcomm_multiplexer_t * multiplexer, uint8_t dlci){
    linked_item_t *it;
    for (it = (linked_item_t *) rfcomm_channels; it ; it = it->next){
        rfcomm_channel_t * channel = ((rfcomm_channel_t *) it);
        if (channel->dlci == dlci && channel->multiplexer == multiplexer) {
            return channel;
        };
    }
    return NULL;
}

static rfcomm_service_t * rfcomm_service_for_channel(uint8_t server_channel){
    linked_item_t *it;
    for (it = (linked_item_t *) rfcomm_services; it ; it = it->next){
        rfcomm_service_t * service = ((rfcomm_service_t *) it);
        if ( service->server_channel == server_channel){
            return service;
        };
    }
    return NULL;
}

/**
 * @param credits - only used for RFCOMM flow control in UIH wiht P/F = 1
 */
static int rfcomm_send_packet_for_multiplexer(rfcomm_multiplexer_t *multiplexer, uint8_t address, uint8_t control, uint8_t credits, uint8_t *data, uint16_t len){

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
	rfcomm_out_buffer[pos++] =  crc8_calc(rfcomm_out_buffer, crc_fields); // calc fcs

    int credits_taken = 0;
    if (multiplexer->l2cap_credits){
        credits_taken++;
        multiplexer->l2cap_credits--;
    } else {
        log_dbg( "rfcomm_send_packet addr %02x, ctrl %02x size %u without l2cap credits\n", address, control, pos);
    }
    
    int err = l2cap_send_internal(multiplexer->l2cap_cid, rfcomm_out_buffer, pos);
    
    if (err) {
        // undo credit counting
        multiplexer->l2cap_credits += credits_taken;
    }
    return err;
}

// C/R Flag in Address
// "For SABM, UA, DM and DISC frames C/R bit is set according to Table 1 in GSM 07.10, section 5.2.1.2"
//    - command initiator = 1 /response responder = 0
//    - command responer  = 0 /response initiator = 1
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
	uint8_t address = (1 << 0) | (multiplexer->outgoing << 1) | (dlci << 2); // command
    return rfcomm_send_packet_for_multiplexer(multiplexer, address, BT_RFCOMM_DISC, 0, NULL, 0);
}

static int rfcomm_send_ua(rfcomm_multiplexer_t *multiplexer, uint8_t dlci){
	uint8_t address = (1 << 0) | ((multiplexer->outgoing ^ 1) << 1) | (dlci << 2); // response
    return rfcomm_send_packet_for_multiplexer(multiplexer, address, BT_RFCOMM_UA, 0, NULL, 0);
}

// DM: Disconnected Mode (response to a command when disconnected)
static int rfcomm_send_dm_pf(rfcomm_multiplexer_t *multiplexer, uint8_t dlci){
	uint8_t address = (1 << 0) | ((multiplexer->outgoing ^ 1) << 1) | (dlci << 2); // response
    return rfcomm_send_packet_for_multiplexer(multiplexer, address, BT_RFCOMM_DM_PF, 0, NULL, 0);
}


static int rfcomm_send_uih_data(rfcomm_multiplexer_t *multiplexer, uint8_t dlci, uint8_t *data, uint16_t len){
	uint8_t address = (1 << 0) | (multiplexer->outgoing << 1) | (dlci << 2); 
    return rfcomm_send_packet_for_multiplexer(multiplexer, address, BT_RFCOMM_UIH, 0, data, len);
}

static int rfcomm_send_uih_msc_cmd(rfcomm_multiplexer_t *multiplexer, uint8_t dlci, uint8_t signals) {
	uint8_t address = (1 << 0) | (multiplexer->outgoing << 1);
	uint8_t payload[4]; 
	uint8_t pos = 0;
	payload[pos++] = BT_RFCOMM_MSC_CMD;
	payload[pos++] = 2 << 1 | 1;  // len
	payload[pos++] = (1 << 0) | (1 << 1) | (dlci << 2); // CMD => C/R = 1
	payload[pos++] = signals;
	return rfcomm_send_packet_for_multiplexer(multiplexer, address, BT_RFCOMM_UIH, 0, (uint8_t *) payload, pos);
}

static int rfcomm_send_uih_msc_rsp(rfcomm_multiplexer_t *multiplexer, uint8_t dlci, uint8_t signals) {
	uint8_t address = (1 << 0) | ((multiplexer->outgoing ^ 1) << 1);
	uint8_t payload[4]; 
	uint8_t pos = 0;
	payload[pos++] = BT_RFCOMM_MSC_RSP;
	payload[pos++] = 2 << 1 | 1;  // len
	payload[pos++] = (1 << 0) | (1 << 1) | (dlci << 2); // CMD => C/R = 1
	payload[pos++] = signals;
	return rfcomm_send_packet_for_multiplexer(multiplexer, address, BT_RFCOMM_UIH, 0, (uint8_t *) payload, pos);
}

static int rfcomm_send_uih_pn_command(rfcomm_multiplexer_t *multiplexer, uint8_t dlci, uint16_t max_frame_size){
	uint8_t payload[10];
	uint8_t address = (1 << 0) | (multiplexer->outgoing << 1); 
	uint8_t pos = 0;
	payload[pos++] = BT_RFCOMM_PN_CMD;
	payload[pos++] = 8 << 1 | 1;  // len
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

// "The response may not change the DLCI, the priority, the convergence layer, or the timer value." RFCOMM-tutorial.pdf
static int rfcomm_send_uih_pn_response(rfcomm_multiplexer_t *multiplexer, uint8_t dlci,
                                            uint8_t priority, uint16_t max_frame_size){
	uint8_t payload[10];
	uint8_t address = (1 << 0) | (multiplexer->outgoing << 1); 
	uint8_t pos = 0;
	payload[pos++] = BT_RFCOMM_PN_RSP;
	payload[pos++] = 8 << 1 | 1;  // len
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


// data: event (8), len(8), address(48), channel (8), rfcomm_cid (16)
static void rfcomm_emit_connection_request(rfcomm_channel_t *channel) {
    uint8_t event[11];
    event[0] = RFCOMM_EVENT_INCOMING_CONNECTION;
    event[1] = sizeof(event) - 2;
    bt_flip_addr(&event[2], channel->multiplexer->remote_addr);
    event[8] = channel->dlci >> 1;
    bt_store_16(event, 9, channel->rfcomm_cid);
    hci_dump_packet(HCI_EVENT_PACKET, 0, event, sizeof(event));
	(*app_packet_handler)(channel->connection, HCI_EVENT_PACKET, 0, (uint8_t *) event, sizeof(event));
}


// API Change: BTstack-0.3.50x uses
// data: event(8), len(8), status (8), address (48), server channel(8), rfcomm_cid(16), max frame size(16)
// next Cydia release will use SVN version of this
// data: event(8), len(8), status (8), address (48), handle (16), server channel(8), rfcomm_cid(16), max frame size(16)
static void rfcomm_emit_channel_opened(rfcomm_channel_t *channel, uint8_t status) {
    uint8_t event[16];
    uint8_t pos = 0;
    event[pos++] = RFCOMM_EVENT_OPEN_CHANNEL_COMPLETE;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = status;
    bt_flip_addr(&event[pos], channel->multiplexer->remote_addr); pos += 6;
    bt_store_16(event,  pos, channel->multiplexer->con_handle);   pos += 2;
	event[pos++] = channel->dlci >> 1;
	bt_store_16(event, pos, channel->rfcomm_cid); pos += 2;       // channel ID
	bt_store_16(event, pos, channel->max_frame_size); pos += 2;   // max frame size
    hci_dump_packet(HCI_EVENT_PACKET, 0, event, sizeof(event));
	(*app_packet_handler)(channel->connection, HCI_EVENT_PACKET, 0, (uint8_t *) event, pos);
}

// data: event(8), len(8), rfcomm_cid(16)
static void rfcomm_emit_channel_closed(rfcomm_channel_t * channel) {
    uint8_t event[4];
    event[0] = RFCOMM_EVENT_CHANNEL_CLOSED;
    event[1] = sizeof(event) - 2;
    bt_store_16(event, 2, channel->rfcomm_cid);
    hci_dump_packet(HCI_EVENT_PACKET, 0, event, sizeof(event));
	(*app_packet_handler)(channel->connection, HCI_EVENT_PACKET, 0, (uint8_t *) event, sizeof(event));
}

static void rfcomm_emit_credits(rfcomm_channel_t * channel, uint8_t credits) {
    uint8_t event[5];
    event[0] = RFCOMM_EVENT_CREDITS;
    event[1] = sizeof(event) - 2;
    bt_store_16(event, 2, channel->rfcomm_cid);
    event[4] = credits;
    hci_dump_packet(HCI_EVENT_PACKET, 0, event, sizeof(event));
	(*app_packet_handler)(channel->connection, HCI_EVENT_PACKET, 0, (uint8_t *) event, sizeof(event));
}

static void rfcomm_emit_service_registered(void *connection, uint8_t status, uint8_t channel){
    uint8_t event[4];
    event[0] = RFCOMM_EVENT_SERVICE_REGISTERED;
    event[1] = sizeof(event) - 2;
    event[2] = status;
    event[3] = channel;
    hci_dump_packet( HCI_EVENT_PACKET, 0, event, sizeof(event));
	(*app_packet_handler)(connection, HCI_EVENT_PACKET, 0, (uint8_t *) event, sizeof(event));
}

static void rfcomm_hand_out_credits(void){
    linked_item_t * it;
    for (it = (linked_item_t *) rfcomm_channels; it ; it = it->next){
        rfcomm_channel_t * channel = (rfcomm_channel_t *) it;
        if (channel->state != RFCOMM_CHANNEL_OPEN) {
            // log_dbg("RFCOMM_EVENT_CREDITS: multiplexer not open\n");
            continue;
        }
        if (channel->packets_granted) {
            // log_dbg("RFCOMM_EVENT_CREDITS: already packets granted\n");
            continue;
        }
        if (!channel->credits_outgoing) {
            // log_dbg("RFCOMM_EVENT_CREDITS: no outgoing credits\n");
            continue;
        }
        if (!channel->multiplexer->l2cap_credits){
            // log_dbg("RFCOMM_EVENT_CREDITS: no l2cap credits\n");
            continue;
        }
        // channel open, multiplexer has l2cap credits and we didn't hand out credit before -> go!
        // log_dbg("RFCOMM_EVENT_CREDITS: 1\n");
        channel->packets_granted += 1;
        rfcomm_emit_credits(channel, 1);
    }        
}

static void rfcomm_multiplexer_finalize(rfcomm_multiplexer_t * multiplexer){
    
    // remove (potential) timer
    if (multiplexer->timer_active) {
        run_loop_remove_timer(&multiplexer->timer);
        multiplexer->timer_active = 0;
    }
    
    // close and remove all channels
    linked_item_t *it = (linked_item_t *) &rfcomm_channels;
    while (it->next){
        rfcomm_channel_t * channel = (rfcomm_channel_t *) it->next;
        if (channel->multiplexer == multiplexer) {
            // emit appropriate events
            if (channel->state == RFCOMM_CHANNEL_OPEN) {
                rfcomm_emit_channel_closed(channel);
            } else {
                rfcomm_emit_channel_opened(channel, RFCOMM_MULTIPLEXER_STOPPED); 
            }
            // remove from list
            it->next = it->next->next;
            // free channel struct
            free(channel);
        } else {
            it = it->next;
        }
    }
    
    // keep reference to l2cap channel
    uint16_t l2cap_cid = multiplexer->l2cap_cid;
    
    // remove mutliplexer
    linked_list_remove( &rfcomm_multiplexers, (linked_item_t *) multiplexer);
    free(multiplexer);
    
    // close l2cap multiplexer channel, too
    l2cap_disconnect_internal(l2cap_cid, 0x13);
}

static void rfcomm_multiplexer_timer_handler(timer_source_t *timer){
    rfcomm_multiplexer_t * multiplexer = (rfcomm_multiplexer_t *) linked_item_get_user( (linked_item_t *) timer);
    if (!rfcomm_multiplexer_has_channels(multiplexer)){
        log_dbg( "rfcomm_multiplexer_timer_handler timeout: shutting down multiplexer!\n");
        rfcomm_multiplexer_finalize(multiplexer);
    }
}

static void rfcomm_multiplexer_prepare_idle_timer(rfcomm_multiplexer_t * multiplexer){
    if (multiplexer->timer_active) {
        run_loop_remove_timer(&multiplexer->timer);
        multiplexer->timer_active = 0;
    }
    if (!rfcomm_multiplexer_has_channels(multiplexer)){
        // start timer for multiplexer timeout check
        run_loop_set_timer(&multiplexer->timer, RFCOMM_MULIPLEXER_TIMEOUT_MS);
        multiplexer->timer.process = rfcomm_multiplexer_timer_handler;
        linked_item_set_user((linked_item_t*) &multiplexer->timer, multiplexer);
        run_loop_add_timer(&multiplexer->timer);
        multiplexer->timer_active = 1;
    }
}

static void rfcomm_channel_provide_credits(rfcomm_channel_t *channel){
    const uint16_t credits = 0x30;
    switch (channel->state) {
        case RFCOMM_CHANNEL_W4_CREDITS:
        case RFCOMM_CHANNEL_OPEN:
            if (channel->credits_incoming < 5){
				uint8_t address = (1 << 0) | (channel->multiplexer->outgoing << 1) |  (channel->dlci << 2); 
				rfcomm_send_packet_for_multiplexer(channel->multiplexer, address, BT_RFCOMM_UIH_PF, credits, NULL, 0);
                channel->credits_incoming += credits;
            }
            break;
        default:
            break;
    }
}

static void rfcomm_channel_opened(rfcomm_channel_t *rfChannel){
    rfChannel->state = RFCOMM_CHANNEL_OPEN;
    rfcomm_emit_channel_opened(rfChannel, 0);
    rfcomm_hand_out_credits();

    // remove (potential) timer
    rfcomm_multiplexer_t *multiplexer = rfChannel->multiplexer;
    if (multiplexer->timer_active) {
        run_loop_remove_timer(&multiplexer->timer);
        multiplexer->timer_active = 0;
    }
    // hack for problem detecting authentication failure
    multiplexer->at_least_one_connection = 1;
    
    // start next connection request if pending
    rfcomm_run();
}

/**
 * @return handled packet
 */
static int rfcomm_multiplexer_hci_event_handler(uint8_t *packet, uint16_t size){
    bd_addr_t event_addr;
    uint16_t  psm;
    uint16_t l2cap_cid;
    hci_con_handle_t con_handle;
    rfcomm_multiplexer_t *multiplexer = NULL;
    switch (packet[0]) {
            
            // accept incoming PSM_RFCOMM connection if no multiplexer exists yet
        case L2CAP_EVENT_INCOMING_CONNECTION:
            // data: event(8), len(8), address(48), handle (16),  psm (16), source cid(16) dest cid(16)
            bt_flip_addr(event_addr, &packet[2]);
            psm        = READ_BT_16(packet, 10); 
            if (psm != PSM_RFCOMM) break;
            l2cap_cid  = READ_BT_16(packet, 12); 
            multiplexer = rfcomm_multiplexer_for_addr(&event_addr);
            log_dbg("L2CAP_EVENT_INCOMING_CONNECTION (l2cap_cid 0x%02x) for PSM_RFCOMM from ", l2cap_cid);
            
            if (multiplexer) {
                log_dbg(" => decline\n");
                // bt_send_cmd(&l2cap_decline_connection, l2cap_cid);
                l2cap_decline_connection_internal(l2cap_cid,  0x13);
            } else {
                log_dbg(" => accept\n");
                // bt_send_cmd(&l2cap_accept_connection, l2cap_cid);
                l2cap_accept_connection_internal(l2cap_cid);
            }
            break;
            
            // l2cap connection opened -> store l2cap_cid, remote_addr
        case L2CAP_EVENT_CHANNEL_OPENED: 
            if (READ_BT_16(packet, 11) != PSM_RFCOMM) break;
            log_dbg("L2CAP_EVENT_CHANNEL_OPENED for PSM_RFCOMM\n");
            // get multiplexer for remote addr
            con_handle = READ_BT_16(packet, 9);
            l2cap_cid = READ_BT_16(packet, 13);
            bt_flip_addr(event_addr, &packet[3]);
            multiplexer = rfcomm_multiplexer_for_addr(&event_addr);
            if (multiplexer) {
                if (multiplexer->state == RFCOMM_MULTIPLEXER_W4_CONNECT) {
                    log_dbg("L2CAP_EVENT_CHANNEL_OPENED: outgoing connection A\n");
                    // wrong remote addr
                    if (BD_ADDR_CMP(event_addr, multiplexer->remote_addr)) break;
                    multiplexer->l2cap_cid = l2cap_cid;
                    multiplexer->con_handle = con_handle;
                    // send SABM #0
                    log_dbg("-> Sending SABM #0\n");
                    rfcomm_send_sabm(multiplexer, 0);
                    multiplexer->state = RFCOMM_MULTIPLEXER_W4_UA_0;
                    return 1;
                } 
                log_dbg("L2CAP_EVENT_CHANNEL_OPENED: multiplexer already exists\n");
                // single multiplexer per baseband connection
                break;
            }
            log_dbg("L2CAP_EVENT_CHANNEL_OPENED: create incoming multiplexer for channel %02x\n", l2cap_cid);
            // create and inititialize new multiplexer instance (incoming)
            // - Max RFCOMM header has 6 bytes (P/F bit is set, payload length >= 128)
            // - therefore, we set RFCOMM max frame size <= Local L2CAP MTU - 6
            multiplexer = rfcomm_multiplexer_create_for_addr(&event_addr);
            multiplexer->con_handle = con_handle;
            multiplexer->l2cap_cid = l2cap_cid;
            multiplexer->state = RFCOMM_MULTIPLEXER_W4_SABM_0;
            multiplexer->max_frame_size = READ_BT_16(packet, 17) - 6;
            return 1;
            
            // l2cap disconnect -> state = RFCOMM_MULTIPLEXER_CLOSED;
            
        case L2CAP_EVENT_CREDITS:
            // data: event(8), len(8), local_cid(16), credits(8)
            l2cap_cid = READ_BT_16(packet, 2);
            multiplexer = rfcomm_multiplexer_for_l2cap_cid(l2cap_cid);
            if (!multiplexer) break;
            multiplexer->l2cap_credits += packet[4];
            
            // new credits, continue with signaling
            rfcomm_run();
            
            // log_dbg("L2CAP_EVENT_CREDITS: %u (now %u)\n", packet[4], multiplexer->l2cap_credits);
            if (multiplexer->state != RFCOMM_MULTIPLEXER_OPEN) break;
            rfcomm_hand_out_credits();
            break;
            
        case L2CAP_EVENT_CHANNEL_CLOSED:
            // data: event (8), len(8), channel (16)
            l2cap_cid = READ_BT_16(packet, 2);
            multiplexer = rfcomm_multiplexer_for_l2cap_cid(l2cap_cid);
            if (!multiplexer) break;
            switch (multiplexer->state) {
                case RFCOMM_MULTIPLEXER_W4_SABM_0:
                case RFCOMM_MULTIPLEXER_W4_UA_0:
                case RFCOMM_MULTIPLEXER_OPEN:
                    rfcomm_multiplexer_finalize(multiplexer);
                    return 1;
                default:
                    break;
            }
            break;
        default:
            break;
    }
    return 0;
}

static int rfcomm_multiplexer_l2cap_packet_handler(uint16_t channel, uint8_t *packet, uint16_t size){

    // tricky part: get or create a multiplexer for a certain device
    rfcomm_multiplexer_t *multiplexer = NULL;
    
    // done with event handling - only l2cap data packets for multiplexer follow
    multiplexer = rfcomm_multiplexer_for_l2cap_cid(channel);
    if (!multiplexer) return 0;

	// but only care for multiplexer control channel
    uint8_t frame_dlci = packet[0] >> 2;
    if (frame_dlci) return 0;
    const uint8_t length_offset = (packet[2] & 1) ^ 1;  // to be used for pos >= 3
    const uint8_t credit_offset = ((packet[1] & BT_RFCOMM_UIH_PF) == BT_RFCOMM_UIH_PF) ? 1 : 0;   // credits for uih_pf frames
    const uint8_t payload_offset = 3 + length_offset + credit_offset;
    switch (packet[1]){
            
        case BT_RFCOMM_SABM:
            if (multiplexer->state == RFCOMM_MULTIPLEXER_W4_SABM_0){
                // SABM #0 -> send UA #0, state = RFCOMM_MULTIPLEXER_OPEN
                log_dbg("Received SABM #0 - Multiplexer up and running\n");
                log_dbg("-> Sending UA #0\n");
                rfcomm_send_ua(multiplexer, 0);
                multiplexer->state = RFCOMM_MULTIPLEXER_OPEN;
                multiplexer->outgoing = 0;
                return 1;
            }
            break;
            
        case BT_RFCOMM_UA:
            if (multiplexer->state == RFCOMM_MULTIPLEXER_W4_UA_0) {
                // UA #0 -> send UA #0, state = RFCOMM_MULTIPLEXER_OPEN
                log_dbg("Received UA #0 - Multiplexer up and running\n");
                multiplexer->state = RFCOMM_MULTIPLEXER_OPEN;
                rfcomm_run();
                rfcomm_multiplexer_prepare_idle_timer(multiplexer);
                return 1;
            }
            break;
            
        case BT_RFCOMM_DISC:
            // DISC #0 -> send UA #0, close multiplexer
            log_dbg("Received DISC #0, (ougoing = %u)\n", multiplexer->outgoing);
            log_dbg("-> Sending UA #0\n");
            log_dbg("-> Closing down multiplexer\n");
            rfcomm_send_ua(multiplexer, 0);
            rfcomm_multiplexer_finalize(multiplexer);
            // try to detect authentication errors: drop link key if multiplexer closed before first channel got opened
            if (!multiplexer->at_least_one_connection){
                hci_send_cmd(&hci_delete_stored_link_key, multiplexer->remote_addr);
            }
            return 1;
            
        case BT_RFCOMM_DM:
            // DM #0 - we shouldn't get this, just give up
            log_dbg("Received DM #0\n");
            log_dbg("-> Closing down multiplexer\n");
            rfcomm_multiplexer_finalize(multiplexer);
            return 1;
            
        case BT_RFCOMM_UIH:
            if (packet[payload_offset] == BT_RFCOMM_CLD_CMD){
                // Multiplexer close down (CLD) -> close mutliplexer
                log_dbg("Received Multiplexer close down command\n");
                log_dbg("-> Closing down multiplexer\n");
                rfcomm_multiplexer_finalize(multiplexer);
                return 1;
            }
            break;
            
        default:
            break;
            
    }
    return 0;
}

static void rfcomm_channel_packet_handler_uih(rfcomm_multiplexer_t *multiplexer, uint8_t * packet, uint16_t size){
    const uint8_t frame_dlci = packet[0] >> 2;
    const uint8_t length_offset = (packet[2] & 1) ^ 1;  // to be used for pos >= 3
    const uint8_t credit_offset = ((packet[1] & BT_RFCOMM_UIH_PF) == BT_RFCOMM_UIH_PF) ? 1 : 0;   // credits for uih_pf frames
    const uint8_t payload_offset = 3 + length_offset + credit_offset;

    rfcomm_channel_t * rfChannel = rfcomm_channel_for_multiplexer_and_dlci(multiplexer, frame_dlci);
    if (!rfChannel) return;
    
    if (packet[1] == BT_RFCOMM_UIH_PF) {
        // handle new credits
        uint16_t new_credits = packet[3+length_offset];
        rfChannel->credits_outgoing += new_credits;
        log_dbg( "RFCOMM data UIH_PF, new credits: %u, now %u\n", new_credits, rfChannel->credits_outgoing);

        // notify daemon -> might trigger re-try of parked connections
        uint8_t event[1];
        event[0] = DAEMON_EVENT_NEW_RFCOMM_CREDITS;
        (*app_packet_handler)(rfChannel->connection, DAEMON_EVENT_PACKET, rfChannel->rfcomm_cid, event, sizeof(event));
        
        if (rfChannel->state == RFCOMM_CHANNEL_W4_CREDITS){
            log_dbg("BT_RFCOMM_UIH_PF for #%u, Got %u credits => can send!\n", frame_dlci, new_credits);
            rfcomm_channel_opened(rfChannel);
        }
    }
    if (rfChannel->credits_incoming > 0){
        rfChannel->credits_incoming--;
    }
    rfcomm_channel_provide_credits(rfChannel);
    
    if (size - 1 > payload_offset){ // don't send empty frames, -1 for header checksum at end
        // log_dbg( "RFCOMM data UIH_PF, size %u, channel %x\n", size-payload_offset-1, (int) rfChannel->connection);
        (*app_packet_handler)(rfChannel->connection, RFCOMM_DATA_PACKET, rfChannel->rfcomm_cid,
                              &packet[payload_offset], size-payload_offset-1);
    }
    // we received new RFCOMM credits, hand them out if possible
    rfcomm_hand_out_credits();
}

void rfcomm_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    
    // multiplexer handler
    int handled = 0;
    switch (packet_type) {
        case HCI_EVENT_PACKET:
            handled = rfcomm_multiplexer_hci_event_handler(packet, size);
            break;
        case L2CAP_DATA_PACKET:
            handled = rfcomm_multiplexer_l2cap_packet_handler(channel, packet, size);
            break;
        default:
            break;
    }
    if (handled) return;
    
    // we only handle l2cap packet over open multiplexer channel now
    if (packet_type != L2CAP_DATA_PACKET) {
        (*app_packet_handler)(NULL, packet_type, channel, packet, size);
        return;
    }
    rfcomm_multiplexer_t * multiplexer = rfcomm_multiplexer_for_l2cap_cid(channel);
    if (!multiplexer || multiplexer->state != RFCOMM_MULTIPLEXER_OPEN) {
        (*app_packet_handler)(NULL, packet_type, channel, packet, size);
        return;
    }
    
    // rfcomm: (0) addr [76543 server channel] [2 direction: initiator uses 1] [1 C/R: CMD by initiator = 1] [0 EA=1]
    const uint8_t frame_dlci = packet[0] >> 2;
    uint8_t message_dlci; // used by commands in UIH(_PF) packets 
	uint8_t message_len;  //   "
	
    // channel data
    if (frame_dlci && (packet[1] == BT_RFCOMM_UIH || packet[1] == BT_RFCOMM_UIH_PF)) {
        rfcomm_channel_packet_handler_uih(multiplexer, packet, size);
        return;
    }
        
    // rfcomm: (1) command/control
    // -- credits_offset = 1 if command == BT_RFCOMM_UIH_PF
    const uint8_t credit_offset = ((packet[1] & BT_RFCOMM_UIH_PF) == BT_RFCOMM_UIH_PF) ? 1 : 0;   // credits for uih_pf frames
    // rfcomm: (2) length. if bit 0 is cleared, 2 byte length is used. (little endian)
    const uint8_t length_offset = (packet[2] & 1) ^ 1;  // to be used for pos >= 3
    // rfcomm: (3+length_offset) credits if credits_offset == 1
    // rfcomm: (3+length_offest+credits_offset)
    const uint8_t payload_offset = 3 + length_offset + credit_offset;
    
    rfcomm_channel_t * rfChannel = NULL;
    rfcomm_service_t * rfService = NULL;
	uint16_t max_frame_size;

    // switch by rfcomm message type
    switch(packet[1]) {
            
        case BT_RFCOMM_SABM:
            // with or without earlier UIH PN
            rfService = rfcomm_service_for_channel(frame_dlci >> 1);
            if (rfService) {
               log_dbg("Received SABM #%u\n", frame_dlci);
 
                // get channel
                rfChannel = rfcomm_channel_for_multiplexer_and_dlci(multiplexer, frame_dlci);
                
                // channel already prepared by incoming PN_CMD
                if (rfChannel && rfChannel->state == RFCOMM_CHANNEL_W4_SABM_OR_PN_CMD){
                    rfChannel->state = RFCOMM_CHANNEL_SEND_UA;
                    break;
                }
                
                // new channel
                if (!rfChannel) {
                    
                    // setup channel
                    rfChannel = rfcomm_channel_create(multiplexer, rfService, frame_dlci >> 1);
                    if (!rfChannel) break;
                    rfChannel->connection = rfService->connection;
                    rfChannel->state = RFCOMM_CHANNEL_W4_CLIENT_AFTER_SABM;
                    // notify client and wait for confirm
                    log_dbg("-> Inform app\n");
                    rfcomm_emit_connection_request(rfChannel);
                    
                    // TODO: if client max frame size is smaller than RFCOMM_DEFAULT_SIZE, send PN

                    break;
                    
                }
            } else {
                // discard request by sending disconnected mode
                rfcomm_send_dm_pf(multiplexer, frame_dlci);
            }
            break;
            
        case BT_RFCOMM_UA:
            rfChannel = rfcomm_channel_for_multiplexer_and_dlci(multiplexer, frame_dlci);
            if (rfChannel && rfChannel->state == RFCOMM_CHANNEL_W4_UA){
                log_dbg("Received RFCOMM unnumbered acknowledgement for #%u - channel opened\n", frame_dlci);
                rfChannel->state = RFCOMM_CHANNEL_SEND_MSC_CMD_W4_MSC_CMD_OR_MSC_RSP;
            }
            break;
            
        case BT_RFCOMM_DISC:
            log_dbg("Received DISC command for #%u\n", frame_dlci);
            log_dbg("-> Sending UA for #%u\n", frame_dlci);
            rfcomm_send_ua(multiplexer, frame_dlci);
            rfChannel = rfcomm_channel_for_multiplexer_and_dlci(multiplexer, frame_dlci);
            if (rfChannel) {
                // signal client
                rfcomm_emit_channel_closed(rfChannel);
                // discard channel
                linked_list_remove( &rfcomm_channels, (linked_item_t *) rfChannel);
                free(rfChannel);
                rfcomm_multiplexer_prepare_idle_timer(multiplexer);
            }
            break;

            
        case BT_RFCOMM_DM:
        case BT_RFCOMM_DM_PF:
            log_dbg("Received DM message for #%u\n", frame_dlci);
            log_dbg("-> Closing channel locally for #%u\n", frame_dlci);
            rfChannel = rfcomm_channel_for_multiplexer_and_dlci(multiplexer, frame_dlci);
            if (rfChannel) {
                rfcomm_emit_channel_closed(rfChannel);
                linked_list_remove( &rfcomm_channels, (linked_item_t *) rfChannel);
                free(rfChannel);
                rfcomm_multiplexer_prepare_idle_timer(multiplexer);
            }
            break;

        case BT_RFCOMM_UIH_PF:
        case BT_RFCOMM_UIH:
            switch (packet[payload_offset]) {
                case BT_RFCOMM_PN_CMD:
                    message_dlci = packet[payload_offset+2];
                    rfService = rfcomm_service_for_channel(message_dlci >> 1);
                    if (rfService){
                        
                        log_dbg("Received UIH Parameter Negotiation Command for #%u\n", message_dlci);
                        
                        max_frame_size = READ_BT_16(packet, payload_offset+6);
                        rfChannel = rfcomm_channel_for_multiplexer_and_dlci(multiplexer, message_dlci);
                        if (!rfChannel){
                            // setup channel
                            rfChannel = rfcomm_channel_create(multiplexer, rfService, message_dlci >> 1);
                            if (!rfChannel) break;
                            rfChannel->connection = rfService->connection;
                            rfChannel->max_frame_size = rfService->max_frame_size;
                            rfChannel->state = RFCOMM_CHANNEL_W4_CLIENT_AFTER_PN_CMD;
                            // priority of client request
                            rfChannel->pn_priority = packet[payload_offset+4];
                            // negotiate max frame size
                            if (rfChannel->max_frame_size > multiplexer->max_frame_size) {
                                rfChannel->max_frame_size = multiplexer->max_frame_size;
                            }
                            if (rfChannel->max_frame_size > max_frame_size) {
                                rfChannel->max_frame_size = max_frame_size;
                            }
                            
                            // new credits
                            rfChannel->credits_outgoing = packet[payload_offset+9];
                            
                            // notify client and wait for confirm
                            log_dbg("-> Inform app\n");
                            rfcomm_emit_connection_request(rfChannel);
                            break;
                        }
                        
                        if (rfChannel->state != RFCOMM_CHANNEL_W4_SABM_OR_PN_CMD) break;
                        
                        // priority of client request
                        rfChannel->pn_priority = packet[payload_offset+4];
                        
                        // mfs of client request
                        if (max_frame_size > rfChannel->max_frame_size) {
                            max_frame_size = rfChannel->max_frame_size;
                        }
                        
                        // new credits
                        rfChannel->credits_outgoing = packet[payload_offset+9];
                        
                        log_dbg("-> Sending UIH Parameter Negotiation Respond for #%u\n", message_dlci);
                        rfcomm_send_uih_pn_response(multiplexer, message_dlci, rfChannel->pn_priority, max_frame_size);
                        
                        // wait for SABM #channel or other UIH PN
                        rfChannel->state = RFCOMM_CHANNEL_W4_SABM_OR_PN_CMD;
                    } else {
                        // discard request by sending disconnected mode
                        rfcomm_send_dm_pf(multiplexer, message_dlci);
                    }
                    break;
                    
                case BT_RFCOMM_PN_RSP:
                    message_dlci = packet[payload_offset+2];
                    log_dbg("Received UIH Parameter Negotiation Response for #%u\n", message_dlci);
                    rfChannel = rfcomm_channel_for_multiplexer_and_dlci(multiplexer, message_dlci);
                    if (rfChannel &&  rfChannel->state == RFCOMM_CHANNEL_W4_PN_RSP){
                        
                        //  received UIH Parameter Negotiation Response
                        max_frame_size = READ_BT_16(packet, payload_offset+6);
                        if (rfChannel->max_frame_size > max_frame_size) {
                            rfChannel->max_frame_size = max_frame_size;
                        }
                        
                        // new credits
                        rfChannel->credits_outgoing = packet[payload_offset+9];
                        
                        log_dbg("UIH Parameter Negotiation Response max frame %u, credits %u\n",
                                max_frame_size, rfChannel->credits_outgoing);
                        
                        rfChannel->state = RFCOMM_CHANNEL_SEND_SABM_W4_UA;
                    }
                    break;
                    
                case BT_RFCOMM_MSC_CMD: 
                    message_dlci = packet[payload_offset+2] >> 2;
                    rfChannel = rfcomm_channel_for_multiplexer_and_dlci(multiplexer, message_dlci);
                    log_dbg("Received MSC CMD for #%u, \n", message_dlci);
                    if (rfChannel) {
                        switch (rfChannel->state) {
                            case RFCOMM_CHANNEL_W4_MSC_CMD_OR_MSC_RSP:
                                rfChannel->state = RFCOMM_CHANNEL_SEND_MSC_RSP_W4_MSC_RSP;
                                break;
                                
                            case RFCOMM_CHANNEL_W4_MSC_CMD:
                                rfChannel->state = RFCOMM_CHANNEL_SEND_MSC_RSP_MSC_CMD_W4_CREDITS;
                                break;
                            default:
                                log_err("WRONG STATE %u for BT_RFCOMM_MSC_CMD\n", rfChannel->state); 
                        }
                        break;
                    }
                    break;
                    
                case BT_RFCOMM_MSC_RSP:
                    message_dlci = packet[payload_offset+2] >> 2;
                    log_dbg("Received MSC RSP for #%u\n", message_dlci);
                    rfChannel = rfcomm_channel_for_multiplexer_and_dlci(multiplexer, message_dlci);
                    if (rfChannel) {
                        switch (rfChannel->state) {
                            case RFCOMM_CHANNEL_W4_MSC_CMD_OR_MSC_RSP:
                                // outgoing channels
                                rfChannel->state = RFCOMM_CHANNEL_W4_MSC_CMD;
                                log_dbg("Waiting for MSC CMD for #%u\n", message_dlci);
                                break;
                            case RFCOMM_CHANNEL_W4_MSC_RSP:
                                // incoming channel
                                rfChannel->state = RFCOMM_CHANNEL_SEND_CREDITS;
                                break;
                            default:
                                break;
                        }
                    }
                    break;
                    
                case BT_RFCOMM_RPN_CMD:
                    // port negotiation command - just accept everything for now
                    message_dlci = packet[payload_offset+2] >> 2;
					message_len  = packet[payload_offset+1] >> 1;
					uint8_t default_rpn_psp[] = {
						BT_RFCOMM_RPN_RSP, 8, message_dlci,
						0xa0 /* 9600 bps */, 0x03 /* 8-n-1 */, 0 /* no flow control */,
						0xd1 /* XON */, 0xd3 /* XOFF */, 0x7f, 0x3f /* parameter mask, all values set */};
					
					switch (message_len) {
						case 1:
							// request for (dummy) parameters
							log_dbg("Received Remote Port Negotiation (Info) for #%u\n", message_dlci);
							log_dbg("-> Sending Remote Port Negotiation RSP for #%u\n", message_dlci);
							rfcomm_send_packet_for_multiplexer(multiplexer, packet[0] ^ 2, BT_RFCOMM_UIH, 0x00, (uint8_t*)default_rpn_psp, sizeof(default_rpn_psp));
							break;
						case 8:
							// control port parameters
							packet[payload_offset]  = BT_RFCOMM_RPN_RSP; 
							log_dbg("Received Remote Port Negotiation for #%u\n", message_dlci);
							log_dbg("-> Sending Remote Port Negotiation RSP for #%u\n", message_dlci);
							rfcomm_send_packet_for_multiplexer(multiplexer, packet[0] ^ 2, BT_RFCOMM_UIH, 0x00, (uint8_t*)&packet[payload_offset], 10);
							break;
						default:
							break;
					}
                    break;
             
                default:
                    break;
            }
            break;
            
        default:
            log_err("Received unknown RFCOMM message type %x\n", packet[1]);
            break;
    }
}

//
void rfcomm_close_connection(void *connection){
    linked_item_t *it;

    // close open channels 
    it = (linked_item_t *) &rfcomm_channels;
    while (it->next){
        rfcomm_channel_t * channel = (rfcomm_channel_t *) it->next;
        if (channel->connection == connection){
            
            // signal client
            rfcomm_emit_channel_closed(channel);
            
            // signal close
            rfcomm_multiplexer_t * multiplexer = channel->multiplexer;
            rfcomm_send_disc(multiplexer, channel->dlci);
            
            // remove from list
            it->next = it->next->next;
            free(channel);
            
            // update multiplexer timeout after channel was removed from list
            rfcomm_multiplexer_prepare_idle_timer(multiplexer);
            
        } else {
            it = it->next;
        }
    }
    
    // unregister services
    it = (linked_item_t *) &rfcomm_services;
    while (it->next) {
        rfcomm_service_t * service = (rfcomm_service_t *) it->next;
        if (service->connection == connection){
            it->next = it->next->next;
            free(service);
        } else {
            it = it->next;
        }
    }
}


// process outstanding signaling tasks
// MARK: RFCOMM RUN
void rfcomm_run(void){
    
    linked_item_t *it;
    for (it = (linked_item_t *) rfcomm_channels; it ; it = it->next){
        rfcomm_channel_t * channel = ((rfcomm_channel_t *) it);
        rfcomm_multiplexer_t * multiplexer = channel->multiplexer;
        
        if (!l2cap_can_send_packet_now(multiplexer->l2cap_cid)) continue;
     
        switch (channel->state){
                
            case RFCOMM_CHANNEL_W4_MULTIPLEXER:
                // sends UIH PN CMD to first channel with RFCOMM_CHANNEL_W4_MULTIPLEXER
                // this can then called after connection open/fail to handle all outgoing requests
                if (channel->outgoing && channel->multiplexer->state == RFCOMM_MULTIPLEXER_OPEN) {
                    log_dbg("Sending UIH Parameter Negotiation Command for #%u\n", channel->dlci );
                    rfcomm_send_uih_pn_command(multiplexer, channel->dlci, channel->max_frame_size);
                    channel->state = RFCOMM_CHANNEL_W4_PN_RSP;
                }
                break;

            case RFCOMM_CHANNEL_SEND_SABM_W4_UA:
                log_dbg("Sending SABM #%u\n", channel->dlci);
                rfcomm_send_sabm(multiplexer, channel->dlci);
                channel->state = RFCOMM_CHANNEL_W4_UA;
                break;
                
            case RFCOMM_CHANNEL_SEND_MSC_CMD_W4_MSC_CMD_OR_MSC_RSP:
                log_dbg("Sending MSC CMD for #%u (RFCOMM_CHANNEL_W4_MSC_CMD_OR_MSC_RSP)\n", channel->dlci);
                rfcomm_send_uih_msc_cmd(multiplexer, channel->dlci , 0x8d);  // ea=1,fc=0,rtc=1,rtr=1,ic=0,dv=1
                channel->state = RFCOMM_CHANNEL_W4_MSC_CMD_OR_MSC_RSP;
                break;
                
            case RFCOMM_CHANNEL_SEND_MSC_RSP_W4_MSC_RSP:
                // outgoing channel
                log_dbg("Sending MSC RSP for #%u\n", channel->dlci);
                rfcomm_send_uih_msc_rsp(multiplexer, channel->dlci, 0x8d);  // ea=1,fc=0,rtc=1,rtr=1,ic=0,dv=1
                channel->state = RFCOMM_CHANNEL_W4_MSC_RSP;
                break;
                
            case RFCOMM_CHANNEL_SEND_MSC_RSP_MSC_CMD_W4_CREDITS:
                // incoming channel
                log_dbg("-> Sending MSC RSP for #%u\n", channel->dlci);
                rfcomm_send_uih_msc_rsp(multiplexer, channel->dlci, 0x8d);  // ea=1,fc=0,rtc=1,rtr=1,ic=0,dv=1
                channel->state = RFCOMM_CHANNEL_SEND_MSC_CMD_SEND_CREDITS;
                break;
                
            case RFCOMM_CHANNEL_SEND_MSC_CMD_SEND_CREDITS:
                // start our negotiation
                log_dbg("Sending MSC CMD for #%u (but should wait for l2cap credits)\n", channel->dlci);
                rfcomm_send_uih_msc_cmd(multiplexer, channel->dlci, 0x8d); // ea=1,fc=0,rtc=1,rtr=1,ic=0,dv=1
                channel->state = RFCOMM_CHANNEL_SEND_CREDITS;
                break;
                
            case RFCOMM_CHANNEL_SEND_CREDITS:
                log_dbg("Providing credits for #%u\n", channel->dlci);
                rfcomm_channel_provide_credits(channel);
                
                if (channel->credits_outgoing){
                    // we already got credits - state == OPEN
                    rfcomm_channel_opened(channel);
                } else {
                    channel->state = RFCOMM_CHANNEL_W4_CREDITS;
                    log_dbg("Waiting for credits for #%u\n", channel->dlci);
                }
                break;
                
            case RFCOMM_CHANNEL_SEND_DM:
                log_dbg("Sending DM_PF for #%u\n", channel->dlci);
                rfcomm_send_dm_pf(multiplexer, channel->dlci);
                // remove from list
                linked_list_remove( &rfcomm_channels, (linked_item_t *) channel);
                // free channel
                free(channel);
                rfcomm_multiplexer_prepare_idle_timer(multiplexer);
                break;
                
            case RFCOMM_CHANNEL_SEND_UA:
                log_dbg("Sending UA #%u\n", channel->dlci);
                rfcomm_send_ua(multiplexer, channel->dlci);
                channel->state = RFCOMM_CHANNEL_W4_MSC_CMD;
                break;
                
            case RFCOMM_CHANNEL_SEND_PN_RSP_W4_SABM_OR_PN_CMD:
                log_dbg("Sending UIH Parameter Negotiation Respond for #%u\n", channel->dlci);
                rfcomm_send_uih_pn_response(multiplexer, channel->dlci, channel->pn_priority, channel->max_frame_size);
                // wait for SABM #channel or other UIH PN
                channel->state = RFCOMM_CHANNEL_W4_SABM_OR_PN_CMD;
                break;

            default:
                break;
        }
    }
}

// MARK: RFCOMM BTstack API

void rfcomm_init(void){
    rfcomm_client_cid_generator = 0;
    rfcomm_multiplexers = NULL;
    rfcomm_services     = NULL;
    rfcomm_channels     = NULL;
}

// register packet handler
void rfcomm_register_packet_handler(void (*handler)(void * connection, uint8_t packet_type,
                                                    uint16_t channel, uint8_t *packet, uint16_t size)){
	app_packet_handler = handler;
}

// send packet over specific channel
int rfcomm_send_internal(uint8_t rfcomm_cid, uint8_t *data, uint16_t len){

    rfcomm_channel_t * channel = rfcomm_channel_for_rfcomm_cid(rfcomm_cid);
    if (!channel){
        log_err("rfcomm_send_internal cid %u doesn't exist!\n", rfcomm_cid);
        return 0;
    }
    
    if (!channel->credits_outgoing){
        log_dbg("rfcomm_send_internal cid %u, no rfcomm outgoing credits!\n", rfcomm_cid);
        return RFCOMM_NO_OUTGOING_CREDITS;
    }

    if (!channel->packets_granted){
        log_dbg("rfcomm_send_internal cid %u, no rfcomm credits granted!\n", rfcomm_cid);
        // return RFCOMM_NO_OUTGOING_CREDITS;
    }
    
    // log_dbg("rfcomm_send_internal: len %u... outgoing credits %u, l2cap credit %us, granted %u\n",
    //        len, channel->credits_outgoing, channel->multiplexer->l2cap_credits, channel->packets_granted);
    

    // send might cause l2cap to emit new credits, update counters first
    channel->credits_outgoing--;
    int packets_granted_decreased = 0;
    if (channel->packets_granted) {
        channel->packets_granted--;
        packets_granted_decreased++;
    }
    
    int result = rfcomm_send_uih_data(channel->multiplexer, channel->dlci, data, len);
    
    if (result != 0) {
        channel->credits_outgoing++;
        channel->packets_granted += packets_granted_decreased;
        log_dbg("rfcomm_send_internal: error %d\n", result);
        return result;
    }
    
    // log_dbg("rfcomm_send_internal: now outgoing credits %u, l2cap credit %us, granted %u\n",
    //        channel->credits_outgoing, channel->multiplexer->l2cap_credits, channel->packets_granted);

    rfcomm_hand_out_credits();
    
    return result;
}

void rfcomm_create_channel_internal(void * connection, bd_addr_t *addr, uint8_t server_channel){

    int send_l2cap_create_channel = 0;
    
    log_dbg("rfcomm_create_channel_internal to ");
    print_bd_addr(*addr);
    log_dbg(" at channel #%02x\n", server_channel);
    rfcomm_dump_channels();

    // create new multiplexer if necessary
    rfcomm_multiplexer_t * multiplexer = rfcomm_multiplexer_for_addr(addr);
    if (!multiplexer) {
        
        // start multiplexer setup
        multiplexer = rfcomm_multiplexer_create_for_addr(addr);
        multiplexer->outgoing = 1;
        multiplexer->state = RFCOMM_MULTIPLEXER_W4_CONNECT;
        
        // create l2cap channel for multiplexer
        send_l2cap_create_channel = 1;
    }


    // prepare channel
    rfcomm_channel_t * channel = rfcomm_channel_create(multiplexer, 0, server_channel);
    channel->connection = connection;
    channel->state = RFCOMM_CHANNEL_W4_MULTIPLEXER;

    rfcomm_dump_channels();

    // start connecting, if multiplexer is already up and running
    if (multiplexer->state == RFCOMM_MULTIPLEXER_OPEN){
        rfcomm_run();
    }
    
    // go!
    if (send_l2cap_create_channel) {
        // doesn't block
        l2cap_create_channel_internal(connection, rfcomm_packet_handler, *addr, PSM_RFCOMM, RFCOMM_L2CAP_MTU);
    }
}

void rfcomm_disconnect_internal(uint16_t rfcomm_cid){
    // TODO: be less drastic
    rfcomm_channel_t * channel = rfcomm_channel_for_rfcomm_cid(rfcomm_cid);
    if (channel) {
        // signal client
        rfcomm_emit_channel_closed(channel);

        // signal close
        rfcomm_multiplexer_t * multiplexer = channel->multiplexer;
        rfcomm_send_disc(multiplexer, channel->dlci);

        // remove from list
        linked_list_remove( &rfcomm_channels, (linked_item_t *) channel);
        free(channel);
        
        rfcomm_multiplexer_prepare_idle_timer(multiplexer);
    }
}

void rfcomm_register_service_internal(void * connection, uint8_t channel, uint16_t max_frame_size){
    
    // check if already registered
    rfcomm_service_t * service = rfcomm_service_for_channel(channel);
    if (service){
        rfcomm_emit_service_registered(service->connection, RFCOMM_CHANNEL_ALREADY_REGISTERED, channel);
        return;
    }
    
    // alloc structure
    service = malloc(sizeof(rfcomm_service_t));
    if (!service) {
        rfcomm_emit_service_registered(service->connection, BTSTACK_MEMORY_ALLOC_FAILED, channel);
        return;
    }

    // register with l2cap if not registered before
    if (linked_list_empty(&rfcomm_services)){
        l2cap_register_service_internal(NULL, rfcomm_packet_handler, PSM_RFCOMM, RFCOMM_L2CAP_MTU);
    }

    // fill in 
    service->connection     = connection;
    service->server_channel = channel;
    service->max_frame_size = max_frame_size;
    
    // add to services list
    linked_list_add(&rfcomm_services, (linked_item_t *) service);
    
    // done
    rfcomm_emit_service_registered(service->connection, 0, channel);
}

void rfcomm_unregister_service_internal(uint8_t service_channel){
    rfcomm_service_t *service = rfcomm_service_for_channel(service_channel);
    if (!service) return;
    linked_list_remove(&rfcomm_services, (linked_item_t *) service);
    free(service);
    
    // unregister if no services active
    if (linked_list_empty(&rfcomm_services)){
        // bt_send_cmd(&l2cap_unregister_service, PSM_RFCOMM);
        l2cap_unregister_service_internal(NULL, PSM_RFCOMM);
    }
}

void rfcomm_accept_connection_internal(uint16_t rfcomm_cid){
    log_dbg("Received Accept Connction\n");
    rfcomm_channel_t * channel = rfcomm_channel_for_rfcomm_cid(rfcomm_cid);
    if (!channel) return;
    switch (channel->state) {
        case RFCOMM_CHANNEL_W4_CLIENT_AFTER_SABM:
            channel->state = RFCOMM_CHANNEL_SEND_UA;
            break;
        case RFCOMM_CHANNEL_W4_CLIENT_AFTER_PN_CMD:
            channel->state = RFCOMM_CHANNEL_SEND_PN_RSP_W4_SABM_OR_PN_CMD;
            break;
        default:
            break;
    }
}

void rfcomm_decline_connection_internal(uint16_t rfcomm_cid){
    log_dbg("Received Decline Connction\n");
    rfcomm_channel_t * channel = rfcomm_channel_for_rfcomm_cid(rfcomm_cid);
    if (!channel) return;
    switch (channel->state) {
        case RFCOMM_CHANNEL_W4_CLIENT_AFTER_SABM:
        case RFCOMM_CHANNEL_W4_CLIENT_AFTER_PN_CMD:
            channel->state = RFCOMM_CHANNEL_SEND_DM;
            break;
        default:
            break;
    }
}



