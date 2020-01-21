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

#define BTSTACK_FILE__ "mesh_configuration_client.c"

#include <string.h>
#include <stdio.h>

#include "mesh/mesh_configuration_client.h"

#include "bluetooth_company_id.h"
#include "btstack_debug.h"
#include "btstack_memory.h"
#include "btstack_util.h"

#include "mesh/mesh_access.h"
#include "mesh/mesh_foundation.h"
#include "mesh/mesh_generic_model.h"
#include "mesh/mesh_keys.h"
#include "mesh/mesh_network.h"
#include "mesh/mesh_upper_transport.h"


// Mesh Composition Data Element iterator
#define MESH_VENDOR_MODEL_SIZE                                  4
#define MESH_SIG_MODEL_SIZE                                     2
#define MESH_COMPOSITION_DATA_ELEMENT_DESCRIPTION_OFFSET       17
#define MESH_COMPOSITION_DATA_ELEMENT_LOCATION_DESCRIPTOR_LEN   2
#define MESH_COMPOSITION_DATA_ELEMENT_MODEL_SIZE_LEN            1

static inline uint16_t mesh_composition_data_iterator_sig_model_list_size(mesh_composite_data_iterator_t * it){
    return it->elements[it->offset + 2] * MESH_SIG_MODEL_SIZE;
}

static inline uint16_t mesh_composition_data_iterator_vendor_model_list_size(mesh_composite_data_iterator_t * it){
    return it->elements[it->offset + 3] * MESH_VENDOR_MODEL_SIZE;
}

static inline uint16_t mesh_composition_data_iterator_element_len(mesh_composite_data_iterator_t * it){
    uint16_t sig_model_list_size    = mesh_composition_data_iterator_sig_model_list_size(it);
    uint16_t vendor_model_list_size = mesh_composition_data_iterator_vendor_model_list_size(it);
    uint16_t previous_fields_len = MESH_COMPOSITION_DATA_ELEMENT_LOCATION_DESCRIPTOR_LEN + 2 * MESH_COMPOSITION_DATA_ELEMENT_MODEL_SIZE_LEN;

    return previous_fields_len + sig_model_list_size + vendor_model_list_size;;
}

uint16_t mesh_subevent_configuration_composition_data_get_num_elements(const uint8_t * event, uint16_t size){
    uint16_t pos = MESH_COMPOSITION_DATA_ELEMENT_DESCRIPTION_OFFSET; 
    uint16_t num_elements = 0;

    while ((pos + 4) <= size){
        // location descriptor
        pos += 2;
        uint8_t num_sig_model_ids = event[pos++];
        uint8_t num_vendor_model_ids = event[pos++];
        pos += (num_sig_model_ids + num_vendor_model_ids) * 2;
        num_elements++;
    }
    return num_elements;
}

void mesh_composition_data_iterator_init(mesh_composite_data_iterator_t * it, const uint8_t * elements, uint16_t size){
    it->elements = elements;
    it->size = size;
    it->offset = MESH_COMPOSITION_DATA_ELEMENT_DESCRIPTION_OFFSET;
}

bool mesh_composition_data_iterator_has_next_element(mesh_composite_data_iterator_t * it){
    return (it->offset + mesh_composition_data_iterator_element_len(it)) <= it->size;
}

void mesh_composition_data_iterator_next_element(mesh_composite_data_iterator_t * it){
    it->sig_model_iterator.models = &it->elements[it->offset + 4];
    it->sig_model_iterator.size = mesh_composition_data_iterator_sig_model_list_size(it);
    it->sig_model_iterator.offset = 0;
    
    it->vendor_model_iterator.models = &it->elements[it->offset + 4 + it->sig_model_iterator.size];
    it->vendor_model_iterator.size = mesh_composition_data_iterator_vendor_model_list_size(it);
    it->vendor_model_iterator.offset = 0;

    it->loc = little_endian_read_16(it->elements, it->offset);
    it->offset += mesh_composition_data_iterator_element_len(it);
}

uint16_t mesh_composition_data_iterator_element_loc(mesh_composite_data_iterator_t * it){
    return it->loc;
}

bool mesh_composition_data_iterator_has_next_sig_model(mesh_composite_data_iterator_t * it){
    return (it->sig_model_iterator.offset + MESH_SIG_MODEL_SIZE) <= it->sig_model_iterator.size;
}

void mesh_composition_data_iterator_next_sig_model(mesh_composite_data_iterator_t * it){
    it->sig_model_iterator.id = little_endian_read_16(it->sig_model_iterator.models, it->sig_model_iterator.offset);
    it->sig_model_iterator.offset += 2;
}

uint16_t mesh_composition_data_iterator_sig_model_id(mesh_composite_data_iterator_t * it){
    return (uint16_t)it->sig_model_iterator.id;
}

bool mesh_composition_data_iterator_has_next_vendor_model(mesh_composite_data_iterator_t * it){
    return (it->vendor_model_iterator.offset + MESH_VENDOR_MODEL_SIZE) <= it->vendor_model_iterator.size;
}

