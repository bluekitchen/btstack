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

#define __BTSTACK_FILE__ "mesh_access.c"

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "mesh/mesh_upper_transport.h"
#include "mesh/beacon.h"
#include "mesh_access.h"
#include "btstack_memory.h"
#include "btstack_debug.h"
#include "mesh_foundation.h"
#include "btstack_tlv.h"

#define MEST_TRANSACTION_TIMEOUT_MS  6000

static void mesh_access_message_process_handler(mesh_pdu_t * pdu);
static void mesh_access_secure_network_beacon_handler(uint8_t packet_type, uint16_t channel, uint8_t * packet, uint16_t size);

static uint16_t primary_element_address;

static mesh_element_t primary_element;
static uint16_t mesh_element_index_next;

static btstack_linked_list_t mesh_elements;

static uint16_t mid_counter;

static const btstack_tlv_t * btstack_tlv_singleton_impl;
static void *                btstack_tlv_singleton_context;

// Transitions
static btstack_linked_list_t  transitions;
static btstack_timer_source_t transitions_timer;
static int transition_step_min_ms;
static uint8_t mesh_transaction_id_counter = 0;

static void mesh_access_setup_tlv(void){
    if (btstack_tlv_singleton_impl) return;
    btstack_tlv_get_instance(&btstack_tlv_singleton_impl, &btstack_tlv_singleton_context);
}

void mesh_access_init(void){
    // Access layer - add Primary Element to list of elements
    mesh_element_add(&primary_element);

    // register with upper transport
    mesh_upper_transport_register_access_message_handler(&mesh_access_message_process_handler);

    // register for secure network beacons
    beacon_register_for_secure_network_beacons(&mesh_access_secure_network_beacon_handler);
}

void mesh_access_emit_state_update_bool(btstack_packet_handler_t * event_handler, uint8_t element_index, uint32_t model_identifier, 
    model_state_id_t state_identifier, model_state_update_reason_t reason, uint8_t value){
    if (event_handler == NULL) return;
    uint8_t event[14] = {HCI_EVENT_MESH_META, 13, MESH_SUBEVENT_STATE_UPDATE_BOOL};
    int pos = 3;
    event[pos++] = element_index;
    little_endian_store_32(event, pos, model_identifier);
    pos += 4;
    little_endian_store_32(event, pos, (uint32_t)state_identifier);
    pos += 4;
    event[pos++] = (uint8_t)reason;
    event[pos++] = value;
    (*event_handler)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

void mesh_access_emit_state_update_int16(btstack_packet_handler_t * event_handler, uint8_t element_index, uint32_t model_identifier, 
    model_state_id_t state_identifier, model_state_update_reason_t reason, int16_t value){
    if (event_handler == NULL) return;
    uint8_t event[14] = {HCI_EVENT_MESH_META, 13, MESH_SUBEVENT_STATE_UPDATE_BOOL};
    int pos = 3;
    event[pos++] = element_index;
    little_endian_store_32(event, pos, model_identifier);
    pos += 4;
    little_endian_store_32(event, pos, (uint32_t)state_identifier);
    pos += 4;
    event[pos++] = (uint8_t)reason;
    little_endian_store_16(event, pos, (uint16_t) value);
    pos += 2;
    (*event_handler)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

// Mesh Model Transitions

void mesh_access_transitions_setup_transaction(mesh_transition_t * transition, uint8_t transaction_identifier, uint16_t src_address, uint16_t dst_address){
    transition->transaction_timestamp_ms = btstack_run_loop_get_time_ms();
    transition->transaction_identifier = transaction_identifier;
    transition->src_address = src_address;
    transition->dst_address = dst_address;
}

void mesh_access_transitions_abort_transaction(mesh_transition_t * transition){
    mesh_access_transitions_remove(transition);
}


static int mesh_access_transitions_transaction_is_expired(mesh_transition_t * transition){
    return (btstack_run_loop_get_time_ms() - transition->transaction_timestamp_ms) > MEST_TRANSACTION_TIMEOUT_MS;
}

mesh_transaction_status_t mesh_access_transitions_transaction_status(mesh_transition_t * transition, uint8_t transaction_identifier, uint16_t src_address, uint16_t dst_address){
    if (transition->src_address != src_address || transition->dst_address != dst_address) return MESH_TRANSACTION_STATUS_DIFFERENT_DST_OR_SRC; 

    if (transition->transaction_identifier == transaction_identifier && !mesh_access_transitions_transaction_is_expired(transition)){
            return MESH_TRANSACTION_STATUS_RETRANSMISSION;
    }
    return MESH_TRANSACTION_STATUS_NEW;
}

uint8_t mesh_access_transitions_num_steps_from_gdtt(uint8_t time_gdtt){
    return time_gdtt >> 2;
}

static uint32_t mesh_access_transitions_step_ms_from_gdtt(uint8_t time_gdtt){
    mesh_default_transition_step_resolution_t step_resolution = (mesh_default_transition_step_resolution_t) (time_gdtt & 0x03u);
    switch (step_resolution){
        case MESH_DEFAULT_TRANSITION_STEP_RESOLUTION_100ms:
            return 100;
        case MESH_DEFAULT_TRANSITION_STEP_RESOLUTION_1s:
            return 1000;
        case MESH_DEFAULT_TRANSITION_STEP_RESOLUTION_10s:
            return 10000;
        case MESH_DEFAULT_TRANSITION_STEP_RESOLUTION_10min:
            return 600000;
        default:
            return 0;
    }
}

uint32_t mesh_access_time_gdtt2ms(uint8_t time_gdtt){
    uint8_t num_steps  = mesh_access_transitions_num_steps_from_gdtt(time_gdtt);
    if (num_steps > 0x3E) return 0;

    return mesh_access_transitions_step_ms_from_gdtt(time_gdtt) * num_steps;
}

static void mesh_access_transitions_timeout_handler(btstack_timer_source_t * timer){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, &transitions);
    while (btstack_linked_list_iterator_has_next(&it)){
        mesh_transition_t * transition = (mesh_transition_t *)btstack_linked_list_iterator_next(&it);
        (transition->transition_callback)(transition, TRANSITION_UPDATE, btstack_run_loop_get_time_ms());
    }
    if (btstack_linked_list_empty(&transitions)) return;
    
    btstack_run_loop_set_timer(timer, transition_step_min_ms); 
    btstack_run_loop_add_timer(timer);
}

static void mesh_access_transitions_timer_start(void){
    btstack_run_loop_remove_timer(&transitions_timer);
    btstack_run_loop_set_timer_handler(&transitions_timer, mesh_access_transitions_timeout_handler);
    btstack_run_loop_set_timer(&transitions_timer, transition_step_min_ms); 
    btstack_run_loop_add_timer(&transitions_timer);
}

static void mesh_access_transitions_timer_stop(void){
    btstack_run_loop_remove_timer(&transitions_timer);
} 

static uint32_t mesh_access_transitions_get_step_min_ms(void){
    uint32_t min_timeout_ms = 0;

    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, &transitions);
    while (btstack_linked_list_iterator_has_next(&it)){
        mesh_transition_t * transition = (mesh_transition_t *)btstack_linked_list_iterator_next(&it);
        if (min_timeout_ms == 0 || transition->step_duration_ms < min_timeout_ms){
            min_timeout_ms = transition->step_duration_ms;
        }
    }
    return min_timeout_ms;
}

