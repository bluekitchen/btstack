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

#define __BTSTACK_FILE__ "l2cap.c"

/*
 *  l2cap.c
 *
 *  Logical Link Control and Adaption Protocl (L2CAP)
 *
 *  Created by Matthias Ringwald on 5/16/09.
 */

#include "l2cap.h"
#include "hci.h"
#include "hci_dump.h"
#include "bluetooth_sdp.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_memory.h"

#ifdef ENABLE_LE_DATA_CHANNELS
#include "ble/sm.h"
#endif

#include <stdarg.h>
#include <string.h>

#include <stdio.h>

// nr of buffered acl packets in outgoing queue to get max performance 
#define NR_BUFFERED_ACL_PACKETS 3

// used to cache l2cap rejects, echo, and informational requests
#define NR_PENDING_SIGNALING_RESPONSES 3

// nr of credits provided to remote if credits fall below watermark
#define L2CAP_LE_DATA_CHANNELS_AUTOMATIC_CREDITS_WATERMARK 5
#define L2CAP_LE_DATA_CHANNELS_AUTOMATIC_CREDITS_INCREMENT 5

// offsets for L2CAP SIGNALING COMMANDS
#define L2CAP_SIGNALING_COMMAND_CODE_OFFSET   0
#define L2CAP_SIGNALING_COMMAND_SIGID_OFFSET  1
#define L2CAP_SIGNALING_COMMAND_LENGTH_OFFSET 2
#define L2CAP_SIGNALING_COMMAND_DATA_OFFSET   4

// internal table
#define L2CAP_FIXED_CHANNEL_TABLE_INDEX_ATTRIBUTE_PROTOCOL 0
#define L2CAP_FIXED_CHANNEL_TABLE_INDEX_SECURITY_MANAGER_PROTOCOL  1
#define L2CAP_FIXED_CHANNEL_TABLE_INDEX_CONNECTIONLESS_CHANNEL 2
#define L2CAP_FIXED_CHANNEL_TABLE_SIZE (L2CAP_FIXED_CHANNEL_TABLE_INDEX_CONNECTIONLESS_CHANNEL+1)

#if defined(ENABLE_LE_DATA_CHANNELS) || defined(ENABLE_CLASSIC)
#define L2CAP_USES_CHANNELS
#endif

// prototypes
static void l2cap_run(void);
static void l2cap_hci_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void l2cap_acl_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size );
static void l2cap_notify_channel_can_send(void);
static void l2cap_emit_can_send_now(btstack_packet_handler_t packet_handler, uint16_t channel);
#ifdef ENABLE_CLASSIC
static void l2cap_finialize_channel_close(l2cap_channel_t *channel);
static inline l2cap_service_t * l2cap_get_service(uint16_t psm);
static void l2cap_emit_channel_opened(l2cap_channel_t *channel, uint8_t status);
static void l2cap_emit_channel_closed(l2cap_channel_t *channel);
static void l2cap_emit_incoming_connection(l2cap_channel_t *channel);
static int  l2cap_channel_ready_for_open(l2cap_channel_t *channel);
#endif
#ifdef ENABLE_LE_DATA_CHANNELS
static void l2cap_emit_le_channel_opened(l2cap_channel_t *channel, uint8_t status);
static void l2cap_emit_le_incoming_connection(l2cap_channel_t *channel);
static l2cap_channel_t * l2cap_le_get_channel_for_local_cid(uint16_t local_cid);
static void l2cap_le_notify_channel_can_send(l2cap_channel_t *channel);
static void l2cap_le_finialize_channel_close(l2cap_channel_t *channel);
static inline l2cap_service_t * l2cap_le_get_service(uint16_t psm);
#endif
#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
static int l2cap_ertm_num_unacknowledged_tx_packets(l2cap_channel_t * channel);
#endif

typedef struct l2cap_fixed_channel {
    btstack_packet_handler_t callback;
    uint8_t waiting_for_can_send_now;
} l2cap_fixed_channel_t;

#ifdef ENABLE_CLASSIC
static btstack_linked_list_t l2cap_channels;
static btstack_linked_list_t l2cap_services;
static uint8_t require_security_level2_for_outgoing_sdp;
#endif

#ifdef ENABLE_LE_DATA_CHANNELS
static btstack_linked_list_t l2cap_le_channels;
static btstack_linked_list_t l2cap_le_services;
#endif

// used to cache l2cap rejects, echo, and informational requests
static l2cap_signaling_response_t signaling_responses[NR_PENDING_SIGNALING_RESPONSES];
static int signaling_responses_pending;

static btstack_packet_callback_registration_t hci_event_callback_registration;
static l2cap_fixed_channel_t fixed_channels[L2CAP_FIXED_CHANNEL_TABLE_SIZE];

#ifdef ENABLE_BLE
// only used for connection parameter update events
static btstack_packet_handler_t l2cap_event_packet_handler;
#endif

#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE

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

#endif


static uint16_t l2cap_fixed_channel_table_channel_id_for_index(int index){
    switch (index){
        case L2CAP_FIXED_CHANNEL_TABLE_INDEX_ATTRIBUTE_PROTOCOL:
            return L2CAP_CID_ATTRIBUTE_PROTOCOL;
        case L2CAP_FIXED_CHANNEL_TABLE_INDEX_SECURITY_MANAGER_PROTOCOL:
            return L2CAP_CID_SECURITY_MANAGER_PROTOCOL;
        case L2CAP_FIXED_CHANNEL_TABLE_INDEX_CONNECTIONLESS_CHANNEL:
            return L2CAP_CID_CONNECTIONLESS_CHANNEL;
        default:
            return 0;
    }  
}
static int l2cap_fixed_channel_table_index_for_channel_id(uint16_t channel_id){
    switch (channel_id){
        case L2CAP_CID_ATTRIBUTE_PROTOCOL:
            return L2CAP_FIXED_CHANNEL_TABLE_INDEX_ATTRIBUTE_PROTOCOL;
        case L2CAP_CID_SECURITY_MANAGER_PROTOCOL:
            return  L2CAP_FIXED_CHANNEL_TABLE_INDEX_SECURITY_MANAGER_PROTOCOL;
        case L2CAP_CID_CONNECTIONLESS_CHANNEL:
            return  L2CAP_FIXED_CHANNEL_TABLE_INDEX_CONNECTIONLESS_CHANNEL;
        default:
            return -1;
        }
}

static int l2cap_fixed_channel_table_index_is_le(int index){
    if (index == L2CAP_CID_CONNECTIONLESS_CHANNEL) return 0;
    return 1;
}

void l2cap_init(void){
    signaling_responses_pending = 0;
    
    uint8_t test[] = {0x0E, 0x00, 0x40, 0x00, 0x02, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09 };
    printf("crc16: %04x\n", crc16_calc(test, sizeof(test)));

#ifdef ENABLE_CLASSIC
    l2cap_channels = NULL;
    l2cap_services = NULL;
    require_security_level2_for_outgoing_sdp = 0;
#endif

#ifdef ENABLE_LE_DATA_CHANNELS
    l2cap_le_services = NULL;
    l2cap_le_channels = NULL;
#endif

#ifdef ENABLE_BLE
    l2cap_event_packet_handler = NULL;
#endif
    memset(fixed_channels, 0, sizeof(fixed_channels));

    // 
    // register callback with HCI
    //
    hci_event_callback_registration.callback = &l2cap_hci_event_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    hci_register_acl_packet_handler(&l2cap_acl_handler);

#ifdef ENABLE_CLASSIC
    gap_connectable_control(0); // no services yet
#endif
}

void l2cap_register_packet_handler(void (*handler)(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)){
#ifdef ENABLE_BLE
    l2cap_event_packet_handler = handler;
#else
    UNUSED(handler);
#endif
}

void l2cap_request_can_send_fix_channel_now_event(hci_con_handle_t con_handle, uint16_t channel_id){
    UNUSED(con_handle);

    int index = l2cap_fixed_channel_table_index_for_channel_id(channel_id);
    if (index < 0) return;
    fixed_channels[index].waiting_for_can_send_now = 1;
    l2cap_notify_channel_can_send();
}

int  l2cap_can_send_fixed_channel_packet_now(hci_con_handle_t con_handle, uint16_t channel_id){
    UNUSED(channel_id);

    return hci_can_send_acl_packet_now(con_handle);
}

uint8_t *l2cap_get_outgoing_buffer(void){
    return hci_get_outgoing_packet_buffer() + COMPLETE_L2CAP_HEADER; // 8 bytes
}

int l2cap_reserve_packet_buffer(void){
    return hci_reserve_packet_buffer();
}

void l2cap_release_packet_buffer(void){
    hci_release_packet_buffer();
}

static void l2cap_setup_header(uint8_t * acl_buffer, hci_con_handle_t con_handle, uint8_t packet_boundary, uint16_t remote_cid, uint16_t len){
    // 0 - Connection handle : PB=pb : BC=00 
    little_endian_store_16(acl_buffer, 0, con_handle | (packet_boundary << 12) | (0 << 14));
    // 2 - ACL length
    little_endian_store_16(acl_buffer, 2,  len + 4);
    // 4 - L2CAP packet length
    little_endian_store_16(acl_buffer, 4,  len + 0);
    // 6 - L2CAP channel DEST
    little_endian_store_16(acl_buffer, 6,  remote_cid);    
}

// assumption - only on LE connections
int l2cap_send_prepared_connectionless(hci_con_handle_t con_handle, uint16_t cid, uint16_t len){
    
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
    return hci_send_acl_packet_buffer(len+8);
}

// assumption - only on LE connections
int l2cap_send_connectionless(hci_con_handle_t con_handle, uint16_t cid, uint8_t *data, uint16_t len){
    
    if (!hci_can_send_acl_packet_now(con_handle)){
        log_info("l2cap_send cid 0x%02x, cannot send", cid);
        return BTSTACK_ACL_BUFFERS_FULL;
    }
    
    hci_reserve_packet_buffer();
    uint8_t *acl_buffer = hci_get_outgoing_packet_buffer();
    
    memcpy(&acl_buffer[8], data, len);
    
    return l2cap_send_prepared_connectionless(con_handle, cid, len);
}

static void l2cap_emit_can_send_now(btstack_packet_handler_t packet_handler, uint16_t channel) {
    log_debug("L2CAP_EVENT_CHANNEL_CAN_SEND_NOW local_cid 0x%x", channel);
    uint8_t event[4];
    event[0] = L2CAP_EVENT_CAN_SEND_NOW;
    event[1] = sizeof(event) - 2;
    little_endian_store_16(event, 2, channel);
    hci_dump_packet( HCI_EVENT_PACKET, 0, event, sizeof(event));
    packet_handler(HCI_EVENT_PACKET, channel, event, sizeof(event));
}

#ifdef L2CAP_USES_CHANNELS
static void l2cap_dispatch_to_channel(l2cap_channel_t *channel, uint8_t type, uint8_t * data, uint16_t size){
    (* (channel->packet_handler))(type, channel->local_cid, data, size);
}

static void l2cap_emit_simple_event_with_cid(l2cap_channel_t * channel, uint8_t event_code){
    uint8_t event[4];
    event[0] = event_code;
    event[1] = sizeof(event) - 2;
    little_endian_store_16(event, 2, channel->local_cid);
    hci_dump_packet( HCI_EVENT_PACKET, 0, event, sizeof(event));
    l2cap_dispatch_to_channel(channel, HCI_EVENT_PACKET, event, sizeof(event));
}
#endif

#ifdef ENABLE_CLASSIC
void l2cap_emit_channel_opened(l2cap_channel_t *channel, uint8_t status) {
    log_info("L2CAP_EVENT_CHANNEL_OPENED status 0x%x addr %s handle 0x%x psm 0x%x local_cid 0x%x remote_cid 0x%x local_mtu %u, remote_mtu %u, flush_timeout %u",
             status, bd_addr_to_str(channel->address), channel->con_handle, channel->psm,
             channel->local_cid, channel->remote_cid, channel->local_mtu, channel->remote_mtu, channel->flush_timeout);
    uint8_t event[24];
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
    event[23] = channel->state_var & L2CAP_CHANNEL_STATE_VAR_INCOMING ? 1 : 0;
    hci_dump_packet( HCI_EVENT_PACKET, 0, event, sizeof(event));
    l2cap_dispatch_to_channel(channel, HCI_EVENT_PACKET, event, sizeof(event));
}

static void l2cap_emit_channel_closed(l2cap_channel_t *channel) {
    log_info("L2CAP_EVENT_CHANNEL_CLOSED local_cid 0x%x", channel->local_cid);
    l2cap_emit_simple_event_with_cid(channel, L2CAP_EVENT_CHANNEL_CLOSED);
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
    hci_dump_packet( HCI_EVENT_PACKET, 0, event, sizeof(event));
    l2cap_dispatch_to_channel(channel, HCI_EVENT_PACKET, event, sizeof(event));
}

static l2cap_channel_t * l2cap_get_channel_for_local_cid(uint16_t local_cid){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, &l2cap_channels);
    while (btstack_linked_list_iterator_has_next(&it)){
        l2cap_channel_t * channel = (l2cap_channel_t *) btstack_linked_list_iterator_next(&it);
        if ( channel->local_cid == local_cid) {
            return channel;
        }
    } 
    return NULL;
}

///

void l2cap_request_can_send_now_event(uint16_t local_cid){
    l2cap_channel_t *channel = l2cap_get_channel_for_local_cid(local_cid);
    if (!channel) return;
    channel->waiting_for_can_send_now = 1;
    l2cap_notify_channel_can_send();
}

int  l2cap_can_send_packet_now(uint16_t local_cid){
    l2cap_channel_t *channel = l2cap_get_channel_for_local_cid(local_cid);
    if (!channel) return 0;
    return hci_can_send_acl_packet_now(channel->con_handle);
}

int  l2cap_can_send_prepared_packet_now(uint16_t local_cid){
    l2cap_channel_t *channel = l2cap_get_channel_for_local_cid(local_cid);
    if (!channel) return 0;
    return hci_can_send_prepared_acl_packet_now(channel->con_handle);
}
uint16_t l2cap_get_remote_mtu_for_local_cid(uint16_t local_cid){
    l2cap_channel_t * channel = l2cap_get_channel_for_local_cid(local_cid);
    if (channel) {
        return channel->remote_mtu;
    } 
    return 0;
}

static l2cap_channel_t * l2cap_channel_for_rtx_timer(btstack_timer_source_t * ts){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, &l2cap_channels);
    while (btstack_linked_list_iterator_has_next(&it)){
        l2cap_channel_t * channel = (l2cap_channel_t *) btstack_linked_list_iterator_next(&it);
        if ( &channel->rtx == ts) {
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
    l2cap_emit_channel_opened(channel, L2CAP_CONNECTION_RESPONSE_RESULT_RTX_TIMEOUT);

    // discard channel
    // no need to stop timer here, it is removed from list during timer callback
    btstack_linked_list_remove(&l2cap_channels, (btstack_linked_item_t *) channel);
    btstack_memory_l2cap_channel_free(channel);
}

static void l2cap_stop_rtx(l2cap_channel_t * channel){
    log_info("l2cap_stop_rtx for local cid 0x%02x", channel->local_cid);
    btstack_run_loop_remove_timer(&channel->rtx);
}

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
    require_security_level2_for_outgoing_sdp = 1;
}

