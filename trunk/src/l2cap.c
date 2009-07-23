/*
 *  l2cap.c
 *
 *  Logical Link Control and Adaption Protocl (L2CAP)
 *
 *  Created by Matthias Ringwald on 5/16/09.
 */

#include "l2cap.h"

#include <stdarg.h>
#include <string.h>

#include <stdio.h>

static uint8_t * sig_buffer;

int l2cap_send_signaling_packet(hci_con_handle_t handle, L2CAP_SIGNALING_COMMANDS cmd, uint8_t identifier, ...){
    va_list argptr;
    va_start(argptr, identifier);
    uint16_t len = l2cap_create_signaling_internal(sig_buffer, handle, cmd, identifier, argptr);
    return hci_send_acl_packet(sig_buffer, len);
}

uint16_t l2cap_create_signaling_packet(uint8_t *acl_buffer, hci_con_handle_t handle, L2CAP_SIGNALING_COMMANDS cmd, uint8_t identifier, ...){
    va_list argptr;
    va_start(argptr, identifier);
    uint16_t len = l2cap_create_signaling_internal(acl_buffer, handle, cmd, identifier, argptr);
    va_end(argptr);
    return len;
}

void l2cap_init(){
    sig_buffer = malloc( 48 );
}