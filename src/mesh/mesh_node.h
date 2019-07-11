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

#ifndef __MESH_NODE_H
#define __MESH_NODE_H

#if defined __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "btstack_linked_list.h"

typedef struct mesh_element {
    // linked list item
    btstack_linked_item_t item;
    
    // element index
    uint16_t element_index;

    // LOC
    uint16_t loc;
    
    // models
    btstack_linked_list_t models;
    uint16_t models_count_sig;
    uint16_t models_count_vendor;

} mesh_element_t;

typedef struct {
    btstack_linked_list_iterator_t it;
} mesh_element_iterator_t;


void mesh_node_init(void);

/**
 * @brief Set unicast address of primary element
 * @param unicast_address
 */
void mesh_node_primary_element_address_set(uint16_t unicast_address);

/**
 * @brief Set location of primary element
 * @note Returned by Configuration Server Composite Data
 * @param location
 */
void mesh_node_set_primary_element_location(uint16_t location);

/**
 * @brief Get unicast address of primary element
 */
uint16_t mesh_node_get_primary_element_address(void);

/**
 * @brief Get Primary Element of this node
 */
mesh_element_t * mesh_node_get_primary_element(void);

/**
 * @brief Add secondary element
 * @param element
 */
void mesh_node_add_element(mesh_element_t * element);

/**
 * @brief Get number elements
 * @returns number of elements on this node
 */
uint16_t mesh_node_element_count(void);

/**
 * @brief Get element for given unicast address
 * @param unicast_address
 */
mesh_element_t * mesh_node_element_for_unicast_address(uint16_t unicast_address);

/**
 * @brief Get element by index
 * @param element_index
 */
mesh_element_t * mesh_node_element_for_index(uint16_t element_index);


// Mesh Element Iterator

void mesh_element_iterator_init(mesh_element_iterator_t * iterator);

int mesh_element_iterator_has_next(mesh_element_iterator_t * iterator);

mesh_element_t * mesh_element_iterator_next(mesh_element_iterator_t * iterator);


#if defined __cplusplus
}
#endif

#endif //__MESH_NODE_H