void mesh_access_transitions_setup(mesh_transition_t * transition, mesh_model_t * mesh_model, 
    uint8_t transition_time_gdtt, uint8_t delay_gdtt,
    void (* transition_callback)(struct mesh_transition * transition, transition_event_t event, uint32_t current_timestamp)){

    //  Only values of 0x00 through 0x3E shall be used to specify the value of the Transition Number of Steps field
    uint8_t num_steps  = mesh_access_transitions_num_steps_from_gdtt(transition_time_gdtt);
    if (num_steps > 0x3E) return;

    transition->state = MESH_TRANSITION_STATE_IDLE;
    transition->phase_start_ms = 0;

    transition->mesh_model = mesh_model;
    transition->transition_callback = transition_callback;
    transition->step_duration_ms = mesh_access_transitions_step_ms_from_gdtt(transition_time_gdtt);
    transition->remaining_delay_time_ms = delay_gdtt * 5;
    transition->remaining_transition_time_ms = num_steps * transition->step_duration_ms;
}

void mesh_access_transitions_add(mesh_transition_t * transition){
    if (transition->step_duration_ms == 0) return;
    
    if (btstack_linked_list_empty(&transitions) || transition->step_duration_ms < transition_step_min_ms){
        transition_step_min_ms = transition->step_duration_ms;
    }
    mesh_access_transitions_timer_start();
    btstack_linked_list_add(&transitions, (btstack_linked_item_t *) transition);
    (transition->transition_callback)(transition, TRANSITION_START, btstack_run_loop_get_time_ms());
}

void mesh_access_transitions_remove(mesh_transition_t * transition){
    mesh_access_transitions_setup(transition, NULL, 0, 0, NULL);
    btstack_linked_list_remove(&transitions, (btstack_linked_item_t *) transition);

    if (btstack_linked_list_empty(&transitions)){
        mesh_access_transitions_timer_stop();
    } else {
        transition_step_min_ms = mesh_access_transitions_get_step_min_ms();        
    }
}

uint8_t mesh_access_transactions_get_next_transaction_id(void){
    mesh_transaction_id_counter++;
    if (mesh_transaction_id_counter == 0){
        mesh_transaction_id_counter = 1;
    }
    return mesh_transaction_id_counter;
}   

// Mesh Node Element functions
mesh_element_t * mesh_primary_element(void){
    return &primary_element;
}

void mesh_access_set_primary_element_address(uint16_t unicast_address){
    primary_element_address = unicast_address;
}

uint16_t mesh_access_get_primary_element_address(void){
    return primary_element_address;
}

uint8_t mesh_access_get_element_index(mesh_model_t * mesh_model){
    return mesh_model->element->element_index;
}

void mesh_access_set_primary_element_location(uint16_t location){
    primary_element.loc = location;
}

void mesh_element_add(mesh_element_t * element){
    element->element_index = mesh_element_index_next++;
    btstack_linked_list_add_tail(&mesh_elements, (void*) element);
}

mesh_element_t * mesh_element_for_unicast_address(uint16_t unicast_address){
    uint16_t element_index = unicast_address - primary_element_address;
    return mesh_element_for_index(element_index);
}

mesh_element_t * mesh_element_for_index(uint16_t element_index){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &mesh_elements);
    while (btstack_linked_list_iterator_has_next(&it)){
        mesh_element_t * element = (mesh_element_t *) btstack_linked_list_iterator_next(&it);
        if (element->element_index != element_index) continue;
        return element;
    }
    return NULL;
}

uint16_t mesh_access_get_element_address(mesh_model_t * mesh_model){
    return primary_element_address + mesh_model->element->element_index;
}

// Model Identifier utilities

uint32_t mesh_model_get_model_identifier(uint16_t vendor_id, uint16_t model_id){
    return (vendor_id << 16) | model_id;
}

uint32_t mesh_model_get_model_identifier_bluetooth_sig(uint16_t model_id){
    return (BLUETOOTH_COMPANY_ID_BLUETOOTH_SIG_INC << 16) | model_id;
}

uint16_t mesh_model_get_model_id(uint32_t model_identifier){
    return model_identifier & 0xFFFFu;
}

uint16_t mesh_model_get_vendor_id(uint32_t model_identifier){
    return model_identifier >> 16;
}

int mesh_model_is_bluetooth_sig(uint32_t model_identifier){
    return mesh_model_get_vendor_id(model_identifier) == BLUETOOTH_COMPANY_ID_BLUETOOTH_SIG_INC;
}

mesh_model_t * mesh_model_get_configuration_server(void){
    return mesh_model_get_by_identifier(&primary_element, mesh_model_get_model_identifier_bluetooth_sig(MESH_SIG_MODEL_ID_CONFIGURATION_SERVER));
}

void mesh_element_add_model(mesh_element_t * element, mesh_model_t * mesh_model){
    if (mesh_model_is_bluetooth_sig(mesh_model->model_identifier)){
        element->models_count_sig++;
    } else {
        element->models_count_vendor++;
    }
    mesh_model->mid = mid_counter++;
    mesh_model->element = element;
    btstack_linked_list_add_tail(&element->models, (btstack_linked_item_t *) mesh_model);
}

void mesh_model_iterator_init(mesh_model_iterator_t * iterator, mesh_element_t * element){
    btstack_linked_list_iterator_init(&iterator->it, &element->models);
}

int mesh_model_iterator_has_next(mesh_model_iterator_t * iterator){
    return btstack_linked_list_iterator_has_next(&iterator->it);
}

mesh_model_t * mesh_model_iterator_next(mesh_model_iterator_t * iterator){
    return (mesh_model_t *) btstack_linked_list_iterator_next(&iterator->it);
}

void mesh_element_iterator_init(mesh_element_iterator_t * iterator){
    btstack_linked_list_iterator_init(&iterator->it, &mesh_elements);
}

int mesh_element_iterator_has_next(mesh_element_iterator_t * iterator){
    return btstack_linked_list_iterator_has_next(&iterator->it);
}

mesh_element_t * mesh_element_iterator_next(mesh_element_iterator_t * iterator){
    return (mesh_element_t *) btstack_linked_list_iterator_next(&iterator->it);
}

mesh_model_t * mesh_model_get_by_identifier(mesh_element_t * element, uint32_t model_identifier){
    mesh_model_iterator_t it;
    mesh_model_iterator_init(&it, element);
    while (mesh_model_iterator_has_next(&it)){
        mesh_model_t * model = mesh_model_iterator_next(&it);
        if (model->model_identifier != model_identifier) continue;
        return model;
    }
    return NULL;
}

mesh_model_t * mesh_access_model_for_address_and_model_identifier(uint16_t element_address, uint32_t model_identifier, uint8_t * status){
    mesh_element_t * element = mesh_element_for_unicast_address(element_address);
    if (element == NULL){
        *status = MESH_FOUNDATION_STATUS_INVALID_ADDRESS;
        return NULL;
    }
    mesh_model_t * model = mesh_model_get_by_identifier(element, model_identifier);
    if (model == NULL) {
        *status = MESH_FOUNDATION_STATUS_INVALID_MODEL;
    } else {
        *status = MESH_FOUNDATION_STATUS_SUCCESS;
    }
    return model;
}

