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

#define __BTSTACK_FILE__ "mesh_generic_level_server.c"

#include <string.h>
#include <stdio.h>

#include "mesh/mesh_generic_level_server.h"

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

// Transition 
static int16_t add_and_clip_int16(int16_t current_value, int16_t increment){
    int32_t value = current_value + increment;
    if (value < -32768){
        value = -32768;
    } else if (value > 32767){
        value = 32767;
    }
   return (int16_t) value;
}

static mesh_transition_t * generic_level_server_get_base_transition(mesh_model_t * mesh_model) {
     mesh_generic_level_state_t * generic_level_server_state = (mesh_generic_level_state_t *)mesh_model->model_data;
    return &generic_level_server_state->transition_data.base_transition;
}

static void mesh_server_transition_state_update_stepwise_value(mesh_transition_int16_t * transition){
    mesh_model_t * generic_level_server_model = transition->base_transition.mesh_model;
    
    if (transition->stepwise_value_increment){
        transition->current_value = add_and_clip_int16(transition->current_value, transition->stepwise_value_increment);
    } else if (transition->delta_from_initial_value){
        transition->current_value = add_and_clip_int16(transition->initial_value, transition->delta_from_initial_value);
    }
    // emit event
    mesh_access_emit_state_update_int16(generic_level_server_model->model_packet_handler, 
        mesh_access_get_element_index(generic_level_server_model), 
        generic_level_server_model->model_identifier, 
        MODEL_STATE_ID_GENERIC_LEVEL, 
        MODEL_STATE_UPDATE_REASON_TRANSITION_END, 
        transition->current_value);
}

static void mesh_server_transition_state_update(mesh_transition_int16_t * transition, uint32_t current_timestamp_ms){
    if (transition->base_transition.remaining_delay_time_ms != 0){
        transition->base_transition.state = MESH_TRANSITION_STATE_DELAYED;
        transition->base_transition.remaining_delay_time_ms = 0;
        transition->base_transition.phase_start_ms = current_timestamp_ms;
        return;
    }

    mesh_model_t * generic_level_server_model = transition->base_transition.mesh_model;
    
    if (transition->base_transition.remaining_transition_time_ms != 0){
        transition->base_transition.state = MESH_TRANSITION_STATE_ACTIVE;
        transition->base_transition.phase_start_ms = current_timestamp_ms;
        return;
    }
    
    transition->current_value = transition->target_value;
    transition->base_transition.remaining_transition_time_ms = 0;

    // emit event
    mesh_access_emit_state_update_int16(generic_level_server_model->model_packet_handler, 
        mesh_access_get_element_index(generic_level_server_model), 
        generic_level_server_model->model_identifier, 
        MODEL_STATE_ID_GENERIC_LEVEL, 
        MODEL_STATE_UPDATE_REASON_TRANSITION_END, 
        transition->current_value);
    
    // done, stop transition
    mesh_access_transitions_remove((mesh_transition_t *)transition);
}

static void mesh_server_transition_step(mesh_transition_t * base_transition, transition_event_t event, uint32_t current_timestamp){
    uint32_t time_step_ms;

    mesh_transition_int16_t * transition = (mesh_transition_int16_t*) base_transition;

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
                mesh_server_transition_state_update_stepwise_value(transition);
            } else {
                transition->base_transition.remaining_transition_time_ms = 0;
                mesh_server_transition_state_update(transition, current_timestamp);
            }
            break;
        default:
            break;
    }
}


static void mesh_server_transition_setup_transition_or_instantaneous_update_int16(mesh_model_t *mesh_model, uint8_t transition_time_gdtt, uint8_t delay_time_gdtt, model_state_update_reason_t reason){
    mesh_generic_level_state_t * generic_level_server_state = (mesh_generic_level_state_t *)mesh_model->model_data;
    mesh_transition_t transition = generic_level_server_state->transition_data.base_transition;

    if (transition_time_gdtt != 0 || delay_time_gdtt != 0) {
        mesh_access_transitions_setup(&transition, mesh_model, transition_time_gdtt, delay_time_gdtt, &mesh_server_transition_step);
        mesh_access_transitions_add(&transition);
    } else {
        // instantaneous update
        generic_level_server_state->transition_data.current_value = generic_level_server_state->transition_data.target_value;
        generic_level_server_state->transition_data.stepwise_value_increment = 0;
        generic_level_server_state->transition_data.delta_from_initial_value = 0;
        generic_level_server_state->transition_data.transition_speed = 0;
        mesh_access_transitions_setup(&transition, mesh_model, 0, 0, NULL);

        mesh_access_emit_state_update_int16(mesh_model->model_packet_handler, 
            mesh_access_get_element_index(mesh_model), 
            mesh_model->model_identifier, 
            MODEL_STATE_ID_GENERIC_LEVEL, 
            reason, 
            generic_level_server_state->transition_data.current_value);
    }
}
// Generic Level State

