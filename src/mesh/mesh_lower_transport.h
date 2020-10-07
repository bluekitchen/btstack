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

#ifndef __MESH_LOWER_TRANSPORT_H
#define __MESH_LOWER_TRANSPORT_H

#include <stdint.h>

#include "mesh/mesh_network.h"
#include "btstack_bool.h"

#ifdef __cplusplus
extern "C"
{
#endif


typedef enum {
    MESH_TRANSPORT_OPCODE_ACK = 0,
    MESH_TRANSPORT_OPCODE_FRIEND_POLL,
    MESH_TRANSPORT_OPCODE_FRIEND_UPDATE,
    MESH_TRANSPORT_OPCODE_FRIEND_REQUEST,
    MESH_TRANSPORT_OPCODE_FRIEND_OFFER,
    MESH_TRANSPORT_OPCODE_FRIEND_CLEAR,
    MESH_TRANSPORT_OPCODE_FRIEND_CLEAR_CONFIRM,
    MESH_TRANSPORT_OPCODE_FRIEND_FRIEND_SUBSCRIPTION_LIST_ADD,
    MESH_TRANSPORT_OPCODE_FRIEND_FRIEND_SUBSCRIPTION_LIST_REMOVE,
    MESH_TRANSPORT_OPCODE_FRIEND_FRIEND_SUBSCRIPTION_LIST_CONFIRM,
    MESH_TRANSPORT_OPCODE_HEARTBEAT,
} mesh_transport_opcode_t;

typedef enum {
    MESH_TRANSPORT_PDU_RECEIVED,
    MESH_TRANSPORT_PDU_SENT,
} mesh_transport_callback_type_t;

typedef enum {
    MESH_TRANSPORT_STATUS_PARTIALLY_SEND,
    MESH_TRANSPORT_STATUS_SUCCESS,
    MESH_TRANSPORT_STATUS_SEND_FAILED,
    MESH_TRANSPORT_STATUS_SEND_ABORT_BY_REMOTE,
} mesh_transport_status_t;

mesh_segmented_pdu_t * mesh_segmented_pdu_get(void);
void mesh_segmented_pdu_free(mesh_segmented_pdu_t * message_pdu);

void mesh_lower_transport_init(void);
void mesh_lower_transport_set_higher_layer_handler(void (*pdu_handler)( mesh_transport_callback_type_t callback_type, mesh_transport_status_t status, mesh_pdu_t * pdu));

void mesh_lower_transport_message_processed_by_higher_layer(mesh_pdu_t * pdu);

bool mesh_lower_transport_can_send_to_dest(uint16_t dest);
void mesh_lower_transport_reserve_slot(void);
void mesh_lower_transport_send_pdu(mesh_pdu_t * pdu);

// test
void mesh_lower_transport_received_message(mesh_network_callback_type_t callback_type, mesh_network_pdu_t *network_pdu);
void mesh_lower_transport_reset(void);
void mesh_lower_transport_dump_network_pdus(const char *name, btstack_linked_list_t *list);
void mesh_lower_transport_reset_network_pdus(btstack_linked_list_t *list);

#ifdef __cplusplus
} /* end of extern "C" */
#endif

#endif