uint16_t mesh_pdu_src(mesh_pdu_t * pdu){
    switch (pdu->pdu_type){
        case MESH_PDU_TYPE_TRANSPORT:
            return mesh_transport_src((mesh_transport_pdu_t*) pdu);
        case MESH_PDU_TYPE_NETWORK:
            return mesh_network_src((mesh_network_pdu_t *) pdu);
        default:
            return MESH_ADDRESS_UNSASSIGNED;
    }
}

uint16_t mesh_pdu_dst(mesh_pdu_t * pdu){
    switch (pdu->pdu_type){
        case MESH_PDU_TYPE_TRANSPORT:
            return mesh_transport_dst((mesh_transport_pdu_t*) pdu);
        case MESH_PDU_TYPE_NETWORK:
            return mesh_network_dst((mesh_network_pdu_t *) pdu);
        default:
            return MESH_ADDRESS_UNSASSIGNED;
    }
}

uint16_t mesh_pdu_netkey_index(mesh_pdu_t * pdu){
    switch (pdu->pdu_type){
        case MESH_PDU_TYPE_TRANSPORT:
            return ((mesh_transport_pdu_t*) pdu)->netkey_index;
        case MESH_PDU_TYPE_NETWORK:
            return ((mesh_network_pdu_t *) pdu)->netkey_index;
        default:
            return 0;
    }
}

uint16_t mesh_pdu_appkey_index(mesh_pdu_t * pdu){
    switch (pdu->pdu_type){
        case MESH_PDU_TYPE_TRANSPORT:
            return ((mesh_transport_pdu_t*) pdu)->appkey_index;
        case MESH_PDU_TYPE_NETWORK:
            return ((mesh_network_pdu_t *) pdu)->appkey_index;
        default:
            return 0;
    }
}

uint16_t mesh_pdu_len(mesh_pdu_t * pdu){
    switch (pdu->pdu_type){
        case MESH_PDU_TYPE_TRANSPORT:
            return ((mesh_transport_pdu_t*) pdu)->len;
        case MESH_PDU_TYPE_NETWORK:
            return ((mesh_network_pdu_t *) pdu)->len - 10;
        default:
            return 0;
    }
}

uint8_t * mesh_pdu_data(mesh_pdu_t * pdu){
    switch (pdu->pdu_type){
        case MESH_PDU_TYPE_TRANSPORT:
            return ((mesh_transport_pdu_t*) pdu)->data;
        case MESH_PDU_TYPE_NETWORK:
            return &((mesh_network_pdu_t *) pdu)->data[10];
        default:
            return NULL;
    }
}

// message parser

static int mesh_access_get_opcode(uint8_t * buffer, uint16_t buffer_size, uint32_t * opcode, uint16_t * opcode_size){
    switch (buffer[0] >> 6){
        case 0:
        case 1:
            if (buffer[0] == 0x7f) return 0;
            *opcode = buffer[0];
            *opcode_size = 1;
            return 1;
        case 2:
            if (buffer_size < 2) return 0;
            *opcode = big_endian_read_16(buffer, 0);
            *opcode_size = 2;
            return 1;
        case 3:
            if (buffer_size < 3) return 0;
            *opcode = (buffer[0] << 16) | little_endian_read_16(buffer, 1);
            *opcode_size = 3;
            return 1;
        default:
            return 0;
    }
}

static int mesh_access_transport_get_opcode(mesh_transport_pdu_t * transport_pdu, uint32_t * opcode, uint16_t * opcode_size){
    return mesh_access_get_opcode(transport_pdu->data, transport_pdu->len, opcode, opcode_size);
}

static int mesh_access_network_get_opcode(mesh_network_pdu_t * network_pdu, uint32_t * opcode, uint16_t * opcode_size){
    // TransMIC already removed by mesh_upper_transport_validate_unsegmented_message_ccm
    return mesh_access_get_opcode(&network_pdu->data[10], network_pdu->len - 10, opcode, opcode_size);
}

int mesh_access_pdu_get_opcode(mesh_pdu_t * pdu, uint32_t * opcode, uint16_t * opcode_size){
    switch (pdu->pdu_type){
        case MESH_PDU_TYPE_TRANSPORT:
            return mesh_access_transport_get_opcode((mesh_transport_pdu_t*) pdu, opcode, opcode_size);
        case MESH_PDU_TYPE_NETWORK:
            return mesh_access_network_get_opcode((mesh_network_pdu_t *) pdu, opcode, opcode_size);
        default:
            return 0;
    }
}

void mesh_access_parser_skip(mesh_access_parser_state_t * state, uint16_t bytes_to_skip){
    state->data += bytes_to_skip;
    state->len  -= bytes_to_skip;
}

int mesh_access_parser_init(mesh_access_parser_state_t * state, mesh_pdu_t * pdu){
    state->data = mesh_pdu_data(pdu);
    state->len  = mesh_pdu_len(pdu);

    uint16_t opcode_size = 0;
    int ok = mesh_access_get_opcode(state->data, state->len, &state->opcode, &opcode_size);
    if (ok){
        mesh_access_parser_skip(state, opcode_size);
    }
    return ok;
}

uint16_t mesh_access_parser_available(mesh_access_parser_state_t * state){
    return state->len;
}

uint8_t mesh_access_parser_get_u8(mesh_access_parser_state_t * state){
    uint8_t value = *state->data;
    mesh_access_parser_skip(state, 1);
    return value;
}

uint16_t mesh_access_parser_get_u16(mesh_access_parser_state_t * state){
    uint16_t value = little_endian_read_16(state->data, 0);
    mesh_access_parser_skip(state, 2);
    return value;
}

uint32_t mesh_access_parser_get_u24(mesh_access_parser_state_t * state){
    uint32_t value = little_endian_read_24(state->data, 0);
    mesh_access_parser_skip(state, 3);
    return value;
}

uint32_t mesh_access_parser_get_u32(mesh_access_parser_state_t * state){
    uint32_t value = little_endian_read_24(state->data, 0);
    mesh_access_parser_skip(state, 4);
    return value;
}

void mesh_access_parser_get_u128(mesh_access_parser_state_t * state, uint8_t * dest){
    reverse_128( state->data, dest);
    mesh_access_parser_skip(state, 16);
}

void mesh_access_parser_get_label_uuid(mesh_access_parser_state_t * state, uint8_t * dest){
    memcpy( dest, state->data, 16);
    mesh_access_parser_skip(state, 16);
}

void mesh_access_parser_get_key(mesh_access_parser_state_t * state, uint8_t * dest){
    memcpy( dest, state->data, 16);
    mesh_access_parser_skip(state, 16);
}

uint32_t mesh_access_parser_get_model_identifier(mesh_access_parser_state_t * parser){
    if (mesh_access_parser_available(parser) == 4){
        return mesh_access_parser_get_u32(parser);
    } else {
        return (BLUETOOTH_COMPANY_ID_BLUETOOTH_SIG_INC << 16) | mesh_access_parser_get_u16(parser);
    }
}

// Mesh Access Message Builder

// message builder

static int mesh_access_setup_opcode(uint8_t * buffer, uint32_t opcode){
    if (opcode < 0x100){
        buffer[0] = opcode;
        return 1;
    }
    if (opcode < 0x10000){
        big_endian_store_16(buffer, 0, opcode);
        return 2;
    }
    buffer[0] = opcode >> 16;
    little_endian_store_16(buffer, 1, opcode & 0xffff);
    return 3;
}

