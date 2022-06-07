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

#define BTSTACK_FILE__ "mesh_node.c"

#include "bluetooth_company_id.h"
#include "mesh/mesh_foundation.h"

#include "mesh/mesh_node.h"

#include "btstack_util.h"

#include <stddef.h>
#include <string.h>

static uint16_t primary_element_address;

static mesh_element_t primary_element;

static uint16_t mesh_element_index_next;

static btstack_linked_list_t mesh_elements;

static uint16_t mid_counter;

static uint8_t mesh_node_device_uuid[16];
static int     mesh_node_have_device_uuid;

static uint16_t mesh_node_company_id;
static uint16_t mesh_node_product_id;
static uint16_t mesh_node_product_version_id;

void mesh_node_primary_element_address_set(uint16_t unicast_address){
    primary_element_address = unicast_address;
}

uint16_t mesh_node_get_primary_element_address(void){
    return primary_element_address; 
}

void mesh_node_init(void){
    // dd Primary Element to list of elements
    mesh_node_add_element(&primary_element);
}

void mesh_node_set_info(uint16_t company_id, uint16_t product_id, uint16_t product_version_id){
    mesh_node_company_id = company_id;
    mesh_node_product_id = product_id;
    mesh_node_product_version_id = product_version_id;
}

uint16_t mesh_node_get_company_id(void){
    return mesh_node_company_id;
}

uint16_t mesh_node_get_product_id(void){
    return mesh_node_product_id;
}

uint16_t mesh_node_get_product_version_id(void){
    return mesh_node_product_version_id;
}

void mesh_node_add_element(mesh_element_t * element){
    element->element_index = mesh_element_index_next++;
    btstack_linked_list_add_tail(&mesh_elements, (void*) element);
}

uint16_t mesh_node_element_count(void){
	return (uint16_t) btstack_linked_list_count(&mesh_elements);
}

mesh_element_t * mesh_node_get_primary_element(void){
    return &primary_element;
}


void mesh_node_set_element_location(mesh_element_t * element, uint16_t location){
    element->loc = location;
}

void mesh_node_set_primary_element_location(uint16_t location){
    mesh_node_set_element_location(&primary_element, location);
}

mesh_element_t * mesh_node_element_for_index(uint16_t element_index){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &mesh_elements);
    while (btstack_linked_list_iterator_has_next(&it)){
        mesh_element_t * element = (mesh_element_t *) btstack_linked_list_iterator_next(&it);
        if (element->element_index != element_index) continue;
        return element;
    }
    return NULL;
}

mesh_element_t * mesh_node_element_for_unicast_address(uint16_t unicast_address){
    uint16_t element_index = unicast_address - mesh_node_get_primary_element_address();
    return mesh_node_element_for_index(element_index);
}

void mesh_element_iterator_init(mesh_element_iterator_t * iterator){
    btstack_linked_list_iterator_init(&iterator->it, &mesh_elements);
}

int mesh_element_iterator_has_next(mesh_element_iterator_t * iterator){
    return btstack_linked_list_iterator_has_next(&iterator->it);
}

mesh_element_t * mesh_element_iterator_next(mesh_element_iterator_t * iterator){
    return (mesh_element_t *) btstack_linked_list_iterator_next(&iterator->it);
}

// Mesh Node Element functions
uint8_t mesh_access_get_element_index(mesh_model_t * mesh_model){
    return mesh_model->element->element_index;
}

uint16_t mesh_access_get_element_address(mesh_model_t * mesh_model){
    return mesh_node_get_primary_element_address() + mesh_model->element->element_index;
}

// Model Identifier utilities

uint32_t mesh_model_get_model_identifier(uint16_t vendor_id, uint16_t model_id){
    return (((uint32_t) vendor_id << 16)) | model_id;
}

uint32_t mesh_model_get_model_identifier_bluetooth_sig(uint16_t model_id){
    return ((uint32_t) BLUETOOTH_COMPANY_ID_BLUETOOTH_SIG_INC << 16) | model_id;
}

uint16_t mesh_model_get_model_id(uint32_t model_identifier){
    return model_identifier & 0xFFFFu;
}

uint16_t mesh_model_get_vendor_id(uint32_t model_identifier){
    return model_identifier >> 16;
}

int mesh_model_is_bluetooth_sig(uint32_t model_identifier){
    return mesh_model_get_vendor_id(model_identifier) == BLUETOOTH_COMPANY_ID_BLUETOOTH_SIG_INC;
}

