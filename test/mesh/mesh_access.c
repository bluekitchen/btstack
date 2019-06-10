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

#define __BTSTACK_FILE__ "mesh_access.c"

#include <string.h>
#include <stdio.h>
#include "mesh_access.h"
#include "btstack_memory.h"
#include "bluetooth_company_id.h"

static mesh_element_t primary_element;

static btstack_linked_list_t mesh_elements;

static uint16_t mid_counter;

void mesh_access_init(void){
}

mesh_element_t * mesh_primary_element(void){
    return &primary_element;
}

void mesh_access_set_primary_element_address(uint16_t unicast_address){
    primary_element.unicast_address = unicast_address;
}

void mesh_element_add(mesh_element_t * element){
    btstack_linked_list_add_tail(&mesh_elements, (void*) element);
}

mesh_element_t * mesh_element_for_unicast_address(uint16_t unicast_address){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &mesh_elements);
    while (btstack_linked_list_iterator_has_next(&it)){
        mesh_element_t * element = (mesh_element_t *) btstack_linked_list_iterator_next(&it);
        if (element->unicast_address != unicast_address) continue;
        return element;
    }
    return NULL;
}

// Model Identifier utilities

uint32_t mesh_model_get_model_identifier(uint16_t vendor_id, uint16_t model_id){
    return (vendor_id << 16) | model_id;
}

uint32_t mesh_model_get_model_identifier_bluetooth_sig(uint16_t model_id){
    return (BLUETOOTH_COMPANY_ID_BLUETOOTH_SIG_INC << 16) | model_id;
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


void mesh_element_add_model(mesh_element_t * element, mesh_model_t * mesh_model){
    if (mesh_model_is_bluetooth_sig(mesh_model->model_identifier)){
        element->models_count_sig++;
    } else {
        element->models_count_vendor++;
    }
    mesh_model->mid = mid_counter++;
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

void mesh_element_iterator_init(mesh_element_iterator_t * iterator){
    btstack_linked_list_iterator_init(&iterator->it, &mesh_elements);
}

int mesh_element_iterator_has_next(mesh_element_iterator_t * iterator){
    return btstack_linked_list_iterator_has_next(&iterator->it);
}

mesh_element_t * mesh_element_iterator_next(mesh_element_iterator_t * iterator){
    return (mesh_element_t *) btstack_linked_list_iterator_next(&iterator->it);
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