mesh_transport_pdu_t * mesh_access_transport_init(uint32_t opcode){
    mesh_transport_pdu_t * pdu = mesh_transport_pdu_get();
    if (!pdu) return NULL;

    pdu->len  = mesh_access_setup_opcode(pdu->data, opcode);
    return pdu;
}

void mesh_access_transport_add_uint8(mesh_transport_pdu_t * pdu, uint8_t value){
    pdu->data[pdu->len++] = value;
}

void mesh_access_transport_add_uint16(mesh_transport_pdu_t * pdu, uint16_t value){
    little_endian_store_16(pdu->data, pdu->len, value);
    pdu->len += 2;
}

void mesh_access_transport_add_uint24(mesh_transport_pdu_t * pdu, uint32_t value){
    little_endian_store_24(pdu->data, pdu->len, value);
    pdu->len += 3;
}

void mesh_access_transport_add_uint32(mesh_transport_pdu_t * pdu, uint32_t value){
    little_endian_store_32(pdu->data, pdu->len, value);
    pdu->len += 4;
}
void mesh_access_transport_add_model_identifier(mesh_transport_pdu_t * pdu, uint32_t model_identifier){
    if (mesh_model_is_bluetooth_sig(model_identifier)){
        mesh_access_transport_add_uint16( pdu, mesh_model_get_model_id(model_identifier) );
    } else {
        mesh_access_transport_add_uint32( pdu, model_identifier );
    }
}

mesh_network_pdu_t * mesh_access_network_init(uint32_t opcode){
    mesh_network_pdu_t * pdu = mesh_network_pdu_get();
    if (!pdu) return NULL;

    pdu->len  = mesh_access_setup_opcode(&pdu->data[10], opcode) + 10;
    return pdu;
}

void mesh_access_network_add_uint8(mesh_network_pdu_t * pdu, uint8_t value){
    pdu->data[pdu->len++] = value;
}

void mesh_access_network_add_uint16(mesh_network_pdu_t * pdu, uint16_t value){
    little_endian_store_16(pdu->data, pdu->len, value);
    pdu->len += 2;
}

void mesh_access_network_add_uint24(mesh_network_pdu_t * pdu, uint16_t value){
    little_endian_store_24(pdu->data, pdu->len, value);
    pdu->len += 3;
}

void mesh_access_network_add_uint32(mesh_network_pdu_t * pdu, uint16_t value){
    little_endian_store_32(pdu->data, pdu->len, value);
    pdu->len += 4;
}

void mesh_access_network_add_model_identifier(mesh_network_pdu_t * pdu, uint32_t model_identifier){
    if (mesh_model_is_bluetooth_sig(model_identifier)){
        mesh_access_network_add_uint16( pdu, mesh_model_get_model_id(model_identifier) );
    } else {
        mesh_access_network_add_uint32( pdu, model_identifier );
    }
}

// access message template

mesh_network_pdu_t * mesh_access_setup_unsegmented_message(const mesh_access_message_t *template, ...){
    mesh_network_pdu_t * network_pdu = mesh_access_network_init(template->opcode);
    if (!network_pdu) return NULL;

    va_list argptr;
    va_start(argptr, template);

    // add params
    const char * format = template->format;
    uint16_t word;
    uint32_t longword;
    while (*format){
        switch (*format){
            case '1':
                word = va_arg(argptr, int);  // minimal va_arg is int: 2 bytes on 8+16 bit CPUs
                mesh_access_network_add_uint8( network_pdu, word);
                break;
            case '2':
                word = va_arg(argptr, int);  // minimal va_arg is int: 2 bytes on 8+16 bit CPUs
                mesh_access_network_add_uint16( network_pdu, word);
                break;
            case '3':
                longword = va_arg(argptr, uint32_t);
                mesh_access_network_add_uint24( network_pdu, longword);
                break;
            case '4':
                longword = va_arg(argptr, uint32_t);
                mesh_access_network_add_uint32( network_pdu, longword);
                break;
            case 'm':
                longword = va_arg(argptr, uint32_t);
                mesh_access_network_add_model_identifier( network_pdu, longword);
                break;
            default:
                log_error("Unsupported mesh message format specifier '%c", *format);
                break;
        }
        format++;
    }

    va_end(argptr);

    return network_pdu;
}

mesh_transport_pdu_t * mesh_access_setup_segmented_message(const mesh_access_message_t *template, ...){
    mesh_transport_pdu_t * transport_pdu = mesh_access_transport_init(template->opcode);
    if (!transport_pdu) return NULL;

    va_list argptr;
    va_start(argptr, template);

    // add params
    const char * format = template->format;
    uint16_t word;
    uint32_t longword;
    while (*format){
        switch (*format++){
            case '1':
                word = va_arg(argptr, int);  // minimal va_arg is int: 2 bytes on 8+16 bit CPUs
                mesh_access_transport_add_uint8( transport_pdu, word);
                break;
            case '2':
                word = va_arg(argptr, int);  // minimal va_arg is int: 2 bytes on 8+16 bit CPUs
                mesh_access_transport_add_uint16( transport_pdu, word);
                break;
            case '3':
                longword = va_arg(argptr, uint32_t);
                mesh_access_transport_add_uint24( transport_pdu, longword);
                break;
            case '4':
                longword = va_arg(argptr, uint32_t);
                mesh_access_transport_add_uint32( transport_pdu, longword);
                break;
            case 'm':
                longword = va_arg(argptr, uint32_t);
                mesh_access_transport_add_model_identifier( transport_pdu, longword);
                break;
            default:
                break;
        }
    }

    va_end(argptr);

    return transport_pdu;
}


static const mesh_operation_t * mesh_model_lookup_operation(mesh_model_t * model, mesh_pdu_t * pdu){

    uint32_t opcode = 0;
    uint16_t opcode_size = 0;
    int ok = mesh_access_pdu_get_opcode( pdu, &opcode, &opcode_size);
    if (!ok) return NULL;

    uint16_t len = mesh_pdu_len(pdu);

    // find opcode in table
    const mesh_operation_t * operation = model->operations;
    if (operation == NULL) return NULL;
    for ( ; operation->handler != NULL ; operation++){
        if (operation->opcode != opcode) continue;
        if ((opcode_size + operation->minimum_length) > len) continue;
        return operation;
    }
    return NULL;
}

static int mesh_access_validate_appkey_index(mesh_model_t * model, uint16_t appkey_index){
    // DeviceKey is valid for all models
    if (appkey_index == MESH_DEVICE_KEY_INDEX) return 1;
    // check if AppKey that is bound to this particular model
    return mesh_model_contains_appkey(model, appkey_index);
}

