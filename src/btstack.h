/*
 *  btstack.h
 *
 *  Created by Matthias Ringwald on 7/1/09.
 *
 *  BTstack client API
 *  
 */

#pragma once
#include "hci.h"
#include "l2cap.h"

// init BTstack library
int bt_open();

// stop using BTstack library
int bt_close();

// send hci cmd packet
int bt_send_cmd(hci_cmd_t *cmd, ...);

// send hci acl packet
int bt_send_acl_packet(uint8_t *packet, int size);

// TODO: temp
int bt_send_l2cap_signaling_packet(hci_con_handle_t handle, L2CAP_SIGNALING_COMMANDS cmd, uint8_t identifier, ...);

// register packet and event handler
void bt_register_event_packet_handler(void (*handler)(uint8_t *packet, int size));
void bt_register_acl_packet_handler  (void (*handler)(uint8_t *packet, int size));