void mesh_generic_level_server_register_packet_handler(mesh_model_t *generic_level_server_model, btstack_packet_handler_t transition_events_packet_handler){
    if (transition_events_packet_handler == NULL){
        log_error("mesh_generic_level_server_register_packet_handler called with NULL callback");
        return;
    }
    if (generic_level_server_model == NULL){
        log_error("mesh_generic_level_server_register_packet_handler called with NULL generic_level_server_model");
        return;
    }
    generic_level_server_model->model_packet_handler = transition_events_packet_handler;
}

const mesh_access_message_t mesh_generic_level_status_transition = {
        MESH_GENERIC_LEVEL_STATUS, "221"
};

const mesh_access_message_t mesh_generic_level_status_instantaneous = {
        MESH_GENERIC_LEVEL_STATUS, "2"
};

static mesh_pdu_t * mesh_generic_level_status_message(mesh_model_t *generic_level_server_model){
    if (generic_level_server_model->element == NULL){
        log_error("generic_level_server_model->element == NULL"); 
    }

    mesh_generic_level_state_t * state = (mesh_generic_level_state_t *) generic_level_server_model->model_data;
    if (state == NULL){
        log_error("generic_level_status ==  NULL");
    }

    // setup message
    mesh_transport_pdu_t * transport_pdu = NULL; 
    if (state->transition_data.base_transition.remaining_transition_time_ms != 0) {
        transport_pdu = mesh_access_setup_segmented_message(&mesh_generic_level_status_transition, state->transition_data.current_value, 
            state->transition_data.target_value, state->transition_data.base_transition.remaining_transition_time_ms);
    } else {
        transport_pdu = mesh_access_setup_segmented_message(&mesh_generic_level_status_instantaneous, state->transition_data.current_value);
    }
    return (mesh_pdu_t *)transport_pdu;
}

static void generic_level_handle_set_target_level_message(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    if (mesh_model == NULL){
        log_error("mesh_model == NULL");
    }
    mesh_generic_level_state_t * generic_level_server_state = (mesh_generic_level_state_t *)mesh_model->model_data;
    
    if (generic_level_server_state == NULL){
        log_error("generic_level_server_state == NULL");
    }
    
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);
    int16_t level_value = (int16_t)mesh_access_parser_get_u16(&parser);
    // The TID field is a transaction identifier indicating whether the message is 
    // a new message or a retransmission of a previously sent message
    uint8_t tid = mesh_access_parser_get_u8(&parser); 
    
    uint8_t transition_time_gdtt = 0;
    uint8_t delay_time_gdtt = 0;
    mesh_transition_t * base_transition = generic_level_server_get_base_transition(mesh_model);

    switch (mesh_access_transitions_transaction_status(base_transition, tid, mesh_pdu_src(pdu), mesh_pdu_dst(pdu))){
        case MESH_TRANSACTION_STATUS_RETRANSMISSION:
            // ignore
            break;
        default:
            mesh_access_transitions_setup_transaction(base_transition, tid, mesh_pdu_src(pdu), mesh_pdu_dst(pdu));
    
            generic_level_server_state->transition_data.initial_value = generic_level_server_state->transition_data.current_value;
            generic_level_server_state->transition_data.target_value = level_value;
            generic_level_server_state->transition_data.stepwise_value_increment = 0;
            generic_level_server_state->transition_data.delta_from_initial_value = 0;

            if (mesh_access_parser_available(&parser) == 2){
                //  Generic Default Transition Time format - num_steps (higher 6 bits), step_resolution (lower 2 bits) 
                transition_time_gdtt = mesh_access_parser_get_u8(&parser);
                delay_time_gdtt = mesh_access_parser_get_u8(&parser);
                int num_steps = mesh_access_transitions_num_steps_from_gdtt(transition_time_gdtt);
                if (num_steps > 0){
                    // TODO: remove division
                    generic_level_server_state->transition_data.stepwise_value_increment = (level_value - generic_level_server_state->transition_data.current_value)/num_steps;
                }
            } 
            mesh_server_transition_setup_transition_or_instantaneous_update_int16(mesh_model, transition_time_gdtt, delay_time_gdtt, MODEL_STATE_UPDATE_REASON_SET);
            mesh_access_state_changed(mesh_model);
            break;
    }
   
}