static void mesh_access_message_process_handler(mesh_pdu_t * pdu){
    // get opcode and size
    uint32_t opcode = 0;
    uint16_t opcode_size = 0;

    int ok = mesh_access_pdu_get_opcode( pdu, &opcode, &opcode_size);
    if (!ok) {
        mesh_access_message_processed(pdu);
        return;
    }

    uint16_t len = mesh_pdu_len(pdu);
    printf("MESH Access Message, Opcode = %x: ", opcode);
    switch (pdu->pdu_type){
        case MESH_PDU_TYPE_NETWORK:
            printf_hexdump(&((mesh_network_pdu_t *) pdu)->data[10], len);
            break;
        case MESH_PDU_TYPE_TRANSPORT:
            printf_hexdump(((mesh_transport_pdu_t *) pdu)->data, len);
            break;
        default:
            break;
    }

    uint16_t dst = mesh_pdu_dst(pdu);
    uint16_t appkey_index = mesh_pdu_appkey_index(pdu);
    if (mesh_network_address_unicast(dst)){
        // loookup element by unicast address
        mesh_element_t * element = mesh_element_for_unicast_address(dst);
        if (element != NULL){
            // iterate over models, look for operation
            mesh_model_iterator_t model_it;
            mesh_model_iterator_init(&model_it, element);
            while (mesh_model_iterator_has_next(&model_it)){
                mesh_model_t * model = mesh_model_iterator_next(&model_it);
                // find opcode in table
                const mesh_operation_t * operation = mesh_model_lookup_operation(model, pdu);
                if (operation == NULL) break;
                if (mesh_access_validate_appkey_index(model, appkey_index) == 0) break;
                operation->handler(model, pdu);
                return;
            }
        }
    } else {
        // iterate over all elements / models, check subscription list
        mesh_element_iterator_t it;
        mesh_element_iterator_init(&it);
        while (mesh_element_iterator_has_next(&it)){
            mesh_element_t * element = (mesh_element_t *) mesh_element_iterator_next(&it);
            mesh_model_iterator_t model_it;
            mesh_model_iterator_init(&model_it, element);
            while (mesh_model_iterator_has_next(&model_it)){
                mesh_model_t * model = mesh_model_iterator_next(&model_it);
                if (mesh_model_contains_subscription(model, dst)){
                    // find opcode in table
                    const mesh_operation_t * operation = mesh_model_lookup_operation(model, pdu);
                    if (operation == NULL) break;
                    if (mesh_access_validate_appkey_index(model, appkey_index) == 0) break;
                    operation->handler(model, pdu);
                    return;
                }
            }
        }
    }

    // operation not found -> done
    printf("Message not handled\n");
    mesh_access_message_processed(pdu);
}

void mesh_access_message_processed(mesh_pdu_t * pdu){
    mesh_upper_transport_message_processed_by_higher_layer(pdu);
}

int mesh_model_contains_subscription(mesh_model_t * mesh_model, uint16_t address){
    int i;
    for (i=0;i<MAX_NR_MESH_SUBSCRIPTION_PER_MODEL;i++){
        if (mesh_model->subscriptions[i] == address) return 1;
    }
    return 0;
}

static uint32_t mesh_network_key_tag_for_internal_index(uint16_t internal_index){
    return ((uint32_t) 'M' << 24) | ((uint32_t) 'N' << 16) | ((uint32_t) internal_index);
}

// Foundation state

static const uint32_t mesh_foundation_state_tag = ((uint32_t) 'M' << 24) | ((uint32_t) 'F' << 16)  | ((uint32_t) 'N' << 8) | ((uint32_t) 'D' << 8);

typedef struct {
    uint8_t gatt_proxy;
    uint8_t beacon;
    uint8_t default_ttl;
    uint8_t network_transmit;
    uint8_t relay;
    uint8_t relay_retransmit;
    uint8_t friend;
} mesh_persistent_foundation_t;

void mesh_foundation_state_load(void){
    mesh_access_setup_tlv();
    mesh_persistent_foundation_t data;

    int app_key_len = btstack_tlv_singleton_impl->get_tag(btstack_tlv_singleton_context, mesh_foundation_state_tag, (uint8_t *) &data, sizeof(data));
    if (app_key_len == 0) return;

    mesh_foundation_gatt_proxy_set(data.gatt_proxy);
    mesh_foundation_beacon_set(data.gatt_proxy);
    mesh_foundation_default_ttl_set(data.default_ttl);
    mesh_foundation_friend_set(data.friend);
    mesh_foundation_network_transmit_set(data.network_transmit);
    mesh_foundation_relay_set(data.relay);
    mesh_foundation_relay_retransmit_set(data.relay_retransmit);
}

void mesh_foundation_state_store(void){
    mesh_access_setup_tlv();
    mesh_persistent_foundation_t data;
    data.gatt_proxy       = mesh_foundation_gatt_proxy_get();
    data.gatt_proxy       = mesh_foundation_beacon_get();
    data.default_ttl      = mesh_foundation_default_ttl_get();
    data.friend           = mesh_foundation_friend_get();
    data.network_transmit = mesh_foundation_network_transmit_get();
    data.relay            = mesh_foundation_relay_get();
    data.relay_retransmit = mesh_foundation_relay_retransmit_get();
    btstack_tlv_singleton_impl->store_tag(btstack_tlv_singleton_context, mesh_foundation_state_tag, (uint8_t *) &data, sizeof(data));
}

// Mesh Network Keys
typedef struct {
    uint16_t netkey_index;

    uint8_t  version;

    // net_key from provisioner or Config Model Client
    uint8_t net_key[16];

    // derived data

    // k1
    uint8_t identity_key[16];
    uint8_t beacon_key[16];

    // k3
    uint8_t network_id[8];

    // k2
    uint8_t nid;
    uint8_t encryption_key[16];
    uint8_t privacy_key[16];
} mesh_persistent_net_key_t;

void mesh_store_network_key(mesh_network_key_t * network_key){
    mesh_access_setup_tlv();
    mesh_persistent_net_key_t data;
    printf("Store NetKey: internal index 0x%x, NetKey Index 0x%06x, NID %02x: ", network_key->internal_index, network_key->netkey_index, network_key->nid);
    printf_hexdump(network_key->net_key, 16);
    uint32_t tag = mesh_network_key_tag_for_internal_index(network_key->internal_index);
    data.netkey_index = network_key->netkey_index;
    memcpy(data.net_key, network_key->net_key, 16);
    memcpy(data.identity_key, network_key->identity_key, 16);
    memcpy(data.beacon_key, network_key->beacon_key, 16);
    memcpy(data.network_id, network_key->network_id, 8);
    data.nid = network_key->nid;
    data.version = network_key->version;
    memcpy(data.encryption_key, network_key->encryption_key, 16);
    memcpy(data.privacy_key, network_key->privacy_key, 16);
    btstack_tlv_singleton_impl->store_tag(btstack_tlv_singleton_context, tag, (uint8_t *) &data, sizeof(mesh_persistent_net_key_t));
}

void mesh_delete_network_key(uint16_t internal_index){
    mesh_access_setup_tlv();
    uint32_t tag = mesh_network_key_tag_for_internal_index(internal_index);
    btstack_tlv_singleton_impl->delete_tag(btstack_tlv_singleton_context, tag);
}


