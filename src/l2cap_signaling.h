/*
 *  l2cap_signaling.h
 *
 *  Created by Matthias Ringwald on 7/23/09.
 */

#pragma once

#include <stdint.h>
#include "utils.h"
#include "hci_cmds.h"

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

extern uint16_t  local_cid;
extern uint8_t   sig_seq_nr;

uint16_t l2cap_create_signaling_packet(uint8_t *acl_buffer, hci_con_handle_t handle, L2CAP_SIGNALING_COMMANDS cmd, uint8_t identifier, ...);
uint16_t l2cap_create_signaling_internal(uint8_t * acl_buffer,hci_con_handle_t handle, L2CAP_SIGNALING_COMMANDS cmd, uint8_t identifier, va_list argptr);
