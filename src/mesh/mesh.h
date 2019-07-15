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

#ifndef __MESH_H
#define __MESH_H

#include "btstack_defines.h"
#include "mesh/provisioning.h"
#include "mesh/mesh_keys.h"
#include "mesh/mesh_access.h"

#if defined __cplusplus
extern "C" {
#endif

/**
 * Init Mesh network stack
 */
void mesh_init(void);

/**
 * Register for Mesh Provisioning Device events
 * @param packet_handler
 */
void mesh_register_provisioning_device_packet_handler(btstack_packet_handler_t packet_handler);

// Foundation state
void mesh_foundation_state_load(void);
void mesh_foundation_state_store(void);

// Mesh IV Index and sequence number
void mesh_store_iv_index_after_provisioning(uint32_t iv_index);
void mesh_store_iv_index_and_sequence_number(void);
int mesh_load_iv_index_and_sequence_number(uint32_t * iv_index, uint32_t * sequence_number);

// Mesh NetKey List
void mesh_store_network_key(mesh_network_key_t * network_key);
void mesh_delete_network_key(uint16_t internal_index);
void mesh_delete_network_keys(void);
void mesh_load_network_keys(void);

// Mesh Appkeys
void mesh_store_app_key(mesh_transport_key_t * app_key);
void mesh_delete_app_key(uint16_t internal_index);
void mesh_delete_app_keys(void);
void mesh_load_app_keys(void);

// Mesh Model to Appkey List
void mesh_load_appkey_lists(void);
void mesh_delete_appkey_lists(void);
void mesh_model_reset_appkeys(mesh_model_t * mesh_model);
uint8_t mesh_model_bind_appkey(mesh_model_t * mesh_model, uint16_t appkey_index);
void mesh_model_unbind_appkey(mesh_model_t * mesh_model, uint16_t appkey_index);
int mesh_model_contains_appkey(mesh_model_t * mesh_model, uint16_t appkey_index);

// Mesh Model Subscriptions
int mesh_model_contains_subscription(mesh_model_t * mesh_model, uint16_t address);

// temp
void mesh_access_setup_from_provisioning_data(const mesh_provisioning_data_t * provisioning_data);
void mesh_access_setup_without_provisiong_data(void);
void mesh_access_key_refresh_revoke_keys(mesh_subnet_t * subnet);
void mesh_access_netkey_finalize(mesh_network_key_t * network_key);
void mesh_access_appkey_finalize(mesh_transport_key_t * transport_key);

#if defined __cplusplus
}
#endif

#endif //__MESH_H