void mesh_composition_data_iterator_next_vendor_model(mesh_composite_data_iterator_t * it){
    uint16_t vendor_id = little_endian_read_16(it->vendor_model_iterator.models, it->vendor_model_iterator.offset);
    it->vendor_model_iterator.offset += 2;
    uint16_t model_id = little_endian_read_16(it->vendor_model_iterator.models, it->vendor_model_iterator.offset);
    it->vendor_model_iterator.offset += 2;
    it->vendor_model_iterator.id = mesh_model_get_model_identifier(vendor_id, model_id);
}

uint32_t mesh_composition_data_iterator_vendor_model_id(mesh_composite_data_iterator_t * it){
    return it->vendor_model_iterator.id;
}

// Configuration client messages

static const mesh_access_message_t mesh_configuration_client_beacon_get = {
        MESH_FOUNDATION_OPERATION_BEACON_GET, ""
};
static const mesh_access_message_t mesh_configuration_client_beacon_set = {
        MESH_FOUNDATION_OPERATION_BEACON_SET, "1"
};


static const mesh_access_message_t mesh_configuration_client_composition_data_get = {
        MESH_FOUNDATION_OPERATION_COMPOSITION_DATA_GET, "1"
};


static const mesh_access_message_t mesh_configuration_client_default_ttl_get = {
        MESH_FOUNDATION_OPERATION_DEFAULT_TTL_GET, ""
};
static const mesh_access_message_t mesh_configuration_client_default_ttl_set = {
        MESH_FOUNDATION_OPERATION_DEFAULT_TTL_SET, "1"
};


static const mesh_access_message_t mesh_configuration_client_gatt_proxy_get = {
        MESH_FOUNDATION_OPERATION_GATT_PROXY_GET, ""
};
static const mesh_access_message_t mesh_configuration_client_gatt_proxy_set = {
        MESH_FOUNDATION_OPERATION_GATT_PROXY_SET, "1"
};


static const mesh_access_message_t mesh_configuration_client_relay_get = {
        MESH_FOUNDATION_OPERATION_RELAY_GET, ""
};
static const mesh_access_message_t mesh_configuration_client_relay_set = {
        MESH_FOUNDATION_OPERATION_RELAY_SET, "11"
};

static const mesh_access_message_t mesh_configuration_client_model_publication_get = {
        MESH_FOUNDATION_OPERATION_MODEL_PUBLICATION_GET, "2m"
};
static const mesh_access_message_t mesh_configuration_client_model_publication_set = {
        MESH_FOUNDATION_OPERATION_MODEL_PUBLICATION_SET, "222111m"
};
static const mesh_access_message_t mesh_configuration_client_model_publication_virtual_address_set = {
        MESH_FOUNDATION_OPERATION_MODEL_PUBLICATION_VIRTUAL_ADDRESS_SET, "2P2111m"
};


static const mesh_access_message_t mesh_configuration_client_model_subscription_add = {
        MESH_FOUNDATION_OPERATION_MODEL_SUBSCRIPTION_ADD, "22m"
};
static const mesh_access_message_t mesh_configuration_client_model_subscription_virtual_address_add = {
        MESH_FOUNDATION_OPERATION_MODEL_SUBSCRIPTION_VIRTUAL_ADDRESS_ADD, "2Pm"
};
static const mesh_access_message_t mesh_configuration_client_model_subscription_delete = {
        MESH_FOUNDATION_OPERATION_MODEL_SUBSCRIPTION_DELETE, "22m"
};
static const mesh_access_message_t mesh_configuration_client_model_subscription_virtual_address_delete = {
        MESH_FOUNDATION_OPERATION_MODEL_SUBSCRIPTION_VIRTUAL_ADDRESS_DELETE, "2Pm"
};
static const mesh_access_message_t mesh_configuration_client_model_subscription_overwrite = {
        MESH_FOUNDATION_OPERATION_MODEL_SUBSCRIPTION_OVERWRITE, "22m"
};
static const mesh_access_message_t mesh_configuration_client_model_subscription_virtual_address_overwrite = {
        MESH_FOUNDATION_OPERATION_MODEL_SUBSCRIPTION_VIRTUAL_ADDRESS_OVERWRITE, "2Pm"
};
static const mesh_access_message_t mesh_configuration_client_model_subscription_delete_all = {
        MESH_FOUNDATION_OPERATION_MODEL_SUBSCRIPTION_DELETE_ALL, "22m"
};


static const mesh_access_message_t mesh_configuration_client_sig_model_subscription_get = {
        MESH_FOUNDATION_OPERATION_SIG_MODEL_SUBSCRIPTION_GET, "22"
};

static const mesh_access_message_t mesh_configuration_client_vendor_model_subscription_get = {
        MESH_FOUNDATION_OPERATION_VENDOR_MODEL_SUBSCRIPTION_GET, "24"
};

static void mesh_configuration_client_send_acknowledged(uint16_t src, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, mesh_pdu_t *pdu, uint32_t ack_opcode){
    uint8_t  ttl  = mesh_foundation_default_ttl_get();
    mesh_upper_transport_setup_access_pdu_header(pdu, netkey_index, appkey_index, ttl, src, dest, 0);
    mesh_access_send_acknowledged_pdu(pdu, mesh_access_acknowledged_message_retransmissions(), ack_opcode);
}

