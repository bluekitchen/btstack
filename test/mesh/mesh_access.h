/*
 * Copyright (C) 2018 BlueKitchen GmbH
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

#ifndef __MESH_ACCESS_H
#define __MESH_ACCESS_H

#include <stdint.h>
#include "btstack_linked_list.h"
#include "mesh/mesh_lower_transport.h"
#include "mesh_keys.h"
#include "bluetooth_company_id.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define MAX_NR_MESH_APPKEYS_PER_MODEL           3u
#define MAX_NR_MESH_SUBSCRIPTION_PER_MODEL      3u

#define MESH_APPKEY_INVALID                     0xffffu

struct mesh_model;
struct mesh_element;

// function to handle model operation message
typedef void (*mesh_operation_handler)(struct mesh_model * mesh_model, mesh_pdu_t * pdu);

// function to publish the current state of a model
// @param mesh_model to publish
// @returns mesh_pdu with status message
typedef mesh_pdu_t * (*mesh_publish_state_t)(struct mesh_model * mesh_model);

typedef enum {
    MESH_DEFAULT_TRANSITION_STEP_RESOLUTION_100ms = 0x00u,
    MESH_DEFAULT_TRANSITION_STEP_RESOLUTION_1s,
    MESH_DEFAULT_TRANSITION_STEP_RESOLUTION_10s,
    MESH_DEFAULT_TRANSITION_STEP_RESOLUTION_10min
} mesh_default_transition_step_resolution_t;

typedef enum {
    MODEL_STATE_UPDATE_REASON_SET = 0x00u, 
    MODEL_STATE_UPDATE_REASON_TRANSITION_START, 
    // MODEL_STATE_UPDATE_REASON_TRANSITION_ACTIVE,
    MODEL_STATE_UPDATE_REASON_TRANSITION_END, 
    MODEL_STATE_UPDATE_REASON_TRANSITION_ABORT, 
    // MODEL_STATE_UPDATE_REASON_BOUND_STATE, 
    MODEL_STATE_UPDATE_REASON_APPLICATION_CHANGE
 } model_state_update_reason_t;

typedef enum {
    TRANSITION_START,
    TRANSITION_UPDATE
} transition_event_t;

typedef enum {
    MESH_TRANSITION_STATE_IDLE,
    MESH_TRANSITION_STATE_DELAYED,
    MESH_TRANSITION_STATE_ACTIVE
} mesh_transition_state_t;

typedef enum {
    MODEL_STATE_ID_GENERIC_ON_OFF = (BLUETOOTH_COMPANY_ID_BLUETOOTH_SIG_INC << 16) | 0u,
    MODEL_STATE_ID_GENERIC_LEVEL  = (BLUETOOTH_COMPANY_ID_BLUETOOTH_SIG_INC << 16) | 1u,
} model_state_id_t;

typedef struct {
    mesh_publish_state_t publish_state_fn;
    uint8_t  publish_now;
    
    uint16_t address;
    uint16_t appkey_index;
    uint8_t  friendship_credential_flag;
    uint8_t  period;
    uint8_t  ttl;
    uint8_t  retransmit;
} mesh_publication_model_t;


typedef struct {
    uint32_t opcode;
    uint16_t minimum_length;
    mesh_operation_handler handler;
} mesh_operation_t;

typedef struct mesh_model {
    // linked list item
    btstack_linked_item_t item;

    // element
    struct mesh_element * element;

    // internal model enumeration
    uint16_t mid;

    // vendor_id << 16 | model id, use BLUETOOTH_COMPANY_ID_BLUETOOTH_SIG_INC for SIG models
    uint32_t model_identifier;

    // model operations
    const mesh_operation_t * operations;

    // publication model if supported
    mesh_publication_model_t * publication_model;

    // data
    void * model_data;

    // bound appkeys
    uint16_t appkey_indices[MAX_NR_MESH_APPKEYS_PER_MODEL];

    // subscription list
    uint16_t subscriptions[MAX_NR_MESH_SUBSCRIPTION_PER_MODEL];

    // packet handler for transition events in server, event callback handler in client
    btstack_packet_handler_t * model_packet_handler;
} mesh_model_t;

typedef struct {
    btstack_linked_list_iterator_t it;
} mesh_model_iterator_t;

typedef struct {
    btstack_linked_list_iterator_t it;
} mesh_element_iterator_t;

typedef struct mesh_element {
    // linked list item
    btstack_linked_item_t item;
    
    // element index
    uint16_t element_index;

    // LOC
    uint16_t loc;
    
    // models
    btstack_linked_list_t models;
    uint16_t models_count_sig;
    uint16_t models_count_vendor;

} mesh_element_t;

typedef struct {
    uint32_t opcode;
    uint8_t * data;
    uint16_t len;
} mesh_access_parser_state_t;

typedef struct {
    uint32_t     opcode;
    const char * format;
} mesh_access_message_t;

typedef enum {
    MESH_TRANSACTION_STATUS_NEW = 0,
    MESH_TRANSACTION_STATUS_RETRANSMISSION,
    MESH_TRANSACTION_STATUS_DIFFERENT_DST_OR_SRC
} mesh_transaction_status_t;

typedef struct mesh_transition {
    btstack_linked_item_t item;

    mesh_transition_state_t state;

    uint8_t  transaction_identifier; 
    uint32_t transaction_timestamp_ms;
    uint16_t src_address;
    uint16_t dst_address; 

    mesh_default_transition_step_resolution_t step_duration_ms;
    uint32_t phase_start_ms;
    uint32_t remaining_delay_time_ms;  
    uint32_t remaining_transition_time_ms;      
    // to send events and/or publish changes
    mesh_model_t * mesh_model;

    // to execute transition
    void (* transition_callback)(struct mesh_transition * transition, transition_event_t event, uint32_t current_timestamp);
} mesh_transition_t;

/**
 * @brief Init access layer
 */
