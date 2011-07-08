/*
 * Copyright (C) 2009 by Matthias Ringwald
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY MATTHIAS RINGWALD AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

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
#include <btstack/utils.h>
#include <btstack/btstack.h>

#if defined __cplusplus
extern "C" {
#endif
    
#define L2CAP_SIG_ID_INVALID 0

void l2cap_init(void);
void l2cap_register_packet_handler(void (*handler)(void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size));
void l2cap_create_channel_internal(void * connection, btstack_packet_handler_t packet_handler, bd_addr_t address, uint16_t psm, uint16_t mtu);
void l2cap_disconnect_internal(uint16_t local_cid, uint8_t reason);
int l2cap_send_internal(uint16_t local_cid, uint8_t *data, uint16_t len);
uint16_t l2cap_get_remote_mtu_for_local_cid(uint16_t local_cid);

void l2cap_block_new_credits(uint8_t blocked);
int  l2cap_can_send_packet_now(uint16_t local_cid);    // non-blocking UART write
    
void l2cap_close_connection(void *connection);

void l2cap_register_service_internal(void *connection, btstack_packet_handler_t packet_handler, uint16_t psm, uint16_t mtu);
void l2cap_unregister_service_internal(void *connection, uint16_t psm);

void l2cap_accept_connection_internal(uint16_t local_cid);
void l2cap_decline_connection_internal(uint16_t local_cid, uint8_t reason);

#if defined __cplusplus
}
#endif
