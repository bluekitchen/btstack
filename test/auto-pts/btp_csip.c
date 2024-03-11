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

#define BTSTACK_FILE__ "btp_csip.c"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "btstack_debug.h"
#include "btpclient.h"
#include "btp.h"
#include "btp_csip.h"
#include "btstack.h"
#include "le-audio/le_audio_base_builder.h"
#include "le_audio_demo_util_source.h"
#include "le-audio/le_audio_base_parser.h"

static btstack_packet_handler_t btp_csip_higher_layer_handler;
static btstack_packet_callback_registration_t hci_event_callback_registration;

#define MAX_NUM_SERVERS 2

// BTP Server Management

static server_t servers[MAX_NUM_SERVERS];

static enum {
    BTP_CSIP_ORDERED_ACCESS_IDLE,
    BTP_CSIP_ORDERED_ACCESS_READ_LOCKS,
    BTP_CSIP_ORDERED_ACCESS_WRITE_LOCKS,
    BTP_CSIP_ORDERED_ACCESS_READY
} btp_csip_ordered_access_state = BTP_CSIP_ORDERED_ACCESS_IDLE;
static uint8_t csip_ordered_access_next_rank;

static server_t * btp_server_initialize(hci_con_handle_t con_handle){
    uint8_t i;
    for (i=0; i < MAX_NUM_SERVERS; i++){
        server_t * server = &servers[i];
        if (server->acl_con_handle == HCI_CON_HANDLE_INVALID){
            // get hci con handle from HCI
            hci_connection_t * connection = hci_connection_for_handle(con_handle);
            btstack_assert(connection != NULL);
            server->acl_con_handle = connection->con_handle;
            memcpy(server->address, connection->address, 6);
            server->address_type = connection->address_type;
            return server;
        }
    }
    btstack_unreachable();
    return NULL;
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

static server_t * btp_server_for_acl_con_handle(hci_con_handle_t acl_con_handle){
    uint8_t i;
    for (i=0; i < MAX_NUM_SERVERS; i++){
        server_t * server = &servers[i];
        if (server->acl_con_handle == acl_con_handle){
            return server;
        }
    }
    return NULL;
}

server_t * btp_server_for_address(bd_addr_type_t address_type, bd_addr_t address){
    uint8_t i;
    for (i=0; i < MAX_NUM_SERVERS; i++){
        server_t * server = &servers[i];
        if (server->address_type == address_type){
            if (memcmp(server->address, address, 6) == 0){
                return server;
            }
        }
    }
    return NULL;
}

static void btp_server_init(void){
    // init servers
    uint8_t i;
    for (i=0;i<MAX_NUM_SERVERS;i++){
        servers[i].server_id = i;
        servers[i].acl_con_handle = HCI_CON_HANDLE_INVALID;
    }
}

static void btp_server_finalize(server_t * server){
    uint8_t server_id = server->server_id;
    memset(servers, 0, sizeof(server_t));
    server->server_id = server_id;
}

//

static void btp_csip_hci_packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    server_t * server;
    switch (packet_type) {
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    server = btp_server_for_acl_con_handle(hci_event_disconnection_complete_get_connection_handle(packet));
                    if (server != NULL){
                        // discard server
                        btp_server_finalize(server);
                    }
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
}


static void btp_csip_ordered_access_next(void){
    uint8_t i;
    switch (btp_csip_ordered_access_state){
        case BTP_CSIP_ORDERED_ACCESS_READ_LOCKS:
            // read lock characteristic of all set members
            for (i=0;i<MAX_NUM_SERVERS;i++) {
                server_t * server = &servers[i];
                if (server->coordinated_set_rank == csip_ordered_access_next_rank) {
                    MESSAGE("CSIP client %u: read lock, cid 0x%04x", server->server_id, server->csis_cid);
                    csip_ordered_access_next_rank++;
                    coordinated_set_identification_service_client_read_member_lock(server->csis_cid);
                    return;
                }
            }

            // done
            MESSAGE("CSIP clients: all locks read");
            btp_csip_ordered_access_state = BTP_CSIP_ORDERED_ACCESS_WRITE_LOCKS;
            csip_ordered_access_next_rank = 1;

            /* fall through */

        case BTP_CSIP_ORDERED_ACCESS_WRITE_LOCKS:
            // write lock characteristic of all set members
            for (i=0;i<MAX_NUM_SERVERS;i++) {
                server_t * server = &servers[i];
                if (server->coordinated_set_rank == csip_ordered_access_next_rank) {
                    MESSAGE("CSIP client %u: write lock, cid 0x%04x", server->server_id, server->csis_cid);
                    csip_ordered_access_next_rank++;
                    coordinated_set_identification_service_client_write_member_lock(server->csis_cid, CSIS_MEMBER_LOCKED);
                    return;
                }
            }

            // done
            MESSAGE("CSIP clients: all locks written");
            btp_csip_ordered_access_state = BTP_CSIP_ORDERED_ACCESS_READY;
            csip_ordered_access_next_rank = 1;

            btp_send(response_service_id, BTP_CSIP_START_ORDERED_ACCESS, 0, 0, NULL);

            break;

        default:
            break;
    }
}

static void btp_csip_client_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_GATTSERVICE_META) return;

    uint8_t status;
    server_t * server = NULL;

    MESSAGE("CSIP subevent 0x%02x", hci_event_gattservice_meta_get_subevent_code(packet));
    switch (hci_event_gattservice_meta_get_subevent_code(packet)){
        case GATTSERVICE_SUBEVENT_CSIS_CLIENT_CONNECTED:
            server = btp_server_for_csis_cid(gattservice_subevent_csis_client_connected_get_csis_cid(packet));
            if (server != NULL){
                if (gattservice_subevent_csis_client_connected_get_status(packet) != ERROR_CODE_SUCCESS){
                    MESSAGE("CSIP client %u: connection failed, cid 0x%04x, con_handle 0x%04x, status 0x%02x",
                           server->server_id, server->csis_cid, server->acl_con_handle,
                           gattservice_subevent_csis_client_connected_get_status(packet));
                    MESSAGE("CSIP client %u: assume individual device", server->server_id);
                    server->coordinated_set_size = 1;
                } else {
                    MESSAGE("CSIP client %u: connected, cid 0x%04x", server->server_id, server->csis_cid);
                    // get coordinated set size
                    coordinated_set_identification_service_client_read_coordinated_set_size(server->csis_cid);
                }
            }
            break;

        case GATTSERVICE_SUBEVENT_CSIS_CLIENT_COORDINATED_SET_SIZE:
            server = btp_server_for_csis_cid(gattservice_subevent_csis_client_coordinated_set_size_get_csis_cid(packet));
            status = gattservice_subevent_csis_client_coordinated_set_size_get_status(packet);
            if (status != ERROR_CODE_SUCCESS){
                MESSAGE("CSIP client %u: read coordinated set size failed, 0x%02x", server->server_id, status);
            } else {
                uint8_t set_size = gattservice_subevent_csis_client_coordinated_set_size_get_coordinated_set_size(packet);
                server->coordinated_set_size = set_size;
                MESSAGE("CSIP client %u: coordinated_set_size %u", server->server_id, set_size);
            }
            coordinated_set_identification_service_client_read_sirk(server->csis_cid);
            break;

        case GATTSERVICE_SUBEVENT_CSIS_CLIENT_SIRK:
            server = btp_server_for_csis_cid(gattservice_subevent_csis_client_sirk_get_csis_cid(packet));
            status = gattservice_subevent_csis_client_sirk_get_status(packet);
            if (status != ERROR_CODE_SUCCESS){
                MESSAGE("CSIP client %u: read SIRK failed, 0x%02x", server->server_id, status);
                MESSAGE("TODO: handle get sirk failed");
            } else {
                gattservice_subevent_csis_client_sirk_get_sirk(packet, server->sirk);
                MESSAGE("CSIP client %u: remote SIRK (%s)", server->server_id,
                        (csis_sirk_type_t)gattservice_subevent_csis_client_sirk_get_sirk_type(packet) == CSIS_SIRK_TYPE_ENCRYPTED ? "ENCRYPTED" : "PLAIN_TEXT");
                coordinated_set_identification_service_client_read_member_rank(server->csis_cid);
            }
            break;

        case GATTSERVICE_SUBEVENT_CSIS_CLIENT_RANK:
            server = btp_server_for_csis_cid(gattservice_subevent_csis_client_rank_get_csis_cid(packet));
            status = gattservice_subevent_csis_client_rank_get_status(packet);
            if (status != ERROR_CODE_SUCCESS){
                MESSAGE("CSIP client %u: read RANK failed, 0x%02x", status, server->server_id);
                break;
            }
            server->coordinated_set_rank = gattservice_subevent_csis_client_rank_get_rank(packet);
            MESSAGE("CSIP client %u: remote member RANK %u", server->server_id, server->coordinated_set_rank);
            break;

        case GATTSERVICE_SUBEVENT_CSIS_RSI_MATCH:
            if (gattservice_subevent_csis_rsi_match_get_match(packet) != 0){
                bd_addr_t addr;
                gattservice_subevent_csis_rsi_match_get_source_address(packet, addr);
                // rsi_match_handler(gattservice_subevent_csis_rsi_match_get_source_address_type(packet), addr);
            }
            break;

        case GATTSERVICE_SUBEVENT_CSIS_CLIENT_LOCK:
            server = btp_server_for_csis_cid(gattservice_subevent_csis_client_lock_get_csis_cid(packet));
            status = gattservice_subevent_csis_client_lock_get_status(packet);
            if (status != ERROR_CODE_SUCCESS){
                MESSAGE("CSIP client %u: read LOCK failed, 0x%02x", server->server_id, status);
            } else {
                btp_csip_ordered_access_next();
            }
            break;

        case GATTSERVICE_SUBEVENT_CSIS_CLIENT_LOCK_WRITE_COMPLETE:
            server = btp_server_for_csis_cid(gattservice_subevent_csis_client_lock_write_complete_get_csis_cid(packet));
            status = gattservice_subevent_csis_client_lock_write_complete_get_status(packet);
            if (status != ERROR_CODE_SUCCESS){
                printf("CSIS client %u: write LOCK failed, 0x%02x\n", server->server_id, status);
            } else {
                btp_csip_ordered_access_next();
            }
            break;

        case GATTSERVICE_SUBEVENT_CSIS_CLIENT_DISCONNECTED:
            server = btp_server_for_csis_cid(gattservice_subevent_csis_client_disconnected_get_csis_cid(packet));
            if (server != NULL){
                server->csis_cid = 0;
                MESSAGE("CSIP client %u: disconnected", server->server_id);
            }
            break;

        default:
            break;
    }

    if (btp_csip_higher_layer_handler != NULL){
        (*btp_csip_higher_layer_handler)(packet_type, channel, packet, size);
    }
}