static uint8_t mesh_access_validate_envelop_params(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index){
    btstack_assert(mesh_model != NULL);
    // TODO: validate other params
    UNUSED(mesh_model);
    UNUSED(dest);
    UNUSED(netkey_index);
    UNUSED(appkey_index);

    return ERROR_CODE_SUCCESS;
}

uint8_t mesh_configuration_client_send_beacon_get(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index){
    uint8_t status = mesh_access_validate_envelop_params(mesh_model, dest, netkey_index, appkey_index);
    if (status != ERROR_CODE_SUCCESS) return status;

    mesh_network_pdu_t * network_pdu = mesh_access_setup_unsegmented_message(&mesh_configuration_client_beacon_get);
    if (!network_pdu) return BTSTACK_MEMORY_ALLOC_FAILED;

    mesh_configuration_client_send_acknowledged(mesh_access_get_element_address(mesh_model), dest, netkey_index, appkey_index, (mesh_pdu_t *) network_pdu, MESH_FOUNDATION_OPERATION_BEACON_STATUS);
    return ERROR_CODE_SUCCESS;
}

uint8_t mesh_configuration_client_send_beacon_set(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, uint8_t beacon){
    uint8_t status = mesh_access_validate_envelop_params(mesh_model, dest, netkey_index, appkey_index);
    if (status != ERROR_CODE_SUCCESS) return status;

    if (beacon > 1) return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;

    mesh_network_pdu_t * network_pdu = mesh_access_setup_unsegmented_message(&mesh_configuration_client_beacon_set, beacon);
    if (!network_pdu) return BTSTACK_MEMORY_ALLOC_FAILED;

    mesh_configuration_client_send_acknowledged(mesh_access_get_element_address(mesh_model), dest, netkey_index, appkey_index, (mesh_pdu_t *) network_pdu, MESH_FOUNDATION_OPERATION_BEACON_STATUS);
    return ERROR_CODE_SUCCESS;
}

uint8_t mesh_configuration_client_send_composition_data_get(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, uint8_t page){
    uint8_t status = mesh_access_validate_envelop_params(mesh_model, dest, netkey_index, appkey_index);
    if (status != ERROR_CODE_SUCCESS) return status;

    mesh_network_pdu_t * network_pdu = mesh_access_setup_unsegmented_message(&mesh_configuration_client_composition_data_get, page);
    if (!network_pdu) return BTSTACK_MEMORY_ALLOC_FAILED;

    mesh_configuration_client_send_acknowledged(mesh_access_get_element_address(mesh_model), dest, netkey_index, appkey_index, (mesh_pdu_t *) network_pdu, MESH_FOUNDATION_OPERATION_COMPOSITION_DATA_GET);
    return ERROR_CODE_SUCCESS;
}

uint8_t mesh_configuration_client_send_default_ttl_get(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index){
    uint8_t status = mesh_access_validate_envelop_params(mesh_model, dest, netkey_index, appkey_index);
    if (status != ERROR_CODE_SUCCESS) return status;

    mesh_network_pdu_t * network_pdu = mesh_access_setup_unsegmented_message(&mesh_configuration_client_default_ttl_get);
    if (!network_pdu) return BTSTACK_MEMORY_ALLOC_FAILED;

    mesh_configuration_client_send_acknowledged(mesh_access_get_element_address(mesh_model), dest, netkey_index, appkey_index, (mesh_pdu_t *) network_pdu, MESH_FOUNDATION_OPERATION_DEFAULT_TTL_GET);
    return ERROR_CODE_SUCCESS;
}

uint8_t mesh_configuration_client_send_default_ttl_set(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, uint8_t ttl){
    uint8_t status = mesh_access_validate_envelop_params(mesh_model, dest, netkey_index, appkey_index);
    if (status != ERROR_CODE_SUCCESS) return status;

    if (ttl == 0x01 || ttl >= 0x80) return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;

    mesh_network_pdu_t * network_pdu = mesh_access_setup_unsegmented_message(&mesh_configuration_client_default_ttl_set, ttl);
    if (!network_pdu) return BTSTACK_MEMORY_ALLOC_FAILED;

    mesh_configuration_client_send_acknowledged(mesh_access_get_element_address(mesh_model), dest, netkey_index, appkey_index, (mesh_pdu_t *) network_pdu, MESH_FOUNDATION_OPERATION_DEFAULT_TTL_SET);
    return ERROR_CODE_SUCCESS;
}

uint8_t mesh_configuration_client_send_gatt_proxy_get(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index){
    uint8_t status = mesh_access_validate_envelop_params(mesh_model, dest, netkey_index, appkey_index);
    if (status != ERROR_CODE_SUCCESS) return status;

    mesh_network_pdu_t * network_pdu = mesh_access_setup_unsegmented_message(&mesh_configuration_client_gatt_proxy_get);
    if (!network_pdu) return BTSTACK_MEMORY_ALLOC_FAILED;

    mesh_configuration_client_send_acknowledged(mesh_access_get_element_address(mesh_model), dest, netkey_index, appkey_index, (mesh_pdu_t *) network_pdu, MESH_FOUNDATION_OPERATION_GATT_PROXY_GET);
    return ERROR_CODE_SUCCESS;
}

