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

#include <stdint.h>

// init BTstack library
int bt_open();

// stop using BTstack library
int bt_close();

// send hci cmd packet
int bt_send_cmd(hci_cmd_t *cmd, ...);

// send hci acl packet
int bt_send_acl_packet(uint8_t *packet, int size);

// register packet and event handler
void bt_register_event_packet_handler(void (*handler)(uint8_t *packet, uint16_t size));
void bt_register_acl_packet_handler  (void (*handler)(uint8_t *packet, uint16_t size));

// TODO: temp
int bt_send_l2cap_signaling_packet(hci_con_handle_t handle, L2CAP_SIGNALING_COMMANDS cmd, uint8_t identifier, ...);

// outgoing connections
l2cap_channel_t * l2cap_create_channel(bd_addr_t bd_addr, uint16_t psm, void (*event_cb)(uint8_t *packet, uint16_t size),
                                       void (*data_cb)(uint8_t *packet, uint16_t size));
void l2cap_disconnect(uint16_t source_cid, uint8_t reason);

void l2cap_send(uint16_t source_cid, uint8_t *data, uint16_t len);
