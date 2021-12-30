/*
 * Copyright (C) 2019 BlueKitchen GmbH
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
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL BLUEKITCHEN
 * GMBH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
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

#define BTSTACK_FILE__ "mesh_generic_default_transition_time_client.c"

#include "mesh/mesh_generic_default_transition_time_client.h"

#include <string.h>
#include <stdio.h>

#include "bluetooth_company_id.h"
#include "btstack_debug.h"
#include "btstack_memory.h"
#include "btstack_util.h"

#include "mesh/mesh_access.h"
#include "mesh/mesh_foundation.h"
#include "mesh/mesh_generic_model.h"
#include "mesh/mesh_keys.h"
#include "mesh/mesh_network.h"
#include "mesh/mesh_upper_transport.h"

// Generic Default Transition Time  Message

static const mesh_access_message_t mesh_generic_default_transition_time_set = {
        MESH_GENERIC_DEFAULT_TRANSITION_TIME_SET, "1"
};

static const mesh_access_message_t mesh_generic_default_transition_time_set_unacknowledged = {
        MESH_GENERIC_DEFAULT_TRANSITION_TIME_SET_UNACKNOWLEDGED, "1"
};

static const mesh_access_message_t mesh_generic_default_transition_time_get = {
        MESH_GENERIC_DEFAULT_TRANSITION_TIME_GET, ""
};


// Generic Default Transition Time  Client Functions

static void generic_client_send_message_unacknowledged(uint16_t src, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, mesh_pdu_t *pdu){
    uint8_t  ttl  = mesh_foundation_default_ttl_get();
    mesh_upper_transport_setup_access_pdu_header(pdu, netkey_index, appkey_index, ttl, src, dest, 0);
    mesh_access_send_unacknowledged_pdu(pdu);
}

static void generic_client_send_message_acknowledged(uint16_t src, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, mesh_pdu_t *pdu, uint32_t ack_opcode){
    uint8_t  ttl  = mesh_foundation_default_ttl_get();
    mesh_upper_transport_setup_access_pdu_header(pdu, netkey_index, appkey_index, ttl, src, dest, 0);
    mesh_access_send_acknowledged_pdu(pdu, mesh_access_acknowledged_message_retransmissions(), ack_opcode);
}

uint8_t mesh_generic_default_transition_time_client_get(mesh_model_t *mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index){
    // setup message
    mesh_upper_transport_pdu_t * transport_pdu = mesh_access_setup_message(&mesh_generic_default_transition_time_get);
    if (!transport_pdu) return BTSTACK_MEMORY_ALLOC_FAILED;
    // send as segmented access pdu
    generic_client_send_message_acknowledged(mesh_access_get_element_address(mesh_model), dest, netkey_index, appkey_index, (mesh_pdu_t *) transport_pdu, MESH_GENERIC_DEFAULT_TRANSITION_TIME_STATUS);
    return ERROR_CODE_SUCCESS;
}

uint8_t mesh_generic_default_transition_time_client_set(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, 
    uint8_t transition_time_gdtt){
    
    mesh_upper_transport_pdu_t *  transport_pdu;
    
    transport_pdu = mesh_access_setup_message(&mesh_generic_default_transition_time_set, transition_time_gdtt);
    
    if (!transport_pdu) return BTSTACK_MEMORY_ALLOC_FAILED;

    generic_client_send_message_acknowledged(mesh_access_get_element_address(mesh_model), dest, netkey_index, appkey_index, (mesh_pdu_t *) transport_pdu, MESH_GENERIC_DEFAULT_TRANSITION_TIME_STATUS);
    return ERROR_CODE_SUCCESS;
}

uint8_t mesh_generic_default_transition_time_client_set_unacknowledged(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, 
    uint8_t transition_time_gdtt){
    mesh_upper_transport_pdu_t *  transport_pdu;
    transport_pdu = mesh_access_setup_message(&mesh_generic_default_transition_time_set_unacknowledged, transition_time_gdtt);

    if (!transport_pdu) return BTSTACK_MEMORY_ALLOC_FAILED;
    generic_client_send_message_unacknowledged(mesh_access_get_element_address(mesh_model), dest, netkey_index, appkey_index, (mesh_pdu_t *) transport_pdu);
    return ERROR_CODE_SUCCESS;
}

// Model Operations

static void generic_default_transition_time_status_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    if (!mesh_model->model_packet_handler){
        log_error("model_packet_handler == NULL");
    }

    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);

    uint8_t transition_time_gdtt = mesh_access_parser_get_uint8(&parser);

    uint8_t event[7] = {HCI_EVENT_MESH_META, 5, MESH_SUBEVENT_GENERIC_DEFAULT_TRANSITION_TIME};
    int pos = 3;
    // dest
    little_endian_store_16(event, pos, mesh_pdu_src(pdu));
    pos += 2;
    event[pos++] = ERROR_CODE_SUCCESS;
    event[pos++] = transition_time_gdtt;
    
    (*mesh_model->model_packet_handler)(HCI_EVENT_PACKET, 0, event, pos);
    mesh_access_message_processed(pdu);
}

const static mesh_operation_t mesh_generic_default_transition_time_model_operations[] = {
    { MESH_GENERIC_DEFAULT_TRANSITION_TIME_STATUS, 0, generic_default_transition_time_status_handler },
    { 0, 0, NULL }
};

const mesh_operation_t * mesh_generic_default_transition_time_client_get_operations(void){
    return mesh_generic_default_transition_time_model_operations;
}

void mesh_generic_default_transition_time_client_register_packet_handler(mesh_model_t *mesh_model, btstack_packet_handler_t transition_events_packet_handler){
    if (transition_events_packet_handler == NULL){
        log_error("mesh_generic_default_transition_time_client_register_packet_handler called with NULL callback");
        return;
    }
    if (mesh_model == NULL){
        log_error("mesh_generic_default_transition_time_client_register_packet_handler called with NULL mesh_model");
        return;
    }
    mesh_model->model_packet_handler = transition_events_packet_handler;
}
