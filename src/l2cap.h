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

#define L2CAP_SIG_ID_INVALID 0

typedef enum {
    L2CAP_STATE_CLOSED,             // no baseband
    L2CAP_STATE_CONNECTED,          // baseband connection
    L2CAP_STATE_W4_L2CA_CONNECTION_RSP,  // from application
    L2CAP_STATE_W4_L2CAP_CONNECTION_RSP, // from peer
    L2CAP_STATE_CONFIG,
    L2CAP_STATE_OPEN,
    L2CAP_STATE_W4_L2CA_DISCON_RSP,  // from application
    L2CAP_STATE_W4_L2CAP_DISCON_RSP, // from peer
} L2CAP_STATE;

typedef struct {
    L2CAP_STATE state;
    uint8_t   sig_id;
    uint16_t  local_cid;
    bd_addr_t address;
    uint16_t  psm;
    hci_con_handle_t handle;
    void (*event_callback)(uint8_t *packet, uint16_t size);
    void (*data_callback)(uint8_t *packet, uint16_t size);
    // uint16_t mtu_incoming;
    // uint16_t mtu_outgoing;
    // uint16_t flush_timeout_incoming;
    // uint16_t flush_timeout_outgoing;
} l2cap_channel_t;

typedef struct {
    
} l2cap_service_t;

void l2cap_init();
int l2cap_send_signaling_packet(hci_con_handle_t handle, L2CAP_SIGNALING_COMMANDS cmd, uint8_t identifier, ...);
