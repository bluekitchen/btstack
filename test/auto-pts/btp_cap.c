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

#define BTSTACK_FILE__ "btp_cap.c"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "btstack_debug.h"
#include "btpclient.h"
#include "btp.h"
#include "btp_cap.h"
#include "btp_csip.h"
#include "btp_server.h"
#include "btstack.h"
#include "le-audio/le_audio_base_builder.h"
#include "le_audio_demo_util_source.h"
#include "le-audio/le_audio_base_parser.h"

// CAP
typedef enum {
    CAP_DISOCVERY_IDLE,
    CAP_DISCOVERY_CSIS_WAIT,
    CAP_DISCOVERY_DONE,
} btp_cap_discovery_state_t;

static void btp_cap_send_discovery_complete(server_t * server) {
    struct btp_cap_discovery_completed_ev discovery_completed_ev;
    discovery_completed_ev.addr_type = server->address_type;
    reverse_bd_addr(server->address, discovery_completed_ev.address);
    MESSAGE("BTP_CAP_EV_DISCOVERY_COMPLETED %s", bd_addr_to_str(server->address));
    btp_send(BTP_SERVICE_ID_CAP, BTP_CAP_EV_DISCOVERY_COMPLETED, 0, sizeof(discovery_completed_ev), (const uint8_t *) &discovery_completed_ev);
}

static void btp_cap_discovery_next(server_t * server) {
    uint8_t ascs_index;
    uint16_t ascs_cid;
    switch((btp_cap_discovery_state_t) server->cap_state){
        case CAP_DISOCVERY_IDLE:
            MESSAGE("BTP_CAP_DISCOVER addr %s, addr type %u", bd_addr_to_str(server->address), server->address_type);
            server->cap_state = (uint8_t) CAP_DISCOVERY_CSIS_WAIT;
            btp_csip_connect_to_server(server);
            break;
        case CAP_DISCOVERY_CSIS_WAIT:
            server->cap_state = (uint8_t) CAP_DISCOVERY_DONE;
            btp_cap_send_discovery_complete(server);
            break;
        default:
            btstack_unreachable();
            break;
    }
}

static void btp_cap_csip_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_GATTSERVICE_META) return;

    switch (hci_event_gattservice_meta_get_subevent_code(packet)){
        case GATTSERVICE_SUBEVENT_CSIS_CLIENT_RANK:
            if ((response_service_id == BTP_SERVICE_ID_CAP) && (response_op == BTP_CAP_DISCOVER)){
                server_t * server = btp_server_for_csis_cid(gattservice_subevent_csis_client_sirk_get_csis_cid(packet));
                btstack_assert(server != NULL);
                MESSAGE("BTP_CAP_DISCOVER complete");
                btp_cap_discovery_next(server);
            }
            break;
        default:
            break;
    }
}

void btp_cap_handler(uint8_t opcode, uint8_t controller_index, uint16_t length, const uint8_t *data) {
    // provide op info for response
    response_len = 0;
    response_service_id = BTP_SERVICE_ID_CAP;
    response_op = opcode;
    server_t * server;
    switch (opcode) {
        case BTP_CAP_READ_SUPPORTED_COMMANDS:
            MESSAGE("BTP_CAP_READ_SUPPORTED_COMMANDS");
            if (controller_index == BTP_INDEX_NON_CONTROLLER) {
                uint8_t commands = 0;
                btp_send(response_service_id, opcode, controller_index, 1, &commands);
            }
            break;
        case BTP_CAP_DISCOVER:
            if (controller_index == 0) {
                bd_addr_type_t addr_type = (bd_addr_type_t) data[0];
                bd_addr_t address;
                reverse_bd_addr(&data[1], address);

                server = btp_server_for_address(addr_type, address);
                server->cap_state = (uint8_t) CAP_DISOCVERY_IDLE;
                btp_cap_discovery_next(server);

                btp_send(response_service_id, opcode, controller_index, 0, NULL);
            }
            break;
        default:
            break;
    }
}

void btp_cap_init(void) {
    // Clients
    btp_csip_register_higher_layer(&btp_cap_csip_handler);
}
