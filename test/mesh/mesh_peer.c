//
// Created by Matthias Ringwald on 2019-04-03.
//

#include "btstack_memory.h"
#include "btstack_util.h"
#include "ble/mesh/beacon.h"
#include "ble/mesh/mesh_upper_transport.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "mesh_peer.h"

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