uint8_t mesh_configuration_client_send_gatt_proxy_set(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, uint8_t gatt_proxy_state){
    uint8_t status = mesh_access_validate_envelop_params(mesh_model, dest, netkey_index, appkey_index);
    if (status != ERROR_CODE_SUCCESS) return status;

    if (gatt_proxy_state > 2) return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;

    mesh_network_pdu_t * network_pdu = mesh_access_setup_unsegmented_message(&mesh_configuration_client_gatt_proxy_set, gatt_proxy_state);
    if (!network_pdu) return BTSTACK_MEMORY_ALLOC_FAILED;

    mesh_configuration_client_send_acknowledged(mesh_access_get_element_address(mesh_model), dest, netkey_index, appkey_index, (mesh_pdu_t *) network_pdu, MESH_FOUNDATION_OPERATION_GATT_PROXY_SET);
    return ERROR_CODE_SUCCESS;
}

uint8_t mesh_configuration_client_send_relay_get(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index){
    uint8_t status = mesh_access_validate_envelop_params(mesh_model, dest, netkey_index, appkey_index);
    if (status != ERROR_CODE_SUCCESS) return status;

    mesh_network_pdu_t * network_pdu = mesh_access_setup_unsegmented_message(&mesh_configuration_client_relay_get);
    if (!network_pdu) return BTSTACK_MEMORY_ALLOC_FAILED;

    mesh_configuration_client_send_acknowledged(mesh_access_get_element_address(mesh_model), dest, netkey_index, appkey_index, (mesh_pdu_t *) network_pdu, MESH_FOUNDATION_OPERATION_RELAY_GET);
    return ERROR_CODE_SUCCESS;
}

uint8_t mesh_configuration_client_send_relay_set(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, uint8_t relay, uint8_t relay_retransmit_count, uint8_t relay_retransmit_interval_steps){
    uint8_t status = mesh_access_validate_envelop_params(mesh_model, dest, netkey_index, appkey_index);
    if (status != ERROR_CODE_SUCCESS) return status;

    if (relay_retransmit_count > 0x07) return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    if (relay_retransmit_interval_steps > 0x1F) return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;

    mesh_network_pdu_t * network_pdu = mesh_access_setup_unsegmented_message(&mesh_configuration_client_relay_set, relay, (relay_retransmit_count << 5) | relay_retransmit_interval_steps);
    if (!network_pdu) return BTSTACK_MEMORY_ALLOC_FAILED;

    mesh_configuration_client_send_acknowledged(mesh_access_get_element_address(mesh_model), dest, netkey_index, appkey_index, (mesh_pdu_t *) network_pdu, MESH_FOUNDATION_OPERATION_RELAY_SET);
    return ERROR_CODE_SUCCESS;
}

uint8_t mesh_configuration_client_send_model_publication_get(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, uint32_t model_id){
    uint8_t status = mesh_access_validate_envelop_params(mesh_model, dest, netkey_index, appkey_index);
    if (status != ERROR_CODE_SUCCESS) return status;

    mesh_network_pdu_t * network_pdu = mesh_access_setup_unsegmented_message(&mesh_configuration_client_model_publication_get, dest, model_id);
    if (!network_pdu) return BTSTACK_MEMORY_ALLOC_FAILED;

    mesh_configuration_client_send_acknowledged(mesh_access_get_element_address(mesh_model), dest, netkey_index, appkey_index, (mesh_pdu_t *) network_pdu, MESH_FOUNDATION_OPERATION_MODEL_PUBLICATION_GET);
    return ERROR_CODE_SUCCESS;
}

static uint8_t mesh_validate_publication_model_config_parameters(mesh_publication_model_config_t * publication_config, bool use_unicast_address){
    if (publication_config->appkey_index > 0xFFF) return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    if (publication_config->credential_flag > 1) return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    if (publication_config->publish_retransmit_count > 0x07) return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    if (publication_config->publish_retransmit_interval_steps > 0x1F) return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    if (use_unicast_address && mesh_network_address_virtual(publication_config->publish_address_unicast)) return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    return ERROR_CODE_SUCCESS;
}

uint8_t mesh_configuration_client_send_model_publication_set(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, uint32_t model_id, mesh_publication_model_config_t * publication_config){
    uint8_t status = mesh_access_validate_envelop_params(mesh_model, dest, netkey_index, appkey_index);
    if (status != ERROR_CODE_SUCCESS) return status;
    
    if (!mesh_network_address_unicast(dest) ||
        mesh_validate_publication_model_config_parameters(publication_config, true) != ERROR_CODE_SUCCESS){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }
    
    mesh_network_pdu_t * network_pdu = mesh_access_setup_unsegmented_message(&mesh_configuration_client_model_publication_set, 
        dest, 
        publication_config->publish_address_unicast,
        (publication_config->credential_flag << 12) | publication_config->appkey_index,
        publication_config->publish_ttl,
        publication_config->publish_period,
        (publication_config->publish_retransmit_interval_steps << 3) | publication_config->publish_retransmit_count,
        model_id);
    if (!network_pdu) return BTSTACK_MEMORY_ALLOC_FAILED;

    mesh_configuration_client_send_acknowledged(mesh_access_get_element_address(mesh_model), dest, netkey_index, appkey_index, (mesh_pdu_t *) network_pdu, MESH_FOUNDATION_OPERATION_MODEL_PUBLICATION_SET);
    return ERROR_CODE_SUCCESS;

}

