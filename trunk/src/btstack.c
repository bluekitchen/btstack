/*
 *  btstack.c
 *
 *  Created by Matthias Ringwald on 7/1/09.
 *
 *  BTstack client API
 */

#include "btstack.h"

#include "l2cap.h"
#include "socket_connection.h"
#include "run_loop.h"

#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>

uint8_t hci_cmd_buffer[3+255];

uint8_t l2cap_sig_buffer[48];

static connection_t *btstack_connection = NULL;

static linked_list_t l2cap_channels = NULL;

/** prototypes */
static void dummy_handler(uint8_t *packet, uint16_t size);
int btstack_packet_handler(connection_t *connection, uint8_t packet_type, uint8_t *data, uint16_t size);

/* callback to L2CAP layer */
static void (*event_packet_handler)(uint8_t *packet, uint16_t size) = dummy_handler;
static void (*acl_packet_handler)  (uint8_t *packet, uint16_t size) = dummy_handler;

// init BTstack library
int bt_open(){

    // BTdaemon
    socket_connection_register_packet_callback(btstack_packet_handler);
    btstack_connection = socket_connection_open_tcp();
    if (!btstack_connection) return -1;

    return 0;
}

// stop using BTstack library
int bt_close(){
    return socket_connection_close_tcp(btstack_connection);
}

void l2cap_event_handler( uint8_t *packet, uint16_t size ){
    // handle connection complete events
    if (packet[0] == HCI_EVENT_CONNECTION_COMPLETE && packet[2] == 0){
        bd_addr_t address;
        bt_flip_addr(address, &packet[5]);
        
        linked_item_t *it;
        for (it = (linked_item_t *) l2cap_channels; it ; it = it->next){
            l2cap_channel_t * chan = (l2cap_channel_t *) it;
            if ( ! BD_ADDR_CMP( chan->address, address) ){
                if (chan->state == L2CAP_STATE_CLOSED) {
                    chan->handle = READ_BT_16(packet, 3);
                    chan->sig_id = l2cap_next_sig_id();
                    chan->source_cid = l2cap_next_source_cid();

                    bt_send_l2cap_signaling_packet( chan->handle, CONNECTION_REQUEST, chan->sig_id, chan->psm, chan->source_cid);                   

                    chan->state = L2CAP_STATE_WAIT_CONNECT_RSP;
                }
            }
        }
    }
    // handle disconnection complete events
    // TODO ...
}

void l2cap_signaling_handler(l2cap_channel_t *channel, uint8_t *packet, uint16_t size){

    static uint8_t config_options[] = { 1, 2, 150, 0}; // mtu = 48 

    uint8_t code       = READ_L2CAP_SIGNALING_CODE( packet );
    uint8_t identifier = READ_L2CAP_SIGNALING_IDENTIFIER( packet );
    
    switch (channel->state) {
            
        case L2CAP_STATE_WAIT_CONNECT_RSP:
            switch (code){
                case CONNECTION_RESPONSE:
                    if ( READ_BT_16 (packet, L2CAP_SIGNALING_DATA_OFFSET+3) == 0){
                        // successfull connection
                        channel->dest_cid = READ_BT_16(packet, L2CAP_SIGNALING_DATA_OFFSET + 0);
                        channel->sig_id = l2cap_next_sig_id();
                        bt_send_l2cap_signaling_packet(channel->handle, CONFIGURE_REQUEST, channel->sig_id, channel->dest_cid, 0, 4, &config_options);
                        channel->state = L2CAP_STATE_WAIT_CONFIG_REQ_RSP;
                    } else {
                        // TODO implement failed
                    }
                    break;
                // TODO implement other signaling packets
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
                    bt_send_l2cap_signaling_packet(channel->handle, CONFIGURE_RESPONSE, identifier, channel->dest_cid, 0, 0, size - 16, &packet[16]);
                    
                    channel->state = L2CAP_STATE_OPEN;
                    
                    //  notify client
                    uint8_t event[8];
                    event[0] = HCI_EVENT_L2CAP_CHANNEL_OPENED;
                    event[1] = 6;
                    bt_store_16(event, 2, channel->handle);
                    bt_store_16(event, 4, channel->source_cid);
                    bt_store_16(event, 6, channel->dest_cid);
                    (*channel->event_callback)(event, sizeof(event));
                    break;
            }
            break;
    }
}

void l2cap_data_handler( uint8_t *packet, uint16_t size ){

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
    linked_item_t *it;
    for (it = (linked_item_t *) l2cap_channels; it ; it = it->next){
        l2cap_channel_t * channel = (l2cap_channel_t *) it;
        if ( channel->source_cid == channel_id && channel->handle == handle) {
            (*channel->data_callback)(packet, size);
        }
    }
}