void mesh_load_network_keys(void){
    mesh_access_setup_tlv();
    printf("Load Network Keys\n");
    uint16_t internal_index;
    for (internal_index = 0; internal_index < MAX_NR_MESH_NETWORK_KEYS; internal_index++){
        mesh_persistent_net_key_t data;
        uint32_t tag = mesh_network_key_tag_for_internal_index(internal_index);
        int netkey_len = btstack_tlv_singleton_impl->get_tag(btstack_tlv_singleton_context, tag, (uint8_t *) &data, sizeof(data));
        if (netkey_len != sizeof(mesh_persistent_net_key_t)) continue;
        
        mesh_network_key_t * network_key = btstack_memory_mesh_network_key_get();
        if (network_key == NULL) return;

        network_key->netkey_index = data.netkey_index;
        memcpy(network_key->net_key, data.net_key, 16);
        memcpy(network_key->identity_key, data.identity_key, 16);
        memcpy(network_key->beacon_key, data.beacon_key, 16);
        memcpy(network_key->network_id, data.network_id, 8);
        network_key->nid = data.nid;
        network_key->version = data.version;
        memcpy(network_key->encryption_key, data.encryption_key, 16);
        memcpy(network_key->privacy_key, data.privacy_key, 16);

#ifdef ENABLE_GATT_BEARER
        // setup advertisement with network id
        network_key->advertisement_with_network_id.adv_length = mesh_proxy_setup_advertising_with_network_id(network_key->advertisement_with_network_id.adv_data, network_key->network_id);
#endif

        mesh_network_key_add(network_key);

        mesh_subnet_setup_for_netkey_index(network_key->netkey_index);

        printf("- internal index 0x%x, NetKey Index 0x%06x, NID %02x: ", network_key->internal_index, network_key->netkey_index, network_key->nid);
        printf_hexdump(network_key->net_key, 16);
    }
}

void mesh_delete_network_keys(void){
    printf("Delete Network Keys\n");
    
    uint16_t internal_index;
    for (internal_index = 0; internal_index < MAX_NR_MESH_NETWORK_KEYS; internal_index++){
        mesh_delete_network_key(internal_index);
    }
}


// Mesh App Keys

typedef struct {
    uint16_t netkey_index;
    uint16_t appkey_index;
    uint8_t  aid;
    uint8_t  version;
    uint8_t  key[16];
} mesh_persistent_app_key_t;

static uint32_t mesh_transport_key_tag_for_internal_index(uint16_t internal_index){
    return ((uint32_t) 'M' << 24) | ((uint32_t) 'A' << 16) | ((uint32_t) internal_index);
}

void mesh_store_app_key(mesh_transport_key_t * app_key){
    mesh_access_setup_tlv();

    mesh_persistent_app_key_t data;
    printf("Store AppKey: internal index 0x%x, AppKey Index 0x%06x, AID %02x: ", app_key->internal_index, app_key->appkey_index, app_key->aid);
    printf_hexdump(app_key->key, 16);
    uint32_t tag = mesh_transport_key_tag_for_internal_index(app_key->internal_index);
    data.netkey_index = app_key->netkey_index;
    data.appkey_index = app_key->appkey_index;
    data.aid = app_key->aid;
    data.version = app_key->version;
    memcpy(data.key, app_key->key, 16);
    btstack_tlv_singleton_impl->store_tag(btstack_tlv_singleton_context, tag, (uint8_t *) &data, sizeof(data));
}

void mesh_delete_app_key(uint16_t internal_index){
    mesh_access_setup_tlv();

    uint32_t tag = mesh_transport_key_tag_for_internal_index(internal_index);
    btstack_tlv_singleton_impl->delete_tag(btstack_tlv_singleton_context, tag);
}

void mesh_load_app_keys(void){
    mesh_access_setup_tlv();
    printf("Load App Keys\n");
    uint16_t internal_index;
    for (internal_index = 0; internal_index < MAX_NR_MESH_TRANSPORT_KEYS; internal_index++){
        mesh_persistent_app_key_t data;
        uint32_t tag = mesh_transport_key_tag_for_internal_index(internal_index);
        int app_key_len = btstack_tlv_singleton_impl->get_tag(btstack_tlv_singleton_context, tag, (uint8_t *) &data, sizeof(data));
        if (app_key_len == 0) continue;
        
        mesh_transport_key_t * key = btstack_memory_mesh_transport_key_get();
        if (key == NULL) return;

        key->internal_index = internal_index;
        key->appkey_index = data.appkey_index;
        key->netkey_index = data.netkey_index;
        key->aid          = data.aid;
        key->akf          = 1;
        key->version      = data.version;
        memcpy(key->key, data.key, 16);
        mesh_transport_key_add(key);
        printf("- internal index 0x%x, AppKey Index 0x%06x, AID %02x: ", key->internal_index, key->appkey_index, key->aid);
        printf_hexdump(key->key, 16);
    }
}

void mesh_delete_app_keys(void){
    printf("Delete App Keys\n");
    
    uint16_t internal_index;
    for (internal_index = 0; internal_index < MAX_NR_MESH_TRANSPORT_KEYS; internal_index++){
        mesh_delete_app_key(internal_index);
    }
}


// Model to Appkey List

static uint32_t mesh_model_tag_for_index(uint16_t internal_model_id){
    return ((uint32_t) 'M' << 24) | ((uint32_t) 'B' << 16) | ((uint32_t) internal_model_id);
}

static void mesh_load_appkey_list(mesh_model_t * model){
    mesh_access_setup_tlv();
    uint32_t tag = mesh_model_tag_for_index(model->mid);
    btstack_tlv_singleton_impl->get_tag(btstack_tlv_singleton_context, tag, (uint8_t *) &model->appkey_indices, sizeof(model->appkey_indices));
}

static void mesh_store_appkey_list(mesh_model_t * model){
    mesh_access_setup_tlv();
    uint32_t tag = mesh_model_tag_for_index(model->mid);
    btstack_tlv_singleton_impl->store_tag(btstack_tlv_singleton_context, tag, (uint8_t *) &model->appkey_indices, sizeof(model->appkey_indices));
}

static void mesh_delete_appkey_list(mesh_model_t * model){
    mesh_access_setup_tlv();
    uint32_t tag = mesh_model_tag_for_index(model->mid);
    btstack_tlv_singleton_impl->delete_tag(btstack_tlv_singleton_context, tag);
}

void mesh_load_appkey_lists(void){
    printf("Load Appkey Lists\n");
    // iterate over elements and models
    mesh_element_iterator_t element_it;
    mesh_element_iterator_init(&element_it);
    while (mesh_element_iterator_has_next(&element_it)){
        mesh_element_t * element = mesh_element_iterator_next(&element_it);
        mesh_model_iterator_t model_it;
        mesh_model_iterator_init(&model_it, element);
        while (mesh_model_iterator_has_next(&model_it)){
            mesh_model_t * model = mesh_model_iterator_next(&model_it);
            mesh_load_appkey_list(model);
        }
    }
}

void mesh_delete_appkey_lists(void){
    printf("Delete Appkey Lists\n");
    mesh_access_setup_tlv();
    // iterate over elements and models
    mesh_element_iterator_t element_it;
    mesh_element_iterator_init(&element_it);
    while (mesh_element_iterator_has_next(&element_it)){
        mesh_element_t * element = mesh_element_iterator_next(&element_it);
        mesh_model_iterator_t model_it;
        mesh_model_iterator_init(&model_it, element);
        while (mesh_model_iterator_has_next(&model_it)){
            mesh_model_t * model = mesh_model_iterator_next(&model_it);
            mesh_delete_appkey_list(model);
        }
    }
}

void mesh_model_reset_appkeys(mesh_model_t * mesh_model){
    int i;
    for (i=0;i<MAX_NR_MESH_APPKEYS_PER_MODEL;i++){
        mesh_model->appkey_indices[i] = MESH_APPKEY_INVALID;
    }
}

