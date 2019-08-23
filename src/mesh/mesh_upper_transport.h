/*
 * Copyright (C) 2018 BlueKitchen GmbH
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

#ifndef __MESH_TRANSPORT_H
#define __MESH_TRANSPORT_H

#include <stdint.h>

#include "mesh/mesh_network.h"
#include "mesh/mesh_lower_transport.h"

#ifdef __cplusplus
extern "C"
{
#endif

void mesh_upper_transport_init(void);

void mesh_upper_transport_message_processed_by_higher_layer(mesh_pdu_t * pdu);

void mesh_upper_transport_set_higher_layer_handler(void (*pdu_handler)( mesh_transport_callback_type_t callback_type, mesh_transport_status_t status, mesh_pdu_t * pdu));

// Control PDUs

void mesh_upper_transport_register_control_message_handler(void (*callback)(mesh_pdu_t * pdu));

uint8_t mesh_upper_transport_setup_control_pdu(mesh_pdu_t * pdu, uint16_t netkey_index,
        uint8_t ttl, uint16_t src, uint16_t dest, uint8_t opcode, const uint8_t * control_pdu_data, uint16_t control_pdu_len);

void mesh_upper_transport_send_control_pdu(mesh_pdu_t * pdu);

// Access PDUs


void mesh_upper_transport_register_access_message_handler(void (*callback)(mesh_pdu_t * pdu));

uint8_t mesh_upper_transport_setup_access_pdu_header(mesh_pdu_t * pdu, uint16_t netkey_index, uint16_t appkey_index,
                                                               uint8_t ttl, uint16_t src, uint16_t dest, uint8_t szmic);

uint8_t mesh_upper_transport_setup_access_pdu(mesh_pdu_t * pdu, uint16_t netkey_index, uint16_t appkey_index,
                                                          uint8_t ttl, uint16_t src, uint16_t dest, uint8_t szmic,
                                                          const uint8_t * access_pdu_data, uint8_t access_pdu_len);

void mesh_upper_transport_send_access_pdu(mesh_pdu_t * pdu);

void mesh_upper_transport_pdu_free(mesh_pdu_t * pdu);

// test
void mesh_upper_transport_dump(void);
void mesh_upper_transport_reset(void);

#ifdef __cplusplus
} /* end of extern "C" */
#endif

#endif
