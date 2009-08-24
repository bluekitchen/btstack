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

static connection_t *btstack_connection = NULL;

/** prototypes */
static void dummy_handler(uint8_t *packet, uint16_t size);
int btstack_packet_handler(connection_t *connection, uint16_t packet_type, uint16_t channel, uint8_t *data, uint16_t size);

/* callback to L2CAP layer */
static void (*event_packet_handler)(uint8_t *packet, uint16_t size) = dummy_handler;
static void (*acl_packet_handler)  (uint8_t *packet, uint16_t size) = dummy_handler;

// init BTstack library
int bt_open(){

    // BTdaemon
    socket_connection_register_packet_callback(btstack_packet_handler);
    btstack_connection = socket_connection_open_unix();
    if (!btstack_connection) return -1;

    return 0;
}

// stop using BTstack library
int bt_close(){
    return socket_connection_close_tcp(btstack_connection);
}

// send hci cmd packet
int bt_send_cmd(hci_cmd_t *cmd, ...){
    va_list argptr;
    va_start(argptr, cmd);
    uint16_t len = hci_create_cmd_internal(hci_cmd_buffer, cmd, argptr);
    va_end(argptr);
    socket_connection_send_packet(btstack_connection, HCI_COMMAND_DATA_PACKET, 0, hci_cmd_buffer, len);
    return 0;
}

//@TODO: merge handler?
int btstack_packet_handler(connection_t *connection, uint16_t packet_type, uint16_t channel, uint8_t *data, uint16_t size){
    switch (packet_type){
        case HCI_EVENT_PACKET:
            (*event_packet_handler)(data, size);
            break;
        // TODO use different handler, or use packet type parameter
        case HCI_ACL_DATA_PACKET:
        case L2CAP_DATA_PACKET:
            (*acl_packet_handler)(data, size);
            break;
        default:
            break;
    }
    return 0;
}

static void dummy_handler(uint8_t *packet, uint16_t size){
}

// register packet and event handler
void bt_register_event_packet_handler(void (*handler)(uint8_t *packet, uint16_t size)){
    event_packet_handler = handler;
}

void bt_register_data_packet_handler  (void (*handler)(uint8_t *packet, uint16_t size)){
    acl_packet_handler = handler;
}

void l2cap_send(uint16_t source_cid, uint8_t *data, uint16_t len){
    // send
    socket_connection_send_packet(btstack_connection, L2CAP_DATA_PACKET, source_cid, data, len);
}

void bt_send_acl_packet(uint8_t * data, uint16_t len){
    // send
    socket_connection_send_packet(btstack_connection, HCI_ACL_DATA_PACKET, 0, data, len);
}