uint8_t mesh_configuration_client_send_model_publication_virtual_address_set(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, uint32_t model_id, mesh_publication_model_config_t * publication_config){
    uint8_t status = mesh_access_validate_envelop_params(mesh_model, dest, netkey_index, appkey_index);
    if (status != ERROR_CODE_SUCCESS) return status;
    
    if (!mesh_network_address_unicast(dest) ||
        mesh_validate_publication_model_config_parameters(publication_config, false) != ERROR_CODE_SUCCESS){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }

    mesh_transport_pdu_t * transport_pdu = mesh_access_setup_segmented_message(&mesh_configuration_client_model_publication_virtual_address_set, 
        dest, 
        publication_config->publish_address_virtual,
        (publication_config->credential_flag << 12) | publication_config->appkey_index,
        publication_config->publish_ttl,
        publication_config->publish_period,
        (publication_config->publish_retransmit_interval_steps << 3) | publication_config->publish_retransmit_count,
        model_id);
    if (!transport_pdu) return BTSTACK_MEMORY_ALLOC_FAILED;

    mesh_configuration_client_send_acknowledged(mesh_access_get_element_address(mesh_model), dest, netkey_index, appkey_index, (mesh_pdu_t *) transport_pdu, MESH_FOUNDATION_OPERATION_MODEL_PUBLICATION_VIRTUAL_ADDRESS_SET);
    return ERROR_CODE_SUCCESS;
}


uint8_t mesh_configuration_client_send_model_subscription_add(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, uint16_t address, uint32_t model_id){
    uint8_t status = mesh_access_validate_envelop_params(mesh_model, dest, netkey_index, appkey_index);
    if (status != ERROR_CODE_SUCCESS) return status;

    mesh_network_pdu_t * network_pdu = mesh_access_setup_unsegmented_message(&mesh_configuration_client_model_subscription_add, dest, address, model_id);
    if (!network_pdu) return BTSTACK_MEMORY_ALLOC_FAILED;

    mesh_configuration_client_send_acknowledged(mesh_access_get_element_address(mesh_model), dest, netkey_index, appkey_index, (mesh_pdu_t *) network_pdu, MESH_FOUNDATION_OPERATION_MODEL_SUBSCRIPTION_ADD);
    return ERROR_CODE_SUCCESS;
}

uint8_t mesh_configuration_client_send_model_subscription_virtual_address_add(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, uint8_t * address, uint32_t model_id){
    uint8_t status = mesh_access_validate_envelop_params(mesh_model, dest, netkey_index, appkey_index);
    if (status != ERROR_CODE_SUCCESS) return status;

    mesh_transport_pdu_t * transport_pdu = mesh_access_setup_segmented_message(&mesh_configuration_client_model_subscription_virtual_address_add, dest, address, model_id);
    if (!transport_pdu) return BTSTACK_MEMORY_ALLOC_FAILED;

    mesh_configuration_client_send_acknowledged(mesh_access_get_element_address(mesh_model), dest, netkey_index, appkey_index, (mesh_pdu_t *) transport_pdu, MESH_FOUNDATION_OPERATION_MODEL_SUBSCRIPTION_VIRTUAL_ADDRESS_ADD);
    return ERROR_CODE_SUCCESS;
}

uint8_t mesh_configuration_client_send_model_subscription_delete(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, uint16_t address, uint32_t model_id){
    uint8_t status = mesh_access_validate_envelop_params(mesh_model, dest, netkey_index, appkey_index);
    if (status != ERROR_CODE_SUCCESS) return status;

    mesh_network_pdu_t * network_pdu = mesh_access_setup_unsegmented_message(&mesh_configuration_client_model_subscription_delete, dest, address, model_id);
    if (!network_pdu) return BTSTACK_MEMORY_ALLOC_FAILED;

    mesh_configuration_client_send_acknowledged(mesh_access_get_element_address(mesh_model), dest, netkey_index, appkey_index, (mesh_pdu_t *) network_pdu, MESH_FOUNDATION_OPERATION_MODEL_SUBSCRIPTION_DELETE);
    return ERROR_CODE_SUCCESS;
}

uint8_t mesh_configuration_client_send_model_subscription_virtual_address_delete(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, uint8_t * address, uint32_t model_id){
    uint8_t status = mesh_access_validate_envelop_params(mesh_model, dest, netkey_index, appkey_index);
    if (status != ERROR_CODE_SUCCESS) return status;

    mesh_transport_pdu_t * transport_pdu = mesh_access_setup_segmented_message(&mesh_configuration_client_model_subscription_virtual_address_delete, dest, address, model_id);
    if (!transport_pdu) return BTSTACK_MEMORY_ALLOC_FAILED;

    mesh_configuration_client_send_acknowledged(mesh_access_get_element_address(mesh_model), dest, netkey_index, appkey_index, (mesh_pdu_t *) transport_pdu, MESH_FOUNDATION_OPERATION_MODEL_SUBSCRIPTION_VIRTUAL_ADDRESS_DELETE);
    return ERROR_CODE_SUCCESS;
}

