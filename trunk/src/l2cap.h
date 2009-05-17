/*
 *  l2cap.h
 *
 *  Logical Link Control and Adaption Protocl (L2CAP)
 *
 *  Created by Matthias Ringwald on 5/16/09.
 */

#include "hci.h"

typedef enum {
    COMMAND_REJECT = 1,
    CONNECTION_REQUEST,
    CONNECTION_RESPONSE,
    CONFIGURE_REQUEST,
    CONFIGURE_RESPONSE,
    DISCONNECTION_REQUEST,
    DISCONNECTION_RESPONSE,
    ECHO_REQUEST,
    ECHO_RESPONSE,
    INFORMATIONAL_REQUEST,
    INFORMATIONAL_RESPONSE
} L2CAP_SIGNALING_COMMANDS;

typedef struct {
    
} l2cap_channel_t;

typedef struct {
    
} l2cap_service_t;

void l2cap_init();
int l2cap_send_signaling_packet(hci_con_handle_t handle, L2CAP_SIGNALING_COMMANDS cmd, uint8_t identifier, ...);

extern uint16_t  local_cid;
extern uint8_t   sig_seq_nr;
