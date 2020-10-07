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

#define BTSTACK_FILE__ "mesh_health_server.c"

#include <string.h>
#include <stdio.h>

#include "mesh/mesh_health_server.h"

#include "bluetooth_company_id.h"
#include "btstack_debug.h"
#include "btstack_memory.h"
#include "btstack_util.h"

#include "mesh/mesh.h"
#include "mesh/mesh_access.h"
#include "mesh/mesh_node.h"
#include "mesh/mesh_foundation.h"
#include "mesh/mesh_generic_model.h"
#include "mesh/mesh_generic_on_off_server.h"
#include "mesh/mesh_keys.h"
#include "mesh/mesh_network.h"
#include "mesh/mesh_upper_transport.h"

static void health_server_send_message(uint16_t src, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, mesh_pdu_t *pdu){
    uint8_t ttl = mesh_foundation_default_ttl_get();
    mesh_upper_transport_setup_access_pdu_header(pdu, netkey_index, appkey_index, ttl, src, dest, 0);
    mesh_access_send_unacknowledged_pdu(pdu);
}

static mesh_health_fault_t * mesh_health_server_fault_for_company_id(mesh_model_t *mesh_model, uint16_t company_id){
    mesh_health_state_t * state = (mesh_health_state_t *) mesh_model->model_data;
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, &state->faults);
    while (btstack_linked_list_iterator_has_next(&it)){
        mesh_health_fault_t * fault = (mesh_health_fault_t *) btstack_linked_list_iterator_next(&it);
        if (fault->company_id == company_id) return fault;
    }
    return NULL;
}
static mesh_health_fault_t * mesh_health_server_active_fault(mesh_model_t *mesh_model){
    mesh_health_state_t * state = (mesh_health_state_t *) mesh_model->model_data;
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &state->faults);
    while (btstack_linked_list_iterator_has_next(&it)){
        mesh_health_fault_t * fault = (mesh_health_fault_t *) btstack_linked_list_iterator_next(&it);
        if (fault->num_current_faults > 0) return fault;
    }
    return NULL;
}

static void mesh_health_server_update_publication_model_period_divisor(mesh_model_t * mesh_model){
    if (mesh_model->publication_model == NULL) return;
    mesh_health_fault_t * fault = mesh_health_server_active_fault(mesh_model);
    mesh_health_state_t * health_state = (mesh_health_state_t *) mesh_model->model_data;
    if (fault == NULL){
        mesh_model->publication_model->period_divisor = health_state->fast_period_divisor;
    } else {
        mesh_model->publication_model->period_divisor = 0;
    }
}


// Health State
const mesh_access_message_t mesh_foundation_health_period_status = {
        MESH_FOUNDATION_OPERATION_HEALTH_PERIOD_STATUS, "1"
};

const mesh_access_message_t mesh_foundation_health_attention_status = {
        MESH_FOUNDATION_OPERATION_HEALTH_ATTENTION_STATUS, "1"
};

static mesh_pdu_t * health_period_status(mesh_model_t * mesh_model){
    mesh_health_state_t * state = (mesh_health_state_t *) mesh_model->model_data;
    // setup message
    mesh_upper_transport_pdu_t * message_pdu = mesh_access_setup_message(&mesh_foundation_health_period_status, state->fast_period_divisor);
    return (mesh_pdu_t *) message_pdu;
}

static mesh_pdu_t * health_attention_status(void){
    // setup message
    mesh_upper_transport_pdu_t * message_pdu = mesh_access_setup_message(&mesh_foundation_health_attention_status, mesh_attention_timer_get());
    return (mesh_pdu_t *) message_pdu;
}

// report fault status - used for both current as well as registered faults, see registered_faults param
static mesh_pdu_t * health_fault_status(mesh_model_t * mesh_model, uint32_t opcode, uint16_t company_id, bool registered_faults){
    mesh_upper_transport_builder_t builder;
    mesh_access_message_init(&builder, opcode);

    mesh_health_fault_t * fault = mesh_health_server_fault_for_company_id(mesh_model, company_id);
    if (fault == NULL){
        // no fault state with company_id found
        mesh_access_message_add_uint8(&builder, 0);
        mesh_access_message_add_uint16(&builder, company_id);
    } else {
        mesh_access_message_add_uint8(&builder, fault->test_id);
        mesh_access_message_add_uint16(&builder, fault->company_id);
        int i;
        if (registered_faults){
            for (i = 0; i < fault->num_registered_faults; i++){
                 mesh_access_message_add_uint8(&builder, fault->registered_faults[i]);
            }
        }  else {
            for (i = 0; i < fault->num_current_faults; i++){
                 mesh_access_message_add_uint8(&builder, fault->current_faults[i]);
            }
        }
    }

    mesh_upper_transport_pdu_t * upper_pdu = mesh_access_message_finalize(&builder);

    return (mesh_pdu_t *) upper_pdu;
}

