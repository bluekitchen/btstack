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
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL BLUEKITCHEN
 * GMBH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
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

/**
 * @title Mesh Access Layer
 *
 * Build and parse access messages and manage model transitions.
 */

#ifndef MESH_ACCESS_H
#define MESH_ACCESS_H

#include <stdint.h>
#include <stdarg.h>

#include "btstack_bool.h"
#include "bluetooth_company_id.h"
#include "btstack_linked_list.h"

#include "mesh/mesh_lower_transport.h"
#include "mesh/mesh_keys.h"
#include "mesh/mesh_node.h"
#include "mesh_upper_transport.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* API_START */

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
    MODEL_STATE_UPDATE_REASON_TRANSITION_ACTIVE,
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

#define MESH_MAX_NUM_FAULTS 5

#define MESH_TRANSITION_NUM_STEPS_INFINITE 0x3f

typedef struct {
    // linked list item
    btstack_linked_item_t item;
    uint8_t  test_id;
    uint16_t company_id;
    uint16_t num_current_faults;
    uint16_t num_registered_faults;
    uint8_t  current_faults[MESH_MAX_NUM_FAULTS];
    uint8_t  registered_faults[MESH_MAX_NUM_FAULTS];
} mesh_health_fault_t;

typedef struct {
    // linked list of mesh_health_fault items
    btstack_linked_list_t faults;
    uint8_t fast_period_divisor;
} mesh_health_state_t;

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
    btstack_timer_source_t timer;

    mesh_transition_state_t state;

    uint8_t  transaction_identifier; 
    uint32_t transaction_timestamp_ms;
    uint16_t src_address;
    uint16_t dst_address; 

    uint8_t num_steps;
    mesh_default_transition_step_resolution_t step_resolution;
    uint32_t step_duration_ms;

    // to send events and/or publish changes
    mesh_model_t * mesh_model;
        
    // to execute transition
    void (* transition_callback)(struct mesh_transition * transition, model_state_update_reason_t event);
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


// Mesh Model Transitions
/**
 * @brief Get the step duration in milliseconds from an encoded transition time.
 */
uint32_t mesh_access_transitions_step_ms_from_gdtt(uint8_t time_gdtt);
/**
 * @brief Get the number of steps from an encoded transition time.
 */
uint8_t  mesh_access_transitions_num_steps_from_gdtt(uint8_t time_gdtt);
/**
 * @brief Convert an encoded transition time to milliseconds.
 */
uint32_t mesh_access_time_gdtt2ms(uint8_t time_gdtt);
/**
 * @brief Initialize a transition transaction.
 */
void mesh_access_transitions_init_transaction(mesh_transition_t * transition, uint8_t transaction_identifier, uint16_t src_address, uint16_t dst_address);
/**
 * @brief Set up a model state transition.
 */
void mesh_access_transition_setup(mesh_model_t *mesh_model, mesh_transition_t * base_transition, uint8_t transition_time_gdtt, uint8_t delay_time_gdtt, void (*transition_callback)(mesh_transition_t * base_transition, model_state_update_reason_t event));
/**
 * @brief Abort a transition transaction.
 */
void mesh_access_transitions_abort_transaction(mesh_transition_t * transition);
/**
 * @brief Get the next transition transaction identifier.
 */
uint8_t mesh_access_transactions_get_next_transaction_id(void);
/**
 * @brief Get the status of a transition transaction.
 */
mesh_transaction_status_t mesh_access_transitions_transaction_status(mesh_transition_t * transition, uint8_t transaction_identifier, uint16_t src_address, uint16_t dst_address);

/**
 * @brief Emit a Boolean model state update event.
 */
void mesh_access_emit_state_update_bool(btstack_packet_handler_t event_handler, uint8_t element_index, uint32_t model_identifier,
    model_state_id_t state_identifier, model_state_update_reason_t reason, uint8_t value);

/**
 * @brief Emit a signed 16-bit model state update event.
 */
void mesh_access_emit_state_update_int16(btstack_packet_handler_t event_handler, uint8_t element_index, uint32_t model_identifier,
    model_state_id_t state_identifier, model_state_update_reason_t reason, int16_t value);

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
/**
 * @brief Get the control flag from a mesh PDU.
 */
uint16_t mesh_pdu_ctl(mesh_pdu_t * pdu);
/**
 * @brief Get the time to live from a mesh PDU.
 */
uint16_t mesh_pdu_ttl(mesh_pdu_t * pdu);
/**
 * @brief Get the source address from a mesh PDU.
 */
uint16_t mesh_pdu_src(mesh_pdu_t * pdu);
/**
 * @brief Get the destination address from a mesh PDU.
 */
uint16_t mesh_pdu_dst(mesh_pdu_t * pdu);
/**
 * @brief Get the network key index from a mesh PDU.
 */
