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
#include "btstack.h"
#include "le-audio/le_audio_base_builder.h"
#include "le_audio_demo_util_source.h"
#include "le-audio/le_audio_base_parser.h"

// CAP
static enum {
    CAP_DISOCVERY_IDLE,
    CAP_DISOCVERY_ASCS_CONNECT,
    CAP_DISCOVERY_ASCS_WAIT,
    CAP_DISCOVERY_CSIP_CONNECT,
    CAP_DISCOVErY_CSIP_WAIT
} btp_cap_discovery_state;

// ASCS Client
#define ASCS_CLIENT_COUNT 2
#define ASCS_CLIENT_NUM_STREAMENDPOINTS 4
static ascs_client_connection_t ascs_connections[ASCS_CLIENT_COUNT];
static ascs_streamendpoint_characteristic_t streamendpoint_characteristics[ASCS_CLIENT_COUNT * ASCS_CLIENT_NUM_STREAMENDPOINTS];

static uint8_t ascs_get_free_slot(void){
    uint8_t i;
    for (i=0;i<ASCS_CLIENT_COUNT;i++){
        MESSAGE("ASCS Clients[%u].state = %u", i, ascs_connections[i].state);
        if (ascs_connections[i].state == AUDIO_STREAM_CONTROL_SERVICE_CLIENT_STATE_IDLE){
            return i;
        }
    }
    btstack_assert(false);
    return 0;
}

static void btp_cap_send_discovery_complete(hci_con_handle_t con_handle) {
    hci_connection_t * hci_connection = hci_connection_for_handle(con_handle);
    btstack_assert(hci_connection != NULL);
    struct btp_cap_discovery_completed_ev discovery_completed_ev;
    discovery_completed_ev.addr_type = hci_connection->address_type;
    reverse_bd_addr(hci_connection->address, discovery_completed_ev.address);
    btp_send(BTP_SERVICE_ID_CAP, BTP_CAP_EV_DISCOVERY_COMPLETED, 0, sizeof(discovery_completed_ev), (const uint8_t *) &discovery_completed_ev);
}

static void btp_cap_discovery_next(hci_con_handle_t con_handle) {
    switch(btp_cap_discovery_state){
        case CAP_DISOCVERY_IDLE:
            // 1. ASCS
            btp_cap_discovery_state = CAP_DISCOVERY_ASCS_WAIT;
            uint8_t ascs_index = ascs_get_free_slot();
            uint16_t ascs_cid;
            audio_stream_control_service_client_connect(&ascs_connections[ascs_index],
                                                        &streamendpoint_characteristics[ascs_index *
                                                                                        ASCS_CLIENT_NUM_STREAMENDPOINTS],
                                                        ASCS_CLIENT_NUM_STREAMENDPOINTS,
                                                        con_handle, &ascs_cid);
            break;
            // 2. CSIP
        default:
            btstack_unreachable();
            break;
    }
}



