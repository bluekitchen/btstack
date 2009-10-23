/*
 *  l2cap.c
 *
 *  Logical Link Control and Adaption Protocl (L2CAP)
 *
 *  Created by Matthias Ringwald on 5/16/09.
 */

#include "l2cap.h"

#include <stdarg.h>
#include <string.h>

#include <stdio.h>

// size of HCI ACL + L2CAP Header for regular data packets
#define COMPLETE_L2CAP_HEADER 8

static void null_event_handler(uint8_t *packet, uint16_t size);
static void null_data_handler(uint16_t source_cid, uint8_t *packet, uint16_t size);

static uint8_t * sig_buffer = NULL;
static linked_list_t l2cap_channels = NULL;
static uint8_t * acl_buffer = NULL;
static void (*event_packet_handler) (uint8_t *packet, uint16_t size) = null_event_handler;
static void (*data_packet_handler)  (uint16_t source_cid, uint8_t *packet, uint16_t size) = null_data_handler;
static connection_t * capture_connection = NULL;

void l2cap_init(){
    sig_buffer = malloc( 48 );
    acl_buffer = malloc( 255 + 8 ); 

    // 
    // register callbacks with HCI
    //
    hci_register_event_packet_handler(&l2cap_event_handler);
    hci_register_acl_packet_handler(&l2cap_acl_handler);
}


/** Register L2CAP packet handlers */
static void null_event_handler(uint8_t *packet, uint16_t size){
}
static void null_data_handler(uint16_t  source_cid, uint8_t *packet, uint16_t size){
}
void l2cap_register_event_packet_handler(void (*handler)(uint8_t *packet, uint16_t size)){
    event_packet_handler = handler;
}
void l2cap_register_data_packet_handler  (void (*handler)(uint16_t source_cid, uint8_t *packet, uint16_t size)){
    data_packet_handler = handler;
}

int l2cap_send_signaling_packet(hci_con_handle_t handle, L2CAP_SIGNALING_COMMANDS cmd, uint8_t identifier, ...){
    va_list argptr;
    va_start(argptr, identifier);
    uint16_t len = l2cap_create_signaling_internal(sig_buffer, handle, cmd, identifier, argptr);
    va_end(argptr);
    return hci_send_acl_packet(sig_buffer, len);
}

l2cap_channel_t * l2cap_get_channel_for_source_cid(uint16_t source_cid){
    linked_item_t *it;
    l2cap_channel_t * channel;
    for (it = (linked_item_t *) l2cap_channels; it ; it = it->next){
        channel = (l2cap_channel_t *) it;
        if ( channel->source_cid == source_cid) {
            return channel;
        }
    }
    return NULL;
}
            
// open outgoing L2CAP channel
void l2cap_create_channel_internal(connection_t * connection, bd_addr_t address, uint16_t psm){
    
    // alloc structure
    l2cap_channel_t * chan = malloc(sizeof(l2cap_channel_t));
    // TODO: emit error event
    if (!chan) return;
    
    // fill in 
    BD_ADDR_COPY(chan->address, address);
    chan->psm = psm;
    chan->handle = 0;
    chan->connection = connection;
    
    // set initial state
    chan->state = L2CAP_STATE_CLOSED;
    chan->sig_id = L2CAP_SIG_ID_INVALID;
    
    // add to connections list
    linked_list_add(&l2cap_channels, (linked_item_t *) chan);
    
    // send connection request
    // BD_ADDR, Packet_Type, Page_Scan_Repetition_Mode, Reserved, Clock_Offset, Allow_Role_Switch
    hci_send_cmd(&hci_create_connection, address, 0x18, 0, 0, 0, 0); 
}

void l2cap_disconnect_internal(uint16_t source_cid, uint8_t reason){
    // find channel for source_cid
    l2cap_channel_t * channel = l2cap_get_channel_for_source_cid(source_cid);
    if (channel) {
        channel->sig_id = l2cap_next_sig_id();
        l2cap_send_signaling_packet( channel->handle, DISCONNECTION_REQUEST, channel->sig_id, channel->dest_cid, channel->source_cid);   
        channel->state = L2CAP_STATE_WAIT_DISCONNECT;
    }
}


