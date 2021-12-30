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

#ifndef __MESH_TRANSPORT_H
#define __MESH_TRANSPORT_H

#include <stdint.h>

#include "mesh/mesh_network.h"
#include "mesh/mesh_lower_transport.h"

#ifdef __cplusplus
extern "C"
{
#endif

// upper transport message builder
typedef struct {
    mesh_upper_transport_pdu_t  * pdu;
    mesh_network_pdu_t * segment;
} mesh_upper_transport_builder_t;

/*
 * @brief reserve 1 x mesh_upper_transport_pdu_t and 14 x mesh_network_pdu_t to allow composition of max access/control message
 * @return true if enough buffer allocated
 */
bool mesh_upper_transport_message_reserve(void);

/*
 * @brief init message builder for unsegmented access, segmented access, and segmented control messages
 * @note uses buffers reserved by mesh_upper_transport_message_reserve
 */
void mesh_upper_transport_message_init(mesh_upper_transport_builder_t * builder, mesh_pdu_type_t pdu_type);

/**
 * @brief append data to message
 * @param builder
 * @param data
 * @param data_len
 */
void mesh_upper_transport_message_add_data(mesh_upper_transport_builder_t * builder, const uint8_t * data, uint16_t data_len);

/**
 * @brief append single byte
 * @param builder
 * @param value
 */
void mesh_upper_transport_message_add_uint8(mesh_upper_transport_builder_t * builder, uint8_t value);
/**
 * @brief append uint16
 * @param builder
 * @param value
 */
void mesh_upper_transport_message_add_uint16(mesh_upper_transport_builder_t * builder, uint16_t value);
/**
 * @brief append uint24
 * @param builder
 * @param value
 */
void mesh_upper_transport_message_add_uint24(mesh_upper_transport_builder_t * builder, uint32_t value);
/**
 * @brief append uint32
 * @param builder
 * @param value
 */
void mesh_upper_transport_message_add_uint32(mesh_upper_transport_builder_t * builder, uint32_t value);
/**
 * @brief finalize message
 * @param builder
 * @return upper_transport_pdu if message was successfully constructed
 */
mesh_upper_transport_pdu_t * mesh_upper_transport_message_finalize(mesh_upper_transport_builder_t * builder);

/**
 * @brief Initialize Upper Transport Layer
 */
void mesh_upper_transport_init(void);

/**
 * @brief Register for control messages and events
 * @param callback
 */
void mesh_upper_transport_register_control_message_handler(void (*callback)(mesh_transport_callback_type_t callback_type, mesh_transport_status_t status, mesh_pdu_t * pdu));

/**
 * @brief Register for access messages and events
 * @param callback
 */
void mesh_upper_transport_register_access_message_handler(void (*callback)(mesh_transport_callback_type_t callback_type, mesh_transport_status_t status, mesh_pdu_t * pdu));

/**
 * @brief Report received PDU as processed
 * @param pdu
 * @note pdu cannot be accessed after call
 */
void mesh_upper_transport_message_processed_by_higher_layer(mesh_pdu_t * pdu);

/**
 * @brief Request to send upper transport PDU
 * @param request
 */
void mesh_upper_transport_request_to_send(btstack_context_callback_registration_t * request);

/**
 * @brief Free any PDU type
 * @param pdu
 */
void mesh_upper_transport_pdu_free(mesh_pdu_t * pdu);

// Control PDUs: setup and send

/**
 * @brief Setup Unsegmented Control Pdu provided by network_pdu param
 * @param network_pdu
 * @param netkey_index
 * @param ttl
 * @param src
 * @param dest
 * @param opcode
 * @param control_pdu_data
 * @param control_pdu_len
 * @return 0 on success
 */
uint8_t mesh_upper_transport_setup_unsegmented_control_pdu(mesh_network_pdu_t * network_pdu, uint16_t netkey_index, uint8_t ttl, uint16_t src, uint16_t dest, uint8_t opcode,
                                                           const uint8_t * control_pdu_data, uint16_t control_pdu_len);

/**
 * @brief Setup header of Segmented Control PDU created with mesh_upper_transport_message_finalize
 * @param upper_pdu
 * @param netkey_index
 * @param ttl
 * @param src
 * @param dest
 * @param opcode
 * @return 0 on success
 */
uint8_t mesh_upper_transport_setup_segmented_control_pdu_header(mesh_upper_transport_pdu_t * upper_pdu, uint16_t netkey_index, uint8_t ttl, uint16_t src, uint16_t dest, uint8_t opcode);

/**
 * @brief Send Control PDU
 * @param pdu
 */
void mesh_upper_transport_send_control_pdu(mesh_pdu_t * pdu);

// Access PDUs: setup and send
/**
 * @brief Setup header of Access PDU created with mesh_upper_transport_message_finalize
 * @param pdu
 * @param netkey_index
 * @param appkey_index
 * @param ttl
 * @param src
 * @param dest
 * @param szmic
 * @return
 */
uint8_t mesh_upper_transport_setup_access_pdu_header(mesh_pdu_t * pdu, uint16_t netkey_index, uint16_t appkey_index,
                                                               uint8_t ttl, uint16_t src, uint16_t dest, uint8_t szmic);

/**
 * @brief Send Access PDU
 * @param pdu
 */
void mesh_upper_transport_send_access_pdu(mesh_pdu_t * pdu);


// test
void mesh_upper_transport_dump(void);
void mesh_upper_transport_reset(void);

#ifdef __cplusplus
} /* end of extern "C" */
#endif

#endif