uint16_t mesh_pdu_netkey_index(mesh_pdu_t * pdu);
/**
 * @brief Get the application key index from a mesh PDU.
 */
uint16_t mesh_pdu_appkey_index(mesh_pdu_t * pdu);
/**
 * @brief Get the payload length from a mesh PDU.
 */
uint16_t mesh_pdu_len(mesh_pdu_t * pdu);
/**
 * @brief Get the payload data from a mesh PDU.
 */
uint8_t * mesh_pdu_data(mesh_pdu_t * pdu);
/**
 * @brief Get the control opcode from a mesh PDU.
 */
uint8_t  mesh_pdu_control_opcode(mesh_pdu_t * pdu);

// Mesh Access Parser
/**
 * @brief Get the opcode from an access PDU.
 */
int mesh_access_pdu_get_opcode(mesh_pdu_t * pdu, uint32_t * opcode, uint16_t * opcode_size);
/**
 * @brief Initialize a parser for an access message.
 */
int  mesh_access_parser_init(mesh_access_parser_state_t * state, mesh_pdu_t * pdu);
/**
 * @brief Skip bytes in the access message being parsed.
 */
void mesh_access_parser_skip(mesh_access_parser_state_t * state, uint16_t bytes_to_skip);
/**
 * @brief Get the number of bytes remaining in the parsed message.
 */
uint16_t mesh_access_parser_available(mesh_access_parser_state_t * state);
/**
 * @brief Read an 8-bit value from the access message.
 */
uint8_t  mesh_access_parser_get_uint8(mesh_access_parser_state_t * state);
/**
 * @brief Read a 16-bit value from the access message.
 */
uint16_t mesh_access_parser_get_uint16(mesh_access_parser_state_t * state);
/**
 * @brief Read a 24-bit value from the access message.
 */
uint32_t mesh_access_parser_get_uint24(mesh_access_parser_state_t * state);
/**
 * @brief Read a 32-bit value from the access message.
 */
uint32_t mesh_access_parser_get_uint32(mesh_access_parser_state_t * state);
/**
 * @brief Read a 128-bit value from the access message.
 */
void mesh_access_parser_get_uint128(mesh_access_parser_state_t * state, uint8_t * dest);
/**
 * @brief Read a label UUID from the access message.
 */
void mesh_access_parser_get_label_uuid(mesh_access_parser_state_t * state, uint8_t * dest);
/**
 * @brief Read a key from the access message.
 */
void mesh_access_parser_get_key(mesh_access_parser_state_t * state, uint8_t * dest);
/**
 * @brief Read a Bluetooth SIG model identifier from the access message.
 */
uint32_t mesh_access_parser_get_sig_model_identifier(mesh_access_parser_state_t * parser);
/**
 * @brief Read a vendor model identifier from the access message.
 */
uint32_t mesh_access_parser_get_vendor_model_identifier(mesh_access_parser_state_t * parser);
/**
 * @brief Read a model identifier from the access message.
 */
uint32_t mesh_access_parser_get_model_identifier(mesh_access_parser_state_t * parser);

/**
 * @brief Initialize an access message for building.
 */
void mesh_access_message_init(mesh_upper_transport_builder_t * builder, uint32_t opcode);
/**
 * @brief Append bytes to an access message.
 */
void mesh_access_message_add_data(mesh_upper_transport_builder_t * builder, const uint8_t * data, uint16_t data_len);
/**
 * @brief Append an 8-bit value to an access message.
 */
void mesh_access_message_add_uint8(mesh_upper_transport_builder_t * builder, uint8_t value);
/**
 * @brief Append a 16-bit value to an access message.
 */
void mesh_access_message_add_uint16(mesh_upper_transport_builder_t * builder, uint16_t value);
/**
 * @brief Append a 24-bit value to an access message.
 */
void mesh_access_message_add_uint24(mesh_upper_transport_builder_t * builder, uint32_t value);
/**
 * @brief Append a 32-bit value to an access message.
 */
void mesh_access_message_add_uint32(mesh_upper_transport_builder_t * builder, uint32_t value);
/**
 * @brief Append a model identifier to an access message.
 */
void mesh_access_message_add_model_identifier(mesh_upper_transport_builder_t * builder, uint32_t model_identifier);
/**
 * @brief Finish building an access message.
 */
mesh_upper_transport_pdu_t * mesh_access_message_finalize(mesh_upper_transport_builder_t * builder);

// message builder using template
/**
 * @brief Set up an access message for transmission.
 */
mesh_upper_transport_pdu_t * mesh_access_setup_message(const mesh_access_message_t *message_template, ...);

/* API_END */

#ifdef __cplusplus
} /* end of extern "C" */
#endif

#endif