uint8_t mesh_model_bind_appkey(mesh_model_t * mesh_model, uint16_t appkey_index){
    int i;
    for (i=0;i<MAX_NR_MESH_APPKEYS_PER_MODEL;i++){
        if (mesh_model->appkey_indices[i] == appkey_index) return MESH_FOUNDATION_STATUS_SUCCESS;
    }
    for (i=0;i<MAX_NR_MESH_APPKEYS_PER_MODEL;i++){
        if (mesh_model->appkey_indices[i] == MESH_APPKEY_INVALID) {
            mesh_model->appkey_indices[i] = appkey_index;
            mesh_store_appkey_list(mesh_model);
            return MESH_FOUNDATION_STATUS_SUCCESS;
        }
    }
    return MESH_FOUNDATION_STATUS_INSUFFICIENT_RESOURCES;
}

void mesh_model_unbind_appkey(mesh_model_t * mesh_model, uint16_t appkey_index){
    int i;
    for (i=0;i<MAX_NR_MESH_APPKEYS_PER_MODEL;i++){
        if (mesh_model->appkey_indices[i] == appkey_index) {
            mesh_model->appkey_indices[i] = MESH_APPKEY_INVALID;
            mesh_store_appkey_list(mesh_model);
        }
    }
}

int mesh_model_contains_appkey(mesh_model_t * mesh_model, uint16_t appkey_index){
    uint16_t i;
    for (i=0;i<MAX_NR_MESH_APPKEYS_PER_MODEL;i++){
        if (mesh_model->appkey_indices[i] == appkey_index) return 1;
    }
    return 0;

}

// Mesh Model Publication
static btstack_timer_source_t mesh_access_publication_timer;

static uint32_t mesh_model_publication_retransmit_count(uint8_t retransmit){
    return retransmit & 0x07u;
}

static uint32_t mesh_model_publication_retransmission_period_ms(uint8_t retransmit){
    return ((uint32_t)((retransmit >> 3) + 1)) * 50;
}

static void mesh_model_publication_setup_publication(mesh_publication_model_t * publication_model, uint32_t now){

    // set retransmit counter
    publication_model->retransmit_count = mesh_model_publication_retransmit_count(publication_model->retransmit);

    // schedule next publication or retransmission
    uint32_t publication_period_ms = mesh_access_transitions_step_ms_from_gdtt(publication_model->period);

    // set next publication
    if (publication_period_ms != 0){
        publication_model->next_publication_ms = now + publication_period_ms;
        publication_model->state = MESH_MODEL_PUBLICATION_STATE_W4_PUBLICATION_MS;
    }
}

static void mesh_model_publication_setup_retransmission(mesh_publication_model_t * publication_model, uint32_t now){
    uint8_t num_retransmits = mesh_model_publication_retransmit_count(publication_model->retransmit);
    if (num_retransmits == 0) return;

    // calc next retransmit time
    uint32_t retransmission_ms = now + mesh_model_publication_retransmission_period_ms(publication_model->retransmit);

    // ignore if retransmission would be after next publication timeout
    if (publication_model->state == MESH_MODEL_PUBLICATION_STATE_W4_PUBLICATION_MS){
        if (btstack_time_delta(retransmission_ms, publication_model->next_publication_ms) > 0) return;   
    }

    // schedule next retransmission
    publication_model->next_retransmit_ms = retransmission_ms;
    publication_model->state = MESH_MODEL_PUBLICATION_STATE_W4_RETRANSMIT_MS;
}

static void mesh_model_publication_publish_now_model(mesh_model_t * mesh_model){
    mesh_publication_model_t * publication_model = mesh_model->publication_model;
    if (publication_model == NULL) return;
    if (publication_model->publish_state_fn == NULL) return;
    uint16_t dest = publication_model->address;
    if (dest == MESH_ADDRESS_UNSASSIGNED) return;
    uint16_t appkey_index = publication_model->appkey_index;
    mesh_transport_key_t * app_key = mesh_transport_key_get(appkey_index);
    if (app_key == NULL) return;

    // compose message
    mesh_pdu_t * pdu = (*publication_model->publish_state_fn)(mesh_model);
    if (pdu == NULL) return;

    mesh_upper_transport_setup_access_pdu_header(pdu, app_key->netkey_index, appkey_index, publication_model->ttl, mesh_access_get_element_address(mesh_model), dest, 0);
    mesh_upper_transport_send_access_pdu(pdu);
}

static void mesh_model_publication_run(btstack_timer_source_t * ts){
    UNUSED(ts);

    uint32_t now = btstack_run_loop_get_time_ms();

    // iterate over elements and models and handle time-based transitions
    mesh_element_iterator_t element_it;
    mesh_element_iterator_init(&element_it);
    while (mesh_element_iterator_has_next(&element_it)){
        mesh_element_t * element = mesh_element_iterator_next(&element_it);
        mesh_model_iterator_t model_it;
        mesh_model_iterator_init(&model_it, element);
        while (mesh_model_iterator_has_next(&model_it)){
            mesh_model_t * mesh_model = mesh_model_iterator_next(&model_it);
            mesh_publication_model_t * publication_model = mesh_model->publication_model;
            if (publication_model == NULL) continue;

            // schedule next
            switch (publication_model->state){
                case MESH_MODEL_PUBLICATION_STATE_W4_PUBLICATION_MS:
                    if (btstack_time_delta(publication_model->next_publication_ms, now) > 0) break;
                    // timeout
                    publication_model->publish_now = 1;
                    // schedule next publication and retransmission
                    mesh_model_publication_setup_publication(publication_model, now);
                    mesh_model_publication_setup_retransmission(publication_model, now);
                    break;
                case MESH_MODEL_PUBLICATION_STATE_W4_RETRANSMIT_MS:
                    if (btstack_time_delta(publication_model->next_retransmit_ms, now) > 0) break;
                    // timeout
                    publication_model->publish_now = 1;
                    publication_model->retransmit_count--;
                    // schedule next retransmission
                    mesh_model_publication_setup_retransmission(publication_model, now);
                    break;
                default:
                    break;
            }

            if (publication_model->publish_now == 0) continue;

            publication_model->publish_now = 0;
            mesh_model_publication_publish_now_model(mesh_model);
        }
    }

    int32_t next_timeout_ms = 0;
    mesh_element_iterator_init(&element_it);
    while (mesh_element_iterator_has_next(&element_it)){
        mesh_element_t * element = mesh_element_iterator_next(&element_it);
        mesh_model_iterator_t model_it;
        mesh_model_iterator_init(&model_it, element);
        while (mesh_model_iterator_has_next(&model_it)){
            mesh_model_t * mesh_model = mesh_model_iterator_next(&model_it);
            mesh_publication_model_t * publication_model = mesh_model->publication_model;
            if (publication_model == NULL) continue;

            // schedule next
            int32_t timeout_delta_ms;
            switch (publication_model->state){
                case MESH_MODEL_PUBLICATION_STATE_W4_PUBLICATION_MS:
                    timeout_delta_ms = btstack_time_delta(publication_model->next_publication_ms, now);
                    if (next_timeout_ms == 0 || timeout_delta_ms < next_timeout_ms){
                        next_timeout_ms = timeout_delta_ms;
                    }
                    break;
                case MESH_MODEL_PUBLICATION_STATE_W4_RETRANSMIT_MS:
                    timeout_delta_ms = btstack_time_delta(publication_model->next_retransmit_ms, now);
                    if (next_timeout_ms == 0 || timeout_delta_ms < next_timeout_ms){
                        next_timeout_ms = timeout_delta_ms;
                    }
                    break;
                default:
                    break;
            }
        }
    }

    // set timer
    if (next_timeout_ms == 0) return;

    btstack_run_loop_set_timer(&mesh_access_publication_timer, next_timeout_ms);
    btstack_run_loop_set_timer_handler(&mesh_access_publication_timer, mesh_model_publication_run);
    btstack_run_loop_add_timer(&mesh_access_publication_timer);
}

