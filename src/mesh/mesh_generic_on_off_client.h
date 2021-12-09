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

#ifndef __MESH_GENERIC_ON_OFF_CLIENT_H
#define __MESH_GENERIC_ON_OFF_CLIENT_H

#include <stdint.h>

#include "mesh/mesh_access.h"

#ifdef __cplusplus
extern "C"
{
#endif

const mesh_operation_t * mesh_generic_on_off_client_get_operations(void);
/**
 * @brief Register packet handler
 * @param generic_on_off_client_model
 * @param events_packet_handler
 */
void mesh_generic_on_off_client_register_packet_handler(mesh_model_t *mesh_model, btstack_packet_handler_t events_packet_handler);

/**
 * @brief Set ON/OFF value acknowledged
 * @param mesh_model
 * @param dest
 * @param netkey_index
 * @param appkey_index
 * @param on_off_value
 * @param transition_time_gdtt
 * @param delay_time_gdtt
 * @param transaction_id
 * @return status    0 if successful 
 */
 uint8_t mesh_generic_on_off_client_set(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, 
    uint8_t on_off_value, uint8_t transition_time_gdtt, uint8_t delay_time_gdtt, uint8_t transaction_id);

/**
 * @brief Set ON/OFF value unacknowledged
 * @param  mesh_model
 * @param dest
 * @param netkey_index
 * @param appkey_index
 * @param on_off_value
 * @param transition_time_gdtt
 * @param delay_time_gdtt
 * @param transaction_id
 * @return status    0 if successful 
 */
uint8_t mesh_generic_on_off_client_set_unacknowledged(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, 
    uint8_t on_off_value, uint8_t transition_time_gdtt, uint8_t delay_time_gdtt, uint8_t transaction_id);

/**
 * @brief  Get present ON/OFF value
 * @param  mesh_model
 * @param dest
 * @param netkey_index
 * @param appkey_index
 * @return status    0 if successful 
 */
uint8_t mesh_generic_on_off_client_get(mesh_model_t *mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index);

/**
 * @brief Publish on_off value by sending an unacknowledged set message to the publish destination
 * @param mesh_model
 * @param on_off_value
 * @param transaction_id
 * @return status    0 if successful 
 */
uint8_t mesh_generic_on_off_client_publish(mesh_model_t * mesh_model, uint8_t on_off_value, uint8_t transaction_id);

#ifdef __cplusplus
} /* end of extern "C" */
#endif

#endif