void mesh_access_init(void);

void mesh_access_message_processed(mesh_pdu_t * pdu);

mesh_element_t * mesh_primary_element(void);

void mesh_access_set_primary_element_address(uint16_t unicast_address);

uint16_t mesh_access_get_primary_element_address(void);

void mesh_access_set_primary_element_location(uint16_t location);

void mesh_element_add(mesh_element_t * element);

uint8_t mesh_access_get_element_index(mesh_model_t * mesh_model);

uint16_t mesh_access_get_element_address(mesh_model_t * mesh_model);

mesh_element_t * mesh_element_for_unicast_address(uint16_t unicast_address);

mesh_element_t * mesh_element_for_index(uint16_t element_index);

void mesh_element_add_model(mesh_element_t * element, mesh_model_t * mesh_model);

// Mesh Element Iterator

void mesh_element_iterator_init(mesh_element_iterator_t * iterator);

int mesh_element_iterator_has_next(mesh_element_iterator_t * iterator);

mesh_element_t * mesh_element_iterator_next(mesh_element_iterator_t * iterator);

// Mesh Model Iterator

void mesh_model_iterator_init(mesh_model_iterator_t * iterator, mesh_element_t * element);

int mesh_model_iterator_has_next(mesh_model_iterator_t * iterator);

mesh_model_t * mesh_model_iterator_next(mesh_model_iterator_t * iterator);

// Mesh Model Utility

mesh_model_t * mesh_model_get_by_identifier(mesh_element_t * element, uint32_t model_identifier);

uint32_t mesh_model_get_model_identifier_bluetooth_sig(uint16_t model_id);

int mesh_model_is_bluetooth_sig(uint32_t model_identifier);

uint16_t mesh_model_get_model_id(uint32_t model_identifier);

uint32_t mesh_model_get_model_identifier(uint16_t vendor_id, uint16_t model_id);

mesh_model_t * mesh_model_get_configuration_server(void);

mesh_model_t * mesh_access_model_for_address_and_model_identifier(uint16_t element_address, uint32_t model_identifier, uint8_t * status);

uint8_t mesh_access_transitions_num_steps_from_gdtt(uint8_t time_gdtt);
uint32_t mesh_access_time_gdtt2ms(uint8_t time_gdtt);

void mesh_access_emit_state_update_bool(btstack_packet_handler_t * event_handler, uint8_t element_index, uint32_t model_identifier, 
    model_state_id_t state_identifier, model_state_update_reason_t reason, uint8_t value);
void mesh_access_emit_state_update_int16(btstack_packet_handler_t * event_handler, uint8_t element_index, uint32_t model_identifier, 
    model_state_id_t state_identifier, model_state_update_reason_t reason, int16_t value);

// Mesh Model Transitions
void mesh_access_transitions_setup_transaction(mesh_transition_t * transition, uint8_t transaction_identifier, uint16_t src_address, uint16_t dst_address);
void mesh_access_transitions_abort_transaction(mesh_transition_t * transition);
mesh_transaction_status_t mesh_access_transitions_transaction_status(mesh_transition_t * transition, uint8_t transaction_identifier, uint16_t src_address, uint16_t dst_address);

void mesh_access_transitions_setup(mesh_transition_t * transition, mesh_model_t * mesh_model, 
    uint8_t transition_time_gdtt, uint8_t delay_gdtt,
    void (* transition_callback)(struct mesh_transition * transition, transition_event_t event, uint32_t current_timestamp));

void mesh_access_transitions_add(mesh_transition_t * transition);
void mesh_access_transitions_remove(mesh_transition_t * transition);
uint8_t mesh_access_transactions_get_next_transaction_id(void);

