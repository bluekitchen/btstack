/*
 *  btstack.c
 *
 *  Created by Matthias Ringwald on 7/1/09.
 *
 *  BTstack client API
 */

#include "btstack.h"

// init BTstack library
int bt_open(){
}

// stop using BTstack library
int bt_close(){
}

// send hci cmd packet
int bt_send_cmd(hci_cmd_t *cmd, ...){
}

// send hci acl packet
int bt_send_acl_packet(uint8_t *packet, int size){
}

// register packet and event handler
void bt_register_event_packet_handler(void (*handler)(uint8_t *packet, int size)){
}
void bt_register_acl_packet_handler  (void (*handler)(uint8_t *packet, int size)){
}

