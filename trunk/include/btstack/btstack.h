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
#include <btstack/run_loop.h>
#include <btstack/utils.h>

#include <stdint.h>

// packet handler
typedef void (*btstack_packet_handler_t) (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);


// init BTstack library
int bt_open();

// stop using BTstack library
int bt_close();

// send hci cmd packet
int bt_send_cmd(hci_cmd_t *cmd, ...);

// register packet handler -- channel only valid for l2cap and rfcomm packets
btstack_packet_handler_t bt_register_packet_handler(btstack_packet_handler_t handler);

void bt_send_acl(uint8_t * data, uint16_t len);

void bt_send_l2cap(uint16_t source_cid, uint8_t *data, uint16_t len);