static int l2cap_security_level_0_allowed_for_PSM(uint16_t psm){
    return (psm == BLUETOOTH_PROTOCOL_SDP) && (!require_security_level2_for_outgoing_sdp);
}

static int l2cap_send_signaling_packet(hci_con_handle_t handle, L2CAP_SIGNALING_COMMANDS cmd, uint8_t identifier, ...){
    if (!hci_can_send_acl_packet_now(handle)){
        log_info("l2cap_send_signaling_packet, cannot send");
        return BTSTACK_ACL_BUFFERS_FULL;
    }
    
    // log_info("l2cap_send_signaling_packet type %u", cmd);
    hci_reserve_packet_buffer();
    uint8_t *acl_buffer = hci_get_outgoing_packet_buffer();
    va_list argptr;
    va_start(argptr, identifier);
    uint16_t len = l2cap_create_signaling_classic(acl_buffer, handle, cmd, identifier, argptr);
    va_end(argptr);
    // log_info("l2cap_send_signaling_packet con %u!", handle);
    return hci_send_acl_packet_buffer(len);
}

// assumption - only on Classic connections
int l2cap_send_prepared(uint16_t local_cid, uint16_t len){
    
    if (!hci_is_packet_buffer_reserved()){
        log_error("l2cap_send_prepared called without reserving packet first");
        return BTSTACK_ACL_BUFFERS_FULL;
    }

    l2cap_channel_t * channel = l2cap_get_channel_for_local_cid(local_cid);
    if (!channel) {
        log_error("l2cap_send_prepared no channel for cid 0x%02x", local_cid);
        return -1;   // TODO: define error
    }

    if (!hci_can_send_prepared_acl_packet_now(channel->con_handle)){
        log_info("l2cap_send_prepared cid 0x%02x, cannot send", local_cid);
        return BTSTACK_ACL_BUFFERS_FULL;
    }
    
    log_debug("l2cap_send_prepared cid 0x%02x, handle %u, 1 credit used", local_cid, channel->con_handle);
    
    int fcs_size = 0;

#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
    if (channel->mode == L2CAP_CHANNEL_MODE_ENHANCED_RETRANSMISSION){
        fcs_size = 2;
    }
#endif

    // set non-flushable packet boundary flag if supported on Controller
    uint8_t *acl_buffer = hci_get_outgoing_packet_buffer();
    uint8_t packet_boundary_flag = hci_non_flushable_packet_boundary_flag_supported() ? 0x00 : 0x02;
    l2cap_setup_header(acl_buffer, channel->con_handle, packet_boundary_flag, channel->remote_cid, len + fcs_size);

#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
    if (channel->mode == L2CAP_CHANNEL_MODE_ENHANCED_RETRANSMISSION){
        // calculate FCS over l2cap data
        uint16_t fcs = crc16_calc(acl_buffer + 4, 4 + len);
        log_info("I-Frame: fcs 0x%04x", fcs);
        little_endian_store_16(acl_buffer, 8 + len, fcs);
    }
#endif

    // send
    return hci_send_acl_packet_buffer(len+8+fcs_size);
}

#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
static inline uint16_t l2cap_encanced_control_field_for_information_frame(uint8_t tx_seq, int final, uint8_t req_seq, l2cap_segmentation_and_reassembly_t sar){
    return (((uint16_t) sar) << 14) | (req_seq << 8) | (final << 7) | (tx_seq << 1) | 0; 
}
static inline uint16_t l2cap_encanced_control_field_for_supevisor_frame(l2cap_supervisory_function_t supervisory_function, int poll, int final, uint8_t req_seq){
    return (req_seq << 8) | (final << 7) | (poll << 4) | (((int) supervisory_function) << 2) | 1; 
}
static int l2cap_next_ertm_seq_nr(int seq_nr){
    return (seq_nr + 1) & 0x3f;
}
static void l2cap_ertm_next_tx_write_index(l2cap_channel_t * channel){
    channel->tx_write_index++;
    if (channel->tx_write_index < channel->num_tx_buffers) return;
    channel->tx_write_index = 0;
}
static int l2cap_ertm_send_information_frame(l2cap_channel_t * channel, int index){
    l2cap_ertm_tx_packet_state_t * tx_state = &channel->tx_packets_state[index];
    hci_reserve_packet_buffer();
    uint8_t *acl_buffer = hci_get_outgoing_packet_buffer();
    uint16_t control = l2cap_encanced_control_field_for_information_frame(tx_state->tx_seq, 0, 0, L2CAP_SEGMENTATION_AND_REASSEMBLY_UNSEGMENTED_L2CAP_SDU);
    log_info("I-Frame: control 0x%04x", control);
    little_endian_store_16(acl_buffer, 8, control);
    memcpy(&acl_buffer[8+2], &channel->tx_packets_data[index * channel->local_mtu], tx_state->len);
    // send
    return l2cap_send_prepared(channel->local_cid, 2 + tx_state->len);
}
static int l2cap_ertm_send(l2cap_channel_t * channel, uint8_t * data, uint16_t len){
    if (len > channel->remote_mtu){
        log_error("l2cap_send cid 0x%02x, data length exceeds remote MTU.", channel->local_cid);
        return L2CAP_DATA_LEN_EXCEEDS_REMOTE_MTU;
    }
    // TODO: check tx_transmit
    // store int tx packet bufferx
    int index = channel->tx_write_index;
    l2cap_ertm_tx_packet_state_t * tx_state = &channel->tx_packets_state[index];
    tx_state->tx_seq = channel->next_tx_seq;
    tx_state->len = len;
    memcpy(&channel->tx_packets_data[index * channel->local_mtu], data, len);
    // update
    channel->next_tx_seq = l2cap_next_ertm_seq_nr(channel->next_tx_seq);
    l2cap_ertm_next_tx_write_index(channel);
    // try to send
    l2cap_run();
    return 0;
}
#endif

// assumption - only on Classic connections
int l2cap_send(uint16_t local_cid, uint8_t *data, uint16_t len){
    l2cap_channel_t * channel = l2cap_get_channel_for_local_cid(local_cid);
    if (!channel) {
        log_error("l2cap_send no channel for cid 0x%02x", local_cid);
        return -1;   // TODO: define error
    }

#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
    // send in ERTM
    if (channel->mode == L2CAP_CHANNEL_MODE_ENHANCED_RETRANSMISSION){
        return l2cap_ertm_send(channel, data, len);
    }
#endif

    if (len > channel->remote_mtu){
        log_error("l2cap_send cid 0x%02x, data length exceeds remote MTU.", local_cid);
        return L2CAP_DATA_LEN_EXCEEDS_REMOTE_MTU;
    }

    if (!hci_can_send_acl_packet_now(channel->con_handle)){
        log_info("l2cap_send cid 0x%02x, cannot send", local_cid);
        return BTSTACK_ACL_BUFFERS_FULL;
    }

    hci_reserve_packet_buffer();
    uint8_t *acl_buffer = hci_get_outgoing_packet_buffer();
    memcpy(&acl_buffer[8], data, len);
    return l2cap_send_prepared(local_cid, len);
}

int l2cap_send_echo_request(hci_con_handle_t con_handle, uint8_t *data, uint16_t len){
    return l2cap_send_signaling_packet(con_handle, ECHO_REQUEST, 0x77, len, data);
}

static inline void channelStateVarSetFlag(l2cap_channel_t *channel, L2CAP_CHANNEL_STATE_VAR flag){
    channel->state_var = (L2CAP_CHANNEL_STATE_VAR) (channel->state_var | flag);
}

static inline void channelStateVarClearFlag(l2cap_channel_t *channel, L2CAP_CHANNEL_STATE_VAR flag){
    channel->state_var = (L2CAP_CHANNEL_STATE_VAR) (channel->state_var & ~flag);
}
#endif


#ifdef ENABLE_BLE
static int l2cap_send_le_signaling_packet(hci_con_handle_t handle, L2CAP_SIGNALING_COMMANDS cmd, uint8_t identifier, ...){

    if (!hci_can_send_acl_packet_now(handle)){
        log_info("l2cap_send_le_signaling_packet, cannot send");
        return BTSTACK_ACL_BUFFERS_FULL;
    }
    
    // log_info("l2cap_send_le_signaling_packet type %u", cmd);
    hci_reserve_packet_buffer();
    uint8_t *acl_buffer = hci_get_outgoing_packet_buffer();
    va_list argptr;
    va_start(argptr, identifier);
    uint16_t len = l2cap_create_signaling_le(acl_buffer, handle, cmd, identifier, argptr);
    va_end(argptr);
    // log_info("l2cap_send_le_signaling_packet con %u!", handle);
    return hci_send_acl_packet_buffer(len);
}
#endif

uint16_t l2cap_max_mtu(void){
    return HCI_ACL_PAYLOAD_SIZE - L2CAP_HEADER_SIZE;
}

uint16_t l2cap_max_le_mtu(void){
    return l2cap_max_mtu();
}

static uint16_t l2cap_setup_options_mtu(l2cap_channel_t * channel, uint8_t * config_options){
    config_options[0] = 1; // MTU
    config_options[1] = 2; // len param
    little_endian_store_16( (uint8_t*)&config_options, 2, channel->local_mtu);
    return 4;
}

#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
static uint16_t l2cap_setup_options_ertm(l2cap_channel_t * channel, uint8_t * config_options){
    config_options[0] = 0x04;   // RETRANSMISSION AND FLOW CONTROL OPTION
    config_options[1] = 9;      // length
    config_options[2] = (uint8_t) channel->mode;
    config_options[3] = channel->num_rx_buffers;    // == TxWindows size
    config_options[4] = channel->local_max_transmit;
    little_endian_store_16( config_options, 5, channel->local_retransmission_timeout_ms); 
    little_endian_store_16( config_options, 7, channel->local_monitor_timeout_ms);
    little_endian_store_16( config_options, 9, channel->local_mtu);
    return 11;
}
static int l2cap_ertm_send_supervisor_frame(l2cap_channel_t * channel, uint16_t control){
    hci_reserve_packet_buffer();
    uint8_t *acl_buffer = hci_get_outgoing_packet_buffer();
    log_info("S-Frame: control 0x%04x", control);
    little_endian_store_16(acl_buffer, 8, control);
    return l2cap_send_prepared(channel->local_cid, 2);
}
static int l2cap_ertm_num_unacknowledged_tx_packets(l2cap_channel_t * channel){
    int unacknowledged_packets = channel->tx_send_index - channel->tx_read_index;
    if (unacknowledged_packets < 0){
        unacknowledged_packets += channel->num_tx_buffers;
    }
    return unacknowledged_packets; 
}
#endif

static uint16_t l2cap_setup_options(l2cap_channel_t * channel, uint8_t * config_options){
#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
    // use ERTM options if supported
    hci_connection_t * connection = hci_connection_for_handle(channel->con_handle);
    if ((connection->l2cap_state.information_state == L2CAP_INFORMATION_STATE_DONE) && (connection->l2cap_state.extended_feature_mask & 0x08)){
        return l2cap_setup_options_ertm(channel, config_options);

    }
#endif
    return l2cap_setup_options_mtu(channel, config_options);
}

static uint32_t l2cap_extended_features_mask(void){
    // extended features request supported, features: fixed channels, unicast connectionless data reception
    uint32_t features = 0x280;
#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
    features |= 0x0008;
#endif
    return features;
}

