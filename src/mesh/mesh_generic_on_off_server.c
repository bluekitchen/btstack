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

#define __BTSTACK_FILE__ "mesh_generic_on_off_server.c"

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

static void mesh_server_transition_step_bool(mesh_transition_t * transition, transition_event_t event, uint32_t current_timestamp);

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

static void mesh_server_transition_state_update(mesh_transition_bool_t * transition, uint32_t current_timestamp_ms){
    if (transition->base_transition.remaining_delay_time_ms != 0){
        transition->base_transition.state = MESH_TRANSITION_STATE_DELAYED;
        transition->base_transition.remaining_delay_time_ms = 0;
        transition->base_transition.phase_start_ms = current_timestamp_ms;
        return;
    }

    mesh_model_t * generic_on_off_server_model = transition->base_transition.mesh_model;
    if (transition->base_transition.remaining_transition_time_ms != 0){
        transition->base_transition.state = MESH_TRANSITION_STATE_ACTIVE;
        transition->base_transition.phase_start_ms = current_timestamp_ms;
        if (transition->target_value == 1){
            transition->current_value = 1;
            mesh_access_emit_state_update_bool(generic_on_off_server_model->model_packet_handler, 
                mesh_access_get_element_index(generic_on_off_server_model), 
                generic_on_off_server_model->model_identifier, 
                MODEL_STATE_ID_GENERIC_ON_OFF, 
                MODEL_STATE_UPDATE_REASON_TRANSITION_START, 
                transition->current_value);

        }
        return;
    }
    transition->current_value = transition->target_value;
    transition->base_transition.remaining_transition_time_ms = 0;

    // emit event
    mesh_access_emit_state_update_bool(generic_on_off_server_model->model_packet_handler, 
        mesh_access_get_element_index(generic_on_off_server_model), 
        generic_on_off_server_model->model_identifier, 
        MODEL_STATE_ID_GENERIC_ON_OFF, 
        MODEL_STATE_UPDATE_REASON_TRANSITION_END, 
        transition->current_value);
    
    // done, stop transition
    mesh_access_transitions_remove((mesh_transition_t *)transition);
}

static void mesh_server_transition_step_bool(mesh_transition_t * base_transition, transition_event_t event, uint32_t current_timestamp){
    uint32_t time_step_ms;

    mesh_transition_bool_t * transition = (mesh_transition_bool_t*) base_transition;

    switch (transition->base_transition.state){
        case MESH_TRANSITION_STATE_IDLE:
            if (event != TRANSITION_START) break;
            mesh_server_transition_state_update(transition, current_timestamp);
            break;
        case MESH_TRANSITION_STATE_DELAYED:
            if (event != TRANSITION_UPDATE) break;
            time_step_ms = current_timestamp - transition->base_transition.phase_start_ms;
            if (transition->base_transition.remaining_delay_time_ms >= time_step_ms){
                transition->base_transition.remaining_delay_time_ms -= time_step_ms;
            } else {
                transition->base_transition.remaining_delay_time_ms = 0;
                mesh_server_transition_state_update(transition, current_timestamp);
            }
            break;
        case MESH_TRANSITION_STATE_ACTIVE:
            if (event != TRANSITION_UPDATE) break;
            time_step_ms = current_timestamp - transition->base_transition.phase_start_ms;
            if (transition->base_transition.remaining_transition_time_ms >= time_step_ms){
                transition->base_transition.remaining_transition_time_ms -= time_step_ms;
            } else {
                transition->base_transition.remaining_transition_time_ms = 0;
                mesh_server_transition_state_update(transition, current_timestamp);
            }
            break;
        default:
            break;
    }
}

static void mesh_server_transition_setup_transition_or_instantaneous_update(mesh_model_t *mesh_model, uint8_t transition_time_gdtt, uint8_t delay_time_gdtt, model_state_update_reason_t reason){
    mesh_generic_on_off_state_t * generic_on_off_server_state = (mesh_generic_on_off_state_t *)mesh_model->model_data;
    mesh_transition_t transition = generic_on_off_server_state->transition_data.base_transition;

    if (transition_time_gdtt != 0 || delay_time_gdtt != 0) {
        mesh_access_transitions_setup(&transition, (mesh_model_t *) mesh_model, 
            transition_time_gdtt, delay_time_gdtt, &mesh_server_transition_step_bool);
        mesh_access_transitions_add(&transition);
    } else {
        generic_on_off_server_state->transition_data.current_value = generic_on_off_server_state->transition_data.target_value;
        transition.phase_start_ms = 0;
        transition.remaining_delay_time_ms = 0;
        transition.remaining_transition_time_ms = 0;
        transition.state = MESH_TRANSITION_STATE_IDLE;
        
        mesh_access_emit_state_update_bool(mesh_model->model_packet_handler, 
            mesh_access_get_element_index(mesh_model), 
            mesh_model->model_identifier, 
            MODEL_STATE_ID_GENERIC_ON_OFF, 
            reason, 
            generic_on_off_server_state->transition_data.current_value);
    }
}


// Generic On Off State

void mesh_generic_on_off_server_register_packet_handler(mesh_model_t *generic_on_off_server_model, btstack_packet_handler_t transition_events_packet_handler){
    if (transition_events_packet_handler == NULL){
        log_error("mesh_generic_on_off_server_register_packet_handler called with NULL callback");
        return;
    }
    if (generic_on_off_server_model == NULL){
        log_error("mesh_generic_on_off_server_register_packet_handler called with NULL generic_on_off_server_model");
        return;
    }
    generic_on_off_server_model->model_packet_handler = transition_events_packet_handler;
}

