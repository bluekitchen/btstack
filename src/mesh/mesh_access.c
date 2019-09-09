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

#include "mesh/mesh_access.h"

#include "btstack_debug.h"
#include "btstack_memory.h"
#include "btstack_tlv.h"

#include "mesh/beacon.h"
#include "mesh/mesh_foundation.h"
#include "mesh/mesh_iv_index_seq_number.h"
#include "mesh/mesh_node.h"
#include "mesh/mesh_proxy.h"
#include "mesh/mesh_upper_transport.h"
#include "mesh/mesh.h"

#define MEST_TRANSACTION_TIMEOUT_MS  6000

static void mesh_access_message_process_handler(mesh_pdu_t * pdu);
static void mesh_access_upper_transport_handler(mesh_transport_callback_type_t callback_type, mesh_transport_status_t status, mesh_pdu_t * pdu);
static const mesh_operation_t * mesh_model_lookup_operation_by_opcode(mesh_model_t * model, uint32_t opcode);

// acknowledged messages
static btstack_linked_list_t  mesh_access_acknowledged_messages;
static btstack_timer_source_t mesh_access_acknowledged_timer;

// Transitions
static btstack_linked_list_t  transitions;
static btstack_timer_source_t transitions_timer;
static uint32_t transition_step_min_ms;
static uint8_t mesh_transaction_id_counter = 0;

void mesh_access_init(void){
    // register with upper transport
    mesh_upper_transport_register_access_message_handler(&mesh_access_message_process_handler);
    mesh_upper_transport_set_higher_layer_handler(&mesh_access_upper_transport_handler);
}

