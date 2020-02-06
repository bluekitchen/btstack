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

#ifndef __MESH_GENERIC_ON_OFF_SERVER_H
#define __MESH_GENERIC_ON_OFF_SERVER_H

#include <stdint.h>

#include "mesh/mesh_access.h"

#ifdef __cplusplus
extern "C"
{
#endif

const mesh_operation_t * mesh_generic_on_off_server_get_operations(void);
/**
 * @brief Register packet handler
 * @param generic_on_off_server_model
 * @param transition_events_packet_handler
 */
void mesh_generic_on_off_server_register_packet_handler(mesh_model_t *generic_on_off_server_model, btstack_packet_handler_t transition_events_packet_handler);

/**
 * @brief Set publication model
 * @param generic_on_off_server_model
 * @param publication_model
 */
void mesh_generic_on_off_server_set_publication_model(mesh_model_t *generic_on_off_server_model, mesh_publication_model_t * publication_model);

/**
 * @brief Set ON/OFF value
 * @param generic_on_off_server_model
 * @param on_off_value
 * @param transition_time_gdtt
 * @param delay_time_gdtt
 */
void mesh_generic_on_off_server_set(mesh_model_t *generic_on_off_server_model, uint8_t on_off_value, uint8_t transition_time_gdtt, uint8_t delay_time_gdtt);

/**
 * @brief  Get present ON/OFF value
 * @param  generic_on_off_server_model
 * @return on_off_value
 */
uint8_t mesh_generic_on_off_server_get(mesh_model_t *generic_on_off_server_model);


#ifdef __cplusplus
} /* end of extern "C" */
#endif

#endif