static void health_fault_get_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);
    uint16_t company_id = mesh_access_parser_get_uint16(&parser);

    mesh_upper_transport_pdu_t * transport_pdu = (mesh_upper_transport_pdu_t *) health_fault_status(mesh_model, MESH_FOUNDATION_OPERATION_HEALTH_FAULT_STATUS, company_id, true);
    if (!transport_pdu) return;
    health_server_send_message(mesh_access_get_element_address(mesh_model), mesh_pdu_src(pdu), mesh_pdu_netkey_index(pdu), mesh_pdu_appkey_index(pdu),(mesh_pdu_t *) transport_pdu);
    mesh_access_message_processed(pdu);
}

static uint16_t process_message_fault_clear(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);
    uint16_t company_id = mesh_access_parser_get_uint16(&parser);

    mesh_health_fault_t * fault = mesh_health_server_fault_for_company_id(mesh_model, company_id);
    if (fault != NULL){
        fault->num_registered_faults = 0;
        memset(fault->registered_faults, 0, sizeof(fault->registered_faults));
    }
    return company_id;
}

static void health_fault_clear_handler(mesh_model_t * mesh_model, mesh_pdu_t * pdu){
    uint16_t company_id = process_message_fault_clear(mesh_model, pdu);

    mesh_upper_transport_pdu_t * transport_pdu = (mesh_upper_transport_pdu_t *) health_fault_status(mesh_model, MESH_FOUNDATION_OPERATION_HEALTH_FAULT_STATUS, company_id, true);
    if (!transport_pdu) return;
    health_server_send_message(mesh_access_get_element_address(mesh_model), mesh_pdu_src(pdu), mesh_pdu_netkey_index(pdu), mesh_pdu_appkey_index(pdu),(mesh_pdu_t *) transport_pdu);
    mesh_access_message_processed(pdu);
}

static void health_fault_clear_unacknowledged_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    (void) process_message_fault_clear(mesh_model, pdu);
    mesh_access_message_processed(pdu);
}


static void health_fault_test_process_message(mesh_model_t *mesh_model, mesh_pdu_t * pdu, bool acknowledged){
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);
    uint8_t  test_id    = mesh_access_parser_get_uint8(&parser);
    uint16_t company_id = mesh_access_parser_get_uint16(&parser);
    
    uint16_t dest = mesh_pdu_src(pdu);
    uint16_t netkey_index = mesh_pdu_netkey_index(pdu);
    uint16_t appkey_index = mesh_pdu_appkey_index(pdu);

    // check if fault state exists for company id
    mesh_health_fault_t * fault = mesh_health_server_fault_for_company_id(mesh_model, company_id);
    if (fault == NULL){
        return;
    }

    // short-cut if not packet handler set, but only for standard test
    if (mesh_model->model_packet_handler == NULL){
        if (test_id == 0) {
            mesh_health_server_report_test_done(dest, netkey_index, appkey_index, test_id, company_id, acknowledged);
        }
        return;
    }

    uint8_t event[13];
    int pos = 0;
    event[pos++] = HCI_EVENT_MESH_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = MESH_SUBEVENT_HEALTH_PERFORM_TEST;
    
    little_endian_store_16(event, pos, dest);
    pos += 2;
    little_endian_store_16(event, pos, netkey_index);
    pos += 2;
    little_endian_store_16(event, pos, appkey_index);
    pos += 2;
    little_endian_store_16(event, pos, company_id);
    pos += 2;
    
    event[pos++] = test_id; 
    event[pos++] = acknowledged;

    (*mesh_model->model_packet_handler)(HCI_EVENT_PACKET, 0, event, pos);
}

static void health_fault_test_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    health_fault_test_process_message(mesh_model, pdu, true);
    mesh_access_message_processed(pdu);
}

static void health_fault_test_unacknowledged_handler(mesh_model_t * mesh_model, mesh_pdu_t * pdu){
    health_fault_test_process_message(mesh_model, pdu, false);
    mesh_access_message_processed(pdu);
}

static void health_period_get_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    mesh_upper_transport_pdu_t * transport_pdu = (mesh_upper_transport_pdu_t *) health_period_status(mesh_model);
    if (!transport_pdu) return;
    health_server_send_message(mesh_access_get_element_address(mesh_model), mesh_pdu_src(pdu), mesh_pdu_netkey_index(pdu), mesh_pdu_appkey_index(pdu),(mesh_pdu_t *) transport_pdu);
    mesh_access_message_processed(pdu);
}

