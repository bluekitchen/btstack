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

#define BTSTACK_FILE__ "mesh_generic_default_transition_time_server.c"

#include "mesh/mesh_generic_default_transition_time_server.h"

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

// Generic Generic Default Transition Time State

const mesh_access_message_t mesh_generic_default_transition_time_status = {
        MESH_GENERIC_DEFAULT_TRANSITION_TIME_STATUS, "1"
};

static mesh_pdu_t * mesh_generic_default_transition_time_status_message(mesh_model_t *generic_default_transition_time_server_model){
    btstack_assert(generic_default_transition_time_server_model->model_data != NULL);

    mesh_generic_default_transition_time_state_t * state = (mesh_generic_default_transition_time_state_t *) generic_default_transition_time_server_model->model_data;
    // setup message
    mesh_upper_transport_pdu_t * transport_pdu = NULL;

    log_info("Default transition time status: value %u", state->value);
    transport_pdu = mesh_access_setup_message(&mesh_generic_default_transition_time_status, state->value);

    return (mesh_pdu_t *) transport_pdu;
}

static void generic_default_transition_time_get_handler(mesh_model_t *generic_default_transition_time_server_model, mesh_pdu_t * pdu){
    mesh_upper_transport_pdu_t * transport_pdu = (mesh_upper_transport_pdu_t *) mesh_generic_default_transition_time_status_message(generic_default_transition_time_server_model);
    if (!transport_pdu) return;
    generic_server_send_message(mesh_access_get_element_address(generic_default_transition_time_server_model), mesh_pdu_src(pdu), mesh_pdu_netkey_index(pdu), mesh_pdu_appkey_index(pdu),(mesh_pdu_t *) transport_pdu);
    mesh_access_message_processed(pdu);
}

// returns if set message was valid
static bool generic_default_transition_time_handle_set_message(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    btstack_assert(mesh_model != NULL);
    btstack_assert(mesh_model->model_data != NULL);

    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);
    uint8_t time_gdtt = mesh_access_parser_get_uint8(&parser);

    uint8_t num_steps  = mesh_access_transitions_num_steps_from_gdtt(time_gdtt);
    // check for valid value
    if (num_steps > 0x3E) return false;
    mesh_generic_default_transition_time_state_t * generic_default_transition_time_server_state = (mesh_generic_default_transition_time_state_t *)mesh_model->model_data;
    generic_default_transition_time_server_state->value = time_gdtt;

    return true;
}

static void generic_default_transition_time_set_handler(mesh_model_t *generic_default_transition_time_server_model, mesh_pdu_t * pdu){
    bool send_status = generic_default_transition_time_handle_set_message(generic_default_transition_time_server_model, pdu);
    if (send_status){
        mesh_upper_transport_pdu_t * transport_pdu = (mesh_upper_transport_pdu_t *) mesh_generic_default_transition_time_status_message(generic_default_transition_time_server_model);
        if (transport_pdu) {
            generic_server_send_message(mesh_access_get_element_address(generic_default_transition_time_server_model), mesh_pdu_src(pdu), mesh_pdu_netkey_index(pdu), mesh_pdu_appkey_index(pdu),(mesh_pdu_t *) transport_pdu);
        }
    }
    mesh_access_message_processed(pdu);
}

static void generic_default_transition_time_set_unacknowledged_handler(mesh_model_t *generic_default_transition_time_server_model, mesh_pdu_t * pdu){
    generic_default_transition_time_handle_set_message(generic_default_transition_time_server_model, pdu);
    mesh_access_message_processed(pdu);
}

// Generic On Off Message
const static mesh_operation_t mesh_generic_default_transition_time_model_operations[] = {
    { MESH_GENERIC_DEFAULT_TRANSITION_TIME_GET,                                   0, generic_default_transition_time_get_handler },
    { MESH_GENERIC_DEFAULT_TRANSITION_TIME_SET,                                   1, generic_default_transition_time_set_handler },
    { MESH_GENERIC_DEFAULT_TRANSITION_TIME_SET_UNACKNOWLEDGED,                    1, generic_default_transition_time_set_unacknowledged_handler },
    { 0, 0, NULL }
};

const mesh_operation_t * mesh_generic_default_transition_time_server_get_operations(void){
    return mesh_generic_default_transition_time_model_operations;
}

void mesh_generic_default_transition_time_server_set(mesh_model_t * mesh_model, uint8_t transition_time_gdtt){
    mesh_generic_default_transition_time_state_t * generic_default_transition_time_server_state = (mesh_generic_default_transition_time_state_t *)mesh_model->model_data;
    generic_default_transition_time_server_state->value = transition_time_gdtt;
}

uint8_t mesh_generic_default_transition_time_server_get(mesh_model_t *generic_default_transition_time_server_model){
    mesh_generic_default_transition_time_state_t * generic_default_transition_time_server_state = (mesh_generic_default_transition_time_state_t *)generic_default_transition_time_server_model->model_data;
    return generic_default_transition_time_server_state->value;
}


