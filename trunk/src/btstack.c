/*
 *  btstack.c
 *
 *  Created by Matthias Ringwald on 7/1/09.
 *
 *  BTstack client API
 */

#include <btstack/btstack.h>

#include "l2cap.h"
#include "socket_connection.h"
#include <btstack/run_loop.h>

#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>

static uint8_t hci_cmd_buffer[3+255];

static connection_t *btstack_connection = NULL;

/** prototypes & dummy functions */
static void dummy_handler(uint8_t packet_type, uint8_t *packet, uint16_t size){};
static int btstack_packet_handler(connection_t *connection, uint16_t packet_type, uint16_t channel, uint8_t *data, uint16_t size);

/** local globals :) */
static void (*client_packet_handler)(uint8_t packet_type, uint8_t *packet, uint16_t size) = dummy_handler;


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

int btstack_packet_handler(connection_t *connection, uint16_t packet_type, uint16_t channel, uint8_t *data, uint16_t size){
    // printf("BTstack client handler: packet type %u, data[0] %x\n", packet_type, data[0]);
    (*client_packet_handler)(packet_type, data, size);
    return 0;
}

// register packet handler
void bt_register_packet_handler(void (*handler)(uint8_t packet_type, uint8_t *packet, uint16_t size)){
    client_packet_handler = handler;
}

void bt_send_l2cap(uint16_t source_cid, uint8_t *data, uint16_t len){
    // send
    socket_connection_send_packet(btstack_connection, L2CAP_DATA_PACKET, source_cid, data, len);
}

void bt_send_acl(uint8_t * data, uint16_t len){
    // send
    socket_connection_send_packet(btstack_connection, HCI_ACL_DATA_PACKET, 0, data, len);
}