// Mesh Model Publicaation

/**
 * Inform Mesh Access that the state of a model has changed. may trigger state publication
 */
void mesh_access_state_changed(mesh_model_t * mesh_model);

// Mesh PDU Getter
uint16_t mesh_pdu_src(mesh_pdu_t * pdu);
uint16_t mesh_pdu_dst(mesh_pdu_t * pdu);
uint16_t mesh_pdu_netkey_index(mesh_pdu_t * pdu);
uint16_t mesh_pdu_appkey_index(mesh_pdu_t * pdu);
uint16_t mesh_pdu_len(mesh_pdu_t * pdu);
uint8_t * mesh_pdu_data(mesh_pdu_t * pdu);

// Mesh NetKey List
void mesh_store_network_key(mesh_network_key_t * network_key);
void mesh_delete_network_key(uint16_t internal_index);
void mesh_delete_network_keys(void);
void mesh_load_network_keys(void);

// Mesh Appkeys
void mesh_store_app_key(uint16_t internal_index, uint16_t netkey_index, uint16_t appkey_index, uint8_t aid, const uint8_t * application_key);
void mesh_delete_app_key(uint16_t internal_index);
void mesh_delete_app_keys(void);
void mesh_load_app_keys(void);

// Mesh Model Subscriptions
int mesh_model_contains_subscription(mesh_model_t * mesh_model, uint16_t address);

// Mesh Model to Appkey List
void mesh_load_appkey_lists(void);
void mesh_delete_appkey_lists(void);
void mesh_model_reset_appkeys(mesh_model_t * mesh_model);
uint8_t mesh_model_bind_appkey(mesh_model_t * mesh_model, uint16_t appkey_index);
void mesh_model_unbind_appkey(mesh_model_t * mesh_model, uint16_t appkey_index);
int mesh_model_contains_appkey(mesh_model_t * mesh_model, uint16_t appkey_index);

// Mesh Access Parser
int mesh_access_pdu_get_opcode(mesh_pdu_t * pdu, uint32_t * opcode, uint16_t * opcode_size);
int  mesh_access_parser_init(mesh_access_parser_state_t * state, mesh_pdu_t * pdu);
void mesh_access_parser_skip(mesh_access_parser_state_t * state, uint16_t bytes_to_skip);
uint16_t mesh_access_parser_available(mesh_access_parser_state_t * state);
uint8_t mesh_access_parser_get_u8(mesh_access_parser_state_t * state);
uint16_t mesh_access_parser_get_u16(mesh_access_parser_state_t * state);
uint32_t mesh_access_parser_get_u24(mesh_access_parser_state_t * state);
uint32_t mesh_access_parser_get_u32(mesh_access_parser_state_t * state);
void mesh_access_parser_get_u128(mesh_access_parser_state_t * state, uint8_t * dest);
void mesh_access_parser_get_label_uuid(mesh_access_parser_state_t * state, uint8_t * dest);
void mesh_access_parser_get_key(mesh_access_parser_state_t * state, uint8_t * dest);
uint32_t mesh_access_parser_get_model_identifier(mesh_access_parser_state_t * parser);

// Foundation state
void mesh_foundation_state_load(void);
void mesh_foundation_state_store(void);

// message builder transport
mesh_transport_pdu_t * mesh_access_transport_init(uint32_t opcode);
void mesh_access_transport_add_uint8(mesh_transport_pdu_t * pdu, uint8_t value);
void mesh_access_transport_add_uint16(mesh_transport_pdu_t * pdu, uint16_t value);
void mesh_access_transport_add_uint24(mesh_transport_pdu_t * pdu, uint32_t value);
void mesh_access_transport_add_uint32(mesh_transport_pdu_t * pdu, uint32_t value);
void mesh_access_transport_add_model_identifier(mesh_transport_pdu_t * pdu, uint32_t model_identifier);

// message builder network
mesh_network_pdu_t * mesh_access_network_init(uint32_t opcode);
void mesh_access_network_add_uint8(mesh_network_pdu_t * pdu, uint8_t value);
void mesh_access_network_add_uint16(mesh_network_pdu_t * pdu, uint16_t value);
void mesh_access_network_add_uint24(mesh_network_pdu_t * pdu, uint16_t value);
void mesh_access_network_add_uint32(mesh_network_pdu_t * pdu, uint16_t value);
void mesh_access_network_add_model_identifier(mesh_network_pdu_t * pdu, uint32_t model_identifier);

// message builder using template
mesh_network_pdu_t * mesh_access_setup_unsegmented_message(const mesh_access_message_t *template, ...);
mesh_transport_pdu_t * mesh_access_setup_segmented_message(const mesh_access_message_t *template, ...);

#ifdef __cplusplus
} /* end of extern "C" */
#endif

#endif