static void generic_level_handle_set_move_message(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    if (mesh_model == NULL){
        log_error("mesh_model == NULL");
    }
    mesh_generic_level_state_t * generic_level_server_state = (mesh_generic_level_state_t *)mesh_model->model_data;
    
    if (generic_level_server_state == NULL){
        log_error("generic_level_server_state == NULL");
    }
    
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);
    int16_t increment_value = (int16_t)mesh_access_parser_get_u16(&parser);
    
    // The TID field is a transaction identifier indicating whether the message is 
    // a new message or a retransmission of a previously sent message
    uint8_t tid = mesh_access_parser_get_u8(&parser); 
    
    uint8_t transition_time_gdtt = 0;
    uint8_t delay_time_gdtt = 0;
    mesh_transition_t * base_transition = generic_level_server_get_base_transition(mesh_model);
    
    switch (mesh_access_transitions_transaction_status(base_transition, tid, mesh_pdu_src(pdu), mesh_pdu_dst(pdu))){
        case MESH_TRANSACTION_STATUS_RETRANSMISSION:
            // ignore retransmission
            break;
        default:
            mesh_access_transitions_setup_transaction(base_transition, tid, mesh_pdu_src(pdu), mesh_pdu_dst(pdu));

            generic_level_server_state->transition_data.initial_value = generic_level_server_state->transition_data.current_value;
            generic_level_server_state->transition_data.target_value = add_and_clip_int16(generic_level_server_state->transition_data.current_value, increment_value);
            generic_level_server_state->transition_data.stepwise_value_increment = increment_value;
            generic_level_server_state->transition_data.delta_from_initial_value = 0;

            if (mesh_access_parser_available(&parser) == 2){
                //  Generic Default Transition Time format - num_steps (higher 6 bits), step_resolution (lower 2 bits) 
                transition_time_gdtt = mesh_access_parser_get_u8(&parser);
                delay_time_gdtt = mesh_access_parser_get_u8(&parser);
                int num_steps = mesh_access_transitions_num_steps_from_gdtt(transition_time_gdtt);
                if (num_steps > 0){
                    // TODO: remove division
                    generic_level_server_state->transition_data.target_value = add_and_clip_int16(generic_level_server_state->transition_data.current_value, increment_value * num_steps);
                }
            } 
            mesh_server_transition_setup_transition_or_instantaneous_update_int16(mesh_model, transition_time_gdtt, delay_time_gdtt, MODEL_STATE_UPDATE_REASON_SET);        
            break;
    }
}

static void generic_level_handle_set_delta_message(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    if (mesh_model == NULL){
        log_error("mesh_model == NULL");
    }
    mesh_generic_level_state_t * generic_level_server_state = (mesh_generic_level_state_t *)mesh_model->model_data;
    
    if (generic_level_server_state == NULL){
        log_error("generic_level_server_state == NULL");
    }
    
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);
    int16_t delta_value = (int16_t) mesh_access_parser_get_u16(&parser);
    
    // The TID field is a transaction identifier indicating whether the message is 
    // a new message or a retransmission of a previously sent message
    uint8_t tid = mesh_access_parser_get_u8(&parser); 
    uint8_t transition_time_gdtt = 0;
    uint8_t delay_time_gdtt = 0;
            
    mesh_transition_t * base_transition = generic_level_server_get_base_transition(mesh_model);
    
    switch (mesh_access_transitions_transaction_status(base_transition, tid, mesh_pdu_src(pdu), mesh_pdu_dst(pdu))){
        case MESH_TRANSACTION_STATUS_DIFFERENT_DST_OR_SRC:
            // abort transaction
            mesh_access_transitions_abort_transaction(base_transition);
            generic_level_server_state->transition_data.current_value = generic_level_server_state->transition_data.initial_value;
            mesh_server_transition_setup_transition_or_instantaneous_update_int16(mesh_model, 0, 0, MODEL_STATE_UPDATE_REASON_TRANSITION_ABORT);       
            break;
        case MESH_TRANSACTION_STATUS_RETRANSMISSION:
            // ignore
            break;
        case MESH_TRANSACTION_STATUS_NEW:
            mesh_access_transitions_setup_transaction(base_transition, tid, mesh_pdu_src(pdu), mesh_pdu_dst(pdu));

            generic_level_server_state->transition_data.initial_value = generic_level_server_state->transition_data.current_value;
            generic_level_server_state->transition_data.target_value = add_and_clip_int16(generic_level_server_state->transition_data.current_value, delta_value);
            generic_level_server_state->transition_data.delta_from_initial_value = delta_value;
            generic_level_server_state->transition_data.stepwise_value_increment = 0;
            
            if (mesh_access_parser_available(&parser) == 2){
                //  Generic Default Transition Time format - num_steps (higher 6 bits), step_resolution (lower 2 bits) 
                transition_time_gdtt = mesh_access_parser_get_u8(&parser);
                delay_time_gdtt = mesh_access_parser_get_u8(&parser);
            } 
            mesh_server_transition_setup_transition_or_instantaneous_update_int16(mesh_model, transition_time_gdtt, delay_time_gdtt, MODEL_STATE_UPDATE_REASON_SET);       
            mesh_access_state_changed(mesh_model);
            break;
        default:
            break;
    }
}


