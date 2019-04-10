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

#define __BTSTACK_FILE__ "mesh_keys.c"

#include <string.h>
#include <stdio.h>
#include "mesh_keys.h"
#include "btstack_util.h"

static void mesh_print_hex(const char * name, const uint8_t * data, uint16_t len){
    printf("%-20s ", name);
    printf_hexdump(data, len);
}

// application key list

// key management
static mesh_transport_key_t   test_application_key;
static mesh_transport_key_t   mesh_transport_device_key;

void mesh_application_key_set(uint16_t appkey_index, uint8_t aid, const uint8_t * application_key){
    test_application_key.index = appkey_index;
    test_application_key.aid   = aid;
    test_application_key.akf   = 1;
    memcpy(test_application_key.key, application_key, 16);
}

void mesh_transport_set_device_key(const uint8_t * device_key){
    mesh_transport_device_key.index = MESH_DEVICE_KEY_INDEX;
    mesh_transport_device_key.aid   = 0;
    mesh_transport_device_key.akf   = 0;
    memcpy(mesh_transport_device_key.key, device_key, 16);
}

const mesh_transport_key_t * mesh_transport_key_get(uint16_t appkey_index){
    if (appkey_index == MESH_DEVICE_KEY_INDEX){
        return &mesh_transport_device_key;
    }
    if (appkey_index != test_application_key.index) return NULL;
    return &test_application_key;
}

// key iterator


void mesh_transport_key_iterator_init(mesh_transport_key_iterator_t *it, uint8_t akf, uint8_t aid){
    it->aid      = aid;
    it->akf      = akf;
    it->first    = 1;
}

int mesh_transport_key_iterator_has_more(mesh_transport_key_iterator_t *it){
    if (!it->first) return 0;
    if (it->akf){
        return it->aid == test_application_key.aid;
    } else {
        return 1;
    }
}

const mesh_transport_key_t * mesh_transport_key_iterator_get_next(mesh_transport_key_iterator_t *it){
    it->first = 0;
    if (it->akf){
        return &test_application_key;
    } else {
        return &mesh_transport_device_key;
    }
}