void mesh_model_publication_start(mesh_model_t * mesh_model){
    mesh_publication_model_t * publication_model = mesh_model->publication_model;
    if (publication_model == NULL) return;

    // reset state
    publication_model->state = MESH_MODEL_PUBLICATION_STATE_IDLE;

    // publish right away
    publication_model->publish_now = 1;

    // setup next publication and retransmission
    uint32_t now = btstack_run_loop_get_time_ms();
    mesh_model_publication_setup_publication(publication_model, now);
    mesh_model_publication_setup_retransmission(publication_model, now);

    mesh_model_publication_run(NULL);
}

void mesh_model_publication_stop(mesh_model_t * mesh_model){
    mesh_publication_model_t * publication_model = mesh_model->publication_model;
    if (publication_model == NULL) return;

    // reset state
    publication_model->state = MESH_MODEL_PUBLICATION_STATE_IDLE;
}

void mesh_access_state_changed(mesh_model_t * mesh_model){
    mesh_publication_model_t * publication_model = mesh_model->publication_model;
    if (publication_model == NULL) return;
    publication_model->publish_now = 1;
    mesh_model_publication_run(NULL);
}

void mesh_access_netkey_finalize(mesh_network_key_t * network_key){
    mesh_network_key_remove(network_key);
    mesh_delete_network_key(network_key->internal_index);
    btstack_memory_mesh_network_key_free(network_key);
}

void mesh_access_appkey_finalize(mesh_transport_key_t * transport_key){
    mesh_transport_key_remove(transport_key);
    mesh_delete_app_key(transport_key->appkey_index);
    btstack_memory_mesh_transport_key_free(transport_key);
}

void mesh_access_key_refresh_revoke_keys(mesh_subnet_t * subnet){
    // delete old netkey index
    mesh_access_netkey_finalize(subnet->old_key);
    subnet->old_key = subnet->new_key;
    subnet->new_key = NULL;

    // delete old appkeys, if any
    mesh_transport_key_iterator_t it;
    mesh_transport_key_iterator_init(&it, subnet->netkey_index);
    while (mesh_transport_key_iterator_has_more(&it)){
        mesh_transport_key_t * transport_key = mesh_transport_key_iterator_get_next(&it);
        if (transport_key->old_key == 0) continue;
        mesh_access_appkey_finalize(transport_key);
    }
}

static void mesh_access_secure_network_beacon_handler(uint8_t packet_type, uint16_t channel, uint8_t * packet, uint16_t size){
    UNUSED(channel);
    if (packet_type != MESH_BEACON_PACKET) return;

    // lookup subnet and netkey by network id
    uint8_t * beacon_network_id = &packet[2];
    mesh_subnet_iterator_t it;
    mesh_subnet_iterator_init(&it);
    mesh_subnet_t * subnet = NULL;
    mesh_network_key_t * network_key = NULL;
    uint8_t new_key = 0;
    while (mesh_subnet_iterator_has_more(&it)){
        mesh_subnet_t * item = mesh_subnet_iterator_get_next(&it);
        if (memcmp(item->old_key->network_id, beacon_network_id, 8) == 0 ) {
            subnet = item;
            network_key = item->old_key;
        }
        if (item->new_key != NULL && memcmp(item->new_key->network_id, beacon_network_id, 8) == 0 ) {
            subnet = item;
            network_key = item->new_key;
            new_key = 1;
        }
        break;
    }
    if (subnet == NULL) return;

    uint8_t flags = packet[1];

    // Key refresh via secure network beacons that are authenticated with new netkey
    if (new_key){
        // either first or second phase (in phase 0, new key is not set)
        int key_refresh_flag = flags & 1;
        if (key_refresh_flag){
            //  transition to phase 3 from either phase 1 or 2
            switch (subnet->key_refresh){
                case MESH_KEY_REFRESH_FIRST_PHASE:
                case MESH_KEY_REFRESH_SECOND_PHASE:
                    mesh_access_key_refresh_revoke_keys(subnet);
                    subnet->key_refresh = MESH_KEY_REFRESH_NOT_ACTIVE;
                    break;
                default:
                    break;
            }
        } else {
            //  transition to phase 2 from either phase 1
            switch (subnet->key_refresh){
                case MESH_KEY_REFRESH_FIRST_PHASE:
                    // -- update state
                    subnet->key_refresh = MESH_KEY_REFRESH_SECOND_PHASE;
                    break;
                default:
                    break;
            }
        }
    }

    // IV Update

    int     beacon_iv_update_active = flags & 2;
    int     local_iv_update_active = mesh_iv_update_active();
    uint32_t beacon_iv_index = big_endian_read_32(packet, 10);
    uint32_t local_iv_index = mesh_get_iv_index();

    int32_t iv_index_delta = (int32_t)(beacon_iv_index - local_iv_update_active);

    // "If a node in Normal Operation receives a Secure Network beacon with an IV index greater than the last known IV Index + 1..."
    if (local_iv_update_active == 0 && iv_index_delta > 1){
        // "... it may initiate an IV Index Recovery procedure, see Section 3.10.6."
        // TODO: initiate IV Index Recovery procedure
        return;
    }

    // "If a node in Normal Operation receives a Secure Network beacon with an IV index equal to the last known IV index+1 and
    //  the IV Update Flag set to 0, the node may update its IV without going to the IV Update in Progress state, or it may initiate
    //  an IV Index Recovery procedure (Section 3.10.6), or it may ignore the Secure Network beacon. The node makes the choice depending
    //  on the time since last IV update and the likelihood that the node has missed the Secure Network beacons with the IV update Flag set to 1.""
    if (local_iv_update_active == 0 && beacon_iv_update_active == 0 && iv_index_delta == 1){
        // instant iv update
        mesh_set_iv_index( beacon_iv_index );
        return;
    }

    // "If a node in Normal Operation receives a Secure Network beacon with an IV index less than the last known IV Index or greater than
    //  the last known IV Index + 42, the Secure Network beacon shall be ignored."
    if (iv_index_delta < 0 || iv_index_delta > 42){
        return;
    }

    // "If this node is a member of a primary subnet and receives a Secure Network beacon on a secondary subnet with an IV Index greater than
    //  the last known IV Index of the primary subnet, the Secure Network beacon shall be ignored."
    int member_of_primary_subnet = mesh_subnet_get_by_netkey_index(0) != NULL;
    int beacon_on_secondary_subnet = subnet->netkey_index != 0;
    if (member_of_primary_subnet && beacon_on_secondary_subnet && iv_index_delta > 0){
        return;
    }

    // TODO: validate IV index w.r.t. local index

    if (mesh_iv_update_active()){
        if (beacon_iv_update_active){
            mesh_trigger_iv_update();
        }
    } else {
        if (beacon_iv_update_active == 0){
            // " At the point of transition, the node shall reset the sequence number to 0x000000."
            mesh_lower_transport_set_seq(0);
            mesh_iv_update_completed();
        }
    }
}
