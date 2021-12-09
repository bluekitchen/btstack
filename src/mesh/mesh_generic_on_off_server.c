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

#define BTSTACK_FILE__ "mesh_generic_on_off_server.c"

#include "mesh/mesh_generic_on_off_server.h"

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

static void generic_server_send_message(uint16_t src, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, mesh_pdu_t *pdu){
    uint8_t  ttl  = mesh_foundation_default_ttl_get();
    mesh_upper_transport_setup_access_pdu_header(pdu, netkey_index, appkey_index, ttl, src, dest, 0);
    mesh_access_send_unacknowledged_pdu(pdu);
}

// Transaction management

static mesh_transition_t * generic_on_off_server_get_base_transition(mesh_model_t * mesh_model) {
    mesh_generic_on_off_state_t * generic_on_off_server_state = (mesh_generic_on_off_state_t *)mesh_model->model_data;
    return &generic_on_off_server_state->transition_data.base_transition;
}

static void mesh_server_transition_handler(mesh_transition_t * base_transition, model_state_update_reason_t event){
    mesh_transition_bool_t * transition = (mesh_transition_bool_t*) base_transition;
    switch (event){
        case MODEL_STATE_UPDATE_REASON_TRANSITION_START:
            if (transition->target_value == 1){
                transition->current_value = 1;
                mesh_access_state_changed(transition->base_transition.mesh_model);
            }
            break;
        case MODEL_STATE_UPDATE_REASON_SET:
        case MODEL_STATE_UPDATE_REASON_TRANSITION_END:
            transition->current_value = transition->target_value;
            mesh_access_state_changed(transition->base_transition.mesh_model);
            break;
        default:
            break;
    }
    
    // notify app
    mesh_model_t * generic_on_off_server_model = transition->base_transition.mesh_model;
    mesh_access_emit_state_update_bool(generic_on_off_server_model->model_packet_handler,
        mesh_access_get_element_index(generic_on_off_server_model),
        generic_on_off_server_model->model_identifier,
        MODEL_STATE_ID_GENERIC_ON_OFF,
        event,
        transition->current_value);
}

static void mesh_server_transition_setup_transition_or_instantaneous_update(mesh_model_t *mesh_model, uint8_t transition_time_gdtt, uint8_t delay_time_gdtt){
    mesh_generic_on_off_state_t * generic_on_off_server_state = (mesh_generic_on_off_state_t *)mesh_model->model_data;
    mesh_transition_t * transition = &generic_on_off_server_state->transition_data.base_transition;
    mesh_access_transition_setup(mesh_model, transition, transition_time_gdtt, delay_time_gdtt, &mesh_server_transition_handler);
}

// Generic On Off State

void mesh_generic_on_off_server_register_packet_handler(mesh_model_t *generic_on_off_server_model, btstack_packet_handler_t transition_events_packet_handler){
    btstack_assert(generic_on_off_server_model != NULL);
    btstack_assert(transition_events_packet_handler != NULL);
    generic_on_off_server_model->model_packet_handler = transition_events_packet_handler;
}

const mesh_access_message_t mesh_generic_on_off_status_transition = {
        MESH_GENERIC_ON_OFF_STATUS, "111"
};

const mesh_access_message_t mesh_generic_on_off_status_instantaneous = {
        MESH_GENERIC_ON_OFF_STATUS, "1"
};

static mesh_pdu_t * mesh_generic_on_off_status_message(mesh_model_t *generic_on_off_server_model){
    btstack_assert(generic_on_off_server_model->model_data != NULL);

    mesh_generic_on_off_state_t * state = (mesh_generic_on_off_state_t *) generic_on_off_server_model->model_data;
    // setup message
    mesh_upper_transport_pdu_t * transport_pdu = NULL; 
    if (state->transition_data.base_transition.num_steps > 0) {
        uint8_t remaining_time = (((uint8_t)state->transition_data.base_transition.step_resolution) << 6) | (state->transition_data.base_transition.num_steps);
        transport_pdu = mesh_access_setup_message(&mesh_generic_on_off_status_transition, state->transition_data.current_value,
            state->transition_data.target_value, remaining_time);
    } else {
        log_info("On/Off Status: value %u, no transition active", state->transition_data.current_value);
        transport_pdu = mesh_access_setup_message(&mesh_generic_on_off_status_instantaneous, state->transition_data.current_value);
    }
    return (mesh_pdu_t *) transport_pdu;
}

static void generic_on_off_get_handler(mesh_model_t *generic_on_off_server_model, mesh_pdu_t * pdu){
    mesh_upper_transport_pdu_t * transport_pdu = (mesh_upper_transport_pdu_t *) mesh_generic_on_off_status_message(generic_on_off_server_model);
    if (!transport_pdu) return;
    generic_server_send_message(mesh_access_get_element_address(generic_on_off_server_model), mesh_pdu_src(pdu), mesh_pdu_netkey_index(pdu), mesh_pdu_appkey_index(pdu),(mesh_pdu_t *) transport_pdu);
    mesh_access_message_processed(pdu);
}

