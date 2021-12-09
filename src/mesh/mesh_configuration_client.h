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

#ifndef __MESH_CONFIGURATION_CLIENT_H
#define __MESH_CONFIGURATION_CLIENT_H

#include <stdint.h>

#include "mesh/mesh_access.h"

#ifdef __cplusplus
extern "C"
{
#endif


typedef struct {
    const uint8_t * models;
    uint16_t size;
    uint16_t offset;

    uint32_t id;
} mesh_model_id_iterator_t;

typedef struct {
    const uint8_t * elements;
    uint16_t size;
    uint16_t offset;

    uint16_t loc;

    mesh_model_id_iterator_t sig_model_iterator;
    mesh_model_id_iterator_t vendor_model_iterator;
} mesh_composite_data_iterator_t;

typedef struct {
    uint16_t publish_address_unicast;       
    uint8_t  publish_address_virtual[16];
    uint16_t appkey_index;
    uint8_t  credential_flag;
    uint8_t  publish_ttl;
    uint8_t  publish_period;
    uint8_t  publish_retransmit_count;
    uint8_t  publish_retransmit_interval_steps;
} mesh_publication_model_config_t;

const mesh_operation_t * mesh_configuration_client_get_operations(void);

/**
 * @brief Initialize iterator for element descriptions list from Composition data in MESH_SUBEVENT_CONFIGURATION_COMPOSITION_DATA event
 * @param iterator
 * @param elements
 * @param size
 */
void mesh_composition_data_iterator_init(mesh_composite_data_iterator_t * iterator, const uint8_t * elements, uint16_t size);

/**
 * @brief Check if there is another element description in the list
 * @param iterator
 * @return has_next_element  
 */
bool mesh_composition_data_iterator_has_next_element(mesh_composite_data_iterator_t * iterator);

/**
 * @brief Select the next element 
 * @param iterator
 */
void mesh_composition_data_iterator_next_element(mesh_composite_data_iterator_t * iterator);

/**
 * @brief Get the element location descriptor for the current element
 * @param iterator
 * @return loc 
 */
uint16_t mesh_composition_data_iterator_element_loc(mesh_composite_data_iterator_t * iterator);

/**
 * @brief Check if there is another SIG model in current element
 * @param iterator
 * @return has_next_sig_model  
 */
bool mesh_composition_data_iterator_has_next_sig_model(mesh_composite_data_iterator_t * iterator);

/**
 * @brief Select the next SIG model
 * @param iterator
 */
void mesh_composition_data_iterator_next_sig_model(mesh_composite_data_iterator_t * iterator);

/**
 * @brief Get the SIG model ID for the current SIG model of the current element
 * @param iterator
 * @return loc 
 */
uint16_t mesh_composition_data_iterator_sig_model_id(mesh_composite_data_iterator_t * iterator);


/**
 * @brief Check if there is another vendor model in current element
 * @param iterator
 * @return has_next_vendor_model  
 */
bool mesh_composition_data_iterator_has_next_vendor_model(mesh_composite_data_iterator_t * iterator);

/**
 * @brief Select the next VVendor model
 * @param iterator
 */
void mesh_composition_data_iterator_next_vendor_model(mesh_composite_data_iterator_t * iterator);

/**
 * @brief Get the Vendor model ID for the current vendor model of the current element
 * @param iterator
 * @return loc 
 */
uint32_t mesh_composition_data_iterator_vendor_model_id(mesh_composite_data_iterator_t * iterator);

/**
 * @brief Get field page from event MESH_SUBEVENT_CONFIGURATION_COMPOSITION_DATA
 * @param event packet
 * @return page
 * @note: btstack_type 1
 */
uint8_t mesh_subevent_configuration_composition_data_get_page(const uint8_t * event);

/**
 * @brief Get field cid from event MESH_SUBEVENT_CONFIGURATION_COMPOSITION_DATA
 * @param event packet
 * @return cid
 * @note: btstack_type 2
 */
uint16_t mesh_subevent_configuration_composition_data_get_cid(const uint8_t * event);

/**
 * @brief Get field pid from event MESH_SUBEVENT_CONFIGURATION_COMPOSITION_DATA
 * @param event packet
 * @return pid
 * @note: btstack_type 2
 */
uint16_t mesh_subevent_configuration_composition_data_get_pid(const uint8_t * event);

/**
 * @brief Get field vid from event MESH_SUBEVENT_CONFIGURATION_COMPOSITION_DATA
 * @param event packet
 * @return vid
 * @note: btstack_type 2
 */
uint16_t mesh_subevent_configuration_composition_data_get_vid(const uint8_t * event);

/**
 * @brief Get field crpl from event MESH_SUBEVENT_CONFIGURATION_COMPOSITION_DATA
 * @param event packet
 * @return crpl
 * @note: btstack_type 2
 */
uint16_t mesh_subevent_configuration_composition_data_get_crpl(const uint8_t * event);

/**
 * @brief Get field features from event MESH_SUBEVENT_CONFIGURATION_COMPOSITION_DATA
 * @param event packet
 * @return features
 * @note: btstack_type 2
 */
uint16_t mesh_subevent_configuration_composition_data_get_features(const uint8_t * event);

/**
 * @brief Get number elements from event MESH_SUBEVENT_CONFIGURATION_COMPOSITION_DATA
 **/
uint16_t mesh_subevent_configuration_composition_data_get_num_elements(const uint8_t * event, uint16_t size);


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

/**
 * @brief Get the current Default GATT proxy state of a node
 * @param mesh_model
 * @param dest
 * @param netkey_index
 * @param appkey_index
 * @return status       ERROR_CODE_SUCCESS if successful, otherwise BTSTACK_MEMORY_ALLOC_FAILED or ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE
 */
uint8_t mesh_configuration_client_send_gatt_proxy_get(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index);

/**
 * @brief Set Default GATT proxy state of a node
 * @param mesh_model
 * @param dest
 * @param netkey_index
 * @param appkey_index
 * @param gatt_proxy_state        0 - the proxy feature is supported and disabled, 1 - supported and enabled, 2 - not supported
 * @return status       ERROR_CODE_SUCCESS if successful, otherwise BTSTACK_MEMORY_ALLOC_FAILED or ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE
 */
uint8_t mesh_configuration_client_send_gatt_proxy_set(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, uint8_t gatt_proxy_state);


/**
 * @brief Get the current Relay and Relay Retransmit states of a node
 * @param mesh_model
 * @param dest
 * @param netkey_index
 * @param appkey_index
 * @return status       ERROR_CODE_SUCCESS if successful, otherwise BTSTACK_MEMORY_ALLOC_FAILED or ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE
 */
uint8_t mesh_configuration_client_send_relay_get(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index);

/**
 * @brief Set the current Relay and Relay Retransmit states of a node
 * @param mesh_model
 * @param dest
 * @param netkey_index
 * @param appkey_index
 * @param relay
 * @param relay_retransmit_count
 * @param relay_retransmit_interval_steps
 * @return status       ERROR_CODE_SUCCESS if successful, otherwise BTSTACK_MEMORY_ALLOC_FAILED or ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE
 */
uint8_t mesh_configuration_client_send_relay_set(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, uint8_t relay, uint8_t relay_retransmit_count, uint8_t relay_retransmit_interval_steps);

/**
 * @brief Get the publish address and parameters of an outgoing message that originates from a model
 * @param mesh_model
 * @param dest         element_address
 * @param netkey_index
 * @param appkey_index
 * @param model_id
 * @return status       ERROR_CODE_SUCCESS if successful, otherwise BTSTACK_MEMORY_ALLOC_FAILED or ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE
 */
uint8_t mesh_configuration_client_send_model_publication_get(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, uint32_t model_id);

/**
 * @brief Set the Model Publication state of an outgoing message that originates from a model.
 * @param mesh_model
 * @param dest         element_address
 * @param netkey_index
 * @param appkey_index
 * @param model_id
 * @param publication_config  
 * @return status       ERROR_CODE_SUCCESS if successful, otherwise BTSTACK_MEMORY_ALLOC_FAILED or ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE
 */
uint8_t mesh_configuration_client_send_model_publication_set(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, uint32_t model_id, mesh_publication_model_config_t * publication_config);

/**
 * @brief Set the Model Publication state of an outgoing message that originates from a model.
 * @param mesh_model
 * @param dest         element_address
 * @param netkey_index
 * @param appkey_index
 * @param model_id
 * @param publication_config  
 * @return status       ERROR_CODE_SUCCESS if successful, otherwise BTSTACK_MEMORY_ALLOC_FAILED or ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE
 */
uint8_t mesh_configuration_client_send_model_publication_virtual_address_set(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, uint32_t model_id, mesh_publication_model_config_t * publication_config);

/**
 * @brief Add an address to a Subscription List of a model
 * @param mesh_model
 * @param dest         element_address
 * @param netkey_index
 * @param appkey_index
 * @param address
 * @param model_id
 * @return status       ERROR_CODE_SUCCESS if successful, otherwise BTSTACK_MEMORY_ALLOC_FAILED or ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE
 */
uint8_t mesh_configuration_client_send_model_subscription_add(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, uint16_t address, uint32_t model_id);
uint8_t mesh_configuration_client_send_model_subscription_virtual_address_add(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, uint8_t * address, uint32_t model_id);

/**
 * @brief Delete an address from a Subscription List of a model
 * @param mesh_model
 * @param dest         element_address
 * @param netkey_index
 * @param appkey_index
 * @param address
 * @param model_id
 * @return status       ERROR_CODE_SUCCESS if successful, otherwise BTSTACK_MEMORY_ALLOC_FAILED or ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE
 */
uint8_t mesh_configuration_client_send_model_subscription_delete(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, uint16_t address, uint32_t model_id);
uint8_t mesh_configuration_client_send_model_subscription_virtual_address_delete(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, uint8_t * address, uint32_t model_id);

/**
 * @brief Discard the Subscription List and add an address to the cleared Subscription List of a model 
 * @param mesh_model
 * @param dest         element_address
 * @param netkey_index
 * @param appkey_index
 * @param address
 * @param model_id
 * @return status       ERROR_CODE_SUCCESS if successful, otherwise BTSTACK_MEMORY_ALLOC_FAILED or ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE
 */
uint8_t mesh_configuration_client_send_model_subscription_overwrite(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, uint16_t address, uint32_t model_id);
uint8_t mesh_configuration_client_send_model_subscription_virtual_address_overwrite(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, uint8_t * address, uint32_t model_id);

/**
 * @brief Discard the Subscription List of a model
 * @param mesh_model
 * @param dest         element_address
 * @param netkey_index
 * @param appkey_index
 * @param address
 * @param model_id
 * @return status       ERROR_CODE_SUCCESS if successful, otherwise BTSTACK_MEMORY_ALLOC_FAILED or ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE
 */
uint8_t mesh_configuration_client_send_model_subscription_delete_all(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, uint16_t address, uint32_t model_id);

/**
 * @brief Get the list of subscription addresses of a SIG or vendor model within the element
 * @param mesh_model
 * @param dest         element_address
 * @param netkey_index
 * @param appkey_index
 * @param vendor_model_id
 * @return status       ERROR_CODE_SUCCESS if successful, otherwise BTSTACK_MEMORY_ALLOC_FAILED or ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE
 */
uint8_t mesh_configuration_client_send_model_subscription_get(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, uint32_t model_id);


/**
 * @brief Add a NetKey to a NetKey List on a node.
 * @param mesh_model
 * @param dest         element_address
 * @param netkey_index
 * @param appkey_index
 * @param index
 * @param netkey
 * @return status       ERROR_CODE_SUCCESS if successful, otherwise BTSTACK_MEMORY_ALLOC_FAILED or ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE
 */
uint8_t mesh_configuration_client_send_netkey_add(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, uint16_t index, uint8_t * netkey);

/**
 * @brief Update NetKey in a NetKey List on a node.
 * @param mesh_model
 * @param dest         element_address
 * @param netkey_index
 * @param appkey_index
 * @param index
 * @param netkey
 * @return status       ERROR_CODE_SUCCESS if successful, otherwise BTSTACK_MEMORY_ALLOC_FAILED or ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE
 */
uint8_t mesh_configuration_client_send_netkey_update(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, uint16_t index, uint8_t * netkey);

/**
 * @brief Delete a NetKey from a NetKey List on a node.
 * @param mesh_model
 * @param dest         element_address
 * @param netkey_index
 * @param appkey_index
 * @param index
 * @return status       ERROR_CODE_SUCCESS if successful, otherwise BTSTACK_MEMORY_ALLOC_FAILED or ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE
 */
uint8_t mesh_configuration_client_send_netkey_delete(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, uint16_t index);

/**
 * @brief Get NetKey List on a node.
 * @param mesh_model
 * @param dest         element_address
 * @param netkey_index
 * @param appkey_index
 * @return status       ERROR_CODE_SUCCESS if successful, otherwise BTSTACK_MEMORY_ALLOC_FAILED or ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE
 */
uint8_t mesh_configuration_client_send_netkey_get(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index);

/**
 * @brief Add a AppKey to a AppKey List on a node.
 * @param mesh_model
 * @param dest         element_address
 * @param netkey_index
 * @param appkey_index
 * @param netk_index
 * @param appk_index
 * @param appkey
 * @return status       ERROR_CODE_SUCCESS if successful, otherwise BTSTACK_MEMORY_ALLOC_FAILED or ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE
 */
uint8_t mesh_configuration_client_send_appkey_add(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, uint16_t netk_index, uint16_t appk_index, uint8_t * appkey);

/**
 * @brief Update AppKey in a AppKey List on a node.
 * @param mesh_model
 * @param dest         element_address
 * @param netkey_index
 * @param appkey_index
 * @param netk_index
 * @param appk_index
 * @param appkey
 * @return status       ERROR_CODE_SUCCESS if successful, otherwise BTSTACK_MEMORY_ALLOC_FAILED or ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE
 */
uint8_t mesh_configuration_client_send_appkey_update(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, uint16_t netk_index, uint16_t appk_index, uint8_t * appkey);

/**
 * @brief Delete a AppKey from a AppKey List on a node.
 * @param mesh_model
 * @param dest         element_address
 * @param netkey_index
 * @param appkey_index
 * @param netk_index
 * @param appk_index
 * @param index
 * @return status       ERROR_CODE_SUCCESS if successful, otherwise BTSTACK_MEMORY_ALLOC_FAILED or ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE
 */
uint8_t mesh_configuration_client_send_appkey_delete(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, uint16_t netk_index, uint16_t appk_index);

/**
 * @brief Get AppKey List on a node that is bound to the NetKey.
 * @param mesh_model
 * @param dest         element_address
 * @param netkey_index
 * @param appkey_index
 * @param netk_index
 * @return status       ERROR_CODE_SUCCESS if successful, otherwise BTSTACK_MEMORY_ALLOC_FAILED or ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE
 */
uint8_t mesh_configuration_client_send_appkey_get(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, uint16_t netk_index);

/**
 * @brief Get the current Node Identity state for a subnet.
 * @param mesh_model
 * @param dest         element_address
 * @param netkey_index
 * @param appkey_index
 * @param netk_index
 * @return status       ERROR_CODE_SUCCESS if successful, otherwise BTSTACK_MEMORY_ALLOC_FAILED or ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE
 */
uint8_t mesh_configuration_client_send_node_identity_get(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, uint16_t netk_index);

/**
 * @brief Set the current Node Identity state for a subnet.
 * @param mesh_model
 * @param dest         element_address
 * @param netkey_index
 * @param appkey_index
 * @param netk_index
 * @param node_identity_state
 * @return status       ERROR_CODE_SUCCESS if successful, otherwise BTSTACK_MEMORY_ALLOC_FAILED or ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE
 */
uint8_t mesh_configuration_client_send_node_identity_set(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, uint16_t netk_index, mesh_node_identity_state_t node_identity_state); 

/**
 * @brief Bind an AppKey to a model.
 * @param mesh_model
 * @param dest         element_address
 * @param netkey_index
 * @param appkey_index
 * @param appk_index
 * @param model_identifier
 * @return status       ERROR_CODE_SUCCESS if successful, otherwise BTSTACK_MEMORY_ALLOC_FAILED or ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE
 */
uint8_t mesh_configuration_client_send_model_app_bind_get(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, uint16_t appk_index, uint32_t model_identifier);

/**
 * @brief Remove the binding between an AppKey and a model.
 * @param mesh_model
 * @param dest         element_address
 * @param netkey_index
 * @param appkey_index
 * @param appk_index
 * @param model_identifier
 * @return status       ERROR_CODE_SUCCESS if successful, otherwise BTSTACK_MEMORY_ALLOC_FAILED or ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE
 */
uint8_t mesh_configuration_client_send_model_app_unbind_set(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, uint16_t appk_index, uint32_t model_identifier); 

/**
 * @brief Report of all AppKeys bound to the Model.
 * @param mesh_model
 * @param dest         element_address
 * @param netkey_index
 * @param appkey_index
 * @param model_identifier
 * @return status       ERROR_CODE_SUCCESS if successful, otherwise BTSTACK_MEMORY_ALLOC_FAILED or ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE
 */
uint8_t mesh_configuration_client_send_model_app_get(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, uint32_t model_identifier); 

/**
 * @brief Reset a node (other than a Provisioner) and remove it from the network.
 * @param mesh_model
 * @param dest         element_address
 * @param netkey_index
 * @param appkey_index
 * @return status       ERROR_CODE_SUCCESS if successful, otherwise BTSTACK_MEMORY_ALLOC_FAILED or ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE
 */
uint8_t mesh_configuration_client_send_node_reset(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index);

/**
 * @brief Get the current Friend state of a node.
 * @param mesh_model
 * @param dest         element_address
 * @param netkey_index
 * @param appkey_index
 * @return status       ERROR_CODE_SUCCESS if successful, otherwise BTSTACK_MEMORY_ALLOC_FAILED or ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE
 */
uint8_t mesh_configuration_client_send_friend_get(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index);

/**
 * @brief Set the current Friend state of a node.
 * @param mesh_model
 * @param dest         element_address
 * @param netkey_index
 * @param appkey_index
 * @param friend_state
 * @return status       ERROR_CODE_SUCCESS if successful, otherwise BTSTACK_MEMORY_ALLOC_FAILED or ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE
 */
uint8_t mesh_configuration_client_send_friend_set(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, mesh_friend_state_t friend_state);

/**
 * @brief Get the current Key Refresh Phase state of the identified network key
 * @param mesh_model
 * @param dest         element_address
 * @param netkey_index
 * @param appkey_index
 * @param netk_index
 * @return status       ERROR_CODE_SUCCESS if successful, otherwise BTSTACK_MEMORY_ALLOC_FAILED or ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE
 */
uint8_t mesh_configuration_client_send_key_refresh_phase_get(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, uint16_t netk_index);

/**
 * @brief Set the current Key Refresh Phase state of the identified network key
 * @param mesh_model
 * @param dest         element_address
 * @param netkey_index
 * @param appkey_index
 * @param netk_index
 * @return status       ERROR_CODE_SUCCESS if successful, otherwise BTSTACK_MEMORY_ALLOC_FAILED or ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE
 */
uint8_t mesh_configuration_client_send_key_refresh_phase_set(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, uint16_t netk_index, uint8_t transition);

/**
 * @brief Get the current Heartbeat Publication state of an element.
 * @param mesh_model
 * @param dest         element_address
 * @param netkey_index
 * @param appkey_index
 * @return status       ERROR_CODE_SUCCESS if successful, otherwise BTSTACK_MEMORY_ALLOC_FAILED or ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE
 */
uint8_t mesh_configuration_client_send_heartbeat_publication_get(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index);

/**
 * @brief Set the current Heartbeat Publication state of an element.
 * @param mesh_model
 * @param dest         element_address
 * @param netkey_index
 * @param appkey_index
 * @param publication_state
 * @return status       ERROR_CODE_SUCCESS if successful, otherwise BTSTACK_MEMORY_ALLOC_FAILED or ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE
 */
uint8_t mesh_configuration_client_send_heartbeat_publication_set(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, mesh_heartbeat_publication_state_t publication_state);

/**
 * @brief Get the current Heartbeat Subscription state of an element.
 * @param mesh_model
 * @param dest         element_address
 * @param netkey_index
 * @param appkey_index
 * @return status       ERROR_CODE_SUCCESS if successful, otherwise BTSTACK_MEMORY_ALLOC_FAILED or ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE
 */
uint8_t mesh_configuration_client_send_heartbeat_subscription_get(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index);

/**
 * @brief Set the current Heartbeat Subscription state of an element.
 * @param mesh_model
 * @param dest         element_address
 * @param netkey_index
 * @param appkey_index
 * @param heartbeat_source
 * @param heartbeat_destination
 * @param period_s
 * @return status       ERROR_CODE_SUCCESS if successful, otherwise BTSTACK_MEMORY_ALLOC_FAILED or ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE
 */
uint8_t mesh_configuration_client_send_heartbeat_subscription_set(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, uint16_t heartbeat_source, uint16_t heartbeat_destination, uint16_t period_s);

/**
 * @brief Get the current value of PollTimeout timer of the Low Power node within a Friend node.
 * @param mesh_model
 * @param dest         element_address
 * @param netkey_index
 * @param appkey_index
 * @return status       ERROR_CODE_SUCCESS if successful, otherwise BTSTACK_MEMORY_ALLOC_FAILED or ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE
 */
uint8_t mesh_configuration_client_send_low_power_node_poll_timeout_get(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index);

/**
 * @brief Get the current Network Transmit state of a node.
 * @param mesh_model
 * @param dest         element_address
 * @param netkey_index
 * @param appkey_index
 * @return status       ERROR_CODE_SUCCESS if successful, otherwise BTSTACK_MEMORY_ALLOC_FAILED or ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE
 */
uint8_t mesh_configuration_client_send_network_transmit_get(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index);

/**
 * @brief Set the current Network Transmit state of a node.
 * @param mesh_model
 * @param dest         element_address
 * @param netkey_index
 * @param appkey_index
 * @param transmit_count
 * @param transmit_interval_steps_ms
 * @return status       ERROR_CODE_SUCCESS if successful, otherwise BTSTACK_MEMORY_ALLOC_FAILED or ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE
 */
uint8_t mesh_configuration_client_send_network_transmit_set(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, uint8_t transmit_count, uint16_t transmit_interval_steps_ms);


#ifdef __cplusplus
} /* end of extern "C" */
#endif

#endif
