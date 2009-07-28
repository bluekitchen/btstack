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
static void dummy_handler(uint8_t *packet, int size);
int btstack_packet_handler(connection_t *connection, uint8_t packet_type, uint8_t *data, uint16_t size);

/* callback to L2CAP layer */
static void (*event_packet_handler)(uint8_t *packet, int size) = dummy_handler;
static void (*acl_packet_handler)  (uint8_t *packet, int size) = dummy_handler;

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
        linked_item_t *it;
        bd_addr_t address;
        bt_flip_addr(address, &packet[5]);
        for (it = (linked_item_t *) l2cap_channels; it ; it = it->next){
            l2cap_channel_t * chan = (l2cap_channel_t *) it;
            if ( ! BD_ADDR_CMP( chan->address, address) ){
                if (chan->state == L2CAP_STATE_CLOSED) {
                    chan->state = L2CAP_STATE_CONNECTED;
                    chan->handle = READ_BT_16(packet, 3);
                    chan->sig_id = l2cap_next_sig_id();
                    chan->local_cid = l2cap_next_local_cid();
                    bt_send_l2cap_signaling_packet( chan->handle, CONNECTION_REQUEST, chan->sig_id, chan->psm, chan->local_cid);                   
                }
            }
        }
    }
    // handle disconnection complete events
    // TODO ...
}
void l2cap_data_handler( uint8_t *packet, uint16_t size ){
    // Get Channel ID 

    // Signaling Packet?
    
    // Get Signaling Identifier
    
    // Find channel for this sig_id
    
    // Handle CONNECTION_RESPONSE
    
    // Handle CONFIGURE_RESPONSE
    
    // Handle CONFIGURE_REQUEST
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

static void dummy_handler(uint8_t *packet, int size){
}

// register packet and event handler
void bt_register_event_packet_handler(void (*handler)(uint8_t *packet, int size)){
    event_packet_handler = handler;
}

void bt_register_acl_packet_handler  (void (*handler)(uint8_t *packet, int size)){
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
    bt_send_cmd(&hci_create_connection, &address, 0x18, 0, 0, 0, 0); 
    
    return chan;
}

void l2cap_disconnect(l2cap_channel_t *channel, uint8_t reason){
    // TODO implement
}