int btstack_packet_handler(connection_t *connection, uint8_t packet_type, uint8_t *data, uint16_t size){
    switch (packet_type){
        case HCI_EVENT_PACKET:
            l2cap_event_handler(data, size);
            (*event_packet_handler)(data, size);
            break;
        case HCI_ACL_DATA_PACKET:
            l2cap_data_handler(data, size);
            (*acl_packet_handler)(data, size);
            break;
        default:
            break;
    }
    return 0;
}

// send hci cmd packet
int bt_send_cmd(hci_cmd_t *cmd, ...){
    va_list argptr;
    va_start(argptr, cmd);
    uint16_t len = hci_create_cmd_internal(hci_cmd_buffer, cmd, argptr);
    va_end(argptr);
    socket_connection_send_packet(btstack_connection, HCI_COMMAND_DATA_PACKET, hci_cmd_buffer, len);
    return 0;
}

// send hci acl packet
int bt_send_acl_packet(uint8_t *packet, int size){
    // printf("Send ACL: "); hexdump(packet,size); printf("\n");
    socket_connection_send_packet(btstack_connection, HCI_ACL_DATA_PACKET, packet, size);
    return 0;
}

int bt_send_l2cap_signaling_packet(hci_con_handle_t handle, L2CAP_SIGNALING_COMMANDS cmd, uint8_t identifier, ...){
    va_list argptr;
    va_start(argptr, identifier);
    uint16_t len = l2cap_create_signaling_internal(l2cap_sig_buffer, handle, cmd, identifier, argptr);
    va_end(argptr);
    return bt_send_acl_packet(l2cap_sig_buffer, len);
}

static void dummy_handler(uint8_t *packet, uint16_t size){
}

// register packet and event handler
void bt_register_event_packet_handler(void (*handler)(uint8_t *packet, uint16_t size)){
    event_packet_handler = handler;
}

void bt_register_acl_packet_handler  (void (*handler)(uint8_t *packet, uint16_t size)){
    acl_packet_handler = handler;
}

// open outgoing L2CAP channel
l2cap_channel_t * l2cap_create_channel(bd_addr_t address, uint16_t psm, void (*event_cb)(uint8_t *packet, uint16_t size),
                                       void (*data_cb)(uint8_t *packet, uint16_t size)){
    // alloc structure
    l2cap_channel_t * chan = malloc(sizeof(l2cap_channel_t));
    if (!chan) return NULL;
    
    // fill in 
    BD_ADDR_COPY(chan->address, address);
    chan->psm = psm;
    chan->handle = 0;
    chan->event_callback = event_cb;
    chan->data_callback  = data_cb;
    
    // set initial state
    chan->state = L2CAP_STATE_CLOSED;
    chan->sig_id = L2CAP_SIG_ID_INVALID;
    
    // add to connections list
    linked_list_add(&l2cap_channels, (linked_item_t *) chan);
    
    // send connection request
    // BD_ADDR, Packet_Type, Page_Scan_Repetition_Mode, Reserved, Clock_Offset, Allow_Role_Switch
    bt_send_cmd(&hci_create_connection, address, 0x18, 0, 0, 0, 0); 
    
    return chan;
}

void l2cap_send(uint16_t source_cid, uint8_t *data, uint16_t len){
    // find channel for source_cid, construct l2cap packet and send
    linked_item_t *it;
    for (it = (linked_item_t *) l2cap_channels; it ; it = it->next){
        l2cap_channel_t * channel = (l2cap_channel_t *) it;
        if ( channel->source_cid == source_cid) {

            // use hci_cmd_buffer for now
            
            // 0 - Connection handle : PB=10 : BC=00 
            bt_store_16(hci_cmd_buffer, 0, channel->handle | (2 << 12) | (0 << 14));
            // 2 - ACL length
            bt_store_16(hci_cmd_buffer, 2,  len + 4);
            // 4 - L2CAP packet length
            bt_store_16(hci_cmd_buffer, 4,  len + 0);
            // 6 - L2CAP channel DEST
            bt_store_16(hci_cmd_buffer, 6, channel->dest_cid);
            // 8 - data
            memcpy(&hci_cmd_buffer[8], data, len);
            // send
            bt_send_acl_packet(hci_cmd_buffer, len+8);
            
            return;
        }
    }
}

void l2cap_disconnect(uint16_t source_cid, uint8_t reason){
    // TODO implement
}