// MARK: L2CAP_RUN
// process outstanding signaling tasks
static void l2cap_run(void){
    
    // log_info("l2cap_run: entered");

    // check pending signaling responses
    while (signaling_responses_pending){
        
        hci_con_handle_t handle = signaling_responses[0].handle;
        
        if (!hci_can_send_acl_packet_now(handle)) break;

        uint8_t  sig_id        = signaling_responses[0].sig_id;
        uint8_t  response_code = signaling_responses[0].code;
        uint16_t infoType      = signaling_responses[0].data;  // INFORMATION_REQUEST
        uint16_t result        = signaling_responses[0].data;  // CONNECTION_REQUEST, COMMAND_REJECT
#ifdef ENABLE_CLASSIC
        uint16_t source_cid    = signaling_responses[0].cid;   // CONNECTION_REQUEST
#endif
        UNUSED(infoType);

        // remove first item before sending (to avoid sending response mutliple times)
        signaling_responses_pending--;
        int i;
        for (i=0; i < signaling_responses_pending; i++){
            memcpy(&signaling_responses[i], &signaling_responses[i+1], sizeof(l2cap_signaling_response_t));
        }

        switch (response_code){
#ifdef ENABLE_CLASSIC
            case CONNECTION_REQUEST:
                l2cap_send_signaling_packet(handle, CONNECTION_RESPONSE, sig_id, source_cid, 0, result, 0);
                // also disconnect if result is 0x0003 - security blocked
                if (result == 0x0003){
                    hci_disconnect_security_block(handle);
                }
                break;
            case ECHO_REQUEST:
                l2cap_send_signaling_packet(handle, ECHO_RESPONSE, sig_id, 0, NULL);
                break;
            case INFORMATION_REQUEST:
                switch (infoType){
                    case 1: { // Connectionless MTU
                            uint16_t connectionless_mtu = hci_max_acl_data_packet_length();
                            l2cap_send_signaling_packet(handle, INFORMATION_RESPONSE, sig_id, infoType, 0, sizeof(connectionless_mtu), &connectionless_mtu);
                        }
                        break;
                    case 2: {  // Extended Features Supported
                            uint32_t features = l2cap_extended_features_mask();
                            l2cap_send_signaling_packet(handle, INFORMATION_RESPONSE, sig_id, infoType, 0, sizeof(features), &features);
                        }
                        break;
                    case 3: { // Fixed Channels Supported
                            uint8_t map[8];
                            memset(map, 0, 8);
                            map[0] = 0x06;  // L2CAP Signaling Channel (0x02) + Connectionless reception (0x04)
                            l2cap_send_signaling_packet(handle, INFORMATION_RESPONSE, sig_id, infoType, 0, sizeof(map), &map);
                        }
                        break;
                    default:
                        // all other types are not supported
                        l2cap_send_signaling_packet(handle, INFORMATION_RESPONSE, sig_id, infoType, 1, 0, NULL);
                        break;                        
                }
                break;
            case COMMAND_REJECT:
                l2cap_send_signaling_packet(handle, COMMAND_REJECT, sig_id, result, 0, NULL);
                break;
#endif
#ifdef ENABLE_BLE
            case LE_CREDIT_BASED_CONNECTION_REQUEST:
                l2cap_send_le_signaling_packet(handle, LE_CREDIT_BASED_CONNECTION_RESPONSE, sig_id, 0, 0, 0, 0, result);
                break;
            case COMMAND_REJECT_LE:
                l2cap_send_le_signaling_packet(handle, COMMAND_REJECT, sig_id, result, 0, NULL);
                break;
#endif
            default:
                // should not happen
                break;
        }
    }
    
    btstack_linked_list_iterator_t it;    
    UNUSED(it);

#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
    // send l2cap information request if neccessary
    hci_connections_get_iterator(&it);
    while(btstack_linked_list_iterator_has_next(&it)){
        hci_connection_t * connection = (hci_connection_t *) btstack_linked_list_iterator_next(&it);
        if (connection->l2cap_state.information_state == L2CAP_INFORMATION_STATE_W2_SEND_EXTENDED_FEATURE_REQUEST){
            connection->l2cap_state.information_state = L2CAP_INFORMATION_STATE_W4_EXTENDED_FEATURE_RESPONSE;
            // send information request for extended features
            uint8_t sig_id = l2cap_next_sig_id();
            uint8_t info_type = 2;
            l2cap_send_signaling_packet(connection->con_handle, INFORMATION_REQUEST, sig_id, info_type);
            return;
        }
    }
#endif

#ifdef ENABLE_CLASSIC
    uint8_t  config_options[10];
    btstack_linked_list_iterator_init(&it, &l2cap_channels);
    while (btstack_linked_list_iterator_has_next(&it)){

        l2cap_channel_t * channel = (l2cap_channel_t *) btstack_linked_list_iterator_next(&it);
        // log_info("l2cap_run: channel %p, state %u, var 0x%02x", channel, channel->state, channel->state_var);
        switch (channel->state){

            case L2CAP_STATE_WAIT_INCOMING_SECURITY_LEVEL_UPDATE:
            case L2CAP_STATE_WAIT_CLIENT_ACCEPT_OR_REJECT:
                if (!hci_can_send_acl_packet_now(channel->con_handle)) break;
                if (channel->state_var & L2CAP_CHANNEL_STATE_VAR_SEND_CONN_RESP_PEND) {
                    channelStateVarClearFlag(channel, L2CAP_CHANNEL_STATE_VAR_SEND_CONN_RESP_PEND);
                    l2cap_send_signaling_packet(channel->con_handle, CONNECTION_RESPONSE, channel->remote_sig_id, channel->local_cid, channel->remote_cid, 1, 0);
                }
                break;

            case L2CAP_STATE_WILL_SEND_CREATE_CONNECTION:
                if (!hci_can_send_command_packet_now()) break;
                // send connection request - set state first
                channel->state = L2CAP_STATE_WAIT_CONNECTION_COMPLETE;
                // BD_ADDR, Packet_Type, Page_Scan_Repetition_Mode, Reserved, Clock_Offset, Allow_Role_Switch
                hci_send_cmd(&hci_create_connection, channel->address, hci_usable_acl_packet_types(), 0, 0, 0, 1); 
                break;
                
            case L2CAP_STATE_WILL_SEND_CONNECTION_RESPONSE_DECLINE:
                if (!hci_can_send_acl_packet_now(channel->con_handle)) break;
                channel->state = L2CAP_STATE_INVALID;
                l2cap_send_signaling_packet(channel->con_handle, CONNECTION_RESPONSE, channel->remote_sig_id, channel->local_cid, channel->remote_cid, channel->reason, 0);
                // discard channel - l2cap_finialize_channel_close without sending l2cap close event
                l2cap_stop_rtx(channel);
                btstack_linked_list_iterator_remove(&it);
                btstack_memory_l2cap_channel_free(channel); 
                break;
                
            case L2CAP_STATE_WILL_SEND_CONNECTION_RESPONSE_ACCEPT:
                if (!hci_can_send_acl_packet_now(channel->con_handle)) break;
                channel->state = L2CAP_STATE_CONFIG;
                channelStateVarSetFlag(channel, L2CAP_CHANNEL_STATE_VAR_SEND_CONF_REQ);
                l2cap_send_signaling_packet(channel->con_handle, CONNECTION_RESPONSE, channel->remote_sig_id, channel->local_cid, channel->remote_cid, 0, 0);
                break;
                
            case L2CAP_STATE_WILL_SEND_CONNECTION_REQUEST:
                if (!hci_can_send_acl_packet_now(channel->con_handle)) break;
                // success, start l2cap handshake
                channel->local_sig_id = l2cap_next_sig_id();
                channel->state = L2CAP_STATE_WAIT_CONNECT_RSP;
                l2cap_send_signaling_packet( channel->con_handle, CONNECTION_REQUEST, channel->local_sig_id, channel->psm, channel->local_cid);
                l2cap_start_rtx(channel);
                break;
            
            case L2CAP_STATE_CONFIG:
                if (!hci_can_send_acl_packet_now(channel->con_handle)) break;
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
                        l2cap_send_signaling_packet(channel->con_handle, CONFIGURE_RESPONSE, channel->remote_sig_id, channel->remote_cid, flags, L2CAP_CONF_RESULT_UNKNOWN_OPTIONS, 0, NULL);
#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
                    } else if (channel->state_var & L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP_REJECTED){
                        channelStateVarClearFlag(channel,L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP_REJECTED);
                        channelStateVarClearFlag(channel, L2CAP_CHANNEL_STATE_VAR_SENT_CONF_RSP);
                        uint16_t options_size = l2cap_setup_options(channel, config_options);
                        l2cap_send_signaling_packet(channel->con_handle, CONFIGURE_RESPONSE, channel->remote_sig_id, channel->remote_cid, flags, L2CAP_CONF_RESULT_UNACCEPTABLE_PARAMETERS, options_size, &config_options);
#endif
                    } else if (channel->state_var & L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP_MTU){
                        channelStateVarClearFlag(channel,L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP_MTU);
                        uint16_t options_size = l2cap_setup_options(channel, config_options);
                        l2cap_send_signaling_packet(channel->con_handle, CONFIGURE_RESPONSE, channel->remote_sig_id, channel->remote_cid, flags, L2CAP_CONF_RESULT_SUCCESS, options_size, &config_options);
                    } else {
                        l2cap_send_signaling_packet(channel->con_handle, CONFIGURE_RESPONSE, channel->remote_sig_id, channel->remote_cid, flags, L2CAP_CONF_RESULT_SUCCESS, 0, NULL);
                    }
                    channelStateVarClearFlag(channel, L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP_CONT);
                }
                else if (channel->state_var & L2CAP_CHANNEL_STATE_VAR_SEND_CONF_REQ){
                    channelStateVarClearFlag(channel, L2CAP_CHANNEL_STATE_VAR_SEND_CONF_REQ);
                    channelStateVarSetFlag(channel, L2CAP_CHANNEL_STATE_VAR_SENT_CONF_REQ);
                    channel->local_sig_id = l2cap_next_sig_id();
                    uint16_t options_size = l2cap_setup_options(channel, config_options);
                    l2cap_send_signaling_packet(channel->con_handle, CONFIGURE_REQUEST, channel->local_sig_id, channel->remote_cid, 0, options_size, &config_options);
                    l2cap_start_rtx(channel);
                }
                if (l2cap_channel_ready_for_open(channel)){
                    channel->state = L2CAP_STATE_OPEN;
                    l2cap_emit_channel_opened(channel, 0);  // success
                }
                break;

            case L2CAP_STATE_WILL_SEND_DISCONNECT_RESPONSE:
                if (!hci_can_send_acl_packet_now(channel->con_handle)) break;
                channel->state = L2CAP_STATE_INVALID;
                l2cap_send_signaling_packet( channel->con_handle, DISCONNECTION_RESPONSE, channel->remote_sig_id, channel->local_cid, channel->remote_cid);   
                // we don't start an RTX timer for a disconnect - there's no point in closing the channel if the other side doesn't respond :)
                l2cap_finialize_channel_close(channel);  // -- remove from list
                break;
                
            case L2CAP_STATE_WILL_SEND_DISCONNECT_REQUEST:
                if (!hci_can_send_acl_packet_now(channel->con_handle)) break;
                channel->local_sig_id = l2cap_next_sig_id();
                channel->state = L2CAP_STATE_WAIT_DISCONNECT;
                l2cap_send_signaling_packet( channel->con_handle, DISCONNECTION_REQUEST, channel->local_sig_id, channel->remote_cid, channel->local_cid);   
                break;
            default:
                break;
        }

#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
        // send s-frame to acknowledge received packets
        if (!hci_can_send_acl_packet_now(channel->con_handle)) continue;

        if (channel->tx_send_index != channel->tx_write_index){
            int unacknowledged_packets = l2cap_ertm_num_unacknowledged_tx_packets(channel);
            // check remote tx window
            log_info("unacknowledged_packets %u, remote tx window size %u", unacknowledged_packets, channel->remote_tx_window_size);
            if (unacknowledged_packets < channel->remote_tx_window_size){
                int index = channel->tx_send_index;
                channel->tx_send_index++;
                if (channel->tx_send_index >= channel->num_tx_buffers){
                    channel->tx_send_index = 0;          
                }
                l2cap_ertm_send_information_frame(channel, index);
                continue;
            }
        }

        if (channel->send_supervisor_frame_receiver_ready){
            channel->send_supervisor_frame_receiver_ready = 0;;
            log_info("Send S-Frame: RR %u", channel->req_seq);
            uint16_t control = l2cap_encanced_control_field_for_supevisor_frame( L2CAP_SUPERVISORY_FUNCTION_RR_RECEIVER_READY, 0, 0, channel->req_seq);
            l2cap_ertm_send_supervisor_frame(channel, control);
            continue;
        }
#endif

    }
#endif

#ifdef ENABLE_LE_DATA_CHANNELS
    btstack_linked_list_iterator_init(&it, &l2cap_le_channels);
    while (btstack_linked_list_iterator_has_next(&it)){
        uint8_t  * acl_buffer;
        uint8_t  * l2cap_payload;
        uint16_t pos;
        uint16_t payload_size;
        l2cap_channel_t * channel = (l2cap_channel_t *) btstack_linked_list_iterator_next(&it);
        // log_info("l2cap_run: channel %p, state %u, var 0x%02x", channel, channel->state, channel->state_var);
        switch (channel->state){
            case L2CAP_STATE_WILL_SEND_LE_CONNECTION_REQUEST:
                if (!hci_can_send_acl_packet_now(channel->con_handle)) break;
                channel->state = L2CAP_STATE_WAIT_LE_CONNECTION_RESPONSE;
                // le psm, source cid, mtu, mps, initial credits
                channel->local_sig_id = l2cap_next_sig_id();
                channel->credits_incoming =  channel->new_credits_incoming;
                channel->new_credits_incoming = 0;
                l2cap_send_le_signaling_packet( channel->con_handle, LE_CREDIT_BASED_CONNECTION_REQUEST, channel->local_sig_id, channel->psm, channel->local_cid, channel->local_mtu, 23, channel->credits_incoming);
                break;
            case L2CAP_STATE_WILL_SEND_LE_CONNECTION_RESPONSE_ACCEPT:
                if (!hci_can_send_acl_packet_now(channel->con_handle)) break;
                // TODO: support larger MPS
                channel->state = L2CAP_STATE_OPEN;
                channel->credits_incoming =  channel->new_credits_incoming;
                channel->new_credits_incoming = 0;
                l2cap_send_le_signaling_packet(channel->con_handle, LE_CREDIT_BASED_CONNECTION_RESPONSE, channel->remote_sig_id, channel->local_cid, channel->local_mtu, 23, channel->credits_incoming, 0);
                // notify client
                l2cap_emit_le_channel_opened(channel, 0);
                break;                       
            case L2CAP_STATE_WILL_SEND_LE_CONNECTION_RESPONSE_DECLINE:
                if (!hci_can_send_acl_packet_now(channel->con_handle)) break;
                channel->state = L2CAP_STATE_INVALID;
                l2cap_send_le_signaling_packet(channel->con_handle, LE_CREDIT_BASED_CONNECTION_RESPONSE, channel->remote_sig_id, 0, 0, 0, 0, channel->reason);
                // discard channel - l2cap_finialize_channel_close without sending l2cap close event
                l2cap_stop_rtx(channel);
                btstack_linked_list_iterator_remove(&it);
                btstack_memory_l2cap_channel_free(channel);
                break;
            case L2CAP_STATE_OPEN:
                if (!hci_can_send_acl_packet_now(channel->con_handle)) break;

                // send credits
                if (channel->new_credits_incoming){
                    log_info("l2cap: sending %u credits", channel->new_credits_incoming);
                    channel->local_sig_id = l2cap_next_sig_id();
                    uint16_t new_credits = channel->new_credits_incoming;
                    channel->new_credits_incoming = 0;
                    channel->credits_incoming += new_credits;
                    l2cap_send_le_signaling_packet(channel->con_handle, LE_FLOW_CONTROL_CREDIT, channel->local_sig_id, channel->remote_cid, new_credits);
                    break;
                }

                // send data
                if (!channel->send_sdu_buffer) break;
                if (!channel->credits_outgoing) break;

                // send part of SDU
                hci_reserve_packet_buffer();
                acl_buffer = hci_get_outgoing_packet_buffer();
                l2cap_payload = acl_buffer + 8;
                pos = 0;
                if (!channel->send_sdu_pos){
                    // store SDU len
                    channel->send_sdu_pos += 2;
                    little_endian_store_16(l2cap_payload, pos, channel->send_sdu_len);
                    pos += 2;
                }
                payload_size = btstack_min(channel->send_sdu_len + 2 - channel->send_sdu_pos, channel->remote_mps - pos);
                log_info("len %u, pos %u => payload %u, credits %u", channel->send_sdu_len, channel->send_sdu_pos, payload_size, channel->credits_outgoing);
                memcpy(&l2cap_payload[pos], &channel->send_sdu_buffer[channel->send_sdu_pos-2], payload_size); // -2 for virtual SDU len
                pos += payload_size;
                channel->send_sdu_pos += payload_size;
                l2cap_setup_header(acl_buffer, channel->con_handle, 0, channel->remote_cid, pos);
                // done

                channel->credits_outgoing--;

                if (channel->send_sdu_pos >= channel->send_sdu_len + 2){
                    channel->send_sdu_buffer = NULL;
                    // send done event
                    l2cap_emit_simple_event_with_cid(channel, L2CAP_EVENT_LE_PACKET_SENT);
                    // inform about can send now
                    l2cap_le_notify_channel_can_send(channel);
                }
                hci_send_acl_packet_buffer(8 + pos);
                break;
            case L2CAP_STATE_WILL_SEND_DISCONNECT_REQUEST:
                if (!hci_can_send_acl_packet_now(channel->con_handle)) break;
                channel->local_sig_id = l2cap_next_sig_id();
                channel->state = L2CAP_STATE_WAIT_DISCONNECT;
                l2cap_send_le_signaling_packet( channel->con_handle, DISCONNECTION_REQUEST, channel->local_sig_id, channel->remote_cid, channel->local_cid);   
                break;
            case L2CAP_STATE_WILL_SEND_DISCONNECT_RESPONSE:
                if (!hci_can_send_acl_packet_now(channel->con_handle)) break;
                channel->state = L2CAP_STATE_INVALID;
                l2cap_send_le_signaling_packet( channel->con_handle, DISCONNECTION_RESPONSE, channel->remote_sig_id, channel->local_cid, channel->remote_cid);   
                l2cap_le_finialize_channel_close(channel);  // -- remove from list
                break;
            default:
                break;
        }
    }
