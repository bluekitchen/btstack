/*
 * Copyright (C) 2018 BlueKitchen GmbH
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

#ifndef __MESH_KEYS_H
#define __MESH_KEYS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif


#include <stdint.h>
#include "ble/mesh/mesh_network.h"

typedef struct {
    btstack_linked_item_t item;

    // index into shared global key list
    uint16_t netkey_index;

    // random net_key
    uint8_t net_key[16];

    // derivative data

    // k1
    uint8_t identity_key[16];
    uint8_t beacon_key[16];

    // k2
    uint8_t nid;
    uint8_t encryption_key[16];
    uint8_t privacy_key[16];

    // k3
    uint8_t network_id[8];
} mesh_network_key_t;

typedef struct {
    uint8_t nid;
    uint8_t first;
} mesh_network_key_iterator_t;

typedef struct {
    btstack_linked_item_t item;

    // index into shared global key list
    uint16_t index;

    // app_key
    uint8_t key[16];

    // application key flag, 0 for device key
    uint8_t akf;

    // application key hash id
    uint8_t aid;
} mesh_transport_key_t;

typedef struct {
    uint8_t  akf;
    uint8_t  aid;
    uint8_t  first;
} mesh_transport_key_iterator_t;

/**
 * @brief Initialize network key list from provisioning data
 * @param provisioning_data
 */
void mesh_network_key_list_add_from_provisioning_data(const mesh_provisioning_data_t * provisioning_data);

/**
 * @brief Get network_key for netkey_index
 * @param netkey_index
 * @returns mesh_network_key_t or NULL
 */
const mesh_network_key_t * mesh_network_key_list_get(uint16_t netkey_index);

/**
 *
 * @param it
 * @param nid
 */
void mesh_network_key_iterator_init(mesh_network_key_iterator_t * it, uint8_t nid);

/**
 *
 * @param it
 * @return
 */
int mesh_network_key_iterator_has_more(mesh_network_key_iterator_t * it);

/**
 *
 * @param it
 * @return
 */
const mesh_network_key_t * mesh_network_key_iterator_get_next(mesh_network_key_iterator_t * it);

/**
 * Set device key
 * @param device_key
 */
void mesh_transport_set_device_key(const uint8_t * device_key);

/**
 * Add Application Key
 * @param appkey_index
 * @param aid
 * @param application_key
 */
void mesh_application_key_set(uint16_t appkey_index, uint8_t aid, const uint8_t * application_key);

/**
 * Get transport key for appkey_index
 * @param appkey_index
 * @return
 */
const mesh_transport_key_t * mesh_transport_key_get(uint16_t appkey_index);

/**
 * Transport Key Iterator - init
 * @param it
 * @param akf
 * @param aid
 */
void mesh_transport_key_iterator_init(mesh_transport_key_iterator_t *it, uint8_t akf, uint8_t aid);

/**
 * Transport Key Iterator - has more?
 * @param it
 * @return
 */
int mesh_transport_key_iterator_has_more(mesh_transport_key_iterator_t *it);

/** Transport Key Iterator - get next
 *
 * @param it
 * @return transport key
 */
const mesh_transport_key_t * mesh_transport_key_iterator_get_next(mesh_transport_key_iterator_t *it);

#ifdef __cplusplus
} /* end of extern "C" */
#endif

#endif