uint8_t mesh_configuration_client_send_model_subscription_overwrite(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, uint16_t address, uint32_t model_id){
        uint8_t status = mesh_access_validate_envelop_params(mesh_model, dest, netkey_index, appkey_index);
    if (status != ERROR_CODE_SUCCESS) return status;

    mesh_network_pdu_t * network_pdu = mesh_access_setup_unsegmented_message(&mesh_configuration_client_model_subscription_overwrite, dest, address, model_id);
    if (!network_pdu) return BTSTACK_MEMORY_ALLOC_FAILED;

    mesh_configuration_client_send_acknowledged(mesh_access_get_element_address(mesh_model), dest, netkey_index, appkey_index, (mesh_pdu_t *) network_pdu, MESH_FOUNDATION_OPERATION_MODEL_SUBSCRIPTION_OVERWRITE);
    return ERROR_CODE_SUCCESS;
}

uint8_t mesh_configuration_client_send_model_subscription_virtual_address_overwrite(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, uint8_t * address, uint32_t model_id){
    uint8_t status = mesh_access_validate_envelop_params(mesh_model, dest, netkey_index, appkey_index);
    if (status != ERROR_CODE_SUCCESS) return status;

    mesh_transport_pdu_t * transport_pdu = mesh_access_setup_segmented_message(&mesh_configuration_client_model_subscription_virtual_address_overwrite, dest, address, model_id);
    if (!transport_pdu) return BTSTACK_MEMORY_ALLOC_FAILED;

    mesh_configuration_client_send_acknowledged(mesh_access_get_element_address(mesh_model), dest, netkey_index, appkey_index, (mesh_pdu_t *) transport_pdu, MESH_FOUNDATION_OPERATION_MODEL_SUBSCRIPTION_VIRTUAL_ADDRESS_OVERWRITE);
    return ERROR_CODE_SUCCESS;
}

uint8_t mesh_configuration_client_send_model_subscription_delete_all(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, uint16_t address, uint32_t model_id){
        uint8_t status = mesh_access_validate_envelop_params(mesh_model, dest, netkey_index, appkey_index);
    if (status != ERROR_CODE_SUCCESS) return status;

    mesh_network_pdu_t * network_pdu = mesh_access_setup_unsegmented_message(&mesh_configuration_client_model_subscription_delete_all, dest, address, model_id);
    if (!network_pdu) return BTSTACK_MEMORY_ALLOC_FAILED;

    mesh_configuration_client_send_acknowledged(mesh_access_get_element_address(mesh_model), dest, netkey_index, appkey_index, (mesh_pdu_t *) network_pdu, MESH_FOUNDATION_OPERATION_MODEL_SUBSCRIPTION_DELETE_ALL);
    return ERROR_CODE_SUCCESS;
}

uint8_t mesh_configuration_client_send_model_subscription_get(mesh_model_t * mesh_model, uint16_t dest, uint16_t netkey_index, uint16_t appkey_index, uint32_t model_id){
    uint8_t status = mesh_access_validate_envelop_params(mesh_model, dest, netkey_index, appkey_index);
    if (status != ERROR_CODE_SUCCESS) return status;

    mesh_network_pdu_t * network_pdu = NULL;
    uint32_t ack_opcode = MESH_FOUNDATION_OPERATION_SIG_MODEL_SUBSCRIPTION_GET;

    if (mesh_model_is_bluetooth_sig(model_id)){
        network_pdu = mesh_access_setup_unsegmented_message(&mesh_configuration_client_sig_model_subscription_get, dest, model_id);
    } else {
        network_pdu = mesh_access_setup_unsegmented_message(&mesh_configuration_client_vendor_model_subscription_get, dest, model_id);
        ack_opcode = MESH_FOUNDATION_OPERATION_VENDOR_MODEL_SUBSCRIPTION_GET;
    }
    
    if (!network_pdu) return BTSTACK_MEMORY_ALLOC_FAILED;

    mesh_configuration_client_send_acknowledged(mesh_access_get_element_address(mesh_model), dest, netkey_index, appkey_index, (mesh_pdu_t *) network_pdu, ack_opcode);
    return ERROR_CODE_SUCCESS;
}

