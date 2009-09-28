/*
 *  btstack.h
 *
 *  Created by Matthias Ringwald on 7/1/09.
 *
 *  BTstack client API
 *  
 */

#pragma once

#include <btstack/hci_cmds.h>
// #include "l2cap.h"

#include <stdint.h>

// init BTstack library
int bt_open();

// stop using BTstack library
int bt_close();

// send hci cmd packet
int bt_send_cmd(hci_cmd_t *cmd, ...);

// register packet and event handler
void bt_register_event_packet_handler(void (*handler)(uint8_t *packet, uint16_t size));
void bt_register_data_packet_handler  (void (*handler)(uint8_t *packet, uint16_t size));

void bt_send_acl_packet(uint8_t * data, uint16_t len);
void l2cap_send(uint16_t source_cid, uint8_t *data, uint16_t len);