#endif

#ifdef ENABLE_BLE
    // send l2cap con paramter update if necessary
    hci_connections_get_iterator(&it);
    while(btstack_linked_list_iterator_has_next(&it)){
        hci_connection_t * connection = (hci_connection_t *) btstack_linked_list_iterator_next(&it);
        if (connection->address_type != BD_ADDR_TYPE_LE_PUBLIC && connection->address_type != BD_ADDR_TYPE_LE_RANDOM) continue;
        if (!hci_can_send_acl_packet_now(connection->con_handle)) continue;
        switch (connection->le_con_parameter_update_state){
            case CON_PARAMETER_UPDATE_SEND_REQUEST:
                connection->le_con_parameter_update_state = CON_PARAMETER_UPDATE_NONE;
                l2cap_send_le_signaling_packet(connection->con_handle, CONNECTION_PARAMETER_UPDATE_REQUEST, connection->le_con_param_update_identifier,
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

    // log_info("l2cap_run: exit");
}

#ifdef ENABLE_CLASSIC
static void l2cap_handle_connection_complete(hci_con_handle_t con_handle, l2cap_channel_t * channel){
    if (channel->state == L2CAP_STATE_WAIT_CONNECTION_COMPLETE || channel->state == L2CAP_STATE_WILL_SEND_CREATE_CONNECTION) {
        log_info("l2cap_handle_connection_complete expected state");
        // success, start l2cap handshake
        channel->con_handle = con_handle;
        // check remote SSP feature first
        channel->state = L2CAP_STATE_WAIT_REMOTE_SUPPORTED_FEATURES;
    }
}

static void l2cap_ready_to_connect(l2cap_channel_t * channel){

#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
    // assumption: outgoing connection
    hci_connection_t * connection = hci_connection_for_handle(channel->con_handle);
    if (connection->l2cap_state.information_state == L2CAP_INFORMATION_STATE_IDLE){
        connection->l2cap_state.information_state = L2CAP_INFORMATION_STATE_W2_SEND_EXTENDED_FEATURE_REQUEST;        
        channel->state = L2CAP_STATE_WAIT_OUTGOING_EXTENDED_FEATURES;
        return;
    }
#endif

    // fine, go ahead
    channel->state = L2CAP_STATE_WILL_SEND_CONNECTION_REQUEST;
}

static void l2cap_handle_remote_supported_features_received(l2cap_channel_t * channel){
    if (channel->state != L2CAP_STATE_WAIT_REMOTE_SUPPORTED_FEATURES) return;

    // we have been waiting for remote supported features, if both support SSP, 
    log_info("l2cap received remote supported features, sec_level_0_allowed for psm %u = %u", channel->psm, l2cap_security_level_0_allowed_for_PSM(channel->psm));
    if (gap_ssp_supported_on_both_sides(channel->con_handle) && !l2cap_security_level_0_allowed_for_PSM(channel->psm)){
        // request security level 2
        channel->state = L2CAP_STATE_WAIT_OUTGOING_SECURITY_LEVEL_UPDATE;
        gap_request_security_level(channel->con_handle, LEVEL_2);
        return;
    }

    l2cap_ready_to_connect(channel);
}
#endif

#ifdef L2CAP_USES_CHANNELS
static l2cap_channel_t * l2cap_create_channel_entry(btstack_packet_handler_t packet_handler, bd_addr_t address, bd_addr_type_t address_type, 
    uint16_t psm, uint16_t local_mtu, gap_security_level_t security_level){

    l2cap_channel_t * channel = btstack_memory_l2cap_channel_get();
    if (!channel) {
        return NULL;
    }

     // Init memory (make valgrind happy)
    memset(channel, 0, sizeof(l2cap_channel_t));
        
    // fill in 
    channel->packet_handler = packet_handler;
    bd_addr_copy(channel->address, address);
    channel->address_type = address_type;
    channel->psm = psm;
    channel->local_mtu  = local_mtu;
    channel->remote_mtu = L2CAP_MINIMAL_MTU;
    channel->required_security_level = security_level;

    // 
    channel->local_cid = l2cap_next_local_cid();
    channel->con_handle = 0;

    // set initial state
    channel->state = L2CAP_STATE_WILL_SEND_CREATE_CONNECTION;
    channel->state_var = L2CAP_CHANNEL_STATE_VAR_NONE;
    channel->remote_sig_id = L2CAP_SIG_ID_INVALID;
    channel->local_sig_id = L2CAP_SIG_ID_INVALID;

    return channel;
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
    // limit MTU to the size of our outtgoing HCI buffer
    uint16_t local_mtu = btstack_min(mtu, l2cap_max_mtu());

    log_info("L2CAP_CREATE_CHANNEL addr %s psm 0x%x mtu %u -> local mtu %u", bd_addr_to_str(address), psm, mtu, local_mtu);

    l2cap_channel_t * channel = l2cap_create_channel_entry(channel_packet_handler, address, BD_ADDR_TYPE_CLASSIC, psm, local_mtu, LEVEL_0);
    if (!channel) {
        return BTSTACK_MEMORY_ALLOC_FAILED;
    }

#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
    channel->mode = L2CAP_CHANNEL_MODE_BASIC;
#endif    

    // add to connections list
    btstack_linked_list_add(&l2cap_channels, (btstack_linked_item_t *) channel);

    // store local_cid
    if (out_local_cid){
       *out_local_cid = channel->local_cid;
    }

    // check if hci connection is already usable
    hci_connection_t * conn = hci_connection_for_bd_addr_and_type(address, BD_ADDR_TYPE_CLASSIC);
    if (conn){
        log_info("l2cap_create_channel, hci connection already exists");
        l2cap_handle_connection_complete(conn->con_handle, channel);
        // check if remote supported fearures are already received
        if (conn->bonding_flags & BONDING_RECEIVED_REMOTE_FEATURES) {
            l2cap_handle_remote_supported_features_received(channel);
        }
    }

    l2cap_run();

    return 0;
}

#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE

static uint8_t l2cap_ertm_validate_local_config(uint8_t max_transmit, uint16_t retransmission_timeout_ms,
    uint16_t monitor_timeout_ms, uint8_t num_tx_buffers, uint8_t num_rx_buffers, uint8_t * buffer, uint32_t size){
    UNUSED(buffer);
    UNUSED(size);

    uint8_t result = ERROR_CODE_SUCCESS;
    if (max_transmit < 1){
        log_error("max_transmit must be >= 1");
        result = ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }
    if (retransmission_timeout_ms < 2000){
        log_error("retransmission_timeout_ms must be >= 2000 ms");
        result = ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }
    if (monitor_timeout_ms < 12000){
        log_error("monitor_timeout_ms must be >= 12000 ms");
        result = ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }
    if (num_rx_buffers < 1){
        log_error("num_rx_buffers must be >= 1");
        result = ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }
    if (num_tx_buffers < 1){
        log_error("num_rx_buffers must be >= 1");
        result = ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }
    return result;
}

static void l2cap_ertm_configure_channel(l2cap_channel_t * channel, int ertm_mandatory, uint8_t max_transmit,
    uint16_t retransmission_timeout_ms, uint16_t monitor_timeout_ms, uint8_t num_tx_buffers, uint8_t num_rx_buffers, uint8_t * buffer, uint32_t size){

    channel->mode  = L2CAP_CHANNEL_MODE_ENHANCED_RETRANSMISSION;
    channel->ertm_mandatory = ertm_mandatory;
    channel->local_max_transmit = max_transmit;
    channel->local_retransmission_timeout_ms = retransmission_timeout_ms;
    channel->local_monitor_timeout_ms = monitor_timeout_ms;
    channel->num_rx_buffers = num_rx_buffers;
    channel->num_tx_buffers = num_tx_buffers;

    // TODO: align buffer pointer
    uint32_t pos = 0;
    channel->rx_packets_state = (l2cap_ertm_rx_packet_state_t *) &buffer[pos];
    pos += num_rx_buffers * sizeof(l2cap_ertm_rx_packet_state_t);
    channel->tx_packets_state = (l2cap_ertm_tx_packet_state_t *) &buffer[pos];
    pos += num_tx_buffers * sizeof(l2cap_ertm_tx_packet_state_t);
    // calculate MTU
    channel->local_mtu = (size - pos) / (num_rx_buffers + num_tx_buffers);
    log_info("Local ERTM MTU: %u", channel->local_mtu);
    channel->rx_packets_data = &buffer[pos];
    pos += num_rx_buffers * channel->local_mtu;
    channel->tx_packets_data = &buffer[pos];
    log_info("RX packets %p, TX packets %p", channel->rx_packets_data, channel->tx_packets_data);
}

uint8_t l2cap_create_ertm_channel(btstack_packet_handler_t packet_handler, bd_addr_t address, uint16_t psm, 
    int ertm_mandatory, uint8_t max_transmit, uint16_t retransmission_timeout_ms, uint16_t monitor_timeout_ms,
    uint8_t num_tx_buffers, uint8_t num_rx_buffers, uint8_t * buffer, uint32_t size, uint16_t * out_local_cid){

    // limit MTU to the size of our outtgoing HCI buffer
    uint16_t local_mtu = l2cap_max_mtu();

    log_info("L2CAP_CREATE_CHANNEL addr %s psm 0x%x -> local mtu %u", bd_addr_to_str(address), psm, local_mtu);

    // validate local config
    uint8_t result = l2cap_ertm_validate_local_config(max_transmit, retransmission_timeout_ms, monitor_timeout_ms, num_tx_buffers, num_rx_buffers, buffer, size);
    if (result) return result;

    l2cap_channel_t * channel = l2cap_create_channel_entry(packet_handler, address, BD_ADDR_TYPE_CLASSIC, psm, local_mtu, LEVEL_0);
    if (!channel) {
        return BTSTACK_MEMORY_ALLOC_FAILED;
    }

    // configure ERTM
    l2cap_ertm_configure_channel(channel, ertm_mandatory, max_transmit, retransmission_timeout_ms,
        monitor_timeout_ms, num_tx_buffers, num_rx_buffers, buffer, size);

    // add to connections list
    btstack_linked_list_add(&l2cap_channels, (btstack_linked_item_t *) channel);

    // store local_cid
    if (out_local_cid){
       *out_local_cid = channel->local_cid;
    }

    // check if hci connection is already usable
    hci_connection_t * conn = hci_connection_for_bd_addr_and_type(address, BD_ADDR_TYPE_CLASSIC);
    if (conn){
        log_info("l2cap_create_channel, hci connection already exists");
        l2cap_handle_connection_complete(conn->con_handle, channel);
        // check if remote supported fearures are already received
        if (conn->bonding_flags & BONDING_RECEIVED_REMOTE_FEATURES) {
            l2cap_handle_remote_supported_features_received(channel);
        }
    }

    l2cap_run(); 

    return 0;     
}
#endif

void 
l2cap_disconnect(uint16_t local_cid, uint8_t reason){
    log_info("L2CAP_DISCONNECT local_cid 0x%x reason 0x%x", local_cid, reason);
    // find channel for local_cid
    l2cap_channel_t * channel = l2cap_get_channel_for_local_cid(local_cid);
    if (channel) {
        channel->state = L2CAP_STATE_WILL_SEND_DISCONNECT_REQUEST;
    }
    // process
    l2cap_run();
}

static void l2cap_handle_connection_failed_for_addr(bd_addr_t address, uint8_t status){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &l2cap_channels);
    while (btstack_linked_list_iterator_has_next(&it)){
        l2cap_channel_t * channel = (l2cap_channel_t *) btstack_linked_list_iterator_next(&it);
        if ( bd_addr_cmp( channel->address, address) != 0) continue;
        // channel for this address found
        switch (channel->state){
            case L2CAP_STATE_WAIT_CONNECTION_COMPLETE:
            case L2CAP_STATE_WILL_SEND_CREATE_CONNECTION:
                // failure, forward error code
                l2cap_emit_channel_opened(channel, status);
                // discard channel
                l2cap_stop_rtx(channel);
                btstack_linked_list_iterator_remove(&it);
                btstack_memory_l2cap_channel_free(channel);
                break;
            default:
                break;               
        }
    }
}

static void l2cap_handle_connection_success_for_addr(bd_addr_t address, hci_con_handle_t handle){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &l2cap_channels);
    while (btstack_linked_list_iterator_has_next(&it)){
        l2cap_channel_t * channel = (l2cap_channel_t *) btstack_linked_list_iterator_next(&it);
        if ( ! bd_addr_cmp( channel->address, address) ){
            l2cap_handle_connection_complete(handle, channel);
        }
    }
    // process
    l2cap_run();
}
#endif

static void l2cap_notify_channel_can_send(void){

#ifdef ENABLE_CLASSIC
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &l2cap_channels);
    while (btstack_linked_list_iterator_has_next(&it)){
        l2cap_channel_t * channel = (l2cap_channel_t *) btstack_linked_list_iterator_next(&it);
        if (!channel->waiting_for_can_send_now) continue;
        if (!hci_can_send_acl_packet_now(channel->con_handle)) continue;
        channel->waiting_for_can_send_now = 0;
        l2cap_emit_can_send_now(channel->packet_handler, channel->local_cid);
    }
#endif

    int i;
    for (i=0;i<L2CAP_FIXED_CHANNEL_TABLE_SIZE;i++){
        if (!fixed_channels[i].callback) continue;
        if (!fixed_channels[i].waiting_for_can_send_now) continue;
        int can_send = 0;
        if (l2cap_fixed_channel_table_index_is_le(i)){
#ifdef ENABLE_BLE
            can_send = hci_can_send_acl_le_packet_now();
#endif
        } else {
#ifdef ENABLE_CLASSIC
            can_send = hci_can_send_acl_classic_packet_now();
#endif
        } 
        if (!can_send) continue;
        fixed_channels[i].waiting_for_can_send_now = 0;
        l2cap_emit_can_send_now(fixed_channels[i].callback, l2cap_fixed_channel_table_channel_id_for_index(i));
    }
}

