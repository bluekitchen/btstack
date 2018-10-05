/*
 * Copyright (C) 2017 BlueKitchen GmbH
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
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
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
 * Please inquire about commercial licensing options at 
 * contact@bluekitchen-gmbh.com
 *
 */


#ifndef __ADV_BEARER_H
#define __ADV_BEARER_H

#include <stdint.h>
#include "btstack_defines.h"

#if defined __cplusplus
extern "C" {
#endif

/**
 * Initialize Advertising Bearer
 */
void adv_bearer_init(void);

/**
 * Register listener for particular message types: Mesh Message, Mesh Beacon, PB-ADV
 */
void adv_bearer_register_for_mesh_message(btstack_packet_handler_t packet_handler);
void adv_bearer_register_for_mesh_beacon(btstack_packet_handler_t packet_handler);
void adv_bearer_register_for_pb_adv(btstack_packet_handler_t packet_handler);

/**
 * Request can send now event for particular message type: Mesh Message, Mesh Beacon, PB-ADV
 */
void adv_bearer_request_can_send_now_for_mesh_message(void);
void adv_bearer_request_can_send_now_for_mesh_beacon(void);
void adv_bearer_request_can_send_now_for_pb_adv(void);

/**
 * Send particular message type: Mesh Message, Mesh Beacon, PB-ADV
 * @param data to send 
 * @param data_len max 29 bytes
 */
void adv_bearer_send_mesh_message(const uint8_t * network_pdu, uint16_t size); 
void adv_bearer_send_mesh_beacon(const uint8_t * beacon_update, uint16_t size); 
void adv_bearer_send_pb_adv(const uint8_t * pb_adv_pdu, uint16_t size); 
 

#if defined __cplusplus
}
#endif

#endif // __ADV_BEARER_H
