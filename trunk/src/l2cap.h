/*
 *  l2cap.h
 *
 *  Logical Link Control and Adaption Protocl (L2CAP)
 *
 *  Created by Matthias Ringwald on 5/16/09.
 */

#pragma once

#include "hci.h"
#include "l2cap_signaling.h"
#include "utils.h"
#include "socket_connection.h"

#define L2CAP_SIG_ID_INVALID 0

typedef enum {
    L2CAP_STATE_CLOSED,           // no baseband
    L2CAP_STATE_WAIT_CONNECT,     // from application
    L2CAP_STATE_WAIT_CONNECT_RSP, // from peer
    L2CAP_STATE_WAIT_CONFIG_REQ_RSP,
    L2CAP_STATE_WAIT_CONFIG_REQ,
    L2CAP_STATE_OPEN,
    L2CAP_STATE_WAIT_DISCONNECT,  // from application
} L2CAP_STATE;

typedef struct {
    // linked list - assert: first field
    linked_item_t    item;

    L2CAP_STATE state;
    uint8_t   sig_id;
    uint16_t  source_cid;
    uint16_t  dest_cid;
    bd_addr_t address;
    uint16_t  psm;
    hci_con_handle_t handle;
    connection_t *connection;
    // uint16_t mtu_incoming;
    // uint16_t mtu_outgoing;
    // uint16_t flush_timeout_incoming;
    // uint16_t flush_timeout_outgoing;
} l2cap_channel_t;

typedef struct {
    
} l2cap_service_t;

void l2cap_init();
void l2cap_create_channel_internal(connection_t * connection, bd_addr_t address, uint16_t psm);
void l2cap_disconnect_internal(uint16_t source_cid, uint8_t reason);
void l2cap_send_internal(uint16_t source_cid, uint8_t *data, uint16_t len);
void l2cap_acl_handler( uint8_t *packet, uint16_t size );
void l2cap_event_handler( uint8_t *packet, uint16_t size );


void l2cap_emit_channel_opened(l2cap_channel_t *channel);
void l2cap_emit_channel_closed(l2cap_channel_t *channel);