static void l2cap_hci_event_handler(uint8_t packet_type, uint16_t cid, uint8_t *packet, uint16_t size){

    UNUSED(packet_type);
    UNUSED(cid);
    UNUSED(size);
    
    bd_addr_t address;
    hci_con_handle_t handle;
    int hci_con_used;
    btstack_linked_list_iterator_t it;

    // avoid unused warnings
    UNUSED(address);
    UNUSED(hci_con_used);
    UNUSED(it);
    UNUSED(handle);
    
    switch(hci_event_packet_get_type(packet)){
            
        // Notify channel packet handler if they can send now
        case HCI_EVENT_TRANSPORT_PACKET_SENT:
        case HCI_EVENT_NUMBER_OF_COMPLETED_PACKETS:
            l2cap_run();    // try sending signaling packets first
            l2cap_notify_channel_can_send();
            break;

        case HCI_EVENT_COMMAND_STATUS:
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
            if (HCI_EVENT_IS_COMMAND_COMPLETE(packet, hci_create_connection_cancel)) {
                if (packet[5] == 0){
                    reverse_bd_addr(&packet[6], address);
                    // CONNECTION TERMINATED BY LOCAL HOST (0X16)
                    l2cap_handle_connection_failed_for_addr(address, 0x16);
                }
            }
            l2cap_run();    // try sending signaling packets first
            break;
#endif
            
        // handle disconnection complete events
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            // send l2cap disconnect events for all channels on this handle and free them
#ifdef ENABLE_CLASSIC
            handle = little_endian_read_16(packet, 3);
            btstack_linked_list_iterator_init(&it, &l2cap_channels);
            while (btstack_linked_list_iterator_has_next(&it)){
                l2cap_channel_t * channel = (l2cap_channel_t *) btstack_linked_list_iterator_next(&it);
                if (channel->con_handle != handle) continue;
                l2cap_emit_channel_closed(channel);
                l2cap_stop_rtx(channel);
                btstack_linked_list_iterator_remove(&it);
                btstack_memory_l2cap_channel_free(channel);
            }
#endif
#ifdef ENABLE_LE_DATA_CHANNELS
            handle = little_endian_read_16(packet, 3);
            btstack_linked_list_iterator_init(&it, &l2cap_le_channels);
            while (btstack_linked_list_iterator_has_next(&it)){
                l2cap_channel_t * channel = (l2cap_channel_t *) btstack_linked_list_iterator_next(&it);
                if (channel->con_handle != handle) continue;
                l2cap_emit_channel_closed(channel);
                btstack_linked_list_iterator_remove(&it);
                btstack_memory_l2cap_channel_free(channel);
            }
#endif
            break;
            
        // HCI Connection Timeouts
#ifdef ENABLE_CLASSIC
        case L2CAP_EVENT_TIMEOUT_CHECK:
            handle = little_endian_read_16(packet, 2);
            if (gap_get_connection_type(handle) != GAP_CONNECTION_ACL) break;
            if (hci_authentication_active_for_handle(handle)) break;
            hci_con_used = 0;
            btstack_linked_list_iterator_init(&it, &l2cap_channels);
            while (btstack_linked_list_iterator_has_next(&it)){
                l2cap_channel_t * channel = (l2cap_channel_t *) btstack_linked_list_iterator_next(&it);
                if (channel->con_handle != handle) continue;
                hci_con_used = 1;
                break;
            }
            if (hci_con_used) break;
            if (!hci_can_send_command_packet_now()) break;
            hci_send_cmd(&hci_disconnect, handle, 0x13); // remote closed connection             
            break;

        case HCI_EVENT_READ_REMOTE_SUPPORTED_FEATURES_COMPLETE:
            handle = little_endian_read_16(packet, 3);
            btstack_linked_list_iterator_init(&it, &l2cap_channels);
            while (btstack_linked_list_iterator_has_next(&it)){
                l2cap_channel_t * channel = (l2cap_channel_t *) btstack_linked_list_iterator_next(&it);
                if (channel->con_handle != handle) continue;
                l2cap_handle_remote_supported_features_received(channel);
                break;
            }
            break;           

        case GAP_EVENT_SECURITY_LEVEL:
            handle = little_endian_read_16(packet, 2);
            log_info("l2cap - security level update");
            btstack_linked_list_iterator_init(&it, &l2cap_channels);
            while (btstack_linked_list_iterator_has_next(&it)){
                l2cap_channel_t * channel = (l2cap_channel_t *) btstack_linked_list_iterator_next(&it);
                if (channel->con_handle != handle) continue;

                log_info("l2cap - state %u", channel->state);

                gap_security_level_t actual_level = (gap_security_level_t) packet[4];
                gap_security_level_t required_level = channel->required_security_level;

                switch (channel->state){
                    case L2CAP_STATE_WAIT_INCOMING_SECURITY_LEVEL_UPDATE:
                        if (actual_level >= required_level){
#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
                            // we need to know if ERTM is supported before sending a config response
                            hci_connection_t * connection = hci_connection_for_handle(channel->con_handle);
                            connection->l2cap_state.information_state = L2CAP_INFORMATION_STATE_W2_SEND_EXTENDED_FEATURE_REQUEST;        
                            channel->state = L2CAP_STATE_WAIT_INCOMING_EXTENDED_FEATURES;
#else
                            channel->state = L2CAP_STATE_WAIT_CLIENT_ACCEPT_OR_REJECT;
                            l2cap_emit_incoming_connection(channel);
#endif
                        } else {
                            channel->reason = 0x0003; // security block
                            channel->state = L2CAP_STATE_WILL_SEND_CONNECTION_RESPONSE_DECLINE;
                        }
                        break;

                    case L2CAP_STATE_WAIT_OUTGOING_SECURITY_LEVEL_UPDATE:
                        if (actual_level >= required_level){
                            l2cap_ready_to_connect(channel);
                        } else {
                            // disconnnect, authentication not good enough
                            hci_disconnect_security_block(handle);
                        }
                        break;

                    default:
                        break;
                } 
            }
            break;
#endif

        default:
            break;
    }
    
    l2cap_run();
}

static void l2cap_register_signaling_response(hci_con_handle_t handle, uint8_t code, uint8_t sig_id, uint16_t cid, uint16_t data){
    // Vol 3, Part A, 4.3: "The DCID and SCID fields shall be ignored when the result field indi- cates the connection was refused."
    if (signaling_responses_pending < NR_PENDING_SIGNALING_RESPONSES) {
        signaling_responses[signaling_responses_pending].handle = handle;
        signaling_responses[signaling_responses_pending].code = code;
        signaling_responses[signaling_responses_pending].sig_id = sig_id;
        signaling_responses[signaling_responses_pending].cid = cid;
        signaling_responses[signaling_responses_pending].data = data;
        signaling_responses_pending++;
        l2cap_run();
    }
}

#ifdef ENABLE_CLASSIC
static void l2cap_handle_disconnect_request(l2cap_channel_t *channel, uint16_t identifier){
    channel->remote_sig_id = identifier;
    channel->state = L2CAP_STATE_WILL_SEND_DISCONNECT_RESPONSE;
    l2cap_run();
}