void l2cap_event_handler( uint8_t *packet, uint16_t size ){
    // handle connection complete events
    if (packet[0] == HCI_EVENT_CONNECTION_COMPLETE) {
        bd_addr_t address;
        bt_flip_addr(address, &packet[5]);
        
        linked_item_t *it;
        for (it = (linked_item_t *) l2cap_channels; it ; it = it->next){
            l2cap_channel_t * chan = (l2cap_channel_t *) it;
            if ( ! BD_ADDR_CMP( chan->address, address) ){
                if (chan->state == L2CAP_STATE_CLOSED) {
                    if (packet[2] == 0){
                        chan->handle = READ_BT_16(packet, 3);
                        chan->sig_id = l2cap_next_sig_id();
                        chan->source_cid = l2cap_next_source_cid();
                        
                        l2cap_send_signaling_packet( chan->handle, CONNECTION_REQUEST, chan->sig_id, chan->psm, chan->source_cid);                   
                        
                        chan->state = L2CAP_STATE_WAIT_CONNECT_RSP;
                    } else {
                        l2cap_emit_channel_opened(chan, packet[2]);  // failure, forward error code
                    }
                }
            }
        }
    }
    
    // handle disconnection complete events
    if (packet[0] == HCI_EVENT_DISCONNECTION_COMPLETE) {
        // send l2cap disconnect events for all channels on this handle
        hci_con_handle_t handle = READ_BT_16(packet, 3);
        linked_item_t *it;
        for (it = (linked_item_t *) l2cap_channels; it ; it = it->next){
            l2cap_channel_t * channel = (l2cap_channel_t *) it;
            if ( channel->handle == handle ){
                l2cap_finialize_channel_close(channel);
            }
        }
    }
    
    // HCI Connection Timeouts
    if (packet[0] == L2CAP_EVENT_TIMEOUT_CHECK){
        hci_con_handle_t handle = READ_BT_16(packet, 2);
        linked_item_t *it;
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
    }
    
    (*event_packet_handler)(packet, size);
}

void l2cap_signaling_handler(l2cap_channel_t *channel, uint8_t *packet, uint16_t size){
    
    static uint8_t config_options[] = { 1, 2, 150, 0}; // mtu = 48 
    
    uint8_t  code       = READ_L2CAP_SIGNALING_CODE( packet );
    uint8_t  identifier = READ_L2CAP_SIGNALING_IDENTIFIER( packet );
    uint16_t result = 0;
    
    switch (channel->state) {
            
        case L2CAP_STATE_WAIT_CONNECT_RSP:
            switch (code){
                case CONNECTION_RESPONSE:
                    result = READ_BT_16 (packet, L2CAP_SIGNALING_DATA_OFFSET+4);
                    switch (result) {
                        case 0:
                            // successfull connection
                            channel->dest_cid = READ_BT_16(packet, L2CAP_SIGNALING_DATA_OFFSET);
                            channel->sig_id = l2cap_next_sig_id();
                            l2cap_send_signaling_packet(channel->handle, CONFIGURE_REQUEST, channel->sig_id, channel->dest_cid, 0, 4, &config_options);
                            channel->state = L2CAP_STATE_WAIT_CONFIG_REQ_RSP;
                            break;
                        case 1:
                            // connection pending. get some coffee
                            break;
                        default:
                            // map l2cap connection response result to BTstack status enumeration
                            l2cap_emit_channel_opened(channel, L2CAP_CONNECTION_RESPONSE_RESULT_SUCCESSFUL + result);
                            break;
                    }
                    break;

                default:
                    //@TODO: implement other signaling packets
                    break;
            }
            break;
            
        case L2CAP_STATE_WAIT_CONFIG_REQ_RSP:
            switch (code) {
                case CONFIGURE_RESPONSE:
                    channel->state = L2CAP_STATE_WAIT_CONFIG_REQ;
                    break;
            }
            break;
            
        case L2CAP_STATE_WAIT_CONFIG_REQ:
            switch (code) {
                case CONFIGURE_REQUEST:
                    
                    // accept the other's configuration options
                    l2cap_send_signaling_packet(channel->handle, CONFIGURE_RESPONSE, identifier, channel->dest_cid, 0, 0, size - 16, &packet[16]);
                    
                    channel->state = L2CAP_STATE_OPEN;
                    l2cap_emit_channel_opened(channel, 0);  // success
                    break;
            }
            break;
            
        case L2CAP_STATE_WAIT_DISCONNECT:
            switch (code) {
                case DISCONNECTION_RESPONSE:
                    l2cap_finialize_channel_close(channel);
                    break;
            }
            break;
    }
}

// finalize closed channel
void l2cap_finialize_channel_close(l2cap_channel_t *channel){
    channel->state = L2CAP_STATE_CLOSED;
    l2cap_emit_channel_closed(channel);
    
    // discard channel
    linked_list_remove(&l2cap_channels, (linked_item_t *) channel);
    free (channel);
}