static void process_message_period_set(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);
    uint8_t fast_period_divisor = mesh_access_parser_get_uint8(&parser);

    mesh_health_state_t * state = (mesh_health_state_t *) mesh_model->model_data;
    state->fast_period_divisor = fast_period_divisor;

    mesh_health_server_update_publication_model_period_divisor(mesh_model);
}

static void health_period_set_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    process_message_period_set(mesh_model, pdu);
    
    mesh_upper_transport_pdu_t * transport_pdu = (mesh_upper_transport_pdu_t *) health_period_status(mesh_model);
    if (!transport_pdu) return;
    health_server_send_message(mesh_access_get_element_address(mesh_model), mesh_pdu_src(pdu), mesh_pdu_netkey_index(pdu), mesh_pdu_appkey_index(pdu),(mesh_pdu_t *) transport_pdu);
    mesh_access_message_processed(pdu);
}

static void health_period_set_unacknowledged_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    process_message_period_set(mesh_model, pdu);
    mesh_access_message_processed(pdu);
}

static void health_attention_get_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    mesh_upper_transport_pdu_t * transport_pdu = (mesh_upper_transport_pdu_t *) health_attention_status();
    if (!transport_pdu) return;
    health_server_send_message(mesh_access_get_element_address(mesh_model), mesh_pdu_src(pdu), mesh_pdu_netkey_index(pdu), mesh_pdu_appkey_index(pdu),(mesh_pdu_t *) transport_pdu);
    mesh_access_message_processed(pdu);
}

static void process_message_attention_set(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);
    uint8_t timer_s = mesh_access_parser_get_uint8(&parser);
    mesh_attention_timer_set(timer_s);
    
    if (mesh_model->model_packet_handler == NULL) return;
    
    uint8_t event[4];
    int pos = 0;
    event[pos++] = HCI_EVENT_MESH_META;
    // reserve for size
    pos++;
    event[pos++] = MESH_SUBEVENT_HEALTH_ATTENTION_TIMER_CHANGED;
    // element index
    event[pos++] = mesh_model->element->element_index; 
    // element index
    event[1] = pos - 2;
    (*mesh_model->model_packet_handler)(HCI_EVENT_PACKET, 0, event, pos);
}

static void health_attention_set_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    process_message_attention_set(mesh_model, pdu);
    
    mesh_upper_transport_pdu_t * transport_pdu = (mesh_upper_transport_pdu_t *) health_attention_status();
    if (!transport_pdu) return;
    health_server_send_message(mesh_access_get_element_address(mesh_model), mesh_pdu_src(pdu), mesh_pdu_netkey_index(pdu), mesh_pdu_appkey_index(pdu),(mesh_pdu_t *) transport_pdu);
    mesh_access_message_processed(pdu);
}

static void health_attention_set_unacknowledged_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    process_message_attention_set(mesh_model, pdu);
    mesh_access_message_processed(pdu);
}

static mesh_pdu_t * mesh_health_server_publish_state_fn(struct mesh_model * mesh_model){
    uint16_t company_id = mesh_node_get_company_id();
    mesh_health_fault_t * fault = mesh_health_server_active_fault(mesh_model);
    if (fault != NULL){
        company_id = fault->company_id;
    }
    // create current status
    return health_fault_status(mesh_model, MESH_FOUNDATION_OPERATION_HEALTH_CURRENT_STATUS, company_id, false);
}

// Health Message
const static mesh_operation_t mesh_health_model_operations[] = {
    { MESH_FOUNDATION_OPERATION_HEALTH_FAULT_GET,                                   2, health_fault_get_handler },
    { MESH_FOUNDATION_OPERATION_HEALTH_FAULT_CLEAR,                                 2, health_fault_clear_handler },
    { MESH_FOUNDATION_OPERATION_HEALTH_FAULT_CLEAR_UNACKNOWLEDGED,                  2, health_fault_clear_unacknowledged_handler },
    { MESH_FOUNDATION_OPERATION_HEALTH_FAULT_TEST,                                  3, health_fault_test_handler },
    { MESH_FOUNDATION_OPERATION_HEALTH_FAULT_TEST_UNACKNOWLEDGED,                   3, health_fault_test_unacknowledged_handler },
    { MESH_FOUNDATION_OPERATION_HEALTH_PERIOD_GET,                                  0, health_period_get_handler },
    { MESH_FOUNDATION_OPERATION_HEALTH_PERIOD_SET,                                  1, health_period_set_handler },
    { MESH_FOUNDATION_OPERATION_HEALTH_PERIOD_SET_UNACKNOWLEDGED,                   1, health_period_set_unacknowledged_handler },
    { MESH_FOUNDATION_OPERATION_HEALTH_ATTENTION_GET,                               0, health_attention_get_handler },
    { MESH_FOUNDATION_OPERATION_HEALTH_ATTENTION_SET,                               1, health_attention_set_handler },
    { MESH_FOUNDATION_OPERATION_HEALTH_ATTENTION_SET_UNACKNOWLEDGED,                1, health_attention_set_unacknowledged_handler },
    { 0, 0, NULL }
};