// Model Operations
static void mesh_configuration_client_composition_data_status_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    // Composition Data has variable of element descriptions, with two lists of model lists
    // Pass raw data to application but provide convenient setters instead of parsing pdu here

    // reuse part of the mesh_network_t / mesh_transport_t struct to create event without memcpy or allocation
    uint8_t * data = mesh_pdu_data(pdu);
    uint8_t * event = &data[-6];

    int pos = 0;
    event[pos++] = HCI_EVENT_MESH_META;
    // Composite Data might be larger than 251 bytes - in this case only lower 8 bit are stored here. packet size is correct
    event[pos++] = (uint8_t) (6 + mesh_pdu_len(pdu));
    event[pos++] = MESH_SUBEVENT_CONFIGURATION_COMPOSITION_DATA;
    // dest
    little_endian_store_16(event, pos, mesh_pdu_src(pdu));
    pos += 2;
    event[pos++] = ERROR_CODE_SUCCESS;

    (*mesh_model->model_packet_handler)(HCI_EVENT_PACKET, 0, event, pos);
    mesh_access_message_processed(pdu);
}

uint8_t mesh_subevent_configuration_composition_data_get_page(const uint8_t * event){
    return event[6];
}

uint16_t mesh_subevent_configuration_composition_data_get_cid(const uint8_t * event){
    return little_endian_read_16(event, 7);
}

uint16_t mesh_subevent_configuration_composition_data_get_pid(const uint8_t * event){
    return little_endian_read_16(event, 9);
}

uint16_t mesh_subevent_configuration_composition_data_get_vid(const uint8_t * event){
    return little_endian_read_16(event, 11);
}

uint16_t mesh_subevent_configuration_composition_data_get_crpl(const uint8_t * event){
    return little_endian_read_16(event, 13);
}

uint16_t mesh_subevent_configuration_composition_data_get_features(const uint8_t * event){
    return little_endian_read_16(event, 15);
}


static inline void mesh_configuration_client_handle_uint8_value(mesh_model_t *mesh_model, mesh_pdu_t * pdu, uint8_t subevent_type){
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);
    
    uint8_t value = mesh_access_parser_get_u8(&parser);

    uint8_t event[7];
    int pos = 0;

    event[pos++] = HCI_EVENT_MESH_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = subevent_type;
    // dest
    little_endian_store_16(event, pos, mesh_pdu_src(pdu));
    pos += 2;
    event[pos++] = ERROR_CODE_SUCCESS;
    event[pos++] = value;
    
    (*mesh_model->model_packet_handler)(HCI_EVENT_PACKET, 0, event, pos);
    mesh_access_message_processed(pdu);
}

static void mesh_configuration_client_beacon_status_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    mesh_configuration_client_handle_uint8_value(mesh_model, pdu, MESH_SUBEVENT_CONFIGURATION_BEACON);
}

static void mesh_configuration_client_default_ttl_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    mesh_configuration_client_handle_uint8_value(mesh_model, pdu, MESH_SUBEVENT_CONFIGURATION_DEFAULT_TTL);
}

static void mesh_configuration_client_gatt_proxy_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    mesh_configuration_client_handle_uint8_value(mesh_model, pdu, MESH_SUBEVENT_CONFIGURATION_GATT_PROXY);
}

static void mesh_configuration_client_relay_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);
    
    uint8_t relay = mesh_access_parser_get_u8(&parser);
    uint8_t retransmition = mesh_access_parser_get_u8(&parser);

    uint8_t event[9];

    int pos = 0;
    event[pos++] = HCI_EVENT_MESH_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = MESH_SUBEVENT_CONFIGURATION_RELAY;
    // dest
    little_endian_store_16(event, pos, mesh_pdu_src(pdu));
    pos += 2;
    event[pos++] = ERROR_CODE_SUCCESS;
    event[pos++] = relay;
    event[pos++] = (retransmition >> 5) + 1;
    event[pos++] = ((retransmition & 0x07) + 1) * 10;
    
    (*mesh_model->model_packet_handler)(HCI_EVENT_PACKET, 0, event, pos);
    mesh_access_message_processed(pdu);
}

static void mesh_configuration_client_model_publication_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);
    uint8_t  status = mesh_access_parser_get_u8(&parser);
    uint16_t publish_addres = mesh_access_parser_get_u16(&parser);
    
    uint16_t value = mesh_access_parser_get_u16(&parser);
    uint16_t appkey_index = value & 0xFFF;
    uint8_t  credential_flag = (value & 0x1000) >> 12;

    uint8_t publish_ttl = mesh_access_parser_get_u8(&parser);
    uint8_t publish_period = mesh_access_parser_get_u8(&parser);
    
    uint8_t retransmit = mesh_access_parser_get_u8(&parser);
    uint8_t publish_retransmit_count = retransmit & 0x111;
    uint8_t publish_retransmit_interval_steps = retransmit >> 5;
    uint32_t model_identifier = mesh_access_parser_get_model_identifier(&parser);

    uint8_t event[19];
    int pos = 0;
    event[pos++] = HCI_EVENT_MESH_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = MESH_SUBEVENT_CONFIGURATION_MODEL_PUBLICATION;
    // dest
    little_endian_store_16(event, pos, mesh_pdu_src(pdu));
    pos += 2;
    event[pos++] = status;

    little_endian_store_16(event, pos, publish_addres);
    pos += 2;

    little_endian_store_16(event, pos, appkey_index);
    pos += 2;

    event[pos++] = credential_flag;
    event[pos++] = publish_ttl;
    event[pos++] = publish_period;
    event[pos++] = publish_retransmit_count;
    event[pos++] = publish_retransmit_interval_steps;

    little_endian_store_32(event, pos, model_identifier);
    pos += 4;

    (*mesh_model->model_packet_handler)(HCI_EVENT_PACKET, 0, event, pos);
    mesh_access_message_processed(pdu);
}

