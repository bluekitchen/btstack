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
 *  l2cap.c
 *
 *  Logical Link Control and Adaption Protocl (L2CAP)
 *
 *  Created by Matthias Ringwald on 5/16/09.
 */

#include "l2cap.h"
#include "hci.h"
#include "hci_dump.h"
#include "debug.h"
#include "btstack_memory.h"

#include <stdarg.h>
#include <string.h>

#include <stdio.h>

// nr of buffered acl packets in outgoing queue to get max performance 
#define NR_BUFFERED_ACL_PACKETS 3

// used to cache l2cap rejects, echo, and informational requests
#define NR_PENDING_SIGNALING_RESPONSES 3

// offsets for L2CAP SIGNALING COMMANDS
#define L2CAP_SIGNALING_COMMAND_CODE_OFFSET   0
#define L2CAP_SIGNALING_COMMAND_SIGID_OFFSET  1
#define L2CAP_SIGNALING_COMMAND_LENGTH_OFFSET 2
#define L2CAP_SIGNALING_COMMAND_DATA_OFFSET   4

static void null_packet_handler(void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void l2cap_packet_handler(uint8_t packet_type, uint8_t *packet, uint16_t size);

// used to cache l2cap rejects, echo, and informational requests
static l2cap_signaling_response_t signaling_responses[NR_PENDING_SIGNALING_RESPONSES];
static int signaling_responses_pending;

static linked_list_t l2cap_channels;
static linked_list_t l2cap_services;
static void (*packet_handler) (void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) = null_packet_handler;
static int new_credits_blocked = 0;

static btstack_packet_handler_t attribute_protocol_packet_handler;
static btstack_packet_handler_t security_protocol_packet_handler;
static btstack_packet_handler_t connectionless_channel_packet_handler;
static uint8_t require_security_level2_for_outgoing_sdp;

// prototypes
static void l2cap_finialize_channel_close(l2cap_channel_t *channel);
static l2cap_service_t * l2cap_get_service(uint16_t psm);
static void l2cap_emit_channel_opened(l2cap_channel_t *channel, uint8_t status);
static void l2cap_emit_channel_closed(l2cap_channel_t *channel);
static void l2cap_emit_connection_request(l2cap_channel_t *channel);
static int l2cap_channel_ready_for_open(l2cap_channel_t *channel);


void l2cap_init(){
    new_credits_blocked = 0;
    signaling_responses_pending = 0;
    
    l2cap_channels = NULL;
    l2cap_services = NULL;

    packet_handler = null_packet_handler;
    attribute_protocol_packet_handler = NULL;
    security_protocol_packet_handler = NULL;
    connectionless_channel_packet_handler = NULL;

    require_security_level2_for_outgoing_sdp = 0;

    // 
    // register callback with HCI
    //
    hci_register_packet_handler(&l2cap_packet_handler);
    hci_connectable_control(0); // no services yet
}


/** Register L2CAP packet handlers */
static void null_packet_handler(void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
}
void l2cap_register_packet_handler(void (*handler)(void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)){
    packet_handler = handler;
}

//  notify client/protocol handler
void l2cap_dispatch(l2cap_channel_t *channel, uint8_t type, uint8_t * data, uint16_t size){
    if (channel->packet_handler) {
        (* (channel->packet_handler))(type, channel->local_cid, data, size);
    } else {
        (*packet_handler)(channel->connection, type, channel->local_cid, data, size);
    }
}

void l2cap_emit_channel_opened(l2cap_channel_t *channel, uint8_t status) {
    log_info("L2CAP_EVENT_CHANNEL_OPENED status 0x%x addr %s handle 0x%x psm 0x%x local_cid 0x%x remote_cid 0x%x local_mtu %u, remote_mtu %u, flush_timeout %u",
             status, bd_addr_to_str(channel->address), channel->handle, channel->psm,
             channel->local_cid, channel->remote_cid, channel->local_mtu, channel->remote_mtu, channel->flush_timeout);
    uint8_t event[23];
    event[0] = L2CAP_EVENT_CHANNEL_OPENED;
    event[1] = sizeof(event) - 2;
    event[2] = status;
    bt_flip_addr(&event[3], channel->address);
    bt_store_16(event,  9, channel->handle);
    bt_store_16(event, 11, channel->psm);
    bt_store_16(event, 13, channel->local_cid);
    bt_store_16(event, 15, channel->remote_cid);
    bt_store_16(event, 17, channel->local_mtu);
    bt_store_16(event, 19, channel->remote_mtu); 
    bt_store_16(event, 21, channel->flush_timeout); 
    hci_dump_packet( HCI_EVENT_PACKET, 0, event, sizeof(event));
    l2cap_dispatch(channel, HCI_EVENT_PACKET, event, sizeof(event));
}

void l2cap_emit_channel_closed(l2cap_channel_t *channel) {
    log_info("L2CAP_EVENT_CHANNEL_CLOSED local_cid 0x%x", channel->local_cid);
    uint8_t event[4];
    event[0] = L2CAP_EVENT_CHANNEL_CLOSED;
    event[1] = sizeof(event) - 2;
    bt_store_16(event, 2, channel->local_cid);
    hci_dump_packet( HCI_EVENT_PACKET, 0, event, sizeof(event));
    l2cap_dispatch(channel, HCI_EVENT_PACKET, event, sizeof(event));
}

void l2cap_emit_connection_request(l2cap_channel_t *channel) {
    log_info("L2CAP_EVENT_INCOMING_CONNECTION addr %s handle 0x%x psm 0x%x local_cid 0x%x remote_cid 0x%x",
             bd_addr_to_str(channel->address), channel->handle,  channel->psm, channel->local_cid, channel->remote_cid);
    uint8_t event[16];
    event[0] = L2CAP_EVENT_INCOMING_CONNECTION;
    event[1] = sizeof(event) - 2;
    bt_flip_addr(&event[2], channel->address);
    bt_store_16(event,  8, channel->handle);
    bt_store_16(event, 10, channel->psm);
    bt_store_16(event, 12, channel->local_cid);
    bt_store_16(event, 14, channel->remote_cid);
    hci_dump_packet( HCI_EVENT_PACKET, 0, event, sizeof(event));
    l2cap_dispatch(channel, HCI_EVENT_PACKET, event, sizeof(event));
}

void l2cap_emit_connection_parameter_update_response(uint16_t handle, uint16_t result){
    uint8_t event[6];
    event[0] = L2CAP_EVENT_CONNECTION_PARAMETER_UPDATE_RESPONSE;
    event[1] = 4;
    bt_store_16(event, 2, handle);
    bt_store_16(event, 4, result);
    hci_dump_packet( HCI_EVENT_PACKET, 0, event, sizeof(event));
    (*packet_handler)(NULL, HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void l2cap_emit_service_registered(void *connection, uint8_t status, uint16_t psm){
    log_info("L2CAP_EVENT_SERVICE_REGISTERED status 0x%x psm 0x%x", status, psm);
    uint8_t event[5];
    event[0] = L2CAP_EVENT_SERVICE_REGISTERED;
    event[1] = sizeof(event) - 2;
    event[2] = status;
    bt_store_16(event, 3, psm);
    hci_dump_packet( HCI_EVENT_PACKET, 0, event, sizeof(event));
    (*packet_handler)(connection, HCI_EVENT_PACKET, 0, event, sizeof(event));
}

void l2cap_emit_credits(l2cap_channel_t *channel, uint8_t credits) {
    
    log_info("L2CAP_EVENT_CREDITS local_cid 0x%x credits %u", channel->local_cid, credits);
    // track credits
    channel->packets_granted += credits;
    
    uint8_t event[5];
    event[0] = L2CAP_EVENT_CREDITS;
    event[1] = sizeof(event) - 2;
    bt_store_16(event, 2, channel->local_cid);
    event[4] = credits;
    hci_dump_packet( HCI_EVENT_PACKET, 0, event, sizeof(event));
    l2cap_dispatch(channel, HCI_EVENT_PACKET, event, sizeof(event));
}

void l2cap_block_new_credits(uint8_t blocked){
    new_credits_blocked = blocked;
}

void l2cap_hand_out_credits(void){

    if (new_credits_blocked) return;    // we're told not to. used by daemon

    linked_list_iterator_t it;    
    linked_list_iterator_init(&it, &l2cap_channels);
    while (linked_list_iterator_has_next(&it)){
        l2cap_channel_t * channel = (l2cap_channel_t *) linked_list_iterator_next(&it);
        if (!hci_number_free_acl_slots_for_handle(channel->handle)) return;
        if (channel->state != L2CAP_STATE_OPEN) continue;
        if (hci_number_outgoing_packets(channel->handle) < NR_BUFFERED_ACL_PACKETS && channel->packets_granted == 0) {
            l2cap_emit_credits(channel, 1);
        }
    }
}

l2cap_channel_t * l2cap_get_channel_for_local_cid(uint16_t local_cid){
    linked_list_iterator_t it;    
    linked_list_iterator_init(&it, &l2cap_channels);
    while (linked_list_iterator_has_next(&it)){
        l2cap_channel_t * channel = (l2cap_channel_t *) linked_list_iterator_next(&it);
        if ( channel->local_cid == local_cid) {
            return channel;
        }
    } 
    return NULL;
}

int  l2cap_can_send_packet_now(uint16_t local_cid){
    l2cap_channel_t *channel = l2cap_get_channel_for_local_cid(local_cid);
    if (!channel) return 0;
    if (!channel->packets_granted) return 0;
    return hci_can_send_acl_packet_now(channel->handle);
}

// @deprecated
int l2cap_can_send_connectionless_packet_now(void){
    // TODO provide real handle
    return l2cap_can_send_fixed_channel_packet_now(0x1234);
}

int  l2cap_can_send_fixed_channel_packet_now(uint16_t handle){
    return hci_can_send_acl_packet_now(handle);
}

uint16_t l2cap_get_remote_mtu_for_local_cid(uint16_t local_cid){
    l2cap_channel_t * channel = l2cap_get_channel_for_local_cid(local_cid);
    if (channel) {
        return channel->remote_mtu;
    } 
    return 0;
}

static l2cap_channel_t * l2cap_channel_for_rtx_timer(timer_source_t * ts){
    linked_list_iterator_t it;    
    linked_list_iterator_init(&it, &l2cap_channels);
    while (linked_list_iterator_has_next(&it)){
        l2cap_channel_t * channel = (l2cap_channel_t *) linked_list_iterator_next(&it);
        if ( &channel->rtx == ts) {
            return channel;
        }
    }
    return NULL;
}

static void l2cap_rtx_timeout(timer_source_t * ts){
    l2cap_channel_t * channel = l2cap_channel_for_rtx_timer(ts);
    if (!ts) return;

    log_info("l2cap_rtx_timeout for local cid 0x%02x", channel->local_cid);

    // "When terminating the channel, it is not necessary to send a L2CAP_DisconnectReq
    //  and enter WAIT_DISCONNECT state. Channels can be transitioned directly to the CLOSED state."
    // notify client
    l2cap_emit_channel_opened(channel, L2CAP_CONNECTION_RESPONSE_RESULT_RTX_TIMEOUT);

    // discard channel
    // no need to stop timer here, it is removed from list during timer callback
    linked_list_remove(&l2cap_channels, (linked_item_t *) channel);
    btstack_memory_l2cap_channel_free(channel);
}

static void l2cap_stop_rtx(l2cap_channel_t * channel){
    log_info("l2cap_stop_rtx for local cid 0x%02x", channel->local_cid);
    run_loop_remove_timer(&channel->rtx);
}

static void l2cap_start_rtx(l2cap_channel_t * channel){
    l2cap_stop_rtx(channel);
    log_info("l2cap_start_rtx for local cid 0x%02x", channel->local_cid);
    run_loop_set_timer_handler(&channel->rtx, l2cap_rtx_timeout);
    run_loop_set_timer(&channel->rtx, L2CAP_RTX_TIMEOUT_MS);
    run_loop_add_timer(&channel->rtx);
}

static void l2cap_start_ertx(l2cap_channel_t * channel){
    log_info("l2cap_start_ertx for local cid 0x%02x", channel->local_cid);
    l2cap_stop_rtx(channel);
    run_loop_set_timer_handler(&channel->rtx, l2cap_rtx_timeout);
    run_loop_set_timer(&channel->rtx, L2CAP_ERTX_TIMEOUT_MS);
    run_loop_add_timer(&channel->rtx);
}

void l2cap_require_security_level_2_for_outgoing_sdp(){
    require_security_level2_for_outgoing_sdp = 1;
}

static int l2cap_security_level_0_allowed_for_PSM(uint16_t psm){
    return (psm == PSM_SDP) && (!require_security_level2_for_outgoing_sdp);
}

int l2cap_send_signaling_packet(hci_con_handle_t handle, L2CAP_SIGNALING_COMMANDS cmd, uint8_t identifier, ...){

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

#ifdef HAVE_BLE
int l2cap_send_le_signaling_packet(hci_con_handle_t handle, L2CAP_SIGNALING_COMMANDS cmd, uint8_t identifier, ...){

    if (!hci_can_send_acl_packet_now(handle)){
        log_info("l2cap_send_signaling_packet, cannot send");
        return BTSTACK_ACL_BUFFERS_FULL;
    }
    
    // log_info("l2cap_send_signaling_packet type %u", cmd);
    hci_reserve_packet_buffer();
    uint8_t *acl_buffer = hci_get_outgoing_packet_buffer();
    va_list argptr;
    va_start(argptr, identifier);
    uint16_t len = l2cap_create_signaling_le(acl_buffer, handle, cmd, identifier, argptr);
    va_end(argptr);
    // log_info("l2cap_send_signaling_packet con %u!", handle);
    return hci_send_acl_packet_buffer(len);
}
#endif

uint8_t *l2cap_get_outgoing_buffer(void){
    return hci_get_outgoing_packet_buffer() + COMPLETE_L2CAP_HEADER; // 8 bytes
}

int l2cap_reserve_packet_buffer(void){
    return hci_reserve_packet_buffer();
}

void l2cap_release_packet_buffer(void){
    hci_release_packet_buffer();
}


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

    if (channel->packets_granted == 0){
        log_error("l2cap_send_prepared cid 0x%02x, no credits!", local_cid);
        return -1;  // TODO: define error
    }

    if (!hci_can_send_prepared_acl_packet_now(channel->handle)){
        log_info("l2cap_send_prepared cid 0x%02x, cannot send", local_cid);
        return BTSTACK_ACL_BUFFERS_FULL;
    }
    
    --channel->packets_granted;

    log_debug("l2cap_send_prepared cid 0x%02x, handle %u, 1 credit used, credits left %u;",
                  local_cid, channel->handle, channel->packets_granted);
    
    uint8_t *acl_buffer = hci_get_outgoing_packet_buffer();

    int pb = hci_non_flushable_packet_boundary_flag_supported() ? 0x00 : 0x02;

    // 0 - Connection handle : PB=pb : BC=00 
    bt_store_16(acl_buffer, 0, channel->handle | (pb << 12) | (0 << 14));
    // 2 - ACL length
    bt_store_16(acl_buffer, 2,  len + 4);
    // 4 - L2CAP packet length
    bt_store_16(acl_buffer, 4,  len + 0);
    // 6 - L2CAP channel DEST
    bt_store_16(acl_buffer, 6, channel->remote_cid);    
    // send
    int err = hci_send_acl_packet_buffer(len+8);
    
    l2cap_hand_out_credits();
    
    return err;
}

int l2cap_send_prepared_connectionless(uint16_t handle, uint16_t cid, uint16_t len){
    
    if (!hci_is_packet_buffer_reserved()){
        log_error("l2cap_send_prepared_connectionless called without reserving packet first");
        return BTSTACK_ACL_BUFFERS_FULL;
    }

    if (!hci_can_send_prepared_acl_packet_now(handle)){
        log_info("l2cap_send_prepared_connectionless handle 0x%02x, cid 0x%02x, cannot send", handle, cid);
        return BTSTACK_ACL_BUFFERS_FULL;
    }
    
    log_debug("l2cap_send_prepared_connectionless handle %u, cid 0x%02x", handle, cid);
    
    uint8_t *acl_buffer = hci_get_outgoing_packet_buffer();
    
    int pb = hci_non_flushable_packet_boundary_flag_supported() ? 0x00 : 0x02;

    // 0 - Connection handle : PB=pb : BC=00 
    bt_store_16(acl_buffer, 0, handle | (pb << 12) | (0 << 14));
    // 2 - ACL length
    bt_store_16(acl_buffer, 2,  len + 4);
    // 4 - L2CAP packet length
    bt_store_16(acl_buffer, 4,  len + 0);
    // 6 - L2CAP channel DEST
    bt_store_16(acl_buffer, 6, cid);    
    // send
    int err = hci_send_acl_packet_buffer(len+8);
    
    l2cap_hand_out_credits();

    return err;
}

int l2cap_send_internal(uint16_t local_cid, uint8_t *data, uint16_t len){

    l2cap_channel_t * channel = l2cap_get_channel_for_local_cid(local_cid);
    if (!channel) {
        log_error("l2cap_send_internal no channel for cid 0x%02x", local_cid);
        return -1;   // TODO: define error
    }

    if (len > channel->remote_mtu){
        log_error("l2cap_send_internal cid 0x%02x, data length exceeds remote MTU.", local_cid);
        return L2CAP_DATA_LEN_EXCEEDS_REMOTE_MTU;
    }

    if (!hci_can_send_acl_packet_now(channel->handle)){
        log_info("l2cap_send_internal cid 0x%02x, cannot send", local_cid);
        return BTSTACK_ACL_BUFFERS_FULL;
    }

    hci_reserve_packet_buffer();
    uint8_t *acl_buffer = hci_get_outgoing_packet_buffer();

    memcpy(&acl_buffer[8], data, len);

    return l2cap_send_prepared(local_cid, len);
}

int l2cap_send_connectionless(uint16_t handle, uint16_t cid, uint8_t *data, uint16_t len){
    
    if (!hci_can_send_acl_packet_now(handle)){
        log_info("l2cap_send_internal cid 0x%02x, cannot send", cid);
        return BTSTACK_ACL_BUFFERS_FULL;
    }
    
    hci_reserve_packet_buffer();
    uint8_t *acl_buffer = hci_get_outgoing_packet_buffer();
    
    memcpy(&acl_buffer[8], data, len);
    
    return l2cap_send_prepared_connectionless(handle, cid, len);
}

int l2cap_send_echo_request(uint16_t handle, uint8_t *data, uint16_t len){
    return l2cap_send_signaling_packet(handle, ECHO_REQUEST, 0x77, len, data);
}

static inline void channelStateVarSetFlag(l2cap_channel_t *channel, L2CAP_CHANNEL_STATE_VAR flag){
    channel->state_var = (L2CAP_CHANNEL_STATE_VAR) (channel->state_var | flag);
}

static inline void channelStateVarClearFlag(l2cap_channel_t *channel, L2CAP_CHANNEL_STATE_VAR flag){
    channel->state_var = (L2CAP_CHANNEL_STATE_VAR) (channel->state_var & ~flag);
}



// MARK: L2CAP_RUN
// process outstanding signaling tasks
void l2cap_run(void){
    
    // check pending signaling responses
    while (signaling_responses_pending){
        
        hci_con_handle_t handle = signaling_responses[0].handle;
        
        if (!hci_can_send_acl_packet_now(handle)) break;

        uint8_t  sig_id = signaling_responses[0].sig_id;
        uint16_t infoType = signaling_responses[0].data;    // INFORMATION_REQUEST
        uint16_t result   = signaling_responses[0].data;    // CONNECTION_REQUEST, COMMAND_REJECT
        uint8_t  response_code = signaling_responses[0].code;

        // remove first item before sending (to avoid sending response mutliple times)
        signaling_responses_pending--;
        int i;
        for (i=0; i < signaling_responses_pending; i++){
            memcpy(&signaling_responses[i], &signaling_responses[i+1], sizeof(l2cap_signaling_response_t));
        }

        switch (response_code){
            case CONNECTION_REQUEST:
                l2cap_send_signaling_packet(handle, CONNECTION_RESPONSE, sig_id, 0, 0, result, 0);
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
                        break;
                    }
                    case 2: { // Extended Features Supported
                        // extended features request supported, features: fixed channels, unicast connectionless data reception
                        uint32_t features = 0x280;
                        l2cap_send_signaling_packet(handle, INFORMATION_RESPONSE, sig_id, infoType, 0, sizeof(features), &features);
                        break;
                    }
                    case 3: { // Fixed Channels Supported
                        uint8_t map[8];
                        memset(map, 0, 8);
                        map[0] = 0x01;  // L2CAP Signaling Channel (0x01) + Connectionless reception (0x02)
                        l2cap_send_signaling_packet(handle, INFORMATION_RESPONSE, sig_id, infoType, 0, sizeof(map), &map);
                        break;
                    }
                    default:
                        // all other types are not supported
                        l2cap_send_signaling_packet(handle, INFORMATION_RESPONSE, sig_id, infoType, 1, 0, NULL);
                        break;                        
                }
                break;
            case COMMAND_REJECT:
                l2cap_send_signaling_packet(handle, COMMAND_REJECT, sig_id, result, 0, NULL);
#ifdef HAVE_BLE
            case COMMAND_REJECT_LE:
                l2cap_send_le_signaling_packet(handle, COMMAND_REJECT, sig_id, result, 0, NULL);
                break;
#endif
            default:
                // should not happen
                break;
        }
    }
    
    uint8_t  config_options[4];
    linked_list_iterator_t it;    
    linked_list_iterator_init(&it, &l2cap_channels);
    while (linked_list_iterator_has_next(&it)){

        l2cap_channel_t * channel = (l2cap_channel_t *) linked_list_iterator_next(&it);
        // log_info("l2cap_run: state %u, var 0x%02x", channel->state, channel->state_var);
        switch (channel->state){

            case L2CAP_STATE_WAIT_INCOMING_SECURITY_LEVEL_UPDATE:
            case L2CAP_STATE_WAIT_CLIENT_ACCEPT_OR_REJECT:
                if (!hci_can_send_acl_packet_now(channel->handle)) break;
                if (channel->state_var & L2CAP_CHANNEL_STATE_VAR_SEND_CONN_RESP_PEND) {
                    channelStateVarClearFlag(channel, L2CAP_CHANNEL_STATE_VAR_SEND_CONN_RESP_PEND);
                    l2cap_send_signaling_packet(channel->handle, CONNECTION_RESPONSE, channel->remote_sig_id, channel->local_cid, channel->remote_cid, 1, 0);
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
                if (!hci_can_send_acl_packet_now(channel->handle)) break;
                l2cap_send_signaling_packet(channel->handle, CONNECTION_RESPONSE, channel->remote_sig_id, channel->local_cid, channel->remote_cid, channel->reason, 0);
                // discard channel - l2cap_finialize_channel_close without sending l2cap close event
                l2cap_stop_rtx(channel);
                linked_list_iterator_remove(&it);
                btstack_memory_l2cap_channel_free(channel); 
                break;
                
            case L2CAP_STATE_WILL_SEND_CONNECTION_RESPONSE_ACCEPT:
                if (!hci_can_send_acl_packet_now(channel->handle)) break;
                channel->state = L2CAP_STATE_CONFIG;
                channelStateVarSetFlag(channel, L2CAP_CHANNEL_STATE_VAR_SEND_CONF_REQ);
                l2cap_send_signaling_packet(channel->handle, CONNECTION_RESPONSE, channel->remote_sig_id, channel->local_cid, channel->remote_cid, 0, 0);
                break;
                
            case L2CAP_STATE_WILL_SEND_CONNECTION_REQUEST:
                if (!hci_can_send_acl_packet_now(channel->handle)) break;
                // success, start l2cap handshake
                channel->local_sig_id = l2cap_next_sig_id();
                channel->state = L2CAP_STATE_WAIT_CONNECT_RSP;
                l2cap_send_signaling_packet( channel->handle, CONNECTION_REQUEST, channel->local_sig_id, channel->psm, channel->local_cid);
                l2cap_start_rtx(channel);
                break;
            
            case L2CAP_STATE_CONFIG:
                if (!hci_can_send_acl_packet_now(channel->handle)) break;
                if (channel->state_var & L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP){
                    uint16_t flags = 0;
                    channelStateVarClearFlag(channel, L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP);
                    if (channel->state_var & L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP_CONT) {
                        flags = 1;
                    } else {
                        channelStateVarSetFlag(channel, L2CAP_CHANNEL_STATE_VAR_SENT_CONF_RSP);
                    }
                    if (channel->state_var & L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP_INVALID){
                        l2cap_send_signaling_packet(channel->handle, CONFIGURE_RESPONSE, channel->remote_sig_id, channel->remote_cid, flags, L2CAP_CONF_RESULT_UNKNOWN_OPTIONS, 0, NULL);
                    } else if (channel->state_var & L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP_MTU){
                        config_options[0] = 1; // MTU
                        config_options[1] = 2; // len param
                        bt_store_16( (uint8_t*)&config_options, 2, channel->remote_mtu);
                        l2cap_send_signaling_packet(channel->handle, CONFIGURE_RESPONSE, channel->remote_sig_id, channel->remote_cid, flags, 0, 4, &config_options);
                        channelStateVarClearFlag(channel,L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP_MTU);
                    } else {
                        l2cap_send_signaling_packet(channel->handle, CONFIGURE_RESPONSE, channel->remote_sig_id, channel->remote_cid, flags, 0, 0, NULL);
                    }
                    channelStateVarClearFlag(channel, L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP_CONT);
                }
                else if (channel->state_var & L2CAP_CHANNEL_STATE_VAR_SEND_CONF_REQ){
                    channelStateVarClearFlag(channel, L2CAP_CHANNEL_STATE_VAR_SEND_CONF_REQ);
                    channelStateVarSetFlag(channel, L2CAP_CHANNEL_STATE_VAR_SENT_CONF_REQ);
                    channel->local_sig_id = l2cap_next_sig_id();
                    config_options[0] = 1; // MTU
                    config_options[1] = 2; // len param
                    bt_store_16( (uint8_t*)&config_options, 2, channel->local_mtu);
                    l2cap_send_signaling_packet(channel->handle, CONFIGURE_REQUEST, channel->local_sig_id, channel->remote_cid, 0, 4, &config_options);
                    l2cap_start_rtx(channel);
                }
                if (l2cap_channel_ready_for_open(channel)){
                    channel->state = L2CAP_STATE_OPEN;
                    l2cap_emit_channel_opened(channel, 0);  // success
                    l2cap_emit_credits(channel, 1);
                }
                break;

            case L2CAP_STATE_WILL_SEND_DISCONNECT_RESPONSE:
                if (!hci_can_send_acl_packet_now(channel->handle)) break;
                l2cap_send_signaling_packet( channel->handle, DISCONNECTION_RESPONSE, channel->remote_sig_id, channel->local_cid, channel->remote_cid);   
                // we don't start an RTX timer for a disconnect - there's no point in closing the channel if the other side doesn't respond :)
                l2cap_finialize_channel_close(channel);  // -- remove from list
                break;
                
            case L2CAP_STATE_WILL_SEND_DISCONNECT_REQUEST:
                if (!hci_can_send_acl_packet_now(channel->handle)) break;
                channel->local_sig_id = l2cap_next_sig_id();
                channel->state = L2CAP_STATE_WAIT_DISCONNECT;
                l2cap_send_signaling_packet( channel->handle, DISCONNECTION_REQUEST, channel->local_sig_id, channel->remote_cid, channel->local_cid);   
                break;
            default:
                break;
        }
    }

#ifdef HAVE_BLE
    // send l2cap con paramter update if necessary
    hci_connections_get_iterator(&it);
    while(linked_list_iterator_has_next(&it)){
        hci_connection_t * connection = (hci_connection_t *) linked_list_iterator_next(&it);
        int result;

        switch (connection->le_con_parameter_update_state){
            case CON_PARAMETER_UPDATE_SEND_RESPONSE:
                result = 0;
                break;
            case CON_PARAMETER_UPDATE_DENY:
                result = 1;
                break;
            default:
                result = -1;
                break;
        }
        if (result < 0) break;
        
        if (!hci_can_send_acl_packet_now(connection->con_handle)) break;
        hci_reserve_packet_buffer();
        uint8_t *acl_buffer = hci_get_outgoing_packet_buffer();
        connection->le_con_parameter_update_state = CON_PARAMETER_UPDATE_CHANGE_HCI_CON_PARAMETERS;
        uint16_t len = l2cap_le_create_connection_parameter_update_response(acl_buffer, connection->con_handle, 0);
        hci_send_acl_packet_buffer(len);
    }
#endif

}

uint16_t l2cap_max_mtu(void){
    return HCI_ACL_PAYLOAD_SIZE - L2CAP_HEADER_SIZE;
}

uint16_t l2cap_max_le_mtu(){
    return l2cap_max_mtu();
}

static void l2cap_handle_connection_complete(uint16_t handle, l2cap_channel_t * channel){
    if (channel->state == L2CAP_STATE_WAIT_CONNECTION_COMPLETE || channel->state == L2CAP_STATE_WILL_SEND_CREATE_CONNECTION) {
        log_info("l2cap_handle_connection_complete expected state");
        // success, start l2cap handshake
        channel->handle = handle;
        channel->local_cid = l2cap_next_local_cid();
        // check remote SSP feature first
        channel->state = L2CAP_STATE_WAIT_REMOTE_SUPPORTED_FEATURES;
    }
}

static void l2cap_handle_remote_supported_features_received(l2cap_channel_t * channel){
    if (channel->state != L2CAP_STATE_WAIT_REMOTE_SUPPORTED_FEATURES) return;

    // we have been waiting for remote supported features, if both support SSP, 
    log_info("l2cap received remote supported features, sec_level_0_allowed for psm %u = %u", channel->psm, l2cap_security_level_0_allowed_for_PSM(channel->psm));
    if (hci_ssp_supported_on_both_sides(channel->handle) && !l2cap_security_level_0_allowed_for_PSM(channel->psm)){
        // request security level 2
        channel->state = L2CAP_STATE_WAIT_OUTGOING_SECURITY_LEVEL_UPDATE;
        gap_request_security_level(channel->handle, LEVEL_2);
        return;
    }
    // fine, go ahead
    channel->state = L2CAP_STATE_WILL_SEND_CONNECTION_REQUEST;
}

// open outgoing L2CAP channel
void l2cap_create_channel_internal(void * connection, btstack_packet_handler_t packet_handler,
                                   bd_addr_t address, uint16_t psm, uint16_t mtu){
    
    log_info("L2CAP_CREATE_CHANNEL_MTU addr %s psm 0x%x mtu %u", bd_addr_to_str(address), psm, mtu);
    
    // alloc structure
    l2cap_channel_t * chan = btstack_memory_l2cap_channel_get();
    if (!chan) {
        // emit error event
        l2cap_channel_t dummy_channel;
        BD_ADDR_COPY(dummy_channel.address, address);
        dummy_channel.psm = psm;
        l2cap_emit_channel_opened(&dummy_channel, BTSTACK_MEMORY_ALLOC_FAILED);
        return;
    }
    // Init memory (make valgrind happy)
    memset(chan, 0, sizeof(l2cap_channel_t));
    // limit local mtu to max acl packet length - l2cap header
    if (mtu > l2cap_max_mtu()) {
        mtu = l2cap_max_mtu();
    }
        
    // fill in 
    BD_ADDR_COPY(chan->address, address);
    chan->psm = psm;
    chan->handle = 0;
    chan->connection = connection;
    chan->packet_handler = packet_handler;
    chan->remote_mtu = L2CAP_MINIMAL_MTU;
    chan->local_mtu = mtu;
    chan->packets_granted = 0;
    
    // set initial state
    chan->state = L2CAP_STATE_WILL_SEND_CREATE_CONNECTION;
    chan->state_var = L2CAP_CHANNEL_STATE_VAR_NONE;
    chan->remote_sig_id = L2CAP_SIG_ID_INVALID;
    chan->local_sig_id = L2CAP_SIG_ID_INVALID;
    chan->required_security_level = LEVEL_0;

    // add to connections list
    linked_list_add(&l2cap_channels, (linked_item_t *) chan);
    
    // check if hci connection is already usable
    hci_connection_t * conn = hci_connection_for_bd_addr_and_type(address, BD_ADDR_TYPE_CLASSIC);
    if (conn){
        log_info("l2cap_create_channel_internal, hci connection already exists");
        l2cap_handle_connection_complete(conn->con_handle, chan);
        // check ir remote supported fearures are already received
        if (conn->bonding_flags & BONDING_RECEIVED_REMOTE_FEATURES) {
            l2cap_handle_remote_supported_features_received(chan);
        }
    }

    l2cap_run();
}

void l2cap_disconnect_internal(uint16_t local_cid, uint8_t reason){
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
    linked_list_iterator_t it;
    linked_list_iterator_init(&it, &l2cap_channels);
    while (linked_list_iterator_has_next(&it)){
        l2cap_channel_t * channel = (l2cap_channel_t *) linked_list_iterator_next(&it);
        if ( BD_ADDR_CMP( channel->address, address) != 0) continue;
        // channel for this address found
        switch (channel->state){
            case L2CAP_STATE_WAIT_CONNECTION_COMPLETE:
            case L2CAP_STATE_WILL_SEND_CREATE_CONNECTION:
                // failure, forward error code
                l2cap_emit_channel_opened(channel, status);
                // discard channel
                l2cap_stop_rtx(channel);
                linked_list_iterator_remove(&it);
                btstack_memory_l2cap_channel_free(channel);
                break;
            default:
                break;               
        }
    }
}

static void l2cap_handle_connection_success_for_addr(bd_addr_t address, hci_con_handle_t handle){
    linked_list_iterator_t it;
    linked_list_iterator_init(&it, &l2cap_channels);
    while (linked_list_iterator_has_next(&it)){
        l2cap_channel_t * channel = (l2cap_channel_t *) linked_list_iterator_next(&it);
        if ( ! BD_ADDR_CMP( channel->address, address) ){
            l2cap_handle_connection_complete(handle, channel);
        }
    }
    // process
    l2cap_run();
}

void l2cap_event_handler(uint8_t *packet, uint16_t size){
    
    bd_addr_t address;
    hci_con_handle_t handle;
    linked_list_iterator_t it;
    int hci_con_used;
    
    switch(packet[0]){
            
        // handle connection complete events
        case HCI_EVENT_CONNECTION_COMPLETE:
            bt_flip_addr(address, &packet[5]);
            if (packet[2] == 0){
                handle = READ_BT_16(packet, 3);
                l2cap_handle_connection_success_for_addr(address, handle);
            } else {
                l2cap_handle_connection_failed_for_addr(address, packet[2]);
            }
            break;
            
        // handle successful create connection cancel command
        case HCI_EVENT_COMMAND_COMPLETE:
            if ( COMMAND_COMPLETE_EVENT(packet, hci_create_connection_cancel) ) {
                if (packet[5] == 0){
                    bt_flip_addr(address, &packet[6]);
                    // CONNECTION TERMINATED BY LOCAL HOST (0X16)
                    l2cap_handle_connection_failed_for_addr(address, 0x16);
                }
            }
            l2cap_run();    // try sending signaling packets first
            break;
            
        case HCI_EVENT_COMMAND_STATUS:
            l2cap_run();    // try sending signaling packets first
            break;
            
        // handle disconnection complete events
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            // send l2cap disconnect events for all channels on this handle and free them
            handle = READ_BT_16(packet, 3);
            linked_list_iterator_init(&it, &l2cap_channels);
            while (linked_list_iterator_has_next(&it)){
                l2cap_channel_t * channel = (l2cap_channel_t *) linked_list_iterator_next(&it);
                if (channel->handle != handle) continue;
                l2cap_emit_channel_closed(channel);
                l2cap_stop_rtx(channel);
                linked_list_iterator_remove(&it);
                btstack_memory_l2cap_channel_free(channel);
            }
            break;
            
        case HCI_EVENT_NUMBER_OF_COMPLETED_PACKETS:
            l2cap_run();    // try sending signaling packets first
            l2cap_hand_out_credits();
            break;
            
        // HCI Connection Timeouts
        case L2CAP_EVENT_TIMEOUT_CHECK:
            handle = READ_BT_16(packet, 2);
            if (hci_authentication_active_for_handle(handle)) break;
            hci_con_used = 0;
            linked_list_iterator_init(&it, &l2cap_channels);
            while (linked_list_iterator_has_next(&it)){
                l2cap_channel_t * channel = (l2cap_channel_t *) linked_list_iterator_next(&it);
                if (channel->handle != handle) continue;
                hci_con_used = 1;
                break;
            }
            if (hci_con_used) break;
            if (!hci_can_send_command_packet_now()) break;
            hci_send_cmd(&hci_disconnect, handle, 0x13); // remote closed connection             
            break;

        case DAEMON_EVENT_HCI_PACKET_SENT:
            linked_list_iterator_init(&it, &l2cap_channels);
            while (linked_list_iterator_has_next(&it)){
                l2cap_channel_t * channel = (l2cap_channel_t *) linked_list_iterator_next(&it);
                if (!channel->packet_handler) continue;
                (* (channel->packet_handler))(HCI_EVENT_PACKET, channel->local_cid, packet, size);
            }
            if (attribute_protocol_packet_handler) {
                (*attribute_protocol_packet_handler)(HCI_EVENT_PACKET, 0, packet, size);
            }
            if (security_protocol_packet_handler) {
                (*security_protocol_packet_handler)(HCI_EVENT_PACKET, 0, packet, size);
            }
            if (connectionless_channel_packet_handler) {
                (*connectionless_channel_packet_handler)(HCI_EVENT_PACKET, 0, packet, size);
            }
            break;

        case HCI_EVENT_READ_REMOTE_SUPPORTED_FEATURES_COMPLETE:
            handle = READ_BT_16(packet, 3);
            linked_list_iterator_init(&it, &l2cap_channels);
            while (linked_list_iterator_has_next(&it)){
                l2cap_channel_t * channel = (l2cap_channel_t *) linked_list_iterator_next(&it);
                if (channel->handle != handle) continue;
                l2cap_handle_remote_supported_features_received(channel);
                break;
            }
            break;           

        case GAP_SECURITY_LEVEL:
            handle = READ_BT_16(packet, 2);
            log_info("l2cap - security level update");
            linked_list_iterator_init(&it, &l2cap_channels);
            while (linked_list_iterator_has_next(&it)){
                l2cap_channel_t * channel = (l2cap_channel_t *) linked_list_iterator_next(&it);
                if (channel->handle != handle) continue;

                log_info("l2cap - state %u", channel->state);

                gap_security_level_t actual_level = packet[4];
                gap_security_level_t required_level = channel->required_security_level;

                switch (channel->state){
                    case L2CAP_STATE_WAIT_INCOMING_SECURITY_LEVEL_UPDATE:
                        if (actual_level >= required_level){
                            channel->state = L2CAP_STATE_WAIT_CLIENT_ACCEPT_OR_REJECT;
                            l2cap_emit_connection_request(channel);                
                        } else {
                            channel->reason = 0x03; // security block
                            channel->state = L2CAP_STATE_WILL_SEND_CONNECTION_RESPONSE_DECLINE;
                        }
                        break;

                    case L2CAP_STATE_WAIT_OUTGOING_SECURITY_LEVEL_UPDATE:
                        if (actual_level >= required_level){
                            channel->state = L2CAP_STATE_WILL_SEND_CONNECTION_REQUEST;
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
            
        default:
            break;
    }
    
    // pass on: main packet handler, att and sm packet handlers
    (*packet_handler)(NULL, HCI_EVENT_PACKET, 0, packet, size);
    if (attribute_protocol_packet_handler){
        (*attribute_protocol_packet_handler)(HCI_EVENT_PACKET, 0, packet, size);
    } 
    if (security_protocol_packet_handler) {
        (*security_protocol_packet_handler)(HCI_EVENT_PACKET, 0, packet, size);
    }
    if (connectionless_channel_packet_handler) {
        (*connectionless_channel_packet_handler)(HCI_EVENT_PACKET, 0, packet, size);
    }

    l2cap_run();
}

static void l2cap_handle_disconnect_request(l2cap_channel_t *channel, uint16_t identifier){
    channel->remote_sig_id = identifier;
    channel->state = L2CAP_STATE_WILL_SEND_DISCONNECT_RESPONSE;
    l2cap_run();
}

static void l2cap_register_signaling_response(hci_con_handle_t handle, uint8_t code, uint8_t sig_id, uint16_t data){
    // Vol 3, Part A, 4.3: "The DCID and SCID fields shall be ignored when the result field indi- cates the connection was refused."
    if (signaling_responses_pending < NR_PENDING_SIGNALING_RESPONSES) {
        signaling_responses[signaling_responses_pending].handle = handle;
        signaling_responses[signaling_responses_pending].code = code;
        signaling_responses[signaling_responses_pending].sig_id = sig_id;
        signaling_responses[signaling_responses_pending].data = data;
        signaling_responses_pending++;
        l2cap_run();
    }
}

static void l2cap_handle_connection_request(hci_con_handle_t handle, uint8_t sig_id, uint16_t psm, uint16_t source_cid){
    
    // log_info("l2cap_handle_connection_request for handle %u, psm %u cid 0x%02x", handle, psm, source_cid);
    l2cap_service_t *service = l2cap_get_service(psm);
    if (!service) {
        // 0x0002 PSM not supported
        l2cap_register_signaling_response(handle, CONNECTION_REQUEST, sig_id, 0x0002);
        return;
    }
    
    hci_connection_t * hci_connection = hci_connection_for_handle( handle );
    if (!hci_connection) {
        // 
        log_error("no hci_connection for handle %u", handle);
        return;
    }

    // reject connection (0x03 security block) and disconnect if both have SSP, connection is not encrypted and PSM != SDP
    if (  hci_ssp_supported_on_both_sides(handle)
    &&    gap_security_level(handle) == LEVEL_0
    &&   !l2cap_security_level_0_allowed_for_PSM(psm)){

        // 0x0003 Security Block
        l2cap_register_signaling_response(handle, CONNECTION_REQUEST, sig_id, 0x0003);
        return;
    }


    // alloc structure
    // log_info("l2cap_handle_connection_request register channel");
    l2cap_channel_t * channel = btstack_memory_l2cap_channel_get();
    if (!channel){
        // 0x0004 No resources available
        l2cap_register_signaling_response(handle, CONNECTION_REQUEST, sig_id, 0x0004);
        return;
    }
    // Init memory (make valgrind happy)
    memset(channel, 0, sizeof(l2cap_channel_t));
    // fill in 
    BD_ADDR_COPY(channel->address, hci_connection->address);
    channel->psm = psm;
    channel->handle = handle;
    channel->connection = service->connection;
    channel->packet_handler = service->packet_handler;
    channel->local_cid  = l2cap_next_local_cid();
    channel->remote_cid = source_cid;
    channel->local_mtu  = service->mtu;
    channel->remote_mtu = L2CAP_DEFAULT_MTU;
    channel->packets_granted = 0;
    channel->remote_sig_id = sig_id; 
    channel->required_security_level = service->required_security_level;

    // limit local mtu to max acl packet length - l2cap header
    if (channel->local_mtu > l2cap_max_mtu()) {
        channel->local_mtu = l2cap_max_mtu();
    }
    
    // set initial state
    channel->state =     L2CAP_STATE_WAIT_INCOMING_SECURITY_LEVEL_UPDATE;
    channel->state_var = L2CAP_CHANNEL_STATE_VAR_SEND_CONN_RESP_PEND;
    
    // add to connections list
    linked_list_add(&l2cap_channels, (linked_item_t *) channel);

    // assert security requirements
    gap_request_security_level(handle, channel->required_security_level);
}

void l2cap_accept_connection_internal(uint16_t local_cid){
    log_info("L2CAP_ACCEPT_CONNECTION local_cid 0x%x", local_cid);
    l2cap_channel_t * channel = l2cap_get_channel_for_local_cid(local_cid);
    if (!channel) {
        log_error("l2cap_accept_connection_internal called but local_cid 0x%x not found", local_cid);
        return;
    }

    channel->state = L2CAP_STATE_WILL_SEND_CONNECTION_RESPONSE_ACCEPT;

    // process
    l2cap_run();
}

void l2cap_decline_connection_internal(uint16_t local_cid, uint8_t reason){
    log_info("L2CAP_DECLINE_CONNECTION local_cid 0x%x, reason %x", local_cid, reason);
    l2cap_channel_t * channel = l2cap_get_channel_for_local_cid( local_cid);
    if (!channel) {
        log_error( "l2cap_decline_connection_internal called but local_cid 0x%x not found", local_cid);
        return;
    }
    channel->state  = L2CAP_STATE_WILL_SEND_CONNECTION_RESPONSE_DECLINE;
    channel->reason = reason;
    l2cap_run();
}

void l2cap_signaling_handle_configure_request(l2cap_channel_t *channel, uint8_t *command){

    channel->remote_sig_id = command[L2CAP_SIGNALING_COMMAND_SIGID_OFFSET];

    uint16_t flags = READ_BT_16(command, 6);
    if (flags & 1) {
        channelStateVarSetFlag(channel, L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP_CONT);
    }

    // accept the other's configuration options
    uint16_t end_pos = 4 + READ_BT_16(command, L2CAP_SIGNALING_COMMAND_LENGTH_OFFSET);
    uint16_t pos     = 8;
    while (pos < end_pos){
        uint8_t option_hint = command[pos] >> 7;
        uint8_t option_type = command[pos] & 0x7f;
        log_info("l2cap cid %u, hint %u, type %u", channel->local_cid, option_hint, option_type);
        pos++;
        uint8_t length = command[pos++];
        // MTU { type(8): 1, len(8):2, MTU(16) }
        if (option_type == 1 && length == 2){
            channel->remote_mtu = READ_BT_16(command, pos);
            // log_info("l2cap cid 0x%02x, remote mtu %u", channel->local_cid, channel->remote_mtu);
            channelStateVarSetFlag(channel, L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP_MTU);
        }
        // Flush timeout { type(8):2, len(8): 2, Flush Timeout(16)}
        if (option_type == 2 && length == 2){
            channel->flush_timeout = READ_BT_16(command, pos);
        }
        // check for unknown options
        if (option_hint == 0 && (option_type == 0 || option_type >= 0x07)){
            log_info("l2cap cid %u, unknown options", channel->local_cid);
            channelStateVarSetFlag(channel, L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP_INVALID);
        }
        pos += length;
    }
}

static int l2cap_channel_ready_for_open(l2cap_channel_t *channel){
    // log_info("l2cap_channel_ready_for_open 0x%02x", channel->state_var);
    if ((channel->state_var & L2CAP_CHANNEL_STATE_VAR_RCVD_CONF_RSP) == 0) return 0;
    if ((channel->state_var & L2CAP_CHANNEL_STATE_VAR_SENT_CONF_RSP) == 0) return 0;
    return 1;
}


void l2cap_signaling_handler_channel(l2cap_channel_t *channel, uint8_t *command){

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
                    result = READ_BT_16 (command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET+4);
                    switch (result) {
                        case 0:
                            // successful connection
                            channel->remote_cid = READ_BT_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET);
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
                                hci_drop_link_key_for_bd_addr(channel->address);
                            }
                            
                            // discard channel
                            linked_list_remove(&l2cap_channels, (linked_item_t *) channel);
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
            result = READ_BT_16 (command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET+4);
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
                    switch (result){
                        case 0: // success
                            channelStateVarSetFlag(channel, L2CAP_CHANNEL_STATE_VAR_RCVD_CONF_RSP);
                            break;
                        case 4: // pending
                            l2cap_start_ertx(channel);
                            break;
                        default:
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
                l2cap_emit_credits(channel, 1);
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


void l2cap_signaling_handler_dispatch( hci_con_handle_t handle, uint8_t * command){
    
    // get code, signalind identifier and command len
    uint8_t code   = command[L2CAP_SIGNALING_COMMAND_CODE_OFFSET];
    uint8_t sig_id = command[L2CAP_SIGNALING_COMMAND_SIGID_OFFSET];
    
    // not for a particular channel, and not CONNECTION_REQUEST, ECHO_[REQUEST|RESPONSE], INFORMATION_REQUEST 
    if (code < 1 || code == ECHO_RESPONSE || code > INFORMATION_REQUEST){
        l2cap_register_signaling_response(handle, COMMAND_REJECT, sig_id, L2CAP_REJ_CMD_UNKNOWN);
        return;
    }

    // general commands without an assigned channel
    switch(code) {
            
        case CONNECTION_REQUEST: {
            uint16_t psm =        READ_BT_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET);
            uint16_t source_cid = READ_BT_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET+2);
            l2cap_handle_connection_request(handle, sig_id, psm, source_cid);
            return;
        }
            
        case ECHO_REQUEST:
            l2cap_register_signaling_response(handle, code, sig_id, 0);
            return;
            
        case INFORMATION_REQUEST: {
            uint16_t infoType = READ_BT_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET);
            l2cap_register_signaling_response(handle, code, sig_id, infoType);
            return;
        }
            
        default:
            break;
    }
    
    
    // Get potential destination CID
    uint16_t dest_cid = READ_BT_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET);
    
    // Find channel for this sig_id and connection handle
    linked_list_iterator_t it;    
    linked_list_iterator_init(&it, &l2cap_channels);
    while (linked_list_iterator_has_next(&it)){
        l2cap_channel_t * channel = (l2cap_channel_t *) linked_list_iterator_next(&it);
        if (channel->handle != handle) continue;
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

void l2cap_acl_handler( uint8_t *packet, uint16_t size ){
        
    // Get Channel ID
    uint16_t channel_id = READ_L2CAP_CHANNEL_ID(packet); 
    hci_con_handle_t handle = READ_ACL_CONNECTION_HANDLE(packet);
    
    switch (channel_id) {
            
        case L2CAP_CID_SIGNALING: {
            
            uint16_t command_offset = 8;
            while (command_offset < size) {                
                
                // handle signaling commands
                l2cap_signaling_handler_dispatch(handle, &packet[command_offset]);
                
                // increment command_offset
                command_offset += L2CAP_SIGNALING_COMMAND_DATA_OFFSET + READ_BT_16(packet, command_offset + L2CAP_SIGNALING_COMMAND_LENGTH_OFFSET);
            }
            break;
        }
            
        case L2CAP_CID_ATTRIBUTE_PROTOCOL:
            if (attribute_protocol_packet_handler) {
                (*attribute_protocol_packet_handler)(ATT_DATA_PACKET, handle, &packet[COMPLETE_L2CAP_HEADER], size-COMPLETE_L2CAP_HEADER);
            }
            break;

        case L2CAP_CID_SECURITY_MANAGER_PROTOCOL:
            if (security_protocol_packet_handler) {
                (*security_protocol_packet_handler)(SM_DATA_PACKET, handle, &packet[COMPLETE_L2CAP_HEADER], size-COMPLETE_L2CAP_HEADER);
            }
            break;

        case L2CAP_CID_CONNECTIONLESS_CHANNEL:
            if (connectionless_channel_packet_handler) {
                (*connectionless_channel_packet_handler)(UCD_DATA_PACKET, handle, &packet[COMPLETE_L2CAP_HEADER], size-COMPLETE_L2CAP_HEADER);
            }
            break;
        
        case L2CAP_CID_SIGNALING_LE: {
            switch (packet[8]){
                case CONNECTION_PARAMETER_UPDATE_RESPONSE: {
                    uint16_t result = READ_BT_16(packet, 12);
                    l2cap_emit_connection_parameter_update_response(handle, result);
                    break;
                }
                case CONNECTION_PARAMETER_UPDATE_REQUEST: {
                    uint8_t event[10];
                    event[0] = L2CAP_EVENT_CONNECTION_PARAMETER_UPDATE_REQUEST;
                    event[1] = 8;
                    memcpy(&event[2], &packet[12], 8);
                
                    hci_connection_t * connection = hci_connection_for_handle(handle);
                    if (connection){ 
                        int update_parameter = 1;
                        le_connection_parameter_range_t existing_range = gap_le_get_connection_parameter_range();
                        uint16_t le_conn_interval_min = READ_BT_16(packet,12);
                        uint16_t le_conn_interval_max = READ_BT_16(packet,14);
                        uint16_t le_conn_latency = READ_BT_16(packet,16);
                        uint16_t le_supervision_timeout = READ_BT_16(packet,18);

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
                    }
                
                    hci_dump_packet( HCI_EVENT_PACKET, 0, event, sizeof(event));
                    (*packet_handler)(NULL, HCI_EVENT_PACKET, 0, event, sizeof(event));

                    break;
                }
                default: {
                    uint8_t sig_id = packet[COMPLETE_L2CAP_HEADER + 1]; 
                    l2cap_register_signaling_response(handle, COMMAND_REJECT_LE, sig_id, L2CAP_REJ_CMD_UNKNOWN);
                    break;
                }
            }
            break;
        }

        default: {
            // Find channel for this channel_id and connection handle
            l2cap_channel_t * channel = l2cap_get_channel_for_local_cid(channel_id);
            if (channel) {
                l2cap_dispatch(channel, L2CAP_DATA_PACKET, &packet[COMPLETE_L2CAP_HEADER], size-COMPLETE_L2CAP_HEADER);
            }
            break;
        }
    }
}

static void l2cap_packet_handler(uint8_t packet_type, uint8_t *packet, uint16_t size){
    switch (packet_type) {
        case HCI_EVENT_PACKET:
            l2cap_event_handler(packet, size);
            break;
        case HCI_ACL_DATA_PACKET:
            l2cap_acl_handler(packet, size);
            break;
        default:
            break;
    }
    l2cap_run();
}

// finalize closed channel - l2cap_handle_disconnect_request & DISCONNECTION_RESPONSE
void l2cap_finialize_channel_close(l2cap_channel_t *channel){
    channel->state = L2CAP_STATE_CLOSED;
    l2cap_emit_channel_closed(channel);
    // discard channel
    l2cap_stop_rtx(channel);
    linked_list_remove(&l2cap_channels, (linked_item_t *) channel);
    btstack_memory_l2cap_channel_free(channel);
}

l2cap_service_t * l2cap_get_service(uint16_t psm){
    linked_list_iterator_t it;
    linked_list_iterator_init(&it, &l2cap_services);
    while (linked_list_iterator_has_next(&it)){
        l2cap_service_t * service = (l2cap_service_t *) linked_list_iterator_next(&it);
        if ( service->psm == psm){
            return service;
        };
    }
    return NULL;
}

void l2cap_register_service_internal(void *connection, btstack_packet_handler_t packet_handler, uint16_t psm, uint16_t mtu, gap_security_level_t security_level){
    
    log_info("L2CAP_REGISTER_SERVICE psm 0x%x mtu %u connection %p", psm, mtu, connection);
    
    // check for alread registered psm 
    // TODO: emit error event
    l2cap_service_t *service = l2cap_get_service(psm);
    if (service) {
        log_error("l2cap_register_service_internal: PSM %u already registered", psm);
        l2cap_emit_service_registered(connection, L2CAP_SERVICE_ALREADY_REGISTERED, psm);
        return;
    }
    
    // alloc structure
    // TODO: emit error event
    service = btstack_memory_l2cap_service_get();
    if (!service) {
        log_error("l2cap_register_service_internal: no memory for l2cap_service_t");
        l2cap_emit_service_registered(connection, BTSTACK_MEMORY_ALLOC_FAILED, psm);
        return;
    }
    
    // fill in 
    service->psm = psm;
    service->mtu = mtu;
    service->connection = connection;
    service->packet_handler = packet_handler;
    service->required_security_level = security_level;

    // add to services list
    linked_list_add(&l2cap_services, (linked_item_t *) service);
    
    // enable page scan
    hci_connectable_control(1);

    // done
    l2cap_emit_service_registered(connection, 0, psm);
}

void l2cap_unregister_service_internal(void *connection, uint16_t psm){
    
    log_info("L2CAP_UNREGISTER_SERVICE psm 0x%x", psm);

    l2cap_service_t *service = l2cap_get_service(psm);
    if (!service) return;
    linked_list_remove(&l2cap_services, (linked_item_t *) service);
    btstack_memory_l2cap_service_free(service);
    
    // disable page scan when no services registered
    if (!linked_list_empty(&l2cap_services)) return;
    hci_connectable_control(0);
}


// Bluetooth 4.0 - allows to register handler for Attribute Protocol and Security Manager Protocol
void l2cap_register_fixed_channel(btstack_packet_handler_t packet_handler, uint16_t channel_id) {
    switch(channel_id){
        case L2CAP_CID_ATTRIBUTE_PROTOCOL:
            attribute_protocol_packet_handler = packet_handler;
            break;
        case L2CAP_CID_SECURITY_MANAGER_PROTOCOL:
            security_protocol_packet_handler = packet_handler;
            break;
        case L2CAP_CID_CONNECTIONLESS_CHANNEL:
            connectionless_channel_packet_handler = packet_handler;
            break;
    }
}

#ifdef HAVE_BLE

// Request LE connection parameter update
int l2cap_le_request_connection_parameter_update(uint16_t handle, uint16_t interval_min, uint16_t interval_max, uint16_t slave_latency, uint16_t timeout_multiplier){
    if (!hci_can_send_acl_packet_now(handle)){
        log_info("l2cap_send_signaling_packet, cannot send");
        return BTSTACK_ACL_BUFFERS_FULL;
    }
    // log_info("l2cap_send_signaling_packet type %u", cmd);
    hci_reserve_packet_buffer();
    uint8_t *acl_buffer = hci_get_outgoing_packet_buffer();
    uint16_t len = l2cap_le_create_connection_parameter_update_request(acl_buffer, handle, interval_min, interval_max, slave_latency, timeout_multiplier);
    return hci_send_acl_packet_buffer(len);
}
#endif