const mesh_operation_t * mesh_health_server_get_operations(void){
    return mesh_health_model_operations;
}

void mesh_health_server_register_packet_handler(mesh_model_t *mesh_model, btstack_packet_handler_t events_packet_handler){
    mesh_model->model_packet_handler = events_packet_handler;
}

void mesh_health_server_add_fault_state(mesh_model_t *mesh_model, uint16_t company_id, mesh_health_fault_t * fault_state){
    mesh_health_state_t * state = (mesh_health_state_t *) mesh_model->model_data;
    mesh_health_fault_t * fault = mesh_health_server_fault_for_company_id(mesh_model, company_id);
    btstack_assert(fault == NULL);
    (void) fault;
    fault_state->company_id = company_id;
    btstack_linked_list_add(&state->faults, (btstack_linked_item_t *) fault_state);
}

void mesh_health_server_set_fault(mesh_model_t *mesh_model, uint16_t company_id, uint8_t fault_code){
    uint16_t i;
    mesh_health_fault_t * fault = mesh_health_server_fault_for_company_id(mesh_model, company_id);
    btstack_assert(fault != NULL);

    // add to registered faults
    bool add_registered_fault = true;
    for (i = 0; i < fault->num_registered_faults; i++){
        if (fault->registered_faults[i] == fault_code){
            add_registered_fault = false;
            break;
        }
    }
    if (add_registered_fault && (fault->num_registered_faults < MESH_MAX_NUM_FAULTS)){
        fault->registered_faults[fault->num_registered_faults] = fault_code;
        fault->num_registered_faults++;
    }

    // add to current faults
    bool add_current_fault = true;
    for (i = 0; i < fault->num_current_faults; i++){
        if (fault->registered_faults[i] == fault_code){
            add_current_fault = false;
            break;
        }
    }
    if (add_current_fault && (fault->num_current_faults < MESH_MAX_NUM_FAULTS)){
        fault->registered_faults[fault->num_current_faults] = fault_code;
        fault->num_current_faults++;
    }

    // update publication model
    mesh_health_server_update_publication_model_period_divisor(mesh_model);
}

void mesh_health_server_clear_fault(mesh_model_t *mesh_model, uint16_t company_id, uint8_t fault_code){
    mesh_health_fault_t * fault = mesh_health_server_fault_for_company_id(mesh_model, company_id);
    btstack_assert(fault != NULL);

    // remove from current faults
    uint16_t i;
    bool shift_faults = false;
    for (i = 0; i < fault->num_current_faults; i++){
        if (!shift_faults){
            if (fault->registered_faults[i] == fault_code){
                shift_faults = true;
            }
        }
        if (i < (MESH_MAX_NUM_FAULTS - 1)){
            fault->registered_faults[i] = fault->registered_faults[i+1];
        }
    }

    // update count
    if (shift_faults){
        fault->num_current_faults--;
    }

    // update publication model
    mesh_health_server_update_publication_model_period_divisor(mesh_model);
}

void mesh_health_server_set_publication_model(mesh_model_t * mesh_model, mesh_publication_model_t * publication_model){
    btstack_assert(mesh_model != NULL);
    btstack_assert(publication_model != NULL);
    publication_model->publish_state_fn = &mesh_health_server_publish_state_fn;
    mesh_model->publication_model = publication_model;
}

void mesh_health_server_report_test_done(uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, uint8_t test_id, uint16_t company_id, bool acknowledged){
    mesh_model_t * mesh_model = mesh_node_get_health_server();
    if (mesh_model == NULL) return;
    
    mesh_health_fault_t * fault = mesh_health_server_fault_for_company_id(mesh_model, company_id);
    fault->test_id = test_id;

    // response for acknowledged health fault test
    if (acknowledged){
        mesh_upper_transport_pdu_t * transport_pdu = (mesh_upper_transport_pdu_t *) health_fault_status(mesh_model, MESH_FOUNDATION_OPERATION_HEALTH_FAULT_STATUS, company_id, company_id);
        if (!transport_pdu) return;
        health_server_send_message(mesh_node_get_primary_element_address(), dest, netkey_index, appkey_index, (mesh_pdu_t *) transport_pdu);
    }
}