//
void l2cap_close_channels_for_connection(connection_t *connection){
    linked_item_t *it;
    l2cap_channel_t * channel;
    for (it = (linked_item_t *) l2cap_channels; it ; it = it->next){
        channel = (l2cap_channel_t *) it;
        if ( channel->connection == connection) {
            channel->sig_id = l2cap_next_sig_id();
            l2cap_send_signaling_packet( channel->handle, DISCONNECTION_REQUEST, channel->sig_id, channel->dest_cid, channel->source_cid);   
            channel->state = L2CAP_STATE_WAIT_DISCONNECT;
        }
    }    
}

//  notify client
void l2cap_emit_channel_opened(l2cap_channel_t *channel, uint8_t status) {
    uint8_t event[17];
    event[0] = L2CAP_EVENT_CHANNEL_OPENED;
    event[1] = sizeof(event) - 2;
    event[2] = status;
    bt_flip_addr(&event[3], channel->address);
    bt_store_16(event,  9, channel->handle);
    bt_store_16(event, 11, channel->psm);
    bt_store_16(event, 13, channel->source_cid);
    bt_store_16(event, 15, channel->dest_cid);
    socket_connection_send_packet(channel->connection, HCI_EVENT_PACKET, 0, event, sizeof(event));
}

void l2cap_emit_channel_closed(l2cap_channel_t *channel) {
    uint8_t event[4];
    event[0] = L2CAP_EVENT_CHANNEL_CLOSED;
    event[1] = sizeof(event) - 2;
    bt_store_16(event, 2, channel->source_cid);
    socket_connection_send_packet(channel->connection, HCI_EVENT_PACKET, 0, event, sizeof(event));
}

void l2cap_acl_handler( uint8_t *packet, uint16_t size ){
    
    // Capturing?
    if (capture_connection) {
        socket_connection_send_packet(capture_connection, HCI_ACL_DATA_PACKET, 0, packet, size);
    }
    
    // forward to higher layers - not needed yet
    // (*data_packet_handler)(channel_id, packet, size);
    
    // Get Channel ID and command code 
    uint16_t channel_id = READ_L2CAP_CHANNEL_ID(packet); 
    uint8_t  code       = READ_L2CAP_SIGNALING_CODE( packet );
    
    // Get Connection
    hci_con_handle_t handle = READ_ACL_CONNECTION_HANDLE(packet);
    
    // Signaling Packet?
    if (channel_id == 1) {
        
        if (code < 1 || code == 2 || code >= 8){
            // not for a particular channel
            return;
        }
        
        // Get Signaling Identifier and potential destination CID
        uint8_t sig_id    = READ_L2CAP_SIGNALING_IDENTIFIER(packet);
        uint16_t dest_cid = READ_BT_16(packet, L2CAP_SIGNALING_DATA_OFFSET);
        
        // Find channel for this sig_id and connection handle
        linked_item_t *it;
        for (it = (linked_item_t *) l2cap_channels; it ; it = it->next){
            l2cap_channel_t * chan = (l2cap_channel_t *) it;
            if (chan->handle == handle) {
                if (code & 1) {
                    // match odd commands by previous signaling identifier 
                    if (chan->sig_id == sig_id) {
                        l2cap_signaling_handler( chan, packet, size);
                    }
                } else {
                    // match even commands by source channel id
                    if (chan->source_cid == dest_cid) {
                        l2cap_signaling_handler( chan, packet, size);
                    }
                }
            }
        }
        return;
    }
    
    // Find channel for this channel_id and connection handle
    l2cap_channel_t * channel = l2cap_get_channel_for_source_cid(channel_id);
    if (channel) {
        socket_connection_send_packet(channel->connection, L2CAP_DATA_PACKET, channel_id,
                                      &packet[COMPLETE_L2CAP_HEADER], size-COMPLETE_L2CAP_HEADER);
    }
}


void l2cap_send_internal(uint16_t source_cid, uint8_t *data, uint16_t len){
    // find channel for source_cid, construct l2cap packet and send
    l2cap_channel_t * channel = l2cap_get_channel_for_source_cid(source_cid);
    if (channel) {
         // 0 - Connection handle : PB=10 : BC=00 
         bt_store_16(acl_buffer, 0, channel->handle | (2 << 12) | (0 << 14));
         // 2 - ACL length
         bt_store_16(acl_buffer, 2,  len + 4);
         // 4 - L2CAP packet length
         bt_store_16(acl_buffer, 4,  len + 0);
         // 6 - L2CAP channel DEST
         bt_store_16(acl_buffer, 6, channel->dest_cid);
         // 8 - data
         memcpy(&acl_buffer[8], data, len);
         // send
         hci_send_acl_packet(acl_buffer, len+8);
     }
}

void l2cap_set_capture_connection(connection_t * connection){
    capture_connection = connection;
}

