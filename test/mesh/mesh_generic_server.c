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

#define __BTSTACK_FILE__ "mesh_generic_server.c"

#include <string.h>
#include <stdio.h>
#include "mesh_generic_server.h"
#include "btstack_util.h"
#include "ble/mesh/mesh_network.h"
#include "mesh_keys.h"
#include "mesh_transport.h"
#include "mesh_access.h"
#include "mesh_foundation.h"
#include "bluetooth_company_id.h"
#include "btstack_memory.h"
#include "btstack_debug.h"

// TODO handler in model?
static btstack_packet_handler_t mesh_packet_handler;

static void generic_server_send_message(mesh_model_t *mesh_model, uint16_t netkey_index, uint16_t dest, mesh_pdu_t *pdu){
    if (mesh_model == NULL){
        log_error("mesh_model == NULL"); 
    }
    if (mesh_model->element == NULL){
        log_error("mesh_model->element == NULL"); 
    }
    uint16_t src  = mesh_model->element->unicast_address;
    uint16_t appkey_index = MESH_DEVICE_KEY_INDEX;
    uint8_t  ttl  = mesh_foundation_default_ttl_get();
    mesh_upper_transport_setup_access_pdu_header(pdu, netkey_index, appkey_index, ttl, src, dest, 0);
    mesh_upper_transport_send_access_pdu(pdu);
}

// Generic On Off State

void mesh_generic_on_off_server_register_packet_handler(btstack_packet_handler_t packet_handler){
    mesh_packet_handler = packet_handler;
}

const mesh_access_message_t mesh_generic_on_off_status = {
        MESH_GENERIC_ON_OFF_STATUS, "111"
};

static void mesh_generic_on_off_status_message(mesh_model_t *generic_on_off_server_model, uint16_t netkey_index, uint16_t dest){
    if (generic_on_off_server_model == NULL){
        log_error("generic_on_off_server_model ==  NULL");
    }
    mesh_generic_on_off_state_t * state = (mesh_generic_on_off_state_t *) generic_on_off_server_model->model_data;
    if (state == NULL){
        log_error("generic_on_off_status ==  NULL");
    }
    // setup message
    mesh_transport_pdu_t * transport_pdu = NULL; 

    if (state->remaining_time_ms != 0) {
        transport_pdu = mesh_access_setup_segmented_message(&mesh_generic_on_off_status, state->current_on_off_value, 
            state->target_on_off_value, state->remaining_time_ms);
    } else {
        transport_pdu = mesh_access_setup_segmented_message(&mesh_generic_on_off_status, state->current_on_off_value);
    }
    if (!transport_pdu) return;

    // send as segmented access pdu
    generic_server_send_message(generic_on_off_server_model, netkey_index, dest, (mesh_pdu_t *) transport_pdu);
}

static void generic_on_off_get_handler(mesh_model_t *generic_on_off_server_model, mesh_pdu_t * pdu){
    mesh_generic_on_off_status_message(generic_on_off_server_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu));
    mesh_access_message_processed(pdu);
}

static uint8_t mesh_get_num_steps_from_gdtt(uint8_t transition_time_gdtt){
    return transition_time_gdtt >> 2;
}

static uint32_t mesh_get_time_ms_from_gdtt(uint8_t transition_time_gdtt){
    uint32_t step_resolution_ms = 0;
    uint8_t num_steps  = mesh_get_num_steps_from_gdtt(transition_time_gdtt);
    
    mesh_default_transition_step_resolution_t step_resolution = (mesh_default_transition_step_resolution_t) (transition_time_gdtt & 0x03u);
    switch (step_resolution){
        case MESH_DEFAULT_TRANSITION_STEP_RESOLUTION_100ms:
            step_resolution_ms = 100;
            break;
        case MESH_DEFAULT_TRANSITION_STEP_RESOLUTION_1s:
            step_resolution_ms = 1000;
            break;
        case MESH_DEFAULT_TRANSITION_STEP_RESOLUTION_10s:
            step_resolution_ms = 10000;
            break;
        case MESH_DEFAULT_TRANSITION_STEP_RESOLUTION_10min:
            step_resolution_ms = 600000;
            break;
        default:
            break;

    }
    return num_steps * step_resolution_ms;
}

static void generic_on_off_set_handler(mesh_model_t *generic_on_off_server_model, mesh_pdu_t * pdu){
    if (generic_on_off_server_model == NULL){
        log_error("generic_on_off_server_model == NULL");
    }
    mesh_generic_on_off_state_t * generic_on_off_server_state = (mesh_generic_on_off_state_t *)generic_on_off_server_model->model_data;
    
    if (generic_on_off_server_state == NULL){
        log_error("generic_on_off_server_state == NULL");
    }
    
    uint8_t status = MESH_FOUNDATION_STATUS_SUCCESS;
    
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);
    uint8_t on_off_value = mesh_access_parser_get_u8(&parser);
    // The TID field is a transaction identifier indicating whether the message is 
    // a new message or a retransmission of a previously sent message
    uint8_t tid = mesh_access_parser_get_u8(&parser); 

    if (tid == generic_on_off_server_state->transaction_identifier){
        printf("retransmission\n");
        return;
    }

    if (mesh_access_parser_available(&parser) == 4){
        //  Generic Default Transition Time format - num_steps (higher 6 bits), step_resolution (lower 2 bits) 
        uint8_t  transition_time_gdtt = mesh_access_parser_get_u8(&parser);
        //  Only values of 0x00 through 0x3E shall be used to specify the value of the Transition Number of Steps field
        uint8_t num_steps  = mesh_get_num_steps_from_gdtt(transition_time_gdtt);
        if (num_steps > 0x3E){

        }

        uint32_t transition_time_ms = mesh_get_time_ms_from_gdtt(transition_time_gdtt);
        // Delay is given in 5 millisecond steps 
        uint16_t  delay_ms = mesh_access_parser_get_u8(&parser) * 5;
        printf("todo check/set transition timer transition_time %d ms, delay %d ms", transition_time_ms, delay_ms);
        //TODO: return;
    }

    // Instantanious update
    generic_on_off_server_state->current_on_off_value = on_off_value;
    generic_on_off_server_state->transaction_identifier = tid;
    generic_on_off_server_state->transition_time_ms = 0;
    generic_on_off_server_state->delay_ms = 0;
    mesh_generic_on_off_status_message(generic_on_off_server_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu));
    mesh_access_message_processed(pdu);
    // TODO
    uint8_t reason = 0;
    uint8_t value = on_off_value;
    uint8_t element_index = 0; // TODO generic_on_off_server_model->element_index?
    uint32_t state_identifier = 0; // TODO
    mesh_access_emit_state_update_bool(mesh_packet_handler, element_index, generic_on_off_server_model->model_identifier, state_identifier, reason, value);
}

// static void generic_on_off_set_unacknowledged_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
// }

// Generic On Off Message
const static mesh_operation_t mesh_generic_on_off_model_operations[] = {
    { MESH_GENERIC_ON_OFF_GET,                                   0, generic_on_off_get_handler },
    { MESH_GENERIC_ON_OFF_SET,                                   2, generic_on_off_set_handler },
    // { MESH_GENERIC_ON_OFF_SET_UNACKNOWLEDGED,                    4, generic_on_off_set_unacknowledged_handler },
    { 0, 0, NULL }
};

const mesh_operation_t * mesh_generic_on_off_server_get_operations(void){
    return mesh_generic_on_off_model_operations;
}
