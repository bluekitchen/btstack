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

#ifndef __MESH_VIRTUAL_ADDRESSES_H
#define __MESH_VIRTUAL_ADDRESSES_H

#include <stdint.h>

#include "btstack_linked_list.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct {
	btstack_linked_item_t item;
    uint16_t pseudo_dst;
    uint16_t hash;
    uint16_t ref_count;
    uint8_t  label_uuid[16];
} mesh_virtual_address_t;

typedef struct {
	btstack_linked_list_iterator_t it;
	uint16_t hash;
	mesh_virtual_address_t * address;
} mesh_virtual_address_iterator_t;

// virtual address management

uint16_t mesh_virtual_addresses_get_free_pseudo_dst(void);

void mesh_virtual_address_add(mesh_virtual_address_t * virtual_address);

void mesh_virtual_address_remove(mesh_virtual_address_t * virtual_address);

mesh_virtual_address_t * mesh_virtual_address_register(uint8_t * label_uuid, uint16_t hash);

mesh_virtual_address_t * mesh_virtual_address_for_pseudo_dst(uint16_t pseudo_dst);

mesh_virtual_address_t * mesh_virtual_address_for_label_uuid(uint8_t * label_uuid);

// virtual address iterator

void mesh_virtual_address_iterator_init(mesh_virtual_address_iterator_t * it, uint16_t hash);

int mesh_virtual_address_iterator_has_more(mesh_virtual_address_iterator_t * it);

const mesh_virtual_address_t * mesh_virtual_address_iterator_get_next(mesh_virtual_address_iterator_t * it);


#ifdef __cplusplus
} /* end of extern "C" */
#endif

#endif
