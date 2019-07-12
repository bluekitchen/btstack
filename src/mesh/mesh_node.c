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

#define __BTSTACK_FILE__ "mesh_node.c"

#include <stddef.h>
#include <string.h>

#include "mesh/mesh_node.h"

static uint16_t primary_element_address;

static mesh_element_t primary_element;

static uint16_t mesh_element_index_next;

static btstack_linked_list_t mesh_elements;

static uint8_t mesh_node_device_uuid[16];

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

void mesh_node_set_device_uuid(const uint8_t * device_uuid){
    memcpy(mesh_node_get_device_uuid, device_uuid, 16);
}

/**
 * @brief Get Device UUID
 */
const uint8_t * mesh_node_get_device_uuid(void){
    return mesh_node_device_uuid;
}