void btp_csip_handler(uint8_t opcode, uint8_t controller_index, uint16_t length, const uint8_t *data) {
    // provide op info for response
    response_len = 0;
    response_service_id = BTP_SERVICE_ID_CSIP;
    response_op = opcode;
    switch (opcode) {
        case BTP_CSIP_READ_SUPPORTED_COMMANDS:
            MESSAGE("BTP_CSIP_READ_SUPPORTED_COMMANDS");
            if (controller_index == BTP_INDEX_NON_CONTROLLER) {
                uint8_t commands = 0;
                btp_send(response_service_id, opcode, controller_index, 1, &commands);
            }
            break;
        case BTP_CSIP_DISCOVER:
            MESSAGE("BTP_CSIP_DISCOVER - ignore currently, as it is done as part of CAP Discover");
            if (controller_index == 0) {
                // ignore
                btp_send(response_service_id, opcode, controller_index, 0, NULL);
            }
            break;
        case BTP_CSIP_START_ORDERED_ACCESS:
            MESSAGE("BTP_CSIP_START_ORDERED_ACCESS");
            if (controller_index == 0) {
                csip_ordered_access_next_rank = 1;
                btp_csip_ordered_access_state = BTP_CSIP_ORDERED_ACCESS_READ_LOCKS;
                btp_csip_ordered_access_next();
            }
            break;
        case BTP_CSIP_SET_COORDINATOR_LOCK:
            MESSAGE("BTP_CSIP_SET_COORDINATOR_LOCK");
            if (controller_index == 0) {
                // ignore
                btp_send(response_service_id, opcode, controller_index, 0, NULL);
            }
            break;
        case BTP_CSIP_SET_COORDINATOR_RELEASE:
            MESSAGE("BTP_CSIP_SET_COORDINATOR_RELEASE");
            if (controller_index == 0) {
                // ignore
                btp_send(response_service_id, opcode, controller_index, 0, NULL);
            }
            break;
        default:
            break;
    }
}


void btp_csip_init(void) {
    btp_server_init();

    // register for HCI events
    hci_event_callback_registration.callback = &btp_csip_hci_packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    coordinated_set_identification_service_client_init(&btp_csip_client_event_handler);
}

void btp_csip_register_higher_layer(btstack_packet_handler_t handler){
    btp_csip_higher_layer_handler = handler;
}

void btp_csip_connect(hci_con_handle_t con_handle){

    server_t * server = btp_server_for_acl_con_handle(con_handle);

    if (server == NULL){
        server = btp_server_initialize(con_handle);
    }

    coordinated_set_identification_service_client_connect(&server->csis_connection, server->acl_con_handle, &server->csis_cid);
    MESSAGE("BTP_CSIP: connect 0x%04x, CSIS CID %u", server->acl_con_handle, server->csis_cid);
}