static void ascs_client_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_GATTSERVICE_META) return;

    ascs_codec_configuration_t codec_configuration;
    hci_con_handle_t con_handle;
    ascs_state_t ase_state;
    uint8_t response_code;
    uint8_t reason;
    uint16_t ascs_cid;
    uint16_t pacs_cid;
    uint8_t ase_id;
    uint8_t num_ases;
    uint8_t i;
    uint8_t opcode;
    hci_connection_t * hci_connection;

    switch (hci_event_gattservice_meta_get_subevent_code(packet)){
        case GATTSERVICE_SUBEVENT_ASCS_CLIENT_CONNECTED:
            ascs_cid = gattservice_subevent_ascs_client_connected_get_ascs_cid(packet);
            con_handle = gattservice_subevent_ascs_client_connected_get_con_handle(packet);
            if (gattservice_subevent_ascs_client_connected_get_status(packet) != ERROR_CODE_SUCCESS){
                // TODO send error response
                MESSAGE("ASCS Client: connection failed, cid 0x%02x, con_handle 0x%04x, status 0x%02x", ascs_cid, con_handle,
                        gattservice_subevent_ascs_client_connected_get_status(packet));
                return;
            }
            // TODO: connect to PACS
            // TODO: connect to BASS
            // TODO: connect to CSIP
            btp_cap_send_discovery_complete(con_handle);
            break;
#if 0
        case GATTSERVICE_SUBEVENT_ASCS_CLIENT_DISCONNECTED:
            MESSAGE("ASCS Client: disconnected, cid 0x%02x", gattservice_subevent_ascs_client_disconnected_get_ascs_cid(packet));
            break;

        case GATTSERVICE_SUBEVENT_ASCS_CLIENT_CODEC_CONFIGURATION:
            ase_id = gattservice_subevent_ascs_client_codec_configuration_get_ase_id(packet);
            ascs_cid = gattservice_subevent_ascs_client_codec_configuration_get_ascs_cid(packet);

            // codec id:
            codec_configuration.coding_format =  gattservice_subevent_ascs_client_codec_configuration_get_coding_format(packet);;
            codec_configuration.company_id = gattservice_subevent_ascs_client_codec_configuration_get_company_id(packet);
            codec_configuration.vendor_specific_codec_id = gattservice_subevent_ascs_client_codec_configuration_get_vendor_specific_codec_id(packet);

            codec_configuration.specific_codec_configuration.codec_configuration_mask = gattservice_subevent_ascs_client_codec_configuration_get_specific_codec_configuration_mask(packet);
            codec_configuration.specific_codec_configuration.sampling_frequency_index = gattservice_subevent_ascs_client_codec_configuration_get_sampling_frequency_index(packet);
            codec_configuration.specific_codec_configuration.frame_duration_index = gattservice_subevent_ascs_client_codec_configuration_get_frame_duration_index(packet);
            codec_configuration.specific_codec_configuration.audio_channel_allocation_mask = gattservice_subevent_ascs_client_codec_configuration_get_audio_channel_allocation_mask(packet);
            codec_configuration.specific_codec_configuration.octets_per_codec_frame = gattservice_subevent_ascs_client_codec_configuration_get_octets_per_frame(packet);
            codec_configuration.specific_codec_configuration.codec_frame_blocks_per_sdu = gattservice_subevent_ascs_client_codec_configuration_get_frame_blocks_per_sdu(packet);

            MESSAGE("ASCS Client: CODEC CONFIGURATION - ase_id %d, ascs_cid 0x%02x", ase_id, ascs_cid);
            response_len = 0;
            btp_append_uint24(gattservice_subevent_ascs_client_codec_configuration_get_presentation_delay_min(packet));
            btp_append_uint24(gattservice_subevent_ascs_client_codec_configuration_get_presentation_delay_max(packet));
            btp_send(BTP_SERVICE_ID_LE_AUDIO, BTP_LE_AUDIO_OP_ASCS_CONFIGURE_CODEC, 0, response_len, response_buffer);
            break;

        case GATTSERVICE_SUBEVENT_ASCS_CLIENT_QOS_CONFIGURATION:
            ase_id  = gattservice_subevent_ascs_server_qos_configuration_get_ase_id(packet);
            ascs_cid = gattservice_subevent_ascs_server_qos_configuration_get_con_handle(packet);

            MESSAGE("ASCS Client: QOS CONFIGURATION - ase_id %d, ascs_cid 0x%02x", ase_id, ascs_cid);
            if (response_op != 0){
                btp_send(BTP_SERVICE_ID_LE_AUDIO, response_op, 0, 0, NULL);
                response_op = 0;
            }
            break;

        case GATTSERVICE_SUBEVENT_ASCS_CLIENT_METADATA:
            ase_id  = gattservice_subevent_ascs_client_metadata_get_ase_id(packet);
            ascs_cid = gattservice_subevent_ascs_client_metadata_get_ascs_cid(packet);

            printf("ASCS Client: METADATA UPDATE - ase_id %d, ascs_cid 0x%02x\n", ase_id, ascs_cid);
            break;

        case GATTSERVICE_SUBEVENT_ASCS_CLIENT_CONTROL_POINT_OPERATION_RESPONSE:
            ase_id        = gattservice_subevent_ascs_client_control_point_operation_response_get_ase_id(packet);
            ascs_cid      = gattservice_subevent_ascs_client_control_point_operation_response_get_ascs_cid(packet);
            response_code = gattservice_subevent_ascs_client_control_point_operation_response_get_response_code(packet);
            reason        = gattservice_subevent_ascs_client_control_point_operation_response_get_reason(packet);
            opcode        = gattservice_subevent_ascs_client_control_point_operation_response_get_opcode(packet);
            printf("ASCS Client: Operation complete - ase_id %d, opcode %u, response [0x%02x, 0x%02x], ascs_cid 0x%02x\n", ase_id, opcode, response_code, reason, ascs_cid);
            if ((opcode == ASCS_OPCODE_RECEIVER_START_READY) && (response_op != 0)){
                MESSAGE("Receiver Start Ready completed");
                btp_send(BTP_SERVICE_ID_LE_AUDIO, response_op, 0, 0, NULL);
                response_op = 0;
            }
            break;

        case GATTSERVICE_SUBEVENT_ASCS_CLIENT_STREAMENDPOINT_STATE:
            log_info("GATTSERVICE_SUBEVENT_ASCS_STREAMENDPOINT_STATE");
            ascs_cid   = gattservice_subevent_ascs_client_streamendpoint_state_get_ascs_cid(packet);
            ase_id     = gattservice_subevent_ascs_client_streamendpoint_state_get_ase_id(packet);
            ase_state  = gattservice_subevent_ascs_client_streamendpoint_state_get_state(packet);
            log_info("ASCS Client: ASE STATE (%s) - ase_id %d, ascs_cid 0x%02x, role %s", ascs_util_ase_state2str(ase_state), ase_id, ascs_cid,
                     (audio_stream_control_service_client_get_ase_role(ascs_cid, ase_id) == LE_AUDIO_ROLE_SOURCE) ? "SOURCE" : "SINK" );

            // stop streaming if remote is a sink
            if ((ase_state == ASCS_STATE_RELEASING) && audio_stream_control_service_client_get_ase_role(ascs_cid, ase_id) == LE_AUDIO_ROLE_SINK){
                log_info("Releasing on Sink Stream Endpoint -> stop sending on all CIS");
                for (i=0;i<MAX_CHANNELS;i++){
                    cis_con_handles[i] = HCI_CON_HANDLE_INVALID;
                }
            }

            // send done
            if (response_service_id == BTP_SERVICE_ID_LE_AUDIO){
                switch (response_op){
                    case BTP_LE_AUDIO_OP_ASCS_ENABLE:
                        switch (ase_state){
                            case ASCS_STATE_ENABLING:
                                btp_send(BTP_SERVICE_ID_LE_AUDIO, response_op, 0, 0, NULL);
                                response_op = 0;
                                break;
                            default:
                                break;
                        }
                        break;
                    case BTP_LE_AUDIO_OP_ASCS_RECEIVER_START_READY:
                        // moved to control operation complete
                        break;
                    case BTP_LE_AUDIO_OP_ASCS_RECEIVER_STOP_READY:
                        switch (ase_state){
                            case ASCS_STATE_IDLE:
                            case ASCS_STATE_QOS_CONFIGURED:
                                btp_send(BTP_SERVICE_ID_LE_AUDIO, response_op, 0, 0, NULL);
                                response_op = 0;
                                break;
                            default:
                                break;
                        }
                        break;
                    case BTP_LE_AUDIO_OP_ASCS_DISABLE:
                        switch (ase_state){
                            case ASCS_STATE_DISABLING:
                            case ASCS_STATE_QOS_CONFIGURED:
                                btp_send(BTP_SERVICE_ID_LE_AUDIO, response_op, 0, 0, NULL);
                                response_op = 0;
                                break;
                            default:
                                break;
                        }
                        break;
                    case BTP_LE_AUDIO_OP_ASCS_RELEASE:
                        btp_send(BTP_SERVICE_ID_LE_AUDIO, response_op, 0, 0, NULL);
                        response_op = 0;
                        break;
                    case BTP_LE_AUDIO_OP_ASCS_UPDATE_METADATA:
                        btp_send(BTP_SERVICE_ID_LE_AUDIO, response_op, 0, 0, NULL);
                        response_op = 0;
                        break;
                    default:
                        break;
                }
            }
            break;
#endif
        default:
            break;
    }
}