const mesh_access_message_t mesh_generic_on_off_status_transition = {
        MESH_GENERIC_ON_OFF_STATUS, "111"
};

const mesh_access_message_t mesh_generic_on_off_status_instantaneous = {
        MESH_GENERIC_ON_OFF_STATUS, "1"
};

static mesh_pdu_t * mesh_generic_on_off_status_message(mesh_model_t *generic_on_off_server_model){
    if (generic_on_off_server_model->element == NULL){
        log_error("generic_on_off_server_model->element == NULL"); 
    }

    mesh_generic_on_off_state_t * state = (mesh_generic_on_off_state_t *) generic_on_off_server_model->model_data;
    if (state == NULL){
        log_error("generic_on_off_status ==  NULL");
    }
    // setup message
    mesh_transport_pdu_t * transport_pdu = NULL; 
    if (state->transition_data.base_transition.remaining_transition_time_ms != 0) {
        transport_pdu = mesh_access_setup_segmented_message(&mesh_generic_on_off_status_transition, state->transition_data.current_value, 
            state->transition_data.target_value, state->transition_data.base_transition.remaining_transition_time_ms);
    } else {
        transport_pdu = mesh_access_setup_segmented_message(&mesh_generic_on_off_status_instantaneous, state->transition_data.current_value);
    }
    return (mesh_pdu_t *) transport_pdu;
}

static void generic_on_off_get_handler(mesh_model_t *generic_on_off_server_model, mesh_pdu_t * pdu){
    mesh_transport_pdu_t * transport_pdu = (mesh_transport_pdu_t *) mesh_generic_on_off_status_message(generic_on_off_server_model);
    if (!transport_pdu) return;
    generic_server_send_message(mesh_access_get_element_address(generic_on_off_server_model), mesh_pdu_src(pdu), mesh_pdu_netkey_index(pdu), mesh_pdu_appkey_index(pdu),(mesh_pdu_t *) transport_pdu);
    mesh_access_message_processed(pdu);
}

static void generic_on_off_handle_set_message(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    if (mesh_model == NULL){
        log_error("mesh_model == NULL");
    }
    mesh_generic_on_off_state_t * generic_on_off_server_state = (mesh_generic_on_off_state_t *)mesh_model->model_data;
    
    if (generic_on_off_server_state == NULL){
        log_error("generic_on_off_server_state == NULL");
    }
    
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);
    uint8_t on_off_value = mesh_access_parser_get_u8(&parser);

    // The TID field is a transaction identifier indicating whether the message is 
    // a new message or a retransmission of a previously sent message
    uint8_t tid = mesh_access_parser_get_u8(&parser); 
    uint8_t transition_time_gdtt = 0;
    uint8_t delay_time_gdtt = 0;
            
    mesh_transition_t * base_transition = generic_on_off_server_get_base_transition(mesh_model);
    switch (mesh_access_transitions_transaction_status(base_transition, tid, mesh_pdu_src(pdu), mesh_pdu_dst(pdu))){
        case MESH_TRANSACTION_STATUS_RETRANSMISSION:
            // ignore on retransmission
            break;
        default:
            mesh_access_transitions_setup_transaction(base_transition, tid, mesh_pdu_src(pdu), mesh_pdu_dst(pdu));
            generic_on_off_server_state->transition_data.target_value = on_off_value;
            
            if (mesh_access_parser_available(&parser) == 2){
                //  Generic Default Transition Time format - num_steps (higher 6 bits), step_resolution (lower 2 bits) 
                transition_time_gdtt = mesh_access_parser_get_u8(&parser);
                delay_time_gdtt = mesh_access_parser_get_u8(&parser);
            } 
            mesh_server_transition_setup_transition_or_instantaneous_update(mesh_model, transition_time_gdtt, delay_time_gdtt, MODEL_STATE_UPDATE_REASON_SET);
            mesh_access_state_changed(mesh_model);
            break;
    }
}

static void generic_on_off_set_handler(mesh_model_t *generic_on_off_server_model, mesh_pdu_t * pdu){
    generic_on_off_handle_set_message(generic_on_off_server_model, pdu);

    mesh_transport_pdu_t * transport_pdu = (mesh_transport_pdu_t *) mesh_generic_on_off_status_message(generic_on_off_server_model);
    if (!transport_pdu) return;
    generic_server_send_message(mesh_access_get_element_address(generic_on_off_server_model), mesh_pdu_src(pdu), mesh_pdu_netkey_index(pdu), mesh_pdu_appkey_index(pdu),(mesh_pdu_t *) transport_pdu);
    mesh_access_message_processed(pdu);
}

static void generic_on_off_set_unacknowledged_handler(mesh_model_t *generic_on_off_server_model, mesh_pdu_t * pdu){
    generic_on_off_handle_set_message(generic_on_off_server_model, pdu);
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
    
    mesh_server_transition_setup_transition_or_instantaneous_update(mesh_model, transition_time_gdtt, delay_time_gdtt, MODEL_STATE_UPDATE_REASON_APPLICATION_CHANGE);
    mesh_access_state_changed(mesh_model);
    // TODO implement publication
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

