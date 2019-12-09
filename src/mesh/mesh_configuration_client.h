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

#ifndef __MESH_CONFIGURATION_CLIENT_H
#define __MESH_CONFIGURATION_CLIENT_H

#include <stdint.h>

#include "mesh/mesh_access.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Register packet handler
 * @param configuration_client_model
 * @param events_packet_handler
 */
void mesh_configuration_client_register_packet_handler(mesh_model_t *configuration_client_model, btstack_packet_handler_t events_packet_handler);

/**
 * @brief Get the current Secure Network Beacon state of a node.
 * @param mesh_model
 * @param dest
 * @param netkey_index
 * @param appkey_index
 * @return status       ERROR_CODE_SUCCESS if successful, otherwise BTSTACK_MEMORY_ALLOC_FAILED or ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE
 */
uint8_t mesh_configuration_client_send_beacon_get(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index);

/**
 * @brief Get the current Secure Network Beacon state of a node.
 * @param mesh_model
 * @param dest
 * @param netkey_index
 * @param appkey_index
 * @param Beacon        0x01 The node is broadcasting a Secure Network beacon, 0x00 broadcastinis  off
 * @return status       ERROR_CODE_SUCCESS if successful, otherwise BTSTACK_MEMORY_ALLOC_FAILED or ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE
 */
uint8_t mesh_configuration_client_send_beacon_set(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, uint8_t beacon);

/**
 * @brief Read one page of the Composition Data.
 * @param mesh_model
 * @param dest
 * @param netkey_index
 * @param appkey_index
 * @param page          Page number of the Composition Data
 * @return status       ERROR_CODE_SUCCESS if successful, otherwise BTSTACK_MEMORY_ALLOC_FAILED or ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE
 */
uint8_t mesh_configuration_client_send_composition_data_get(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, uint8_t page);

/**
 * @brief Get the current Default TTL state of a node
 * @param mesh_model
 * @param dest
 * @param netkey_index
 * @param appkey_index
 * @return status       ERROR_CODE_SUCCESS if successful, otherwise BTSTACK_MEMORY_ALLOC_FAILED or ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE
 */
uint8_t mesh_configuration_client_send_default_ttl_get(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index);

/**
 * @brief Set Default TTL state of a node
 * @param mesh_model
 * @param dest
 * @param netkey_index
 * @param appkey_index
 * @param ttl           allowed values: 0x00, 0x02–0x7F
 * @return status       ERROR_CODE_SUCCESS if successful, otherwise BTSTACK_MEMORY_ALLOC_FAILED or ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE
 */
uint8_t mesh_configuration_client_send_default_ttl_set(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, uint8_t ttl);

#ifdef __cplusplus
} /* end of extern "C" */
#endif

#endif