static void pacs_client_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_GATTSERVICE_META) return;

    uint32_t audio_allocation_mask;
    uint16_t ascs_cid;
    uint16_t pacs_cid;
    hci_con_handle_t con_handle;
    uint8_t i;
    uint8_t status;

    switch (hci_event_gattservice_meta_get_subevent_code(packet)){
        case GATTSERVICE_SUBEVENT_PACS_CLIENT_CONNECTED:
            pacs_cid = gattservice_subevent_pacs_client_connected_get_pacs_cid(packet);
            con_handle = gattservice_subevent_pacs_client_connected_get_con_handle(packet);
            status = gattservice_subevent_pacs_client_connected_get_status(packet);
            if (status != ERROR_CODE_SUCCESS){
                // TODO send error response
                MESSAGE("PACS Client: connection failed, cid 0x%02x, con_handle 0x%02x, status 0x%02x", pacs_cid, con_handle, status);
                return;
            }
            break;
#if 0
        case GATTSERVICE_SUBEVENT_PACS_CLIENT_DISCONNECTED:
            pacs_cid = 0;
            MESSAGE("PACS Client: disconnected\n");
            break;

        case GATTSERVICE_SUBEVENT_PACS_CLIENT_OPERATION_DONE:
            if (gattservice_subevent_pacs_client_operation_done_get_status(packet) == ERROR_CODE_SUCCESS){
                MESSAGE("      Operation successful");
            } else {
                MESSAGE("      Operation failed with status 0x%02X", gattservice_subevent_pacs_client_operation_done_get_status(packet));
            }
            break;

        case GATTSERVICE_SUBEVENT_PACS_CLIENT_AUDIO_LOCATIONS:
            audio_allocation_mask = gattservice_subevent_pacs_client_audio_locations_get_audio_locations_mask(packet);
            MESSAGE("PACS Client: %s Audio Locations 0x%04x",
                    gattservice_subevent_pacs_client_audio_locations_get_le_audio_role(packet) == LE_AUDIO_ROLE_SINK ? "Sink" : "Source", audio_allocation_mask);
            break;

        case GATTSERVICE_SUBEVENT_PACS_CLIENT_AVAILABLE_AUDIO_CONTEXTS:
            MESSAGE("PACS Client: Available Audio Contexts");
            MESSAGE("      Sink   0x%02X", gattservice_subevent_pacs_client_available_audio_contexts_get_sink_mask(packet));
            MESSAGE("      Source 0x%02X", gattservice_subevent_pacs_client_available_audio_contexts_get_source_mask(packet));
            break;

        case GATTSERVICE_SUBEVENT_PACS_CLIENT_SUPPORTED_AUDIO_CONTEXTS:
            MESSAGE("PACS Client: Supported Audio Contexts\n");
            MESSAGE("      Sink   0x%02X\n", gattservice_subevent_pacs_client_supported_audio_contexts_get_sink_mask(packet));
            MESSAGE("      Source 0x%02X\n", gattservice_subevent_pacs_client_supported_audio_contexts_get_source_mask(packet));
            break;

        case GATTSERVICE_SUBEVENT_PACS_CLIENT_PACK_RECORD:
            MESSAGE("PACS Client: %s PAC Record\n", gattservice_subevent_pacs_client_pack_record_get_le_audio_role(packet) == LE_AUDIO_ROLE_SINK ? "Sink" : "Source");
            MESSAGE("      %s PAC Record DONE\n", gattservice_subevent_pacs_client_pack_record_done_get_le_audio_role(packet) == LE_AUDIO_ROLE_SINK ? "Sink" : "Source");
            break;
        case GATTSERVICE_SUBEVENT_PACS_CLIENT_PACK_RECORD_DONE:
            MESSAGE("      %s PAC Record DONE\n", gattservice_subevent_pacs_client_pack_record_done_get_le_audio_role(packet) == LE_AUDIO_ROLE_SINK ? "Sink" : "Source");
            break;
#endif
        default:
            break;
    }
}

void btp_cap_handler(uint8_t opcode, uint8_t controller_index, uint16_t length, const uint8_t *data) {
    // provide op info for response
    response_len = 0;
    response_service_id = BTP_SERVICE_ID_CAP;
    response_op = opcode;
    uint8_t ase_id;
    uint8_t i;
    uint8_t ascs_index;
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
                const hci_connection_t * hci_connection = hci_connection_for_bd_addr_and_type(address, addr_type);
                btstack_assert(hci_connection != NULL);
                hci_con_handle_t con_handle = hci_connection->con_handle;
                MESSAGE("BTP_CAP_DISCOVER %s, con handle 0x%04x", bd_addr_to_str(address), con_handle);

                btp_cap_discovery_state = CAP_DISOCVERY_IDLE;
                btp_cap_discovery_next(con_handle);

                btp_send(response_service_id, opcode, controller_index, 0, NULL);
            }
            break;
        default:
            break;
    }
}

void btp_cap_init(void) {
    // Clients
    audio_stream_control_service_client_init(&ascs_client_event_handler);
    published_audio_capabilities_service_client_init(&pacs_client_event_handler);
}
