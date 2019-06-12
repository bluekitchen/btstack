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

    if (state->remaining_time != 0) {
        transport_pdu = mesh_access_setup_segmented_message(&mesh_generic_on_off_status, state->current_on_off_value, 
            state->target_on_off_value, state->remaining_time);
    } else {
        transport_pdu = mesh_access_setup_segmented_message(&mesh_generic_on_off_status, state->current_on_off_value);
    }
    if (!transport_pdu) return;

    // send as segmented access pdu
    generic_server_send_message(generic_on_off_server_model, netkey_index, dest, (mesh_pdu_t *) transport_pdu);
}

static void generic_on_off_get_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    mesh_generic_on_off_status_message(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu));
    mesh_access_message_processed(pdu);
}

// static void generic_on_off_set_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
// }
// static void generic_on_off_set_unacknowledged_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
// }

// Generic On Off Message
const static mesh_operation_t mesh_generic_on_off_model_operations[] = {
    { MESH_GENERIC_ON_OFF_GET,                                   0, generic_on_off_get_handler },
    // { MESH_GENERIC_ON_OFF_SET,                                   4, generic_on_off_set_handler },
    // { MESH_GENERIC_ON_OFF_SET_UNACKNOWLEDGED,                    4, generic_on_off_set_unacknowledged_handler },
    { 0, 0, NULL }
};

const mesh_operation_t * mesh_generic_on_off_server_get_operations(void){
    return mesh_generic_on_off_model_operations;
}
