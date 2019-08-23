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
#include <stdarg.h>

#include "bluetooth_company_id.h"
#include "btstack_linked_list.h"

#include "mesh/mesh_lower_transport.h"
#include "mesh/mesh_keys.h"
#include "mesh/mesh_node.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define MESH_SEQUENCE_NUMBER_STORAGE_INTERVAL 1000

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

#define MESH_MAX_NUM_FAULTS 3

typedef struct {
    // linked list item
    btstack_linked_item_t item;
    uint8_t  test_id;
    uint16_t company_id;
    uint16_t num_faults;
    uint8_t  faults[MESH_MAX_NUM_FAULTS];
} mesh_fault_t;

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

/**
 * @brief Inform access layer that access message was processed by higher layer
 * @param pdu
 */
void mesh_access_message_processed(mesh_pdu_t * pdu);

/**
 * @brief Get number of retransmissions used by default
 */
uint8_t mesh_access_acknowledged_message_retransmissions(void);

/**
 * @brief Get retransmission timeout
 */
uint32_t mesh_access_acknowledged_message_timeout_ms(void);

/**
 * @brief Send unacknowledged message
 * @param pdu
 */
void mesh_access_send_unacknowledged_pdu(mesh_pdu_t * pdu);

/**
 * @brief Send acknowledged message. Retransmits message if no acknowledgement with expected opcode is received
 * @param pdu
 * @param retransmissions
 * @param ack_opcode opcode of acknowledgement
 */
void mesh_access_send_acknowledged_pdu(mesh_pdu_t * pdu, uint8_t retransmissions, uint32_t ack_opcode);

uint8_t mesh_access_transitions_num_steps_from_gdtt(uint8_t time_gdtt);
uint32_t mesh_access_time_gdtt2ms(uint8_t time_gdtt);

void mesh_access_emit_state_update_bool(btstack_packet_handler_t event_handler, uint8_t element_index, uint32_t model_identifier,
    model_state_id_t state_identifier, model_state_update_reason_t reason, uint8_t value);
void mesh_access_emit_state_update_int16(btstack_packet_handler_t event_handler, uint8_t element_index, uint32_t model_identifier, 
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
 * @param mesh_model
 */
void mesh_access_state_changed(mesh_model_t * mesh_model);

/**
 * Start Model Publication
 * @param mesh_model
 */
void mesh_model_publication_start(mesh_model_t * mesh_model);

/**
 * Stop Model Publication
 * @param mesh_model
 */
void mesh_model_publication_stop(mesh_model_t * mesh_model);

// Mesh PDU Getter
uint16_t mesh_pdu_ttl(mesh_pdu_t * pdu);
uint16_t mesh_pdu_src(mesh_pdu_t * pdu);
uint16_t mesh_pdu_dst(mesh_pdu_t * pdu);
uint16_t mesh_pdu_netkey_index(mesh_pdu_t * pdu);
uint16_t mesh_pdu_appkey_index(mesh_pdu_t * pdu);
uint16_t mesh_pdu_len(mesh_pdu_t * pdu);
uint8_t * mesh_pdu_data(mesh_pdu_t * pdu);
uint8_t  mesh_pdu_control_opcode(mesh_pdu_t * pdu);

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
mesh_network_pdu_t * mesh_access_setup_unsegmented_message(const mesh_access_message_t *message_template, ...);
mesh_transport_pdu_t * mesh_access_setup_segmented_message(const mesh_access_message_t *message_template, ...);

#ifdef __cplusplus
} /* end of extern "C" */
#endif

#endif
