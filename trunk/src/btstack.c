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

/** prototypes */
static void dummy_handler(uint8_t *packet, int size);
int btstack_packet_handler(connection_t *connection, uint8_t packet_type, uint8_t *data, uint16_t size);

/* callback to L2CAP layer */
static void (*event_packet_handler)(uint8_t *packet, int size) = dummy_handler;
static void (*acl_packet_handler)  (uint8_t *packet, int size) = dummy_handler;


// init BTstack library
int bt_open(){
    socket_connection_register_packet_callback(btstack_packet_handler);
    btstack_connection = socket_connection_open_tcp();
    if (!btstack_connection) return -1;
    return 0;
}

// stop using BTstack library
int bt_close(){
    return socket_connection_close_tcp(btstack_connection);
}

int btstack_packet_handler(connection_t *connection, uint8_t packet_type, uint8_t *data, uint16_t size){
    switch (packet_type){
        case HCI_EVENT_PACKET:
            (*event_packet_handler)(data, size);
            break;
        case HCI_ACL_DATA_PACKET:
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
    socket_connection_send_packet(btstack_connection, HCI_ACL_DATA_PACKET, hci_cmd_buffer, size);
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