static void l2cap_handle_connection_request(hci_con_handle_t handle, uint8_t sig_id, uint16_t psm, uint16_t source_cid){
    
    // log_info("l2cap_handle_connection_request for handle %u, psm %u cid 0x%02x", handle, psm, source_cid);
    l2cap_service_t *service = l2cap_get_service(psm);
    if (!service) {
        // 0x0002 PSM not supported
        l2cap_register_signaling_response(handle, CONNECTION_REQUEST, sig_id, source_cid, 0x0002);
        return;
    }
    
    hci_connection_t * hci_connection = hci_connection_for_handle( handle );
    if (!hci_connection) {
        // 
        log_error("no hci_connection for handle %u", handle);
        return;
    }

    // alloc structure
    // log_info("l2cap_handle_connection_request register channel");
    l2cap_channel_t * channel = l2cap_create_channel_entry(service->packet_handler, hci_connection->address, BD_ADDR_TYPE_CLASSIC, 
    psm, service->mtu, service->required_security_level);
    if (!channel){
        // 0x0004 No resources available
        l2cap_register_signaling_response(handle, CONNECTION_REQUEST, sig_id, source_cid, 0x0004);
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
    channel->state =      L2CAP_STATE_WAIT_INCOMING_SECURITY_LEVEL_UPDATE;
    channel->state_var  = (L2CAP_CHANNEL_STATE_VAR) (L2CAP_CHANNEL_STATE_VAR_SEND_CONN_RESP_PEND | L2CAP_CHANNEL_STATE_VAR_INCOMING);
    
    // add to connections list
    btstack_linked_list_add(&l2cap_channels, (btstack_linked_item_t *) channel);

    // assert security requirements
    gap_request_security_level(handle, channel->required_security_level);
}

void l2cap_accept_connection(uint16_t local_cid){
    log_info("L2CAP_ACCEPT_CONNECTION local_cid 0x%x", local_cid);
    l2cap_channel_t * channel = l2cap_get_channel_for_local_cid(local_cid);
    if (!channel) {
        log_error("l2cap_accept_connection called but local_cid 0x%x not found", local_cid);
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


#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
uint8_t l2cap_accept_ertm_connection(uint16_t local_cid, int ertm_mandatory, uint8_t max_transmit,
    uint16_t retransmission_timeout_ms, uint16_t monitor_timeout_ms, uint8_t num_tx_buffers, uint8_t num_rx_buffers, uint8_t * buffer, uint32_t size){

    log_info("L2CAP_ACCEPT_ERTM_CONNECTION local_cid 0x%x", local_cid);
    l2cap_channel_t * channel = l2cap_get_channel_for_local_cid(local_cid);
    if (!channel) {
        log_error("l2cap_accept_connection called but local_cid 0x%x not found", local_cid);
        return L2CAP_LOCAL_CID_DOES_NOT_EXIST;
    }

    // validate local config
    uint8_t result = l2cap_ertm_validate_local_config(max_transmit, retransmission_timeout_ms, monitor_timeout_ms, num_tx_buffers, num_rx_buffers, buffer, size);
    if (result) return result;

    // configure L2CAP ERTM
    l2cap_ertm_configure_channel(channel, ertm_mandatory, max_transmit, retransmission_timeout_ms, monitor_timeout_ms, num_tx_buffers, num_rx_buffers, buffer, size);

    // continue
    channel->state = L2CAP_STATE_WILL_SEND_CONNECTION_RESPONSE_ACCEPT;

    // process
    l2cap_run();

    return ERROR_CODE_SUCCESS;
}

static void l2cap_ertm_handle_req_seq(l2cap_channel_t * l2cap_channel, uint8_t req_seq){
    l2cap_ertm_tx_packet_state_t * tx_state;
    tx_state = &l2cap_channel->tx_packets_state[l2cap_channel->tx_read_index];
    if ( ((req_seq - 1) & 0x3f) == tx_state->tx_seq){
        log_info("RR seq %u == seq of oldest tx packet -> packet done", req_seq);
        l2cap_channel->tx_read_index++;
        if (l2cap_channel->tx_read_index >= l2cap_channel->num_rx_buffers){
            l2cap_channel->tx_read_index = 0;
        }
    } else {
        log_info("RR seq %u != seq of oldest tx packet %u ???", req_seq, tx_state->tx_seq);
    }
}                                
#endif


void l2cap_decline_connection(uint16_t local_cid){
    log_info("L2CAP_DECLINE_CONNECTION local_cid 0x%x", local_cid);
    l2cap_channel_t * channel = l2cap_get_channel_for_local_cid( local_cid);
    if (!channel) {
        log_error( "l2cap_decline_connection called but local_cid 0x%x not found", local_cid);
        return;
    }
    channel->state  = L2CAP_STATE_WILL_SEND_CONNECTION_RESPONSE_DECLINE;
    channel->reason = 0x04; // no resources available
    l2cap_run();
}

static void l2cap_signaling_handle_configure_request(l2cap_channel_t *channel, uint8_t *command){

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
        log_info("l2cap cid %u, hint %u, type %u", channel->local_cid, option_hint, option_type);
        pos++;
        uint8_t length = command[pos++];
        // MTU { type(8): 1, len(8):2, MTU(16) }
        if (option_type == 1 && length == 2){
            channel->remote_mtu = little_endian_read_16(command, pos);
            if (channel->remote_mtu > l2cap_max_mtu()){
                log_info("Remote MTU %u larger than outgoing buffer, only using MTU = %u", channel->remote_mtu, l2cap_max_mtu());
                channel->remote_mtu = l2cap_max_mtu();
            }
            channelStateVarSetFlag(channel, L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP_MTU);
        }
        // Flush timeout { type(8):2, len(8): 2, Flush Timeout(16)}
        if (option_type == 2 && length == 2){
            channel->flush_timeout = little_endian_read_16(command, pos);
        }

#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
        // Retransmission and Flow Control Option
        if (option_type == 4 && length == 9){
            l2cap_channel_mode_t mode = (l2cap_channel_mode_t) command[pos];
            switch(channel->mode){
                case L2CAP_CHANNEL_MODE_ENHANCED_RETRANSMISSION:
                    // Store remote config
                    channel->remote_tx_window_size = command[pos+1];
                    channel->remote_max_transmit   = command[pos+2];
                    channel->remote_retransmission_timeout_ms = little_endian_read_16(command, pos + 3);
                    channel->remote_monitor_timeout_ms = little_endian_read_16(command, pos + 5);
                    log_info("FC&C config: tx window: %u, max transmit %u, retrans timeout %u, monitor timeout %u",
                        channel->remote_tx_window_size,
                        channel->remote_max_transmit,
                        channel->remote_retransmission_timeout_ms,
                        channel->remote_monitor_timeout_ms);
                    // we store remote MPS in remote_mtu, might need to get re-evaluated
                    channel->remote_mtu = little_endian_read_16(command, pos + 7);
                    // If ERTM mandatory, but remote doens't offer ERTM -> disconnect
                    if (channel->ertm_mandatory && mode != L2CAP_CHANNEL_MODE_ENHANCED_RETRANSMISSION){
                        channel->state = L2CAP_STATE_WILL_SEND_DISCONNECT_REQUEST;
                    } else {
                        channelStateVarSetFlag(channel, L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP_MTU);
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
                            channelStateVarSetFlag(channel, L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP_MTU);
                            break;
                    }
                    break;
                default:
                    break;
            }
        }        
#endif        
        // check for unknown options
        if (option_hint == 0 && (option_type == 0 || option_type >= 0x07)){
            log_info("l2cap cid %u, unknown options", channel->local_cid);
            channelStateVarSetFlag(channel, L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP_INVALID);
        }
        pos += length;
    }
}

static void l2cap_signaling_handle_configure_response(l2cap_channel_t *channel, uint8_t result, uint8_t *command){
    log_info("l2cap_signaling_handle_configure_response");
#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
    uint16_t end_pos = 4 + little_endian_read_16(command, L2CAP_SIGNALING_COMMAND_LENGTH_OFFSET);
    uint16_t pos     = 10;
    while (pos < end_pos){
        uint8_t option_hint = command[pos] >> 7;
        uint8_t option_type = command[pos] & 0x7f;
        log_info("l2cap cid %u, hint %u, type %u", channel->local_cid, option_hint, option_type);
        pos++;
        uint8_t length = command[pos++];

        // Retransmission and Flow Control Option
        if (option_type == 4 && length == 9){
            switch (channel->mode){
                case L2CAP_CHANNEL_MODE_ENHANCED_RETRANSMISSION:
                    if (channel->ertm_mandatory){
                        // ??
                    } else {
                        // On 'Reject - Unacceptable Parameters' to our optional ERTM request, fall back to BASIC mode
                        if (result == L2CAP_CONF_RESULT_UNACCEPTABLE_PARAMETERS){
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
        if (option_hint == 0 && (option_type == 0 || option_type >= 0x07)){
            log_info("l2cap cid %u, unknown options", channel->local_cid);
            channelStateVarSetFlag(channel, L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP_INVALID);
        }

        pos += length;
    }
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


static void l2cap_signaling_handler_channel(l2cap_channel_t *channel, uint8_t *command){

    uint8_t  code       = command[L2CAP_SIGNALING_COMMAND_CODE_OFFSET];
    uint8_t  identifier = command[L2CAP_SIGNALING_COMMAND_SIGID_OFFSET];
    uint16_t result = 0;
    
    log_info("L2CAP signaling handler code %u, state %u", code, channel->state);
    
    // handle DISCONNECT REQUESTS seperately
    if (code == DISCONNECTION_REQUEST){
        switch (channel->state){
            case L2CAP_STATE_CONFIG:
            case L2CAP_STATE_OPEN:
            case L2CAP_STATE_WILL_SEND_DISCONNECT_REQUEST:
            case L2CAP_STATE_WAIT_DISCONNECT:
                l2cap_handle_disconnect_request(channel, identifier);
                break;

            default:
                // ignore in other states
                break;
        }
        return;
    }
    
    // @STATEMACHINE(l2cap)
    switch (channel->state) {
            
        case L2CAP_STATE_WAIT_CONNECT_RSP:
            switch (code){
                case CONNECTION_RESPONSE:
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
                            l2cap_emit_channel_opened(channel, L2CAP_CONNECTION_RESPONSE_RESULT_SUCCESSFUL + result);
                            
                            // drop link key if security block
                            if (L2CAP_CONNECTION_RESPONSE_RESULT_SUCCESSFUL + result == L2CAP_CONNECTION_RESPONSE_RESULT_REFUSED_SECURITY){
                                gap_drop_link_key_for_bd_addr(channel->address);
                            }
                            
                            // discard channel
                            btstack_linked_list_remove(&l2cap_channels, (btstack_linked_item_t *) channel);
                            btstack_memory_l2cap_channel_free(channel);
                            break;
                    }
                    break;
                    
                default:
                    //@TODO: implement other signaling packets
                    break;
            }
            break;

        case L2CAP_STATE_CONFIG:
            result = little_endian_read_16 (command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET+4);
            switch (code) {
                case CONFIGURE_REQUEST:
                    channelStateVarSetFlag(channel, L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP);
                    l2cap_signaling_handle_configure_request(channel, command);
                    if (!(channel->state_var & L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP_CONT)){
                        // only done if continuation not set
                        channelStateVarSetFlag(channel, L2CAP_CHANNEL_STATE_VAR_RCVD_CONF_REQ);
                    }
                    break;
                case CONFIGURE_RESPONSE:
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
                // for open:
                channel->state = L2CAP_STATE_OPEN;
                l2cap_emit_channel_opened(channel, 0);
            }
            break;
            
        case L2CAP_STATE_WAIT_DISCONNECT:
            switch (code) {
                case DISCONNECTION_RESPONSE:
                    l2cap_finialize_channel_close(channel);
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


static void l2cap_signaling_handler_dispatch( hci_con_handle_t handle, uint8_t * command){
    
    btstack_linked_list_iterator_t it;    

    // get code, signalind identifier and command len
    uint8_t code   = command[L2CAP_SIGNALING_COMMAND_CODE_OFFSET];
    uint8_t sig_id = command[L2CAP_SIGNALING_COMMAND_SIGID_OFFSET];
    
    // not for a particular channel, and not CONNECTION_REQUEST, ECHO_[REQUEST|RESPONSE], INFORMATION_RESPONSE 
    if (code < 1 || code == ECHO_RESPONSE || code > INFORMATION_RESPONSE){
        l2cap_register_signaling_response(handle, COMMAND_REJECT, sig_id, 0, L2CAP_REJ_CMD_UNKNOWN);
        return;
    }

    // general commands without an assigned channel
    switch(code) {
            
        case CONNECTION_REQUEST: {
            uint16_t psm =        little_endian_read_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET);
            uint16_t source_cid = little_endian_read_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET+2);
            l2cap_handle_connection_request(handle, sig_id, psm, source_cid);
            return;
        }
            
        case ECHO_REQUEST:
            l2cap_register_signaling_response(handle, code, sig_id, 0, 0);
            return;
            
        case INFORMATION_REQUEST: {
            uint16_t infoType = little_endian_read_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET);
            l2cap_register_signaling_response(handle, code, sig_id, 0, infoType);
            return;
        }

#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
        case INFORMATION_RESPONSE: {
            hci_connection_t * connection = hci_connection_for_handle(handle);
            if (!connection) return;
            uint16_t info_type = little_endian_read_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET);
            uint16_t result =  little_endian_read_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET+2);
            if (result != 0) return;
            if (info_type != 0x02) return;
            connection->l2cap_state.information_state = L2CAP_INFORMATION_STATE_DONE; 
            connection->l2cap_state.extended_feature_mask = little_endian_read_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET+4);
            log_info("extended features mask 0x%02x", connection->l2cap_state.extended_feature_mask);
            // trigger connection request
            btstack_linked_list_iterator_init(&it, &l2cap_channels);
            while (btstack_linked_list_iterator_has_next(&it)){
                l2cap_channel_t * channel = (l2cap_channel_t *) btstack_linked_list_iterator_next(&it);
                if (channel->con_handle != handle) continue;
                // bail if ERTM was requested but is not supported
                if ((channel->mode == L2CAP_CHANNEL_MODE_ENHANCED_RETRANSMISSION) && ((connection->l2cap_state.extended_feature_mask & 0x08) == 0)){
                    if (channel->ertm_mandatory){
                        // channel closed
                        channel->state = L2CAP_STATE_CLOSED;
                        // map l2cap connection response result to BTstack status enumeration
                        l2cap_emit_channel_opened(channel, L2CAP_CONNECTION_RESPONSE_RESULT_ERTM_NOT_SUPPORTD);
                        // discard channel
                        btstack_linked_list_remove(&l2cap_channels, (btstack_linked_item_t *) channel);
                        btstack_memory_l2cap_channel_free(channel);
                        continue;
                    } else {
                        // fallback to Basic mode
                        channel->mode = L2CAP_CHANNEL_MODE_BASIC;
                    }
                }
                // start connecting
                if (channel->state == L2CAP_STATE_WAIT_OUTGOING_EXTENDED_FEATURES){
                    channel->state = L2CAP_STATE_WILL_SEND_CONNECTION_REQUEST;
                }
                // respond to connection request
                if (channel->state == L2CAP_STATE_WAIT_INCOMING_EXTENDED_FEATURES){
                    channel->state = L2CAP_STATE_WAIT_CLIENT_ACCEPT_OR_REJECT;
                    l2cap_emit_incoming_connection(channel);    
                }
            }
            return;
        }
#endif

        default:
            break;
    }
    
    
    // Get potential destination CID
    uint16_t dest_cid = little_endian_read_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET);
    
    // Find channel for this sig_id and connection handle
    btstack_linked_list_iterator_init(&it, &l2cap_channels);
    while (btstack_linked_list_iterator_has_next(&it)){
        l2cap_channel_t * channel = (l2cap_channel_t *) btstack_linked_list_iterator_next(&it);
        if (channel->con_handle != handle) continue;
        if (code & 1) {
            // match odd commands (responses) by previous signaling identifier 
            if (channel->local_sig_id == sig_id) {
                l2cap_signaling_handler_channel(channel, command);
                break;
            }
        } else {
            // match even commands (requests) by local channel id
            if (channel->local_cid == dest_cid) {
                l2cap_signaling_handler_channel(channel, command);
                break;
            }
        }
    }
}
#endif

#ifdef ENABLE_BLE

static void l2cap_emit_connection_parameter_update_response(hci_con_handle_t con_handle, uint16_t result){
    uint8_t event[6];
    event[0] = L2CAP_EVENT_CONNECTION_PARAMETER_UPDATE_RESPONSE;
    event[1] = 4;
    little_endian_store_16(event, 2, con_handle);
    little_endian_store_16(event, 4, result);
    hci_dump_packet( HCI_EVENT_PACKET, 0, event, sizeof(event));
    if (!l2cap_event_packet_handler) return;
    (*l2cap_event_packet_handler)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

// @returns valid
static int l2cap_le_signaling_handler_dispatch(hci_con_handle_t handle, uint8_t * command, uint8_t sig_id){
    hci_connection_t * connection;
    uint16_t result;
    uint8_t  event[10];

#ifdef ENABLE_LE_DATA_CHANNELS
    btstack_linked_list_iterator_t it;    
    l2cap_channel_t * channel;
    uint16_t local_cid;
    uint16_t le_psm;
    uint16_t new_credits;
    uint16_t credits_before;
    l2cap_service_t * service;
    uint16_t source_cid;
#endif

    uint8_t code   = command[L2CAP_SIGNALING_COMMAND_CODE_OFFSET];
    log_info("l2cap_le_signaling_handler_dispatch: command 0x%02x, sig id %u", code, sig_id);

    switch (code){

        case CONNECTION_PARAMETER_UPDATE_RESPONSE:
            result = little_endian_read_16(command, 4);
            l2cap_emit_connection_parameter_update_response(handle, result);
            break;

        case CONNECTION_PARAMETER_UPDATE_REQUEST:
            connection = hci_connection_for_handle(handle);
            if (connection){
                if (connection->role != HCI_ROLE_MASTER){
                    // reject command without notifying upper layer when not in master role
                    return 0;
                }
                int update_parameter = 1;
                le_connection_parameter_range_t existing_range;
                gap_get_connection_parameter_range(&existing_range);
                uint16_t le_conn_interval_min = little_endian_read_16(command,8);
                uint16_t le_conn_interval_max = little_endian_read_16(command,10);
                uint16_t le_conn_latency = little_endian_read_16(command,12);
                uint16_t le_supervision_timeout = little_endian_read_16(command,14);

                if (le_conn_interval_min < existing_range.le_conn_interval_min) update_parameter = 0;
                if (le_conn_interval_max > existing_range.le_conn_interval_max) update_parameter = 0;

                if (le_conn_latency < existing_range.le_conn_latency_min) update_parameter = 0;
                if (le_conn_latency > existing_range.le_conn_latency_max) update_parameter = 0;

                if (le_supervision_timeout < existing_range.le_supervision_timeout_min) update_parameter = 0;
                if (le_supervision_timeout > existing_range.le_supervision_timeout_max) update_parameter = 0;

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

            if (!l2cap_event_packet_handler) break;

            event[0] = L2CAP_EVENT_CONNECTION_PARAMETER_UPDATE_REQUEST;
            event[1] = 8;
            memcpy(&event[2], &command[4], 8);
            hci_dump_packet( HCI_EVENT_PACKET, 0, event, sizeof(event));
            (*l2cap_event_packet_handler)( HCI_EVENT_PACKET, 0, event, sizeof(event));
            break;

#ifdef ENABLE_LE_DATA_CHANNELS

        case COMMAND_REJECT:
            // Find channel for this sig_id and connection handle
            channel = NULL;
            btstack_linked_list_iterator_init(&it, &l2cap_le_channels);
            while (btstack_linked_list_iterator_has_next(&it)){
                l2cap_channel_t * a_channel = (l2cap_channel_t *) btstack_linked_list_iterator_next(&it);
                if (a_channel->con_handle   != handle) continue;
                if (a_channel->local_sig_id != sig_id) continue;
                channel = a_channel;
                break; 
            }
            if (!channel) break;

            // if received while waiting for le connection response, assume legacy device
            if (channel->state == L2CAP_STATE_WAIT_LE_CONNECTION_RESPONSE){
                channel->state = L2CAP_STATE_CLOSED;
                // no official value for this, use: Connection refused – LE_PSM not supported - 0x0002
                l2cap_emit_le_channel_opened(channel, 0x0002);
                                
                // discard channel
                btstack_linked_list_remove(&l2cap_le_channels, (btstack_linked_item_t *) channel);
                btstack_memory_l2cap_channel_free(channel);
                break;
            }
            break;

        case LE_CREDIT_BASED_CONNECTION_REQUEST:
 
            // get hci connection, bail if not found (must not happen)
            connection = hci_connection_for_handle(handle);
            if (!connection) return 0;

            // check if service registered
            le_psm  = little_endian_read_16(command, 4);
            service = l2cap_le_get_service(le_psm);
            source_cid = little_endian_read_16(command, 6);
                
            if (service){
                if (source_cid < 0x40){
                    // 0x0009 Connection refused - Invalid Source CID
                    l2cap_register_signaling_response(handle, LE_CREDIT_BASED_CONNECTION_REQUEST, sig_id, source_cid, 0x0009);
                    return 1;
                }

                // go through list of channels for this ACL connection and check if we get a match
                btstack_linked_list_iterator_init(&it, &l2cap_le_channels);
                while (btstack_linked_list_iterator_has_next(&it)){
                    l2cap_channel_t * a_channel = (l2cap_channel_t *) btstack_linked_list_iterator_next(&it);
                    if (a_channel->con_handle != handle) continue;
                    if (a_channel->remote_cid != source_cid) continue;
                    // 0x000a Connection refused - Source CID already allocated
                    l2cap_register_signaling_response(handle, LE_CREDIT_BASED_CONNECTION_REQUEST, sig_id, source_cid, 0x000a);
                    return 1;
                }                    

                // security: check encryption
                if (service->required_security_level >= LEVEL_2){
                    if (sm_encryption_key_size(handle) == 0){
                        // 0x0008 Connection refused - insufficient encryption 
                        l2cap_register_signaling_response(handle, LE_CREDIT_BASED_CONNECTION_REQUEST, sig_id, source_cid, 0x0008);
                        return 1;
                    }
                    // anything less than 16 byte key size is insufficient
                    if (sm_encryption_key_size(handle) < 16){
                        // 0x0007 Connection refused – insufficient encryption key size
                        l2cap_register_signaling_response(handle, LE_CREDIT_BASED_CONNECTION_REQUEST, sig_id, source_cid, 0x0007);
                        return 1;
                    }
                }

                // security: check authencation
                if (service->required_security_level >= LEVEL_3){
                    if (!sm_authenticated(handle)){
                        // 0x0005 Connection refused – insufficient authentication
                        l2cap_register_signaling_response(handle, LE_CREDIT_BASED_CONNECTION_REQUEST, sig_id, source_cid, 0x0005);
                        return 1;
                    }
                }

                // security: check authorization
                if (service->required_security_level >= LEVEL_4){
                    if (sm_authorization_state(handle) != AUTHORIZATION_GRANTED){
                        // 0x0006 Connection refused – insufficient authorization
                        l2cap_register_signaling_response(handle, LE_CREDIT_BASED_CONNECTION_REQUEST, sig_id, source_cid, 0x0006);
                        return 1;
                    }
                }

                // allocate channel
                channel = l2cap_create_channel_entry(service->packet_handler, connection->address,
                    BD_ADDR_TYPE_LE_RANDOM, le_psm, service->mtu, service->required_security_level);
                if (!channel){
                    // 0x0004 Connection refused – no resources available
                    l2cap_register_signaling_response(handle, LE_CREDIT_BASED_CONNECTION_REQUEST, sig_id, source_cid, 0x0004);
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
                btstack_linked_list_add(&l2cap_le_channels, (btstack_linked_item_t *) channel);

                // post connection request event
                l2cap_emit_le_incoming_connection(channel);

            } else {
                // Connection refused – LE_PSM not supported
                l2cap_register_signaling_response(handle, LE_CREDIT_BASED_CONNECTION_REQUEST, sig_id, source_cid, 0x0002);
            }
            break;

        case LE_CREDIT_BASED_CONNECTION_RESPONSE:
            // Find channel for this sig_id and connection handle
            channel = NULL;
            btstack_linked_list_iterator_init(&it, &l2cap_le_channels);
            while (btstack_linked_list_iterator_has_next(&it)){
                l2cap_channel_t * a_channel = (l2cap_channel_t *) btstack_linked_list_iterator_next(&it);
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
                // map l2cap connection response result to BTstack status enumeration
                l2cap_emit_le_channel_opened(channel, result);
                                
                // discard channel
                btstack_linked_list_remove(&l2cap_le_channels, (btstack_linked_item_t *) channel);
                btstack_memory_l2cap_channel_free(channel);
                break;
            }

            // success
            channel->remote_cid = little_endian_read_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET + 0);
            channel->remote_mtu = little_endian_read_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET + 2);
            channel->remote_mps = little_endian_read_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET + 4);
            channel->credits_outgoing = little_endian_read_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET + 6);
            channel->state = L2CAP_STATE_OPEN;
            l2cap_emit_le_channel_opened(channel, result);
            break;

        case LE_FLOW_CONTROL_CREDIT:
            // find channel
            local_cid = little_endian_read_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET + 0);
            channel = l2cap_le_get_channel_for_local_cid(local_cid);
            if (!channel) {
                log_error("l2cap: no channel for cid 0x%02x", local_cid);
                break;
            }
            new_credits = little_endian_read_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET + 2);
            credits_before = channel->credits_outgoing;
            channel->credits_outgoing += new_credits;
            // check for credit overrun
            if (credits_before > channel->credits_outgoing){
                log_error("l2cap: new credits caused overrrun for cid 0x%02x, disconnecting", local_cid);
                channel->state = L2CAP_STATE_WILL_SEND_DISCONNECT_REQUEST;
                break;
            }            
            log_info("l2cap: %u credits for 0x%02x, now %u", new_credits, local_cid, channel->credits_outgoing);
            break;

        case DISCONNECTION_REQUEST:
            // find channel
            local_cid = little_endian_read_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET + 0);
            channel = l2cap_le_get_channel_for_local_cid(local_cid);
            if (!channel) {
                log_error("l2cap: no channel for cid 0x%02x", local_cid);
                break;
            }
            channel->remote_sig_id = sig_id;
            channel->state = L2CAP_STATE_WILL_SEND_DISCONNECT_RESPONSE;
            break;

