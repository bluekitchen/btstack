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

#define BTSTACK_FILE__ "mesh_generic_on_off_client.c"

#include "mesh/mesh_generic_on_off_client.h"

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

// Generic On Off Message

static const mesh_access_message_t mesh_generic_on_off_set_with_transition = {
        MESH_GENERIC_ON_OFF_SET, "1111"
};

static const mesh_access_message_t mesh_generic_on_off_set_instantaneous = {
        MESH_GENERIC_ON_OFF_SET, "11"
};

static const mesh_access_message_t mesh_generic_on_off_set_unacknowledged_with_transition = {
        MESH_GENERIC_ON_OFF_SET_UNACKNOWLEDGED, "1111"
};

static const mesh_access_message_t mesh_generic_on_off_set_unacknowledged_instantaneous = {
        MESH_GENERIC_ON_OFF_SET_UNACKNOWLEDGED, "11"
};

static const mesh_access_message_t mesh_generic_on_off_get = {
        MESH_GENERIC_ON_OFF_GET, ""
};


// Generic On Off Client Functions

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

uint8_t mesh_generic_on_off_client_get(mesh_model_t *mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index){
    // setup message
    mesh_upper_transport_pdu_t * transport_pdu = mesh_access_setup_message(&mesh_generic_on_off_get);
    if (!transport_pdu) return BTSTACK_MEMORY_ALLOC_FAILED;
    // send as segmented access pdu
    generic_client_send_message_acknowledged(mesh_access_get_element_address(mesh_model), dest, netkey_index, appkey_index, (mesh_pdu_t *) transport_pdu, MESH_GENERIC_ON_OFF_STATUS);
    return ERROR_CODE_SUCCESS;
}

uint8_t mesh_generic_on_off_client_set(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, 
    uint8_t on_off_value, uint8_t transition_time_gdtt, uint8_t delay_time_gdtt, uint8_t transaction_id){
    
    mesh_upper_transport_pdu_t *  transport_pdu;
    if (transition_time_gdtt != 0) {
        transport_pdu = mesh_access_setup_message(&mesh_generic_on_off_set_with_transition, on_off_value, transaction_id, transition_time_gdtt, delay_time_gdtt);
    } else {
        transport_pdu = mesh_access_setup_message(&mesh_generic_on_off_set_instantaneous, on_off_value, transaction_id);
    }
    if (!transport_pdu) return BTSTACK_MEMORY_ALLOC_FAILED;

    generic_client_send_message_acknowledged(mesh_access_get_element_address(mesh_model), dest, netkey_index, appkey_index, (mesh_pdu_t *) transport_pdu, MESH_GENERIC_ON_OFF_STATUS);
    return ERROR_CODE_SUCCESS;
}

uint8_t mesh_generic_on_off_client_set_unacknowledged(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, 
    uint8_t on_off_value, uint8_t transition_time_gdtt, uint8_t delay_time_gdtt, uint8_t transaction_id){
    mesh_upper_transport_pdu_t *  transport_pdu;
    if (transition_time_gdtt != 0) {
        transport_pdu = mesh_access_setup_message(&mesh_generic_on_off_set_unacknowledged_with_transition, on_off_value, transaction_id, transition_time_gdtt, delay_time_gdtt);
    } else {
        transport_pdu = mesh_access_setup_message(&mesh_generic_on_off_set_unacknowledged_instantaneous, on_off_value, transaction_id);
    }
    if (!transport_pdu) return BTSTACK_MEMORY_ALLOC_FAILED;
    generic_client_send_message_unacknowledged(mesh_access_get_element_address(mesh_model), dest, netkey_index, appkey_index, (mesh_pdu_t *) transport_pdu);
    return ERROR_CODE_SUCCESS;
}

uint8_t mesh_generic_on_off_client_publish(mesh_model_t * mesh_model, uint8_t on_off_value, uint8_t transaction_id){
    mesh_publication_model_t * publication_model = mesh_model->publication_model;
    uint16_t appkey_index = publication_model->appkey_index;
    mesh_transport_key_t * app_key = mesh_transport_key_get(appkey_index);
    if (app_key == NULL) return MESH_ERROR_APPKEY_INDEX_INVALID;
    return mesh_generic_on_off_client_set_unacknowledged(mesh_model, publication_model->address, app_key->netkey_index, appkey_index, on_off_value, 0, 0, transaction_id);
}

// Model Operations

static void generic_on_off_status_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    if (!mesh_model->model_packet_handler){
        log_error("model_packet_handler == NULL");
    }

    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);
    
    uint8_t present_value = mesh_access_parser_get_uint8(&parser);
    uint8_t target_value = 0;
    uint8_t remaining_time_gdtt = 0;

    if (mesh_access_parser_available(&parser) == 2){
        target_value = mesh_access_parser_get_uint8(&parser);
        remaining_time_gdtt = mesh_access_parser_get_uint8(&parser);
    }

    uint8_t event[12] = {HCI_EVENT_MESH_META, 10, MESH_SUBEVENT_GENERIC_ON_OFF};
    int pos = 3;
    // dest
    little_endian_store_16(event, pos, mesh_pdu_src(pdu));
    pos += 2;
    event[pos++] = ERROR_CODE_SUCCESS;
    
    event[pos++] = present_value;
    event[pos++] = target_value;
    little_endian_store_32(event, pos, (uint32_t) mesh_access_time_gdtt2ms(remaining_time_gdtt));
    pos += 4;
    
    (*mesh_model->model_packet_handler)(HCI_EVENT_PACKET, 0, event, pos);
    mesh_access_message_processed(pdu);
}

const static mesh_operation_t mesh_generic_on_off_model_operations[] = {
    { MESH_GENERIC_ON_OFF_STATUS, 0, generic_on_off_status_handler },
    { 0, 0, NULL }
};

const mesh_operation_t * mesh_generic_on_off_client_get_operations(void){
    return mesh_generic_on_off_model_operations;
}

void mesh_generic_on_off_client_register_packet_handler(mesh_model_t *mesh_model, btstack_packet_handler_t transition_events_packet_handler){
    if (transition_events_packet_handler == NULL){
        log_error("mesh_generic_on_off_client_register_packet_handler called with NULL callback");
        return;
    }
    if (mesh_model == NULL){
        log_error("mesh_generic_on_off_client_register_packet_handler called with NULL mesh_model");
        return;
    }
    mesh_model->model_packet_handler = transition_events_packet_handler;
}