void mesh_access_emit_state_update_bool(btstack_packet_handler_t event_handler, uint8_t element_index, uint32_t model_identifier,
    model_state_id_t state_identifier, model_state_update_reason_t reason, uint8_t value){
    if (event_handler == NULL) return;
    uint8_t event[14] = {HCI_EVENT_MESH_META, 12, MESH_SUBEVENT_STATE_UPDATE_BOOL};
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

void mesh_access_emit_state_update_int16(btstack_packet_handler_t event_handler, uint8_t element_index, uint32_t model_identifier,
    model_state_id_t state_identifier, model_state_update_reason_t reason, int16_t value){
    if (event_handler == NULL) return;
    uint8_t event[15] = {HCI_EVENT_MESH_META, 13, MESH_SUBEVENT_STATE_UPDATE_BOOL};
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

uint8_t mesh_access_acknowledged_message_retransmissions(void){
    return 3;
}

uint32_t mesh_access_acknowledged_message_timeout_ms(void){
    return 30000;
}

#define MESH_ACCESS_OPCODE_INVALID 0xFFFFFFFFu

void mesh_access_send_unacknowledged_pdu(mesh_pdu_t * pdu){
    pdu->ack_opcode = MESH_ACCESS_OPCODE_INVALID;;
    mesh_upper_transport_send_access_pdu(pdu);
}

void mesh_access_send_acknowledged_pdu(mesh_pdu_t * pdu, uint8_t retransmissions, uint32_t ack_opcode){
    pdu->retransmit_count = retransmissions;
    pdu->ack_opcode = ack_opcode;

    mesh_upper_transport_send_access_pdu(pdu);
}

#define MESH_SUBEVENT_MESSAGE_NOT_ACKNOWLEDGED                                        0x30

static void mesh_access_acknowledged_run(btstack_timer_source_t * ts){
    UNUSED(ts);

    uint32_t now = btstack_run_loop_get_time_ms();

    // handle timeouts
    btstack_linked_list_iterator_t ack_it;
    btstack_linked_list_iterator_init(&ack_it, &mesh_access_acknowledged_messages);
    while (btstack_linked_list_iterator_has_next(&ack_it)){
        mesh_pdu_t * pdu = (mesh_pdu_t *) btstack_linked_list_iterator_next(&ack_it);
        if (btstack_time_delta(now, pdu->retransmit_timeout_ms) >= 0) {
            // remove from list
            btstack_linked_list_remove(&mesh_access_acknowledged_messages, (btstack_linked_item_t*) pdu);
            // retransmit or report failure
            if (pdu->retransmit_count){
                pdu->retransmit_count--;
                mesh_upper_transport_send_access_pdu(pdu);
            } else {
                // find correct model and emit error
                uint16_t src = mesh_pdu_src(pdu);
                uint16_t dst = mesh_pdu_dst(pdu);
                mesh_element_t * element = mesh_node_element_for_unicast_address(src);
                if (element){
                    // find
                    mesh_model_iterator_t model_it;
                    mesh_model_iterator_init(&model_it, element);
                    while (mesh_model_iterator_has_next(&model_it)){
                        mesh_model_t * model = mesh_model_iterator_next(&model_it);
                        // find opcode in table
                        const mesh_operation_t * operation = mesh_model_lookup_operation_by_opcode(model, pdu->ack_opcode);
                        if (operation == NULL) continue;
                        if (model->model_packet_handler == NULL) continue;
                        // emit event
                        uint8_t event[13];
                        event[0] = HCI_EVENT_MESH_META;
                        event[1] = sizeof(event) - 2;
                        event[2] = element->element_index;
                        little_endian_store_32(event, 3, model->model_identifier);
                        little_endian_store_32(event, 7, pdu->ack_opcode);
                        little_endian_store_16(event, 11, dst);
                        (*model->model_packet_handler)(HCI_EVENT_PACKET, 0, event, sizeof(event));
                    }
                }

                // free
                mesh_upper_transport_pdu_free(pdu);
            }
        }
    }

    // find earliest timeout and set timer
    btstack_linked_list_iterator_init(&ack_it, &mesh_access_acknowledged_messages);
    int32_t next_timeout_ms = 0;
    while (btstack_linked_list_iterator_has_next(&ack_it)){
        mesh_pdu_t * pdu = (mesh_pdu_t *) btstack_linked_list_iterator_next(&ack_it);
        int32_t timeout_delta_ms = btstack_time_delta(pdu->retransmit_timeout_ms, now);
        if (next_timeout_ms == 0 || timeout_delta_ms < next_timeout_ms){
            next_timeout_ms = timeout_delta_ms;
        }
    }

    // set timer
    if (next_timeout_ms == 0) return;

    btstack_run_loop_set_timer(&mesh_access_acknowledged_timer, next_timeout_ms);
    btstack_run_loop_set_timer_handler(&mesh_access_acknowledged_timer, mesh_access_acknowledged_run);
    btstack_run_loop_add_timer(&mesh_access_acknowledged_timer);
}

static void mesh_access_acknowledged_received(uint16_t rx_src, uint32_t opcode){
    // check if received src matches our dest
    // free acknowledged messages if we were waiting for this message

    btstack_linked_list_iterator_t ack_it;
    btstack_linked_list_iterator_init(&ack_it, &mesh_access_acknowledged_messages);
    while (btstack_linked_list_iterator_has_next(&ack_it)){
        mesh_pdu_t * tx_pdu = (mesh_pdu_t *) btstack_linked_list_iterator_next(&ack_it);
        uint16_t tx_dest = mesh_pdu_dst(tx_pdu);
        if (tx_dest != rx_src) continue;
        if (tx_pdu->ack_opcode != opcode) continue;
        // got expected response from dest, remove from outgoing messages
        mesh_upper_transport_pdu_free(tx_pdu);
        return;
    }
}

static void mesh_access_upper_transport_handler(mesh_transport_callback_type_t callback_type, mesh_transport_status_t status, mesh_pdu_t * pdu){
    UNUSED(status);
    switch (callback_type){
        case MESH_TRANSPORT_PDU_SENT:
            // unacknowledged -> free
            if (pdu->ack_opcode == MESH_ACCESS_OPCODE_INVALID){
                mesh_upper_transport_pdu_free(pdu);
                break;
            }
            // setup timeout
            pdu->retransmit_timeout_ms = btstack_run_loop_get_time_ms() + mesh_access_acknowledged_message_timeout_ms();
            // add to mesh_access_acknowledged_messages
            btstack_linked_list_add(&mesh_access_acknowledged_messages, (btstack_linked_item_t *) pdu);
            // update timer
            mesh_access_acknowledged_run(NULL);
            break;
        default:
            break;
    }
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
    return time_gdtt & 0x3fu;
}

static uint32_t mesh_access_transitions_step_ms_from_gdtt(uint8_t time_gdtt){
    mesh_default_transition_step_resolution_t step_resolution = (mesh_default_transition_step_resolution_t) (time_gdtt >> 6);
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

uint16_t mesh_pdu_ttl(mesh_pdu_t * pdu){
    switch (pdu->pdu_type){
        case MESH_PDU_TYPE_TRANSPORT:
            return mesh_transport_ttl((mesh_transport_pdu_t*) pdu);
        case MESH_PDU_TYPE_NETWORK:
            return mesh_network_ttl((mesh_network_pdu_t *) pdu);
        default:
            return 0;
    }
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

uint8_t mesh_pdu_control_opcode(mesh_pdu_t * pdu){
    switch (pdu->pdu_type){
        case MESH_PDU_TYPE_TRANSPORT:
            return mesh_transport_control_opcode((mesh_transport_pdu_t*) pdu);
        case MESH_PDU_TYPE_NETWORK:
            return mesh_network_control_opcode((mesh_network_pdu_t *) pdu);
        default:
            return 0xff;
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

mesh_network_pdu_t * mesh_access_setup_unsegmented_message(const mesh_access_message_t *message_template, ...){
    mesh_network_pdu_t * network_pdu = mesh_access_network_init(message_template->opcode);
    if (!network_pdu) return NULL;

    va_list argptr;
    va_start(argptr, message_template);

    // add params
    const char * format = message_template->format;
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

mesh_transport_pdu_t * mesh_access_setup_segmented_message(const mesh_access_message_t *message_template, ...){
    mesh_transport_pdu_t * transport_pdu = mesh_access_transport_init(message_template->opcode);
    if (!transport_pdu) return NULL;

    va_list argptr;
    va_start(argptr, message_template);

    // add params
    const char * format = message_template->format;
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

static const mesh_operation_t * mesh_model_lookup_operation_by_opcode(mesh_model_t * model, uint32_t opcode){
    // find opcode in table
    const mesh_operation_t * operation = model->operations;
    if (operation == NULL) return NULL;
    for ( ; operation->handler != NULL ; operation++){
        if (operation->opcode != opcode) continue;
        return operation;
    }
    return NULL;
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
    printf_hexdump(mesh_pdu_data(pdu), len);

    uint16_t src = mesh_pdu_src(pdu);
    uint16_t dst = mesh_pdu_dst(pdu);
    uint16_t appkey_index = mesh_pdu_appkey_index(pdu);
    if (mesh_network_address_unicast(dst)){
        // loookup element by unicast address
        mesh_element_t * element = mesh_node_element_for_unicast_address(dst);
        if (element != NULL){
            // iterate over models, look for operation
            mesh_model_iterator_t model_it;
            mesh_model_iterator_init(&model_it, element);
            while (mesh_model_iterator_has_next(&model_it)){
                mesh_model_t * model = mesh_model_iterator_next(&model_it);
                // find opcode in table
                const mesh_operation_t * operation = mesh_model_lookup_operation(model, pdu);
                if (operation == NULL) continue;
                if (mesh_access_validate_appkey_index(model, appkey_index) == 0) continue;
                mesh_access_acknowledged_received(src, opcode);
                operation->handler(model, pdu);
                return;
            }
        }
    }
    else if (mesh_network_address_group(dst)){

        // handle fixed group address
        if (dst >= 0xff00){
            int deliver_to_primary_element = 1;
            switch (dst){
                case MESH_ADDRESS_ALL_PROXIES:
                    if (mesh_foundation_gatt_proxy_get() == 1){
                        deliver_to_primary_element = 1;                        
                    } 
                    break;
                case MESH_ADDRESS_ALL_FRIENDS:
                    // TODO: not implemented
                    break;
                case MESH_ADDRESS_ALL_RELAYS:
                    if (mesh_foundation_relay_get() == 1){
                        deliver_to_primary_element =1;
                    }
                    break;
                case MESH_ADDRESS_ALL_NODES:
                    deliver_to_primary_element = 1;
                    break;
                default:
                    break;
            }
            if (deliver_to_primary_element){
                mesh_model_iterator_t model_it;
                mesh_model_iterator_init(&model_it, mesh_node_get_primary_element());
                while (mesh_model_iterator_has_next(&model_it)){
                    mesh_model_t * model = mesh_model_iterator_next(&model_it);
                    // find opcode in table
                    const mesh_operation_t * operation = mesh_model_lookup_operation(model, pdu);
                    if (operation == NULL) continue;
                    if (mesh_access_validate_appkey_index(model, appkey_index) == 0) continue;
                    mesh_access_acknowledged_received(src, opcode);
                    operation->handler(model, pdu);
                    return;
                }
            }
        }
        else {
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
                        if (operation == NULL) continue;
                        if (mesh_access_validate_appkey_index(model, appkey_index) == 0) continue;
                        mesh_access_acknowledged_received(src, opcode);
                        operation->handler(model, pdu);
                        return;
                    }
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
    uint32_t publication_period_ms = mesh_access_time_gdtt2ms(publication_model->period);

    // set next publication
    if (publication_period_ms != 0){
        publication_model->next_publication_ms = now + publication_period_ms;
        publication_model->state = MESH_MODEL_PUBLICATION_STATE_W4_PUBLICATION_MS;
    } else {
        publication_model->state = MESH_MODEL_PUBLICATION_STATE_IDLE;
    }
}

// assumes retransmit_count is valid
static void mesh_model_publication_setup_retransmission(mesh_publication_model_t * publication_model, uint32_t now){
    uint32_t publication_period_ms = mesh_access_time_gdtt2ms(publication_model->period);

    // retransmission done
    if (publication_model->retransmit_count == 0) {
        // wait for next main event if periodic and retransmission complete
        if (publication_period_ms != 0){
            publication_model->state = MESH_MODEL_PUBLICATION_STATE_W4_PUBLICATION_MS;
        } else {
            publication_model->state = MESH_MODEL_PUBLICATION_STATE_IDLE;
        }
        return;
    }

    // calc next retransmit time
    uint32_t retransmission_ms = now + mesh_model_publication_retransmission_period_ms(publication_model->retransmit);

    // check next publication timeout is before next retransmission
    if (publication_period_ms != 0){
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

            // check if either timer fired
            switch (publication_model->state){
                case MESH_MODEL_PUBLICATION_STATE_W4_PUBLICATION_MS:
                    if (btstack_time_delta(publication_model->next_publication_ms, now) > 0) break;
                    publication_model->state = MESH_MODEL_PUBLICATION_STATE_PUBLICATION_READY;
                    break;
                case MESH_MODEL_PUBLICATION_STATE_W4_RETRANSMIT_MS:
                    if (btstack_time_delta(publication_model->next_retransmit_ms, now) > 0) break;
                    publication_model->state = MESH_MODEL_PUBLICATION_STATE_RETRANSMIT_READY;
                    break;
                default:
                    break;
            }

            switch (publication_model->state){
                case MESH_MODEL_PUBLICATION_STATE_PUBLICATION_READY:
                    // schedule next publication and retransmission
                    mesh_model_publication_setup_publication(publication_model, now);
                    mesh_model_publication_setup_retransmission(publication_model, now);
                    mesh_model_publication_publish_now_model(mesh_model);
                    break;
                case MESH_MODEL_PUBLICATION_STATE_RETRANSMIT_READY:
                    // schedule next retransmission
                    publication_model->retransmit_count--;
                    mesh_model_publication_setup_retransmission(publication_model, now);
                    mesh_model_publication_publish_now_model(mesh_model);
                    break;
                default:
                    break;
            }
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

    // remove current timer if active
    if (ts == NULL){
        btstack_run_loop_remove_timer(&mesh_access_publication_timer);

    }

    // new timeout?
    if (next_timeout_ms == 0) return;

    // set timer
    btstack_run_loop_set_timer(&mesh_access_publication_timer, next_timeout_ms);
    btstack_run_loop_set_timer_handler(&mesh_access_publication_timer, mesh_model_publication_run);
    btstack_run_loop_add_timer(&mesh_access_publication_timer);
}

void mesh_model_publication_start(mesh_model_t * mesh_model){
    mesh_publication_model_t * publication_model = mesh_model->publication_model;
    if (publication_model == NULL) return;

    // publish right away
    publication_model->state = MESH_MODEL_PUBLICATION_STATE_PUBLICATION_READY;
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
    publication_model->state = MESH_MODEL_PUBLICATION_STATE_PUBLICATION_READY;
    mesh_model_publication_run(NULL);
}