mesh_model_t * mesh_node_get_configuration_server(void){
    return mesh_model_get_by_identifier(mesh_node_get_primary_element(), mesh_model_get_model_identifier_bluetooth_sig(MESH_SIG_MODEL_ID_CONFIGURATION_SERVER));
}

mesh_model_t * mesh_node_get_health_server(void){
    return mesh_model_get_by_identifier(mesh_node_get_primary_element(), mesh_model_get_model_identifier_bluetooth_sig(MESH_SIG_MODEL_ID_HEALTH_SERVER));
}

void mesh_model_reset_appkeys(mesh_model_t * mesh_model){
    uint16_t i;
    for (i=0;i<MAX_NR_MESH_APPKEYS_PER_MODEL;i++){
        mesh_model->appkey_indices[i] = MESH_APPKEY_INVALID;
    }
}

void mesh_element_add_model(mesh_element_t * element, mesh_model_t * mesh_model){
    // reset app keys
    mesh_model_reset_appkeys(mesh_model);

    if (mesh_model_is_bluetooth_sig(mesh_model->model_identifier)){
        element->models_count_sig++;
    } else {
        element->models_count_vendor++;
    }
    mesh_model->mid = mid_counter++;
    mesh_model->element = element;
    btstack_linked_list_add_tail(&element->models, (btstack_linked_item_t *) mesh_model);
}

void mesh_model_iterator_init(mesh_model_iterator_t * iterator, mesh_element_t * element){
    btstack_linked_list_iterator_init(&iterator->it, &element->models);
}

int mesh_model_iterator_has_next(mesh_model_iterator_t * iterator){
    return btstack_linked_list_iterator_has_next(&iterator->it);
}

mesh_model_t * mesh_model_iterator_next(mesh_model_iterator_t * iterator){
    return (mesh_model_t *) btstack_linked_list_iterator_next(&iterator->it);
}

mesh_model_t * mesh_model_get_by_identifier(mesh_element_t * element, uint32_t model_identifier){
    mesh_model_iterator_t it;
    mesh_model_iterator_init(&it, element);
    while (mesh_model_iterator_has_next(&it)){
        mesh_model_t * model = mesh_model_iterator_next(&it);
        if (model->model_identifier != model_identifier) continue;
        return model;
    }
    return NULL;
}

mesh_model_t * mesh_access_model_for_address_and_model_identifier(uint16_t element_address, uint32_t model_identifier, uint8_t * status){
    mesh_element_t * element = mesh_node_element_for_unicast_address(element_address);
    if (element == NULL){
        *status = MESH_FOUNDATION_STATUS_INVALID_ADDRESS;
        return NULL;
    }
    mesh_model_t * model = mesh_model_get_by_identifier(element, model_identifier);
    if (model == NULL) {
        *status = MESH_FOUNDATION_STATUS_INVALID_MODEL;
    } else {
        *status = MESH_FOUNDATION_STATUS_SUCCESS;
    }
    return model;
}
// Mesh Model Subscription
int mesh_model_contains_subscription(mesh_model_t * mesh_model, uint16_t address){
    uint16_t i;
    for (i=0;i<MAX_NR_MESH_SUBSCRIPTION_PER_MODEL;i++){
        if (mesh_model->subscriptions[i] == address) return 1;
    }
    return 0;
}

void mesh_node_set_device_uuid(const uint8_t * device_uuid){
    (void)memcpy(mesh_node_device_uuid, device_uuid, 16);
    mesh_node_have_device_uuid = 1;
}

/**
 * @brief Get Device UUID
 */
const uint8_t * mesh_node_get_device_uuid(void){
    if (mesh_node_have_device_uuid == 0) return NULL;
    return mesh_node_device_uuid;
}


// Heartbeat (helper)
uint16_t mesh_heartbeat_pwr2(uint8_t value){
    if (value == 0 )                    return 0x0000;
    if (value == 0xff || value == 0x11) return 0xffff;
    return 1 << (value-1);
}

uint8_t mesh_heartbeat_count_log(uint16_t value){
    if (value == 0)      return 0x00;
    if (value == 0xffff) return 0xff;
    // count leading zeros, supported by clang and gcc
    // note: CountLog(8) == CountLog(7) = 3
    return 33 - btstack_clz(value - 1);
}

uint8_t mesh_heartbeat_period_log(uint16_t value){
    if (value == 0)      return 0x00;
    // count leading zeros, supported by clang and gcc
    // note: PeriodLog(8) == PeriodLog(7) = 3
    return 33 - btstack_clz(value - 1);
}