#endif

        case DISCONNECTION_RESPONSE:
            break;

        default:
            // command unknown -> reject command
            return 0;
    }
    return 1;
}
#endif

static void l2cap_acl_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size ){
    UNUSED(packet_type);
    UNUSED(channel);
            
    l2cap_channel_t * l2cap_channel;
    UNUSED(l2cap_channel);

    // Get Channel ID
    uint16_t channel_id = READ_L2CAP_CHANNEL_ID(packet); 
    hci_con_handle_t handle = READ_ACL_CONNECTION_HANDLE(packet);

    switch (channel_id) {
            
#ifdef ENABLE_CLASSIC
        case L2CAP_CID_SIGNALING: {
            uint16_t command_offset = 8;
            while (command_offset < size) {                
                // handle signaling commands
                l2cap_signaling_handler_dispatch(handle, &packet[command_offset]);
                
                // increment command_offset
                command_offset += L2CAP_SIGNALING_COMMAND_DATA_OFFSET + little_endian_read_16(packet, command_offset + L2CAP_SIGNALING_COMMAND_LENGTH_OFFSET);
            }
            break;
        }
#endif

#ifdef ENABLE_BLE
        case L2CAP_CID_SIGNALING_LE: {
            uint16_t sig_id = packet[COMPLETE_L2CAP_HEADER + 1];
            int      valid  = l2cap_le_signaling_handler_dispatch(handle, &packet[COMPLETE_L2CAP_HEADER], sig_id);
            if (!valid){
                l2cap_register_signaling_response(handle, COMMAND_REJECT_LE, sig_id, 0, L2CAP_REJ_CMD_UNKNOWN);
            }
            break;
        }
#endif

        case L2CAP_CID_ATTRIBUTE_PROTOCOL:
            if (fixed_channels[L2CAP_FIXED_CHANNEL_TABLE_INDEX_ATTRIBUTE_PROTOCOL].callback) {
                (*fixed_channels[L2CAP_FIXED_CHANNEL_TABLE_INDEX_ATTRIBUTE_PROTOCOL].callback)(ATT_DATA_PACKET, handle, &packet[COMPLETE_L2CAP_HEADER], size-COMPLETE_L2CAP_HEADER);
            }
            break;

        case L2CAP_CID_SECURITY_MANAGER_PROTOCOL:
            if (fixed_channels[L2CAP_FIXED_CHANNEL_TABLE_INDEX_SECURITY_MANAGER_PROTOCOL].callback) {
                (*fixed_channels[L2CAP_FIXED_CHANNEL_TABLE_INDEX_SECURITY_MANAGER_PROTOCOL].callback)(SM_DATA_PACKET, handle, &packet[COMPLETE_L2CAP_HEADER], size-COMPLETE_L2CAP_HEADER);
            }
            break;

        case L2CAP_CID_CONNECTIONLESS_CHANNEL:
            if (fixed_channels[L2CAP_FIXED_CHANNEL_TABLE_INDEX_CONNECTIONLESS_CHANNEL].callback) {
                (*fixed_channels[L2CAP_FIXED_CHANNEL_TABLE_INDEX_CONNECTIONLESS_CHANNEL].callback)(UCD_DATA_PACKET, handle, &packet[COMPLETE_L2CAP_HEADER], size-COMPLETE_L2CAP_HEADER);
            }
            break;

        default: 
#ifdef ENABLE_CLASSIC
            // Find channel for this channel_id and connection handle
            l2cap_channel = l2cap_get_channel_for_local_cid(channel_id);
            if (l2cap_channel) {
#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
                if (l2cap_channel->mode == L2CAP_CHANNEL_MODE_ENHANCED_RETRANSMISSION){

                    // verify FCS
                    uint16_t fcs_calculated = crc16_calc(&packet[4], size - (4+2));
                    uint16_t fcs_packet     = little_endian_read_16(packet, size-2);
                    log_info("Packet FCS 0x%04x, calculated FCS 0x%04x", fcs_packet, fcs_calculated);
                    if (fcs_calculated != fcs_packet){
                        log_error("FCS mismatch! Packet 0x%04x, calculated 0x%04x", fcs_packet, fcs_calculated);
                        // TODO: trigger retransmission or something like that
                        break;
                    }

                    // switch on packet type
                    uint16_t control = little_endian_read_16(packet, COMPLETE_L2CAP_HEADER);
                    uint8_t  req_seq = (control >> 8) & 0x3f;
                    if (control & 1){
                        // S-Frame
                        // int poll = (control >> 7) & 0x01;
                        l2cap_supervisory_function_t s = (l2cap_supervisory_function_t) ((control >> 2) & 0x03);
                        switch (s){
                            case L2CAP_SUPERVISORY_FUNCTION_RR_RECEIVER_READY:
                                l2cap_ertm_handle_req_seq(l2cap_channel, req_seq);                                
                                break;
                            default:
                                break;
                        }
                        break;
                    } else {
                        // I-Frame
                        // get control
                        l2cap_segmentation_and_reassembly_t sar = (l2cap_segmentation_and_reassembly_t) (control >> 14);
                        uint8_t tx_seq = (control >> 1) & 0x3f;
                        log_info("Control: 0x%04x => SAR %u, ReqSeq %02u, R?, TxSeq %02u", control, (int) sar, req_seq, tx_seq);
                        log_info("SAR: pos %u", l2cap_channel->rx_packets_state->pos);
                        log_info("State: expected_tx_seq %02u, req_seq %02u", l2cap_channel->expected_tx_seq, l2cap_channel->req_seq);
                        l2cap_ertm_handle_req_seq(l2cap_channel, req_seq);                                
                        // check ordering
                        if (l2cap_channel->expected_tx_seq == tx_seq){
                            log_info("Received expected frame with TxSeq == ExpectedTxSeq == %02u", tx_seq);
                            l2cap_channel->req_seq = tx_seq;
                            l2cap_channel->send_supervisor_frame_receiver_ready = 1;
                            l2cap_channel->expected_tx_seq = l2cap_next_ertm_seq_nr(l2cap_channel->expected_tx_seq);
                            uint16_t sdu_length;
                            uint16_t segment_length;
                            uint16_t payload_offset;
                            switch (sar){
                                case L2CAP_SEGMENTATION_AND_REASSEMBLY_UNSEGMENTED_L2CAP_SDU:
                                    payload_offset = COMPLETE_L2CAP_HEADER+2;
                                    segment_length = payload_offset-2;
                                    l2cap_dispatch_to_channel(l2cap_channel, L2CAP_DATA_PACKET, &packet[COMPLETE_L2CAP_HEADER+2], segment_length);
                                    break;
                                case L2CAP_SEGMENTATION_AND_REASSEMBLY_START_OF_L2CAP_SDU:
                                    // TODO: use current packet
                                    // TODO: check if reassembly started
                                    // TODO: check len against local mtu
                                    sdu_length = little_endian_read_16(packet, COMPLETE_L2CAP_HEADER+2);
                                    payload_offset = COMPLETE_L2CAP_HEADER+4;
                                    segment_length = size - payload_offset-2;
                                    memcpy(&l2cap_channel->rx_packets_data[0], &packet[payload_offset], segment_length);
                                    l2cap_channel->rx_packets_state->sdu_length = sdu_length;
                                    l2cap_channel->rx_packets_state->pos = segment_length;
                                    break;
                                case L2CAP_SEGMENTATION_AND_REASSEMBLY_END_OF_L2CAP_SDU:
                                    payload_offset = COMPLETE_L2CAP_HEADER+2;
                                    segment_length = size - payload_offset-2;
                                    memcpy(&l2cap_channel->rx_packets_data[l2cap_channel->rx_packets_state->pos], &packet[payload_offset], segment_length);
                                    l2cap_channel->rx_packets_state->pos += segment_length;
                                    l2cap_dispatch_to_channel(l2cap_channel, L2CAP_DATA_PACKET, l2cap_channel->rx_packets_data, l2cap_channel->rx_packets_state[0].pos);
                                    break; 
                                case L2CAP_SEGMENTATION_AND_REASSEMBLY_CONTINUATION_OF_L2CAP_SDU:
                                    payload_offset = COMPLETE_L2CAP_HEADER+2;
                                    segment_length = size - payload_offset-2;
                                    memcpy(&l2cap_channel->rx_packets_data[l2cap_channel->rx_packets_state->pos], &packet[payload_offset], segment_length);
                                    l2cap_channel->rx_packets_state->pos += segment_length;
                                    break;
                                }
                            }
                        }
                    break;
                }
#endif                
                l2cap_dispatch_to_channel(l2cap_channel, L2CAP_DATA_PACKET, &packet[COMPLETE_L2CAP_HEADER], size-COMPLETE_L2CAP_HEADER);
            }
#endif
#ifdef ENABLE_LE_DATA_CHANNELS
            l2cap_channel = l2cap_le_get_channel_for_local_cid(channel_id);
            if (l2cap_channel) {
                // credit counting
                if (l2cap_channel->credits_incoming == 0){
                    log_error("LE Data Channel packet received but no incoming credits");
                    l2cap_channel->state = L2CAP_STATE_WILL_SEND_DISCONNECT_REQUEST;
                    break;
                }
                l2cap_channel->credits_incoming--;

                // automatic credits
                if (l2cap_channel->credits_incoming < L2CAP_LE_DATA_CHANNELS_AUTOMATIC_CREDITS_WATERMARK && l2cap_channel->automatic_credits){
                    l2cap_channel->new_credits_incoming = L2CAP_LE_DATA_CHANNELS_AUTOMATIC_CREDITS_INCREMENT;
                }

                // first fragment
                uint16_t pos = 0;
                if (!l2cap_channel->receive_sdu_len){
                    l2cap_channel->receive_sdu_len = little_endian_read_16(packet, COMPLETE_L2CAP_HEADER);
                    l2cap_channel->receive_sdu_pos = 0;                   
                    pos  += 2;
                    size -= 2;
                }
                memcpy(&l2cap_channel->receive_sdu_buffer[l2cap_channel->receive_sdu_pos], &packet[COMPLETE_L2CAP_HEADER+pos], size-COMPLETE_L2CAP_HEADER);
                l2cap_channel->receive_sdu_pos += size - COMPLETE_L2CAP_HEADER;
                // done?
                log_info("le packet pos %u, len %u", l2cap_channel->receive_sdu_pos, l2cap_channel->receive_sdu_len);
                if (l2cap_channel->receive_sdu_pos >= l2cap_channel->receive_sdu_len){
                    l2cap_dispatch_to_channel(l2cap_channel, L2CAP_DATA_PACKET, l2cap_channel->receive_sdu_buffer, l2cap_channel->receive_sdu_len);
                    l2cap_channel->receive_sdu_len = 0;
                }
            } else {
                log_error("LE Data Channel packet received but no channel found for cid 0x%02x", channel_id);
            }
#endif
            break;
    }

    l2cap_run();
}

// Bluetooth 4.0 - allows to register handler for Attribute Protocol and Security Manager Protocol
void l2cap_register_fixed_channel(btstack_packet_handler_t the_packet_handler, uint16_t channel_id) {
    int index = l2cap_fixed_channel_table_index_for_channel_id(channel_id);
    if (index < 0) return;
    fixed_channels[index].callback = the_packet_handler;
}

#ifdef ENABLE_CLASSIC
// finalize closed channel - l2cap_handle_disconnect_request & DISCONNECTION_RESPONSE
void l2cap_finialize_channel_close(l2cap_channel_t * channel){
    channel->state = L2CAP_STATE_CLOSED;
    l2cap_emit_channel_closed(channel);
    // discard channel
    l2cap_stop_rtx(channel);
    btstack_linked_list_remove(&l2cap_channels, (btstack_linked_item_t *) channel);
    btstack_memory_l2cap_channel_free(channel);
}

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