static void generic_level_get_handler(mesh_model_t *generic_level_server_model, mesh_pdu_t * pdu){
    mesh_transport_pdu_t * transport_pdu = (mesh_transport_pdu_t *) mesh_generic_level_status_message(generic_level_server_model);
    if (!transport_pdu) return;
    generic_server_send_message(mesh_access_get_element_address(generic_level_server_model), mesh_pdu_src(pdu), mesh_pdu_netkey_index(pdu), mesh_pdu_appkey_index(pdu), (mesh_pdu_t *) transport_pdu);
    mesh_access_message_processed(pdu);
}

static void generic_level_set_handler(mesh_model_t *generic_level_server_model, mesh_pdu_t * pdu){
    generic_level_handle_set_target_level_message(generic_level_server_model, pdu);

    mesh_transport_pdu_t * transport_pdu = (mesh_transport_pdu_t *) mesh_generic_level_status_message(generic_level_server_model);
    if (!transport_pdu) return;
    generic_server_send_message(mesh_access_get_element_address(generic_level_server_model), mesh_pdu_src(pdu), mesh_pdu_netkey_index(pdu), mesh_pdu_appkey_index(pdu), (mesh_pdu_t *) transport_pdu);
    mesh_access_message_processed(pdu);
}

static void generic_level_set_unacknowledged_handler(mesh_model_t *generic_level_server_model, mesh_pdu_t * pdu){
    generic_level_handle_set_target_level_message(generic_level_server_model, pdu);
}

static void generic_delta_set_handler(mesh_model_t *generic_level_server_model, mesh_pdu_t * pdu){
    generic_level_handle_set_delta_message(generic_level_server_model, pdu);

    mesh_transport_pdu_t * transport_pdu = (mesh_transport_pdu_t *) mesh_generic_level_status_message(generic_level_server_model);
    if (!transport_pdu) return;
    generic_server_send_message(mesh_access_get_element_address(generic_level_server_model), mesh_pdu_src(pdu), mesh_pdu_netkey_index(pdu), mesh_pdu_appkey_index(pdu), (mesh_pdu_t *) transport_pdu);
    mesh_access_message_processed(pdu);
}

static void generic_delta_set_unacknowledged_handler(mesh_model_t *generic_level_server_model, mesh_pdu_t * pdu){
    generic_level_handle_set_delta_message(generic_level_server_model, pdu);
}

static void generic_move_get_handler(mesh_model_t *generic_level_server_model, mesh_pdu_t * pdu){
    generic_level_handle_set_move_message(generic_level_server_model, pdu);

    mesh_transport_pdu_t * transport_pdu = (mesh_transport_pdu_t *) mesh_generic_level_status_message(generic_level_server_model);
    if (!transport_pdu) return;
    generic_server_send_message(mesh_access_get_element_address(generic_level_server_model), mesh_pdu_src(pdu), mesh_pdu_netkey_index(pdu), mesh_pdu_appkey_index(pdu), (mesh_pdu_t *) transport_pdu);
    mesh_access_message_processed(pdu);
}

static void generic_move_set_unacknowledged_handler(mesh_model_t *generic_level_server_model, mesh_pdu_t * pdu){
    generic_level_handle_set_move_message(generic_level_server_model, pdu);
}

// Generic On Off Message
const static mesh_operation_t mesh_generic_level_model_operations[] = {
    { MESH_GENERIC_LEVEL_GET,                                   0, generic_level_get_handler },
    { MESH_GENERIC_LEVEL_SET,                                   3, generic_level_set_handler },
    { MESH_GENERIC_LEVEL_SET_UNACKNOWLEDGED,                    3, generic_level_set_unacknowledged_handler },
    { MESH_GENERIC_DELTA_SET,                                   3, generic_delta_set_handler },
    { MESH_GENERIC_DELTA_SET_UNACKNOWLEDGED,                    3, generic_delta_set_unacknowledged_handler },
    { MESH_GENERIC_MOVE_SET,                                    3, generic_move_get_handler },
    { MESH_GENERIC_MOVE_SET_UNACKNOWLEDGED,                     3, generic_move_set_unacknowledged_handler },
    { 0, 0, NULL }
};

const mesh_operation_t * mesh_generic_level_server_get_operations(void){
    return mesh_generic_level_model_operations;
}

void mesh_generic_level_server_set_publication_model(mesh_model_t *generic_level_server_model, mesh_publication_model_t * publication_model){
    if (generic_level_server_model == NULL) return;
    if (publication_model == NULL) return;
    publication_model->publish_state_fn = &mesh_generic_level_status_message;
    generic_level_server_model->publication_model = publication_model;
}
