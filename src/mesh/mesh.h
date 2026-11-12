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

/**
 * @title Bluetooth Mesh
 *
 * Initialize and manage a Bluetooth Mesh node and its persistent state.
 */

#ifndef MESH_H
#define MESH_H

#include "btstack_defines.h"
#include "mesh/provisioning.h"
#include "mesh/mesh_keys.h"
#include "mesh/mesh_access.h"
#include "mesh/mesh_virtual_addresses.h"

#if defined __cplusplus
extern "C" {
#endif

/* API_START */

/**
 * Mesh Network Stack
 *
 * @TLV 'MA' x x
 * @TLV 'MB' x x
 * @TLV 'MFIS
 * @TLV 'MFND
 * @TLV 'MN' x x
 * @TLV 'MP' x x
 * @TLV 'MS' x x
 * @TLV 'MV' x x
 * @TLV 'PROV'
 */

/**
 * Init Mesh network stack
 */
void mesh_init(void);

/**
 * Register for Mesh Provisioning Device events
 * @param packet_handler
 */
void mesh_register_provisioning_device_packet_handler(btstack_packet_handler_t packet_handler);

// Mesh Virtual Address Management
/**
 * @brief Load virtual addresses from persistent storage.
 */
void mesh_load_virtual_addresses(void);
/**
 * @brief Delete stored virtual addresses.
 */
void mesh_delete_virtual_addresses(void);
/**
 * @brief Decrease the reference count of a virtual address.
 */
void mesh_virtual_address_decrease_refcount(mesh_virtual_address_t * virtual_address);
/**
 * @brief Increase the reference count of a virtual address.
 */
void mesh_virtual_address_increase_refcount(mesh_virtual_address_t * virtual_address);

// Foundation state
/**
 * @brief Load foundation model state from persistent storage.
 */
void mesh_foundation_state_load(void);
/**
 * @brief Store foundation model state in persistent storage.
 */
void mesh_foundation_state_store(void);

// Mesh Model Subscriptions
/**
 * @brief Load model subscriptions from persistent storage.
 */
void mesh_load_subscriptions(void);
/**
 * @brief Store a model's subscriptions in persistent storage.
 */
void mesh_model_store_subscriptions(mesh_model_t * model);
/**
 * @brief Delete stored model subscriptions.
 */
void mesh_delete_subscriptions(void);

// Mesh Model Publication
/**
 * @brief Load model publications from persistent storage.
 */
void mesh_load_publications(void);
/**
 * @brief Delete stored model publications.
 */
void mesh_delete_publications(void);
/**
 * @brief Store a model's publication configuration.
 */
void mesh_model_store_publication(mesh_model_t * mesh_model);

// Mesh NetKey List
/**
 * @brief Store a network key in persistent storage.
 */
void mesh_store_network_key(mesh_network_key_t * network_key);
/**
 * @brief Delete a stored network key.
 */
void mesh_delete_network_key(uint16_t internal_index);
/**
 * @brief Delete stored network keys.
 */
void mesh_delete_network_keys(void);
/**
 * @brief Load network keys from persistent storage.
 */
void mesh_load_network_keys(void);

// Mesh Appkeys
/**
 * @brief Store an application key in persistent storage.
 */
void mesh_store_app_key(mesh_transport_key_t * app_key);
/**
 * @brief Delete a stored application key.
 */
void mesh_delete_app_key(uint16_t internal_index);
/**
 * @brief Delete stored application keys.
 */
void mesh_delete_app_keys(void);
/**
 * @brief Load application keys from persistent storage.
 */
void mesh_load_app_keys(void);

// Mesh Model to Appkey List
/**
 * @brief Load model application key bindings from persistent storage.
 */
void mesh_load_appkey_lists(void);
/**
 * @brief Delete stored model application key bindings.
 */
void mesh_delete_appkey_lists(void);
/**
 * @brief Bind an application key to a model.
 */
uint8_t mesh_model_bind_appkey(mesh_model_t * mesh_model, uint16_t appkey_index);
/**
 * @brief Remove an application key binding from a model.
 */
void mesh_model_unbind_appkey(mesh_model_t * mesh_model, uint16_t appkey_index);
/**
 * @brief Check whether an application key is bound to a model.
 */
int mesh_model_contains_appkey(mesh_model_t * mesh_model, uint16_t appkey_index);

// Mesh Node Reset
/**
 * @brief Reset the mesh node and its configuration.
 */
void mesh_node_reset(void);

// Attention Timer
/**
 * @brief Set the node's attention timer.
 */
void    mesh_attention_timer_set(uint8_t timer_s);
/**
 * @brief Get the node's attention timer.
 */
uint8_t mesh_attention_timer_get(void);

// temp
/**
 * @brief Revoke old keys during key refresh.
 */
void mesh_access_key_refresh_revoke_keys(mesh_subnet_t * subnet);
/**
 * @brief Remove a network key and its stored state.
 */
void mesh_access_netkey_finalize(mesh_network_key_t * network_key);
/**
 * @brief Remove an application key and its stored state.
 */
void mesh_access_appkey_finalize(mesh_transport_key_t * transport_key);

/* API_END */

#if defined __cplusplus
}
#endif

#endif //MESH_H
