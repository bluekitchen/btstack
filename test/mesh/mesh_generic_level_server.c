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
#include "mesh_generic_level_server.h"
#include "btstack_util.h"
#include "ble/mesh/mesh_network.h"
#include "mesh_keys.h"
#include "mesh_transport.h"
#include "mesh_access.h"
#include "mesh_foundation.h"
#include "bluetooth_company_id.h"
#include "btstack_memory.h"
#include "btstack_debug.h"


static void generic_server_send_message(uint16_t src, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, mesh_pdu_t *pdu){
    uint8_t  ttl  = mesh_foundation_default_ttl_get();
    mesh_upper_transport_setup_access_pdu_header(pdu, netkey_index, appkey_index, ttl, src, dest, 0);
    mesh_upper_transport_send_access_pdu(pdu);
}

// Generic On Off State

void mesh_generic_level_server_register_packet_handler(mesh_model_t *generic_level_server_model, btstack_packet_handler_t transition_events_packet_handler){
    if (transition_events_packet_handler == NULL){
        log_error("mesh_generic_level_server_register_packet_handler called with NULL callback");
        return;
    }
    if (generic_level_server_model == NULL){
        log_error("mesh_generic_level_server_register_packet_handler called with NULL generic_level_server_model");
        return;
    }
    generic_level_server_model->transition_events_packet_handler = &transition_events_packet_handler;
}

const mesh_access_message_t mesh_generic_level_status_transition = {
        MESH_GENERIC_LEVEL_STATUS, "221"
};

const mesh_access_message_t mesh_generic_level_status_instantaneous = {
        MESH_GENERIC_LEVEL_STATUS, "2"
};

static void mesh_generic_level_status_message(mesh_model_t *generic_level_server_model, uint16_t netkey_index, uint16_t dest, uint16_t appkey_index){
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
    if (!transport_pdu) return;

    // send as segmented access pdu
    generic_server_send_message(mesh_access_get_element_address(generic_level_server_model), dest, netkey_index, appkey_index, (mesh_pdu_t *) transport_pdu);
}

static void generic_level_get_handler(mesh_model_t *generic_level_server_model, mesh_pdu_t * pdu){
    mesh_generic_level_status_message(generic_level_server_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), mesh_pdu_appkey_index(pdu));
    mesh_access_message_processed(pdu);
}

// Generic On Off Message
const static mesh_operation_t mesh_generic_level_model_operations[] = {
    { MESH_GENERIC_LEVEL_GET,                                   0, generic_level_get_handler },
    // { MESH_GENERIC_LEVEL_SET,                                   2, generic_level_set_handler },
    // { MESH_GENERIC_LEVEL_SET_UNACKNOWLEDGED,                    2, generic_level_set_unacknowledged_handler },
    // { MESH_GENERIC_DELTA_GET,                                   0, generic_delta_get_handler },
    // { MESH_GENERIC_DELTA_SET,                                   2, generic_delta_set_handler },
    // { MESH_GENERIC_MOVE_GET,                                    0, generic_move_get_handler },
    // { MESH_GENERIC_MOVE_SET,                                    2, generic_move_set_handler },
    { 0, 0, NULL }
};

const mesh_operation_t * mesh_generic_level_server_get_operations(void){
    return mesh_generic_level_model_operations;
}

