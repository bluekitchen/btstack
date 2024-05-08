/*
 * Copyright (C) 2024 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "btp_server.c"

#include "btp_server.h"

#include "btstack_bool.h"
#include "btstack_config.h"
#include "btstack_debug.h"

#include "btpclient.h"

#include <stdint.h>
#include <string.h>
#include "hci.h"

// BTP Server Management

static server_t servers[MAX_NUM_SERVERS];

server_t * btp_server_for_address(bd_addr_type_t address_type, bd_addr_t address){
    const hci_connection_t * hci_connection = hci_connection_for_bd_addr_and_type(address, address_type);
    if (hci_connection == NULL){
        MESSAGE("btp_server_for_address: no hci connection for addr %s type %u", bd_addr_to_str(address), address_type);
        btstack_unreachable();
        return NULL;
    }
    hci_con_handle_t con_handle = hci_connection->con_handle;
    server_t * server = btp_server_for_acl_con_handle(con_handle);
    if (server == NULL){
        uint8_t i;
        for (i=0; i < MAX_NUM_SERVERS; i++){
            server_t * server = &servers[i];
            if (server->acl_con_handle == HCI_CON_HANDLE_INVALID){
                server->acl_con_handle = hci_connection->con_handle;
                memcpy(server->address, address, 6);
                server->address_type =address_type;
                MESSAGE("btp_server_for_address: setup slot %u (%p) for addr %s type %u", i, server, bd_addr_to_str(address), address_type);
                return server;
            }
        }
        MESSAGE("btp_server_for_address: no free server slot for for addr %s type %u", bd_addr_to_str(address), address_type);
        btstack_unreachable();
        return NULL;
    }
    return server;
}

server_t * btp_server_for_csis_cid(uint16_t csis_cid){
    uint8_t i;
    for (i=0; i < MAX_NUM_SERVERS; i++){
        server_t * server = &servers[i];
        if (server->csis_cid == csis_cid){
            return server;
        }
    }
    return NULL;
}

server_t * btp_server_for_pacs_cid(uint16_t pacs_cid){
    uint8_t i;
    for (i=0; i < MAX_NUM_SERVERS; i++){
        server_t * server = &servers[i];
        if (server->pacs_cid == pacs_cid){
            return server;
        }
    }
    return NULL;
}

server_t * btp_server_for_ascs_cid(uint16_t ascs_cid){
    uint8_t i;
    for (i=0; i < MAX_NUM_SERVERS; i++){
        server_t * server = &servers[i];
        if (server->ascs_cid == ascs_cid){
            return server;
        }
    }
    return NULL;
}

server_t * btp_server_for_bass_cid(uint16_t bass_cid){
    uint8_t i;
    for (i=0; i < MAX_NUM_SERVERS; i++){
        server_t * server = &servers[i];
        if (server->bass_cid == bass_cid){
            return server;
        }
    }
    return NULL;
}

server_t * btp_server_for_acl_con_handle(hci_con_handle_t acl_con_handle){
    uint8_t i;
    for (i=0; i < MAX_NUM_SERVERS; i++){
        server_t * server = &servers[i];
        if (server->acl_con_handle == acl_con_handle){
            return server;
        }
    }
    return NULL;
}

server_t * btp_server_for_index(uint8_t index){
    btstack_assert(index < MAX_NUM_SERVERS);
    return &servers[index];
}

void btp_server_init(void){
    // init servers
    uint8_t i;
    for (i=0;i<MAX_NUM_SERVERS;i++){
        servers[i].server_id = i;
        servers[i].acl_con_handle = HCI_CON_HANDLE_INVALID;
    }
}

void btp_server_finalize(server_t * server){
    uint8_t server_id = server->server_id;
    memset(servers, 0, sizeof(server_t));
    server->server_id = server_id;
}