static void mesh_configuration_client_model_subscription_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);
    uint8_t  status = mesh_access_parser_get_u8(&parser);
    uint16_t address = mesh_access_parser_get_u16(&parser);
    uint32_t model_identifier = mesh_access_parser_get_model_identifier(&parser);

    uint8_t event[12];
    int pos = 0;
    event[pos++] = HCI_EVENT_MESH_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = MESH_SUBEVENT_CONFIGURATION_MODEL_SUBSCRIPTION;
    // dest
    little_endian_store_16(event, pos, mesh_pdu_src(pdu));
    pos += 2;
    event[pos++] = status;

    little_endian_store_16(event, pos, address);
    pos += 2;

    little_endian_store_32(event, pos, model_identifier);
    pos += 4;

    (*mesh_model->model_packet_handler)(HCI_EVENT_PACKET, 0, event, pos);
    mesh_access_message_processed(pdu);
}

static void mesh_configuration_client_model_subscription_event(mesh_model_t *mesh_model, mesh_pdu_t * pdu, uint8_t subevent_type){
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);
    uint8_t  status = mesh_access_parser_get_u8(&parser);
    uint16_t address = mesh_access_parser_get_u16(&parser);
    uint32_t model_identifier;

    if (subevent_type == MESH_SUBEVENT_CONFIGURATION_SIG_MODEL_SUBSCRIPTION_LIST_ITEM) {
        model_identifier = mesh_access_parser_get_sig_model_identifier(&parser);
    } else {
        model_identifier = mesh_access_parser_get_vendor_model_identifier(&parser);
    }
    uint8_t list_size = mesh_access_parser_available(&parser)/2;

    uint8_t event[12];
    int pos = 0;
    event[pos++] = HCI_EVENT_MESH_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = subevent_type;
    // dest
    little_endian_store_16(event, pos, mesh_pdu_src(pdu));
    pos += 2;
    event[pos++] = status;

    little_endian_store_16(event, pos, address);
    pos += 2;

    event[pos++] = list_size;
    uint8_t i;
    for (i = 0; i < list_size; i++){
        event[pos++] = i;
        little_endian_store_16(event, pos, mesh_access_parser_get_u16(&parser));
        (*mesh_model->model_packet_handler)(HCI_EVENT_PACKET, 0, event, pos + 2);
    }
    mesh_access_message_processed(pdu);
}

static void mesh_configuration_client_sig_model_subscription_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    mesh_configuration_client_model_subscription_event(mesh_model, pdu, MESH_SUBEVENT_CONFIGURATION_SIG_MODEL_SUBSCRIPTION_LIST_ITEM);
}

static void mesh_configuration_client_vendor_model_subscription_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
        mesh_configuration_client_model_subscription_event(mesh_model, pdu, MESH_SUBEVENT_CONFIGURATION_VENDOR_MODEL_SUBSCRIPTION_LIST_ITEM);
}

const static mesh_operation_t mesh_configuration_client_model_operations[] = {
    { MESH_FOUNDATION_OPERATION_BEACON_STATUS,                  1, mesh_configuration_client_beacon_status_handler },
    { MESH_FOUNDATION_OPERATION_COMPOSITION_DATA_STATUS,       10, mesh_configuration_client_composition_data_status_handler },
    { MESH_FOUNDATION_OPERATION_DEFAULT_TTL_STATUS,             1, mesh_configuration_client_default_ttl_handler },
    { MESH_FOUNDATION_OPERATION_GATT_PROXY_STATUS,              1, mesh_configuration_client_gatt_proxy_handler },
    { MESH_FOUNDATION_OPERATION_RELAY_STATUS,                   2, mesh_configuration_client_relay_handler },
    { MESH_FOUNDATION_OPERATION_MODEL_PUBLICATION_STATUS,      12, mesh_configuration_client_model_publication_handler },
    { MESH_FOUNDATION_OPERATION_MODEL_SUBSCRIPTION_STATUS,      7, mesh_configuration_client_model_subscription_handler },
    { MESH_FOUNDATION_OPERATION_SIG_MODEL_SUBSCRIPTION_LIST,    5, mesh_configuration_client_sig_model_subscription_handler},
    { MESH_FOUNDATION_OPERATION_VENDOR_MODEL_SUBSCRIPTION_LIST, 7, mesh_configuration_client_vendor_model_subscription_handler},
    { 0, 0, NULL }
};

const mesh_operation_t * mesh_configuration_client_get_operations(void){
    return mesh_configuration_client_model_operations;
}

void mesh_configuration_client_register_packet_handler(mesh_model_t *configuration_client_model, btstack_packet_handler_t events_packet_handler){
    btstack_assert(events_packet_handler != NULL);
    btstack_assert(configuration_client_model != NULL);
    
    configuration_client_model->model_packet_handler = events_packet_handler;
}

