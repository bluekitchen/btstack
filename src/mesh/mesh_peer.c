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

#include "mesh/mesh_peer.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "btstack_memory.h"
#include "btstack_util.h"

#include "mesh/beacon.h"
#include "mesh/mesh_upper_transport.h"

#define MESH_NUM_PEERS 5

static mesh_peer_t mesh_peers[MESH_NUM_PEERS];

void mesh_seq_auth_reset(void){
    int i;
    for(i=0;i<MESH_NUM_PEERS;i++){
        memset(&mesh_peers[i], 0, sizeof(mesh_peer_t));
    }
}

mesh_peer_t * mesh_peer_for_addr(uint16_t address){
    int i;
    for (i=0;i<MESH_NUM_PEERS;i++){
        if (mesh_peers[i].address == address){
            return &mesh_peers[i];
        }
    }
    for (i=0;i<MESH_NUM_PEERS;i++){
        if (mesh_peers[i].address== MESH_ADDRESS_UNSASSIGNED){
            memset(&mesh_peers[i], 0, sizeof(mesh_peer_t));
            mesh_peers[i].address = address;
            return &mesh_peers[i];
        }
    }
    return NULL;
}