// returns if set message was valid
static bool generic_on_off_handle_set_message(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    btstack_assert(mesh_model != NULL);
    btstack_assert(mesh_model->model_data != NULL);

    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);
    uint8_t on_off_value = mesh_access_parser_get_uint8(&parser);

    // check for valid value
    if (on_off_value > 1) return false;
    
    // The TID field is a transaction identifier indicating whether the message is 
    // a new message or a retransmission of a previously sent message
    mesh_generic_on_off_state_t * generic_on_off_server_state = (mesh_generic_on_off_state_t *)mesh_model->model_data;
    uint8_t tid = mesh_access_parser_get_uint8(&parser);
    uint8_t transition_time_gdtt = 0;
    uint8_t delay_time_gdtt = 0;
            
    mesh_transition_t * base_transition = generic_on_off_server_get_base_transition(mesh_model);
    switch (mesh_access_transitions_transaction_status(base_transition, tid, mesh_pdu_src(pdu), mesh_pdu_dst(pdu))){
        case MESH_TRANSACTION_STATUS_RETRANSMISSION:
            // ignore on retransmission
            break;
        default:
            mesh_access_transitions_init_transaction(base_transition, tid, mesh_pdu_src(pdu), mesh_pdu_dst(pdu));
            generic_on_off_server_state->transition_data.target_value = on_off_value;
            
            if (mesh_access_parser_available(&parser) == 2){
                //  Generic Default Transition Time format - num_steps (higher 6 bits), step_resolution (lower 2 bits) 
                transition_time_gdtt = mesh_access_parser_get_uint8(&parser);
                delay_time_gdtt = mesh_access_parser_get_uint8(&parser);
            } 
            mesh_server_transition_setup_transition_or_instantaneous_update(mesh_model, transition_time_gdtt, delay_time_gdtt);
            mesh_access_state_changed(mesh_model);
            break;
    }
    return true;
}

static void generic_on_off_set_handler(mesh_model_t *generic_on_off_server_model, mesh_pdu_t * pdu){
    bool send_status = generic_on_off_handle_set_message(generic_on_off_server_model, pdu);
    if (send_status){
        mesh_upper_transport_pdu_t * transport_pdu = (mesh_upper_transport_pdu_t *) mesh_generic_on_off_status_message(generic_on_off_server_model);
        if (transport_pdu) {
            generic_server_send_message(mesh_access_get_element_address(generic_on_off_server_model), mesh_pdu_src(pdu), mesh_pdu_netkey_index(pdu), mesh_pdu_appkey_index(pdu),(mesh_pdu_t *) transport_pdu);
        }
    }
    mesh_access_message_processed(pdu);
}

static void generic_on_off_set_unacknowledged_handler(mesh_model_t *generic_on_off_server_model, mesh_pdu_t * pdu){
    generic_on_off_handle_set_message(generic_on_off_server_model, pdu);
    mesh_access_message_processed(pdu);
}

// Generic On Off Message
const static mesh_operation_t mesh_generic_on_off_model_operations[] = {
    { MESH_GENERIC_ON_OFF_GET,                                   0, generic_on_off_get_handler },
    { MESH_GENERIC_ON_OFF_SET,                                   2, generic_on_off_set_handler },
    { MESH_GENERIC_ON_OFF_SET_UNACKNOWLEDGED,                    2, generic_on_off_set_unacknowledged_handler },
    { 0, 0, NULL }
};

const mesh_operation_t * mesh_generic_on_off_server_get_operations(void){
    return mesh_generic_on_off_model_operations;
}

void mesh_generic_on_off_server_set(mesh_model_t * mesh_model, uint8_t on_off_value, uint8_t transition_time_gdtt, uint8_t delay_time_gdtt){
    mesh_generic_on_off_state_t * generic_on_off_server_state = (mesh_generic_on_off_state_t *)mesh_model->model_data;
    generic_on_off_server_state->transition_data.target_value = on_off_value;
    
    mesh_server_transition_setup_transition_or_instantaneous_update(mesh_model, transition_time_gdtt, delay_time_gdtt);
    mesh_access_state_changed(mesh_model);
}

uint8_t mesh_generic_on_off_server_get(mesh_model_t *generic_on_off_server_model){
    mesh_generic_on_off_state_t * generic_on_off_server_state = (mesh_generic_on_off_state_t *)generic_on_off_server_model->model_data;
    return generic_on_off_server_state->transition_data.current_value;
}

void mesh_generic_on_off_server_set_publication_model(mesh_model_t *generic_on_off_server_model, mesh_publication_model_t * publication_model){
    if (generic_on_off_server_model == NULL) return;
    if (publication_model == NULL) return;
    publication_model->publish_state_fn = &mesh_generic_on_off_status_message;
    generic_on_off_server_model->publication_model = publication_model;
}