static inline l2cap_service_t * l2cap_get_service(uint16_t psm){
    return l2cap_get_service_internal(&l2cap_services, psm);
}


uint8_t l2cap_register_service(btstack_packet_handler_t service_packet_handler, uint16_t psm, uint16_t mtu, gap_security_level_t security_level){
    
    log_info("L2CAP_REGISTER_SERVICE psm 0x%x mtu %u", psm, mtu);
    
    // check for alread registered psm 
    l2cap_service_t *service = l2cap_get_service(psm);
    if (service) {
        log_error("l2cap_register_service: PSM %u already registered", psm);
        return L2CAP_SERVICE_ALREADY_REGISTERED;
    }
    
    // alloc structure
    service = btstack_memory_l2cap_service_get();
    if (!service) {
        log_error("l2cap_register_service: no memory for l2cap_service_t");
        return BTSTACK_MEMORY_ALLOC_FAILED;
    }
    
    // fill in 
    service->psm = psm;
    service->mtu = mtu;
    service->packet_handler = service_packet_handler;
    service->required_security_level = security_level;

    // add to services list
    btstack_linked_list_add(&l2cap_services, (btstack_linked_item_t *) service);
    
    // enable page scan
    gap_connectable_control(1);

    return 0;
}

uint8_t l2cap_unregister_service(uint16_t psm){
    
    log_info("L2CAP_UNREGISTER_SERVICE psm 0x%x", psm);

    l2cap_service_t *service = l2cap_get_service(psm);
    if (!service) return L2CAP_SERVICE_DOES_NOT_EXIST;
    btstack_linked_list_remove(&l2cap_services, (btstack_linked_item_t *) service);
    btstack_memory_l2cap_service_free(service);
    
    // disable page scan when no services registered
    if (btstack_linked_list_empty(&l2cap_services)) {
        gap_connectable_control(0);
    }
    return 0;
}
#endif


#ifdef ENABLE_LE_DATA_CHANNELS

static void l2cap_le_notify_channel_can_send(l2cap_channel_t *channel){
    if (!channel->waiting_for_can_send_now) return;
    if (channel->send_sdu_buffer) return;
    channel->waiting_for_can_send_now = 0;
    log_info("L2CAP_EVENT_CHANNEL_LE_CAN_SEND_NOW local_cid 0x%x", channel->local_cid);
    l2cap_emit_simple_event_with_cid(channel, L2CAP_EVENT_LE_CAN_SEND_NOW);
}

// 1BH2222
static void l2cap_emit_le_incoming_connection(l2cap_channel_t *channel) {
    log_info("L2CAP_EVENT_LE_INCOMING_CONNECTION addr_type %u, addr %s handle 0x%x psm 0x%x local_cid 0x%x remote_cid 0x%x, remote_mtu %u",
             channel->address_type, bd_addr_to_str(channel->address), channel->con_handle,  channel->psm, channel->local_cid, channel->remote_cid, channel->remote_mtu);
    uint8_t event[19];
    event[0] = L2CAP_EVENT_LE_INCOMING_CONNECTION;
    event[1] = sizeof(event) - 2;
    event[2] = channel->address_type;
    reverse_bd_addr(channel->address, &event[3]);
    little_endian_store_16(event,  9, channel->con_handle);
    little_endian_store_16(event, 11, channel->psm);
    little_endian_store_16(event, 13, channel->local_cid);
    little_endian_store_16(event, 15, channel->remote_cid);
    little_endian_store_16(event, 17, channel->remote_mtu);
    hci_dump_packet( HCI_EVENT_PACKET, 0, event, sizeof(event));
    l2cap_dispatch_to_channel(channel, HCI_EVENT_PACKET, event, sizeof(event));
}
// 11BH22222
static void l2cap_emit_le_channel_opened(l2cap_channel_t *channel, uint8_t status) {
    log_info("L2CAP_EVENT_LE_CHANNEL_OPENED status 0x%x addr_type %u addr %s handle 0x%x psm 0x%x local_cid 0x%x remote_cid 0x%x local_mtu %u, remote_mtu %u",
             status, channel->address_type, bd_addr_to_str(channel->address), channel->con_handle, channel->psm,
             channel->local_cid, channel->remote_cid, channel->local_mtu, channel->remote_mtu);
    uint8_t event[23];
    event[0] = L2CAP_EVENT_LE_CHANNEL_OPENED;
    event[1] = sizeof(event) - 2;
    event[2] = status;
    event[3] = channel->address_type;
    reverse_bd_addr(channel->address, &event[4]);
    little_endian_store_16(event, 10, channel->con_handle);
    event[12] = channel->state_var & L2CAP_CHANNEL_STATE_VAR_INCOMING ? 1 : 0;
    little_endian_store_16(event, 13, channel->psm);
    little_endian_store_16(event, 15, channel->local_cid);
    little_endian_store_16(event, 17, channel->remote_cid);
    little_endian_store_16(event, 19, channel->local_mtu);
    little_endian_store_16(event, 21, channel->remote_mtu); 
    hci_dump_packet( HCI_EVENT_PACKET, 0, event, sizeof(event));
    l2cap_dispatch_to_channel(channel, HCI_EVENT_PACKET, event, sizeof(event));
}

static l2cap_channel_t * l2cap_le_get_channel_for_local_cid(uint16_t local_cid){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, &l2cap_le_channels);
    while (btstack_linked_list_iterator_has_next(&it)){
        l2cap_channel_t * channel = (l2cap_channel_t *) btstack_linked_list_iterator_next(&it);
        if ( channel->local_cid == local_cid) {
            return channel;
        }
    } 
    return NULL;
}

// finalize closed channel - l2cap_handle_disconnect_request & DISCONNECTION_RESPONSE
void l2cap_le_finialize_channel_close(l2cap_channel_t * channel){
    channel->state = L2CAP_STATE_CLOSED;
    l2cap_emit_simple_event_with_cid(channel, L2CAP_EVENT_CHANNEL_CLOSED);
    // discard channel
    btstack_linked_list_remove(&l2cap_le_channels, (btstack_linked_item_t *) channel);
    btstack_memory_l2cap_channel_free(channel);
}

static inline l2cap_service_t * l2cap_le_get_service(uint16_t le_psm){
    return l2cap_get_service_internal(&l2cap_le_services, le_psm);
}

uint8_t l2cap_le_register_service(btstack_packet_handler_t packet_handler, uint16_t psm, gap_security_level_t security_level){
    
    log_info("L2CAP_LE_REGISTER_SERVICE psm 0x%x", psm);
    
    // check for alread registered psm 
    l2cap_service_t *service = l2cap_le_get_service(psm);
    if (service) {
        return L2CAP_SERVICE_ALREADY_REGISTERED;
    }
    
    // alloc structure
    service = btstack_memory_l2cap_service_get();
    if (!service) {
        log_error("l2cap_register_service_internal: no memory for l2cap_service_t");
        return BTSTACK_MEMORY_ALLOC_FAILED;
    }
    
    // fill in 
    service->psm = psm;
    service->mtu = 0;
    service->packet_handler = packet_handler;
    service->required_security_level = security_level;

    // add to services list
    btstack_linked_list_add(&l2cap_le_services, (btstack_linked_item_t *) service);
    
    // done
    return 0;
}

uint8_t l2cap_le_unregister_service(uint16_t psm) {
    log_info("L2CAP_LE_UNREGISTER_SERVICE psm 0x%x", psm);
    l2cap_service_t *service = l2cap_le_get_service(psm);
    if (!service) return L2CAP_SERVICE_DOES_NOT_EXIST;

    btstack_linked_list_remove(&l2cap_le_services, (btstack_linked_item_t *) service);
    btstack_memory_l2cap_service_free(service);
    return 0;
}

uint8_t l2cap_le_accept_connection(uint16_t local_cid, uint8_t * receive_sdu_buffer, uint16_t mtu, uint16_t initial_credits){
    // get channel
    l2cap_channel_t * channel = l2cap_le_get_channel_for_local_cid(local_cid);
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

    // test
    // channel->new_credits_incoming = 1;

    // go
    l2cap_run();
    return 0;
}

/** 
 * @brief Deny incoming LE Data Channel connection due to resource constraints
 * @param local_cid             L2CAP LE Data Channel Identifier
 */

uint8_t l2cap_le_decline_connection(uint16_t local_cid){
    // get channel
    l2cap_channel_t * channel = l2cap_le_get_channel_for_local_cid(local_cid);
    if (!channel) return L2CAP_LOCAL_CID_DOES_NOT_EXIST;

    // validate state
    if (channel->state != L2CAP_STATE_WAIT_CLIENT_ACCEPT_OR_REJECT){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    // set state decline connection
    channel->state  = L2CAP_STATE_WILL_SEND_LE_CONNECTION_RESPONSE_DECLINE;
    channel->reason = 0x04; // no resources available
    l2cap_run();
    return 0;
}

uint8_t l2cap_le_create_channel(btstack_packet_handler_t packet_handler, hci_con_handle_t con_handle, 
    uint16_t psm, uint8_t * receive_sdu_buffer, uint16_t mtu, uint16_t initial_credits, gap_security_level_t security_level,
    uint16_t * out_local_cid) {

    log_info("L2CAP_LE_CREATE_CHANNEL handle 0x%04x psm 0x%x mtu %u", con_handle, psm, mtu);


    hci_connection_t * connection = hci_connection_for_handle(con_handle);
    if (!connection) {
        log_error("no hci_connection for handle 0x%04x", con_handle);
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    l2cap_channel_t * channel = l2cap_create_channel_entry(packet_handler, connection->address, connection->address_type, psm, mtu, security_level);
    if (!channel) {
        return BTSTACK_MEMORY_ALLOC_FAILED;
    }
    log_info("l2cap_le_create_channel %p", channel);

    // store local_cid
    if (out_local_cid){
       *out_local_cid = channel->local_cid;
    }

    // provide buffer
    channel->con_handle = con_handle;
    channel->receive_sdu_buffer = receive_sdu_buffer;
    channel->state = L2CAP_STATE_WILL_SEND_LE_CONNECTION_REQUEST;
    channel->new_credits_incoming = initial_credits;
    channel->automatic_credits    = initial_credits == L2CAP_LE_AUTOMATIC_CREDITS;

    // add to connections list
    btstack_linked_list_add(&l2cap_le_channels, (btstack_linked_item_t *) channel);

    // go
    l2cap_run();
    return 0;
}

/**
 * @brief Provide credtis for LE Data Channel
 * @param local_cid             L2CAP LE Data Channel Identifier
 * @param credits               Number additional credits for peer
 */
uint8_t l2cap_le_provide_credits(uint16_t local_cid, uint16_t credits){

    l2cap_channel_t * channel = l2cap_le_get_channel_for_local_cid(local_cid);
    if (!channel) {
        log_error("l2cap_le_provide_credits no channel for cid 0x%02x", local_cid);
        return L2CAP_LOCAL_CID_DOES_NOT_EXIST;
    }

    // check state
    if (channel->state != L2CAP_STATE_OPEN){
        log_error("l2cap_le_provide_credits but channel 0x%02x not open yet", local_cid);
    }

    // assert incoming credits + credits <= 0xffff
    uint32_t total_credits = channel->credits_incoming;
    total_credits += channel->new_credits_incoming;
    total_credits += credits;
    if (total_credits > 0xffff){
        log_error("l2cap_le_provide_credits overrun: current %u, scheduled %u, additional %u", channel->credits_incoming,
            channel->new_credits_incoming, credits);
    }

    // set credits_granted
    channel->new_credits_incoming += credits;

    // go
    l2cap_run();
    return 0;
}

/**
 * @brief Check if outgoing buffer is available and that there's space on the Bluetooth module
 * @param local_cid             L2CAP LE Data Channel Identifier
 */
int l2cap_le_can_send_now(uint16_t local_cid){
    l2cap_channel_t * channel = l2cap_le_get_channel_for_local_cid(local_cid);
    if (!channel) {
        log_error("l2cap_le_provide_credits no channel for cid 0x%02x", local_cid);
        return 0;
    }

    // check state
    if (channel->state != L2CAP_STATE_OPEN) return 0;

    // check queue
    if (channel->send_sdu_buffer) return 0;    

    // fine, go ahead
    return 1;
}

/**
 * @brief Request emission of L2CAP_EVENT_CAN_SEND_NOW as soon as possible
 * @note L2CAP_EVENT_CAN_SEND_NOW might be emitted during call to this function
 *       so packet handler should be ready to handle it
 * @param local_cid             L2CAP LE Data Channel Identifier
 */
uint8_t l2cap_le_request_can_send_now_event(uint16_t local_cid){
    l2cap_channel_t * channel = l2cap_le_get_channel_for_local_cid(local_cid);
    if (!channel) {
        log_error("l2cap_le_request_can_send_now_event no channel for cid 0x%02x", local_cid);
        return 0;
    }
    channel->waiting_for_can_send_now = 1;
    l2cap_le_notify_channel_can_send(channel);
    return 0;
}

/**
 * @brief Send data via LE Data Channel
 * @note Since data larger then the maximum PDU needs to be segmented into multiple PDUs, data needs to stay valid until ... event
 * @param local_cid             L2CAP LE Data Channel Identifier
 * @param data                  data to send
 * @param size                  data size
 */
uint8_t l2cap_le_send_data(uint16_t local_cid, uint8_t * data, uint16_t len){

    l2cap_channel_t * channel = l2cap_le_get_channel_for_local_cid(local_cid);
    if (!channel) {
        log_error("l2cap_send no channel for cid 0x%02x", local_cid);
        return L2CAP_LOCAL_CID_DOES_NOT_EXIST;
    }

    if (len > channel->remote_mtu){
        log_error("l2cap_send cid 0x%02x, data length exceeds remote MTU.", local_cid);
        return L2CAP_DATA_LEN_EXCEEDS_REMOTE_MTU;
    }

    if (channel->send_sdu_buffer){
        log_info("l2cap_send cid 0x%02x, cannot send", local_cid);
        return BTSTACK_ACL_BUFFERS_FULL;
    }

    channel->send_sdu_buffer = data;
    channel->send_sdu_len    = len;
    channel->send_sdu_pos    = 0;

    l2cap_run();
    return 0;
}

/**
 * @brief Disconnect from LE Data Channel
 * @param local_cid             L2CAP LE Data Channel Identifier
 */
uint8_t l2cap_le_disconnect(uint16_t local_cid)
{
    l2cap_channel_t * channel = l2cap_le_get_channel_for_local_cid(local_cid);
    if (!channel) {
        log_error("l2cap_send no channel for cid 0x%02x", local_cid);
        return L2CAP_LOCAL_CID_DOES_NOT_EXIST;
    }

    channel->state = L2CAP_STATE_WILL_SEND_DISCONNECT_REQUEST;
    l2cap_run();
    return 0;
}

#endif
