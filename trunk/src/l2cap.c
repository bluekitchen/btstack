/*
 * Copyright (C) 2009 by Matthias Ringwald
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

#include <stdarg.h>
#include <string.h>

#include <stdio.h>

// size of HCI ACL + L2CAP Header for regular data packets
#define COMPLETE_L2CAP_HEADER 8

// minimum signaling MTU
#define L2CAP_MINIMAL_MTU 48
#define L2CAP_DEFAULT_MTU 672

// nr of buffered acl packets in outgoing queue to get max performance 
#define NR_BUFFERED_ACL_PACKETS 3

// offsets for L2CAP SIGNALING COMMANDS
#define L2CAP_SIGNALING_COMMAND_CODE_OFFSET   0
#define L2CAP_SIGNALING_COMMAND_SIGID_OFFSET  1
#define L2CAP_SIGNALING_COMMAND_LENGTH_OFFSET 2
#define L2CAP_SIGNALING_COMMAND_DATA_OFFSET   4

static void null_packet_handler(void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void l2cap_packet_handler(uint8_t packet_type, uint8_t *packet, uint16_t size);

static uint8_t * sig_buffer = NULL;
static linked_list_t l2cap_channels = NULL;
static linked_list_t l2cap_services = NULL;
static uint8_t * acl_buffer = NULL;
static void (*packet_handler) (void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) = null_packet_handler;
static int new_credits_blocked = 0;

void l2cap_init(){
    sig_buffer = malloc( L2CAP_MINIMAL_MTU );
    acl_buffer = malloc( HCI_ACL_3DH5_SIZE); 

    new_credits_blocked = 0;

    // 
    // register callback with HCI
    //
    hci_register_packet_handler(&l2cap_packet_handler);
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
    uint8_t event[21];
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
    hci_dump_packet( HCI_EVENT_PACKET, 0, event, sizeof(event));
    l2cap_dispatch(channel, HCI_EVENT_PACKET, event, sizeof(event));
}

void l2cap_emit_channel_closed(l2cap_channel_t *channel) {
    uint8_t event[4];
    event[0] = L2CAP_EVENT_CHANNEL_CLOSED;
    event[1] = sizeof(event) - 2;
    bt_store_16(event, 2, channel->local_cid);
    hci_dump_packet( HCI_EVENT_PACKET, 0, event, sizeof(event));
    l2cap_dispatch(channel, HCI_EVENT_PACKET, event, sizeof(event));
}

void l2cap_emit_connection_request(l2cap_channel_t *channel) {
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

void l2cap_emit_credits(l2cap_channel_t *channel, uint8_t credits) {
    // track credits
    channel->packets_granted += credits;
    // log_dbg("l2cap_emit_credits for cid %u, credits given: %u (+%u)\n", channel->local_cid, channel->packets_granted, credits);
    
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
    
    linked_item_t *it;
    for (it = (linked_item_t *) l2cap_channels; it ; it = it->next){
        if (!hci_number_free_acl_slots()) return;
        l2cap_channel_t * channel = (l2cap_channel_t *) it;
        if (channel->state != L2CAP_STATE_OPEN) continue;
        if (hci_number_outgoing_packets(channel->handle) < NR_BUFFERED_ACL_PACKETS && channel->packets_granted == 0) {
            l2cap_emit_credits(channel, 1);
        }
    }
}

l2cap_channel_t * l2cap_get_channel_for_local_cid(uint16_t local_cid){
    linked_item_t *it;
    for (it = (linked_item_t *) l2cap_channels; it ; it = it->next){
        l2cap_channel_t * channel = (l2cap_channel_t *) it;
        if ( channel->local_cid == local_cid) {
            return channel;
        }
    }
    return NULL;
}

uint16_t l2cap_get_remote_mtu_for_local_cid(uint16_t local_cid){
    l2cap_channel_t * channel = l2cap_get_channel_for_local_cid(local_cid);
    if (channel) {
        return channel->remote_mtu;
    } 
    return 0;
}

int l2cap_send_signaling_packet(hci_con_handle_t handle, L2CAP_SIGNALING_COMMANDS cmd, uint8_t identifier, ...){
    // log_dbg("l2cap_send_signaling_packet type %u\n", cmd);
    va_list argptr;
    va_start(argptr, identifier);
    uint16_t len = l2cap_create_signaling_internal(sig_buffer, handle, cmd, identifier, argptr);
    va_end(argptr);
    // log_dbg("l2cap_send_signaling_packet con %u!\n", handle);
    return hci_send_acl_packet(sig_buffer, len);
}

int l2cap_send_internal(uint16_t local_cid, uint8_t *data, uint16_t len){

    // check for free places on BT module
    if (!hci_number_free_acl_slots()) {
        log_dbg("l2cap_send_internal cid %u, BT module full <-----\n", local_cid);
        return BTSTACK_ACL_BUFFERS_FULL;
    }
    int err = 0;
    
    // find channel for local_cid, construct l2cap packet and send
    l2cap_channel_t * channel = l2cap_get_channel_for_local_cid(local_cid);
    if (channel) {
        if (channel->packets_granted > 0){
            --channel->packets_granted;
            // log_dbg("l2cap_send_internal cid %u, handle %u, 1 credit used, credits left %u;\n",
            //        local_cid, channel->handle, channel->packets_granted);
        } else {
            log_err("l2cap_send_internal cid %u, no credits!\n", local_cid);
        }
        
        // 0 - Connection handle : PB=10 : BC=00 
        bt_store_16(acl_buffer, 0, channel->handle | (2 << 12) | (0 << 14));
        // 2 - ACL length
        bt_store_16(acl_buffer, 2,  len + 4);
        // 4 - L2CAP packet length
        bt_store_16(acl_buffer, 4,  len + 0);
        // 6 - L2CAP channel DEST
        bt_store_16(acl_buffer, 6, channel->remote_cid);
        // 8 - data
        memcpy(&acl_buffer[8], data, len);
        
        // send
        err = hci_send_acl_packet(acl_buffer, len+8);
    }
    
    l2cap_hand_out_credits();
    
    return err;
}
 
// process outstanding signaling tasks
void l2cap_run(void){
    uint8_t  config_options[4];
    linked_item_t *it;
    for (it = (linked_item_t *) l2cap_channels; it ; it = it->next){
        
        // can send?
        
        l2cap_channel_t * channel = (l2cap_channel_t *) it;
        switch (channel->state){

            case L2CAP_STATE_WILL_SEND_CONNECTION_RESPONSE_DECLINE:
                l2cap_send_signaling_packet(channel->handle, CONNECTION_RESPONSE, channel->sig_id, 0, 0, channel->reason, 0);
                // discard channel - l2cap_finialize_channel_close without sending l2cap close event
                linked_list_remove(&l2cap_channels, (linked_item_t *) channel);
                free (channel);
                break;
                
            case L2CAP_STATE_WILL_SEND_CONNECTION_RESPONSE_ACCEPT:
                l2cap_send_signaling_packet(channel->handle, CONNECTION_RESPONSE, channel->sig_id, channel->local_cid, channel->remote_cid, 0, 0);
                channel->state = L2CAP_STATE_WAIT_CONFIG_REQ_OR_SEND_CONFIG_REQ;
                break;
                
            case L2CAP_STATE_WILL_SEND_CONNECTION_REQUEST:
                // success, start l2cap handshake
                channel->sig_id = l2cap_next_sig_id();
                l2cap_send_signaling_packet( channel->handle, CONNECTION_REQUEST, channel->sig_id, channel->psm, channel->local_cid);                   
                channel->state = L2CAP_STATE_WAIT_CONNECT_RSP;
                break;
                
            case L2CAP_STATE_WAIT_CONFIG_REQ_OR_SEND_CONFIG_REQ:
                // after connection response was received
                channel->sig_id = l2cap_next_sig_id();
                config_options[0] = 1; // MTU
                config_options[1] = 2; // len param
                bt_store_16( (uint8_t*)&config_options, 2, channel->local_mtu);
                l2cap_send_signaling_packet(channel->handle, CONFIGURE_REQUEST, channel->sig_id, channel->remote_cid, 0, 4, &config_options);
                channel->state = L2CAP_STATE_WAIT_CONFIG_REQ_RSP_OR_CONFIG_REQ;
                break;

            case L2CAP_STATE_WAIT_CONFIG_REQ_RSP_OR_WILL_SEND_CONFIG_REQ_RSP:
                l2cap_send_signaling_packet(channel->handle, CONFIGURE_RESPONSE, channel->sig_id, channel->remote_cid, 0, 0, 0, NULL);
                channel->state = L2CAP_STATE_WAIT_CONFIG_REQ_RSP;
                break;

            case L2CAP_STATE_WILL_SEND_CONFIG_REQ_RSP:
                l2cap_send_signaling_packet(channel->handle, CONFIGURE_RESPONSE, channel->sig_id, channel->remote_cid, 0, 0, 0, NULL);
                channel->state = L2CAP_STATE_OPEN;
                l2cap_emit_channel_opened(channel, 0);  // success
                l2cap_emit_credits(channel, 1);
                break;
                
            case L2CAP_STATE_WILL_SEND_CONFIG_REQ_AND_CONFIG_REQ_RSP:
                l2cap_send_signaling_packet(channel->handle, CONFIGURE_RESPONSE, channel->sig_id, channel->remote_cid, 0, 0, 0, NULL);
                channel->state = L2CAP_STATE_WILL_SEND_CONFIG_REQ;
                break;
                
            case L2CAP_STATE_WILL_SEND_CONFIG_REQ:
                channel->sig_id = l2cap_next_sig_id();
                config_options[0] = 1; // MTU
                config_options[1] = 2; // len param
                bt_store_16( (uint8_t*)&config_options, 2, channel->local_mtu);
                l2cap_send_signaling_packet(channel->handle, CONFIGURE_REQUEST, channel->sig_id, channel->remote_cid, 0, 4, &config_options);
                channel->state = L2CAP_STATE_WAIT_CONFIG_REQ_RSP;
                break;
          
            case L2CAP_STATE_WILL_SEND_DISCONNECT_RESPONSE:
                l2cap_send_signaling_packet( channel->handle, DISCONNECTION_RESPONSE, channel->sig_id, channel->local_cid, channel->remote_cid);   
                l2cap_finialize_channel_close(channel);
                break;
                
            case L2CAP_STATE_WILL_SEND_DISCONNECT_REQUEST:
                channel->sig_id = l2cap_next_sig_id();
                l2cap_send_signaling_packet( channel->handle, DISCONNECTION_REQUEST, channel->sig_id, channel->remote_cid, channel->local_cid);   
                channel->state = L2CAP_STATE_WAIT_DISCONNECT;
                break;
            default:
                break;
        }
    }
}

// open outgoing L2CAP channel
void l2cap_create_channel_internal(void * connection, btstack_packet_handler_t packet_handler,
                                   bd_addr_t address, uint16_t psm, uint16_t mtu){
    
    // alloc structure
    l2cap_channel_t * chan = malloc(sizeof(l2cap_channel_t));
    // TODO: emit error event
    if (!chan) return;
    
    // limit local mtu to max acl packet length
    if (mtu > hci_max_acl_data_packet_length()) {
        mtu = hci_max_acl_data_packet_length();
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
    chan->state = L2CAP_STATE_CLOSED;
    chan->sig_id = L2CAP_SIG_ID_INVALID;
    
    // add to connections list
    linked_list_add(&l2cap_channels, (linked_item_t *) chan);
    
    // send connection request
    // BD_ADDR, Packet_Type, Page_Scan_Repetition_Mode, Reserved, Clock_Offset, Allow_Role_Switch
    hci_send_cmd(&hci_create_connection, address, 0xcc18, 0, 0, 0, 1); 
}

void l2cap_disconnect_internal(uint16_t local_cid, uint8_t reason){
    // find channel for local_cid
    l2cap_channel_t * channel = l2cap_get_channel_for_local_cid(local_cid);
    if (channel) {
        channel->state = L2CAP_STATE_WILL_SEND_DISCONNECT_REQUEST;
    }
    // process
    l2cap_run();
}

static void l2cap_handle_connection_failed_for_addr(bd_addr_t address, uint8_t status){
    linked_item_t *it = (linked_item_t *) &l2cap_channels;
    while (it->next){
        l2cap_channel_t * channel = (l2cap_channel_t *) it->next;
        if ( ! BD_ADDR_CMP( channel->address, address) ){
            if (channel->state == L2CAP_STATE_CLOSED) {
                // failure, forward error code
                l2cap_emit_channel_opened(channel, status);
                // discard channel
                it->next = it->next->next;
                free (channel);
            }
        } else {
            it = it->next;
        }
    }
}

static void l2cap_handle_connection_success_for_addr(bd_addr_t address, hci_con_handle_t handle){
    linked_item_t *it;
    for (it = (linked_item_t *) l2cap_channels; it ; it = it->next){
        l2cap_channel_t * channel = (l2cap_channel_t *) it;
        if ( ! BD_ADDR_CMP( channel->address, address) ){
            if (channel->state == L2CAP_STATE_CLOSED) {
                // success, start l2cap handshake
                channel->state = L2CAP_STATE_WILL_SEND_CONNECTION_REQUEST;
                channel->handle = handle;
                channel->local_cid = l2cap_next_local_cid();
            }
        }
    }
    // process
    l2cap_run();
}

void l2cap_event_handler( uint8_t *packet, uint16_t size ){
    
    bd_addr_t address;
    hci_con_handle_t handle;
    linked_item_t *it;
    
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
            break;
            
        // handle disconnection complete events
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            // send l2cap disconnect events for all channels on this handle
            handle = READ_BT_16(packet, 3);
            it = (linked_item_t *) &l2cap_channels;
            while (it->next){
                l2cap_channel_t * channel = (l2cap_channel_t *) it->next;
                if ( channel->handle == handle ){
                    // update prev item before free'ing next element - don't call l2cap_finalize_channel_close
                    it->next = it->next->next;
                    l2cap_emit_channel_closed(channel);
                    free (channel);
                } else {
                    it = it->next;
                }
            }
            break;
            
        case HCI_EVENT_NUMBER_OF_COMPLETED_PACKETS:
            l2cap_hand_out_credits();
            break;
            
        // HCI Connection Timeouts
        case L2CAP_EVENT_TIMEOUT_CHECK:
            handle = READ_BT_16(packet, 2);
            if (hci_authentication_active_for_handle(handle)) break;
            l2cap_channel_t * channel;
            int used = 0;
            for (it = (linked_item_t *) l2cap_channels; it ; it = it->next){
                channel = (l2cap_channel_t *) it;
                if (channel->handle == handle) {
                    used = 1;
                }
            }
            if (!used) {
                hci_send_cmd(&hci_disconnect, handle, 0x13); // remote closed connection             
            }
            break;
            
        default:
            break;
    }
    
    // pass on
    (*packet_handler)(NULL, HCI_EVENT_PACKET, 0, packet, size);
}

static void l2cap_handle_disconnect_request(l2cap_channel_t *channel, uint16_t identifier){
    channel->sig_id = identifier;
    channel->state = L2CAP_STATE_WILL_SEND_DISCONNECT_RESPONSE;
    l2cap_run();
}

static void l2cap_handle_connection_request(hci_con_handle_t handle, uint8_t sig_id, uint16_t psm, uint16_t source_cid){
    
    // log_dbg("l2cap_handle_connection_request for handle %u, psm %u cid %u\n", handle, psm, source_cid);
    l2cap_service_t *service = l2cap_get_service(psm);
    if (!service) {
        // 0x0002 PSM not supported
        // log_dbg("l2cap_handle_connection_request no PSM for psm %u/n", psm);
        l2cap_send_signaling_packet(handle, CONNECTION_RESPONSE, sig_id, 0, 0, 0x0002, 0);
        return;
    }
    
    hci_connection_t * hci_connection = connection_for_handle( handle );
    if (!hci_connection) {
        log_err("no hci_connection for handle %u\n", handle);
        // TODO: emit error
        return;
    }
    // alloc structure
    // log_dbg("l2cap_handle_connection_request register channel\n");
    l2cap_channel_t * channel = malloc(sizeof(l2cap_channel_t));
    // TODO: emit error event
    if (!channel) return;
    
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

    // limit local mtu to max acl packet length
    if (channel->local_mtu > hci_max_acl_data_packet_length()) {
        channel->local_mtu = hci_max_acl_data_packet_length();
    }
    
    // set initial state
    channel->state = L2CAP_STATE_WAIT_CLIENT_ACCEPT_OR_REJECT;
    
    // temp. store req sig id
    channel->sig_id = sig_id; 
    
    // add to connections list
    linked_list_add(&l2cap_channels, (linked_item_t *) channel);
    
    // emit incoming connection request
    l2cap_emit_connection_request(channel);
}

void l2cap_accept_connection_internal(uint16_t local_cid){
    l2cap_channel_t * channel = l2cap_get_channel_for_local_cid(local_cid);
    if (!channel) {
        log_err("l2cap_accept_connection_internal called but local_cid 0x%x not found", local_cid);
        return;
    }

    channel->state = L2CAP_STATE_WILL_SEND_CONNECTION_RESPONSE_ACCEPT;

    // process
    l2cap_run();
}

void l2cap_decline_connection_internal(uint16_t local_cid, uint8_t reason){
    l2cap_channel_t * channel = l2cap_get_channel_for_local_cid( local_cid);
    if (!channel) {
        log_err( "l2cap_decline_connection_internal called but local_cid 0x%x not found", local_cid);
        return;
    }
    channel->state  = L2CAP_STATE_WILL_SEND_CONNECTION_RESPONSE_DECLINE;
    channel->reason = reason;
    l2cap_run();
}

void l2cap_signaling_handle_configure_request(l2cap_channel_t *channel, uint8_t *command){
    // accept the other's configuration options
    uint16_t end_pos = 4 + READ_BT_16(command, L2CAP_SIGNALING_COMMAND_LENGTH_OFFSET);
    uint16_t pos     = 8;
    while (pos < end_pos){
        uint8_t type   = command[pos++];
        uint8_t length = command[pos++];
        // MTU { type(8): 1, len(8):2, MTU(16) }
        if ((type & 0x7f) == 1 && length == 2){
            channel->remote_mtu = READ_BT_16(command, pos);
            // log_dbg("l2cap cid %u, remote mtu %u\n", channel->local_cid, channel->remote_mtu);
        }
        pos += length;
    }
    uint8_t identifier = command[L2CAP_SIGNALING_COMMAND_SIGID_OFFSET];
    channel->sig_id = command[L2CAP_SIGNALING_COMMAND_SIGID_OFFSET];
}

void l2cap_signaling_handler_channel(l2cap_channel_t *channel, uint8_t *command){

    uint8_t  code       = command[L2CAP_SIGNALING_COMMAND_CODE_OFFSET];
    uint8_t  identifier = command[L2CAP_SIGNALING_COMMAND_SIGID_OFFSET];
    uint16_t result = 0;
    
    // log_dbg("signaling handler code %u\n", code);
    
    switch (channel->state) {
            
        case L2CAP_STATE_WAIT_CONNECT_RSP:
            switch (code){
                case CONNECTION_RESPONSE:
                    result = READ_BT_16 (command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET+4);
                    switch (result) {
                        case 0:
                            // successful connection
                            channel->remote_cid = READ_BT_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET);
                            channel->state = L2CAP_STATE_WAIT_CONFIG_REQ_OR_SEND_CONFIG_REQ;
#if 0
    channel->state = L2CAP_STATE_OPEN;
    l2cap_emit_channel_opened(channel, 0);  // success
    l2cap_emit_credits(channel, 1);
#endif
                            break;
                        case 1:
                            // connection pending. get some coffee
                            break;
                        default:
                            // channel closed
                            channel->state = L2CAP_STATE_CLOSED;

                            // map l2cap connection response result to BTstack status enumeration
                            l2cap_emit_channel_opened(channel, L2CAP_CONNECTION_RESPONSE_RESULT_SUCCESSFUL + result);
                            
                            // drop link key if security block
                            if (L2CAP_CONNECTION_RESPONSE_RESULT_SUCCESSFUL + result == L2CAP_CONNECTION_RESPONSE_RESULT_REFUSED_SECURITY){
                                hci_drop_link_key_for_bd_addr(&channel->address);
                            }
                            
                            // discard channel
                            linked_list_remove(&l2cap_channels, (linked_item_t *) channel);
                            free (channel);
                            break;
                    }
                    break;
                    
                case DISCONNECTION_REQUEST:
                    l2cap_handle_disconnect_request(channel, identifier);
                    break;
                    
                default:
                    //@TODO: implement other signaling packets
                    break;
            }
            break;

        case L2CAP_STATE_WAIT_CONFIG_REQ_OR_SEND_CONFIG_REQ:
            switch (code) {
                case CONFIGURE_REQUEST:
                    l2cap_signaling_handle_configure_request(channel, command);
                    channel->state = L2CAP_STATE_WILL_SEND_CONFIG_REQ_AND_CONFIG_REQ_RSP;
                    break;
                case DISCONNECTION_REQUEST:
                    l2cap_handle_disconnect_request(channel, identifier);
                    break;
                default:
                    //@TODO: implement other signaling packets
                    break;
            }
            break;
            
        case L2CAP_STATE_WAIT_CONFIG_REQ_RSP_OR_CONFIG_REQ:
            switch (code) {
                case CONFIGURE_RESPONSE:
                    channel->state = L2CAP_STATE_WAIT_CONFIG_REQ;
                    break;
                case CONFIGURE_REQUEST:
                    l2cap_signaling_handle_configure_request(channel, command);
                    channel->state = L2CAP_STATE_WAIT_CONFIG_REQ_RSP_OR_WILL_SEND_CONFIG_REQ_RSP;
                    break;
                case DISCONNECTION_REQUEST:
                    l2cap_handle_disconnect_request(channel, identifier);
                    break;
                default:
                    //@TODO: implement other signaling packets
                    break;
            }
            break;
            
        case L2CAP_STATE_WAIT_CONFIG_REQ:
            switch (code) {
                case CONFIGURE_REQUEST:
                    l2cap_signaling_handle_configure_request(channel, command);
                    channel->state = L2CAP_STATE_WILL_SEND_CONFIG_REQ_RSP;
                    break;
                case DISCONNECTION_REQUEST:
                    l2cap_handle_disconnect_request(channel, identifier);
                    break;
                default:
                    //@TODO: implement other signaling packets
                    break;
            }
            break;
            
        case L2CAP_STATE_WAIT_CONFIG_REQ_RSP:
            switch (code) {
                case CONFIGURE_RESPONSE:
                    channel->state = L2CAP_STATE_OPEN;
                    l2cap_emit_channel_opened(channel, 0);  // success
                    l2cap_emit_credits(channel, 1);
                    break;
                case DISCONNECTION_REQUEST:
                    l2cap_handle_disconnect_request(channel, identifier);
                    break;
                default:
                    //@TODO: implement other signaling packets
                    break;
            }
            break;
            
        case L2CAP_STATE_WAIT_DISCONNECT:
            switch (code) {
                case DISCONNECTION_RESPONSE:
                    l2cap_finialize_channel_close(channel);
                    break;
                case DISCONNECTION_REQUEST:
                    l2cap_handle_disconnect_request(channel, identifier);
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
            switch (code) {
                case DISCONNECTION_REQUEST:
                    l2cap_handle_disconnect_request(channel, identifier);
                    break;
                default:
                    //@TODO: implement other signaling packets, e.g. re-configure
                    break;
            }
            break;
        default:
            break;
    }
    // log_dbg("new state %u\n", channel->state);
}


void l2cap_signaling_handler_dispatch( hci_con_handle_t handle, uint8_t * command){
    
    // get code, signalind identifier and command len
    uint8_t code   = command[L2CAP_SIGNALING_COMMAND_CODE_OFFSET];
    uint8_t sig_id = command[L2CAP_SIGNALING_COMMAND_SIGID_OFFSET];
    uint16_t len   = READ_BT_16(command, L2CAP_SIGNALING_COMMAND_LENGTH_OFFSET);
    
    // not for a particular channel, and not CONNECTION_REQUEST, ECHO_[REQUEST|RESPONSE], INFORMATION_REQUEST 
    if (code < 1 || code == ECHO_RESPONSE || code > INFORMATION_REQUEST){
        return;
    }

    // general commands without an assigned channel
    switch(code) {
            
        case CONNECTION_REQUEST: {
            uint16_t psm =        READ_BT_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET);
            uint16_t source_cid = READ_BT_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET+2);
            l2cap_handle_connection_request(handle, sig_id, psm, source_cid);
            break;
        }
            
        case ECHO_REQUEST: {
            // send back packet
            l2cap_send_signaling_packet(handle, ECHO_RESPONSE, sig_id, len, &command[L2CAP_SIGNALING_COMMAND_DATA_OFFSET]);
            break;
        }
            
        case INFORMATION_REQUEST: {
            // we neither support connectionless L2CAP data nor support any flow control modes yet
            uint16_t infoType = READ_BT_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET);
            if (infoType == 2) {
                uint32_t features = 0;
                // extended features request supported, however no features present
                l2cap_send_signaling_packet(handle, INFORMATION_RESPONSE, sig_id, infoType, 0, 4, &features);
            } else {
                // all other types are not supported
                l2cap_send_signaling_packet(handle, INFORMATION_RESPONSE, sig_id, infoType, 1, 0, NULL);
            }
            break;;
        }
            
        default:
            break;
    }
    
    
    // Get potential destination CID
    uint16_t dest_cid = READ_BT_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET);
    
    // Find channel for this sig_id and connection handle
    linked_item_t *it;
    for (it = (linked_item_t *) l2cap_channels; it ; it = it->next){
        l2cap_channel_t * channel = (l2cap_channel_t *) it;
        if (channel->handle == handle) {
            if (code & 1) {
                // match odd commands by previous signaling identifier 
                if (channel->sig_id == sig_id) {
                    l2cap_signaling_handler_channel(channel, command);
                    break;
                }
            } else {
                // match even commands by local channel id
                if (channel->local_cid == dest_cid) {
                    l2cap_signaling_handler_channel(channel, command);
                    break;
                }
            }
        }
    }
}

void l2cap_acl_handler( uint8_t *packet, uint16_t size ){
        
    // Get Channel ID
    uint16_t channel_id = READ_L2CAP_CHANNEL_ID(packet); 
    
    // Signaling Packet?
    if (channel_id == 1) {
        
        // Get Connection
        hci_con_handle_t handle = READ_ACL_CONNECTION_HANDLE(packet);
        
        uint16_t command_offset = 8;
        while (command_offset < size) {
            
            // handle signaling commands
            l2cap_signaling_handler_dispatch(handle, &packet[command_offset]);

            // increment command_offset
            command_offset += L2CAP_SIGNALING_COMMAND_DATA_OFFSET + READ_BT_16(packet, command_offset + L2CAP_SIGNALING_COMMAND_LENGTH_OFFSET);
        }
        return;
    }
    
    // Find channel for this channel_id and connection handle
    l2cap_channel_t * channel = l2cap_get_channel_for_local_cid(channel_id);
    if (channel) {
        l2cap_dispatch(channel, L2CAP_DATA_PACKET, &packet[COMPLETE_L2CAP_HEADER], size-COMPLETE_L2CAP_HEADER);
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
}

// finalize closed channel - l2cap_handle_disconnect_request & DISCONNECTION_RESPONSE
void l2cap_finialize_channel_close(l2cap_channel_t *channel){
    channel->state = L2CAP_STATE_CLOSED;
    l2cap_emit_channel_closed(channel);
    // discard channel
    linked_list_remove(&l2cap_channels, (linked_item_t *) channel);
    free (channel);
}

l2cap_service_t * l2cap_get_service(uint16_t psm){
    linked_item_t *it;
    
    // close open channels
    for (it = (linked_item_t *) l2cap_services; it ; it = it->next){
        l2cap_service_t * service = ((l2cap_service_t *) it);
        if ( service->psm == psm){
            return service;
        };
    }
    return NULL;
}

void l2cap_register_service_internal(void *connection, btstack_packet_handler_t packet_handler, uint16_t psm, uint16_t mtu){
    // check for alread registered psm // TODO: emit error event
    l2cap_service_t *service = l2cap_get_service(psm);
    if (service) return;
    
    // alloc structure     // TODO: emit error event
    service = malloc(sizeof(l2cap_service_t));
    if (!service) return;
    
    // fill in 
    service->psm = psm;
    service->mtu = mtu;
    service->connection = connection;
    service->packet_handler = packet_handler;

    // add to services list
    linked_list_add(&l2cap_services, (linked_item_t *) service);
}

void l2cap_unregister_service_internal(void *connection, uint16_t psm){
    l2cap_service_t *service = l2cap_get_service(psm);
    if (!service) return;
    linked_list_remove(&l2cap_services, (linked_item_t *) service);
    free(service);
}

//
void l2cap_close_connection(void *connection){
    linked_item_t *it;
    
    // close open channels - note to myself: no channel is freed, so no new for fancy iterator tricks
    l2cap_channel_t * channel;
    for (it = (linked_item_t *) l2cap_channels; it ; it = it->next){
        channel = (l2cap_channel_t *) it;
        if (channel->connection == connection) {
            channel->state = L2CAP_STATE_WILL_SEND_DISCONNECT_REQUEST;
        }
    }   
    
    // unregister services
    it = (linked_item_t *) &l2cap_services;
    while (it->next) {
        l2cap_service_t * service = (l2cap_service_t *) it->next;
        if (service->connection == connection){
            it->next = it->next->next;
            free(service);
        } else {
            it = it->next;
        }
    }
    
    // process
    l2cap_run();
}
    