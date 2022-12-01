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

#define BTSTACK_FILE__ "btp_le_audio.c"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "btstack_debug.h"
#include "btpclient.h"
#include "btp.h"
#include "btstack.h"

#define ASCS_CLIENT_NUM_STREAMENDPOINTS 4

#define MAX_CHANNELS 2
#define MAX_NUM_CIS  2
#define MAX_LC3_FRAME_BYTES   155

static btstack_packet_callback_registration_t hci_event_callback_registration;

// ASCS
static ascs_client_connection_t ascs_connection;
static uint8_t ase_count;
static ascs_streamendpoint_characteristic_t streamendpoint_characteristics[ASCS_CLIENT_NUM_STREAMENDPOINTS];
static uint16_t ascs_cid;
static uint8_t  ase_index = 0;
static ascs_client_codec_configuration_request_t ascs_codec_configuration_request;
static ascs_qos_configuration_t ascs_qos_configuration;
static le_audio_metadata_t ascs_metadata;

// PACS
static uint16_t pacs_cid;
static pacs_client_connection_t pacs_connection;
static uint32_t pacs_audio_locations_sink;
static uint32_t pacs_audio_locations_source;

// CIG/CIS
static le_audio_cig_t        cig;
static le_audio_cig_params_t cig_params;
static uint16_t cig_frame_duration_us;
static uint8_t  cig_framed;
static uint8_t  cig_num_cis;
static uint8_t  cis_num_channels;
static uint16_t cis_sampling_frequency_hz;
static uint16_t cis_max_octets_per_frame;
static hci_con_handle_t cis_con_handles[MAX_CHANNELS];
static bool cis_established[MAX_CHANNELS];
static unsigned int     cis_setup_next_index;
static uint16_t packet_sequence_numbers[MAX_NUM_CIS];
static uint8_t iso_payload[MAX_CHANNELS * MAX_LC3_FRAME_BYTES];


static le_audio_role_t bap_get_ase_role(uint8_t ase_index){
    return streamendpoint_characteristics[ase_index].role;
}

static void bap_service_client_setup_cig(void){
    printf("Create CIG\n");

    uint8_t num_cis = 1;
    cig_params.cig_id = 1;
    cig_params.num_cis = 1;
    cig_params.sdu_interval_c_to_p = cig_frame_duration_us;
    cig_params.sdu_interval_p_to_c = cig_frame_duration_us;
    cig_params.worst_case_sca = 0; // 251 ppm to 500 ppm
    cig_params.packing = 0;        // sequential
    cig_params.framing = cig_framed;
    cig_params.max_transport_latency_c_to_p = 40;
    cig_params.max_transport_latency_p_to_c = 40;
    uint8_t i;
    for (i=0; i < num_cis; i++){
        cig_params.cis_params[i].cis_id = 1+i;
        cig_params.cis_params[i].max_sdu_c_to_p = (pacs_audio_locations_sink != 0)   ? cis_max_octets_per_frame : 0;
        cig_params.cis_params[i].max_sdu_p_to_c = (pacs_audio_locations_source != 0) ? cis_max_octets_per_frame : 0;
        cig_params.cis_params[i].phy_c_to_p = 2;  // 2M
        cig_params.cis_params[i].phy_p_to_c = 2;  // 2M
        cig_params.cis_params[i].rtn_c_to_p = 2;
        cig_params.cis_params[i].rtn_p_to_c = 2;
    }

    gap_cig_create(&cig, &cig_params);
}

static void bap_service_client_setup_cis(void){
    uint8_t i;
    cis_setup_next_index = 0;
    printf("Create CIS\n");
    hci_con_handle_t acl_connection_handles[MAX_CHANNELS];
    for (i=0; i < cig_num_cis; i++){
        acl_connection_handles[i] = remote_handle;
    }
    gap_cis_create(cig_params.cig_id, cis_con_handles, acl_connection_handles);
}


static void send_iso_packet(uint8_t cis_index){
    bool ok = hci_reserve_packet_buffer();
    btstack_assert(ok);
    uint8_t * buffer = hci_get_outgoing_packet_buffer();
    // complete SDU, no TimeStamp
    little_endian_store_16(buffer, 0, cis_con_handles[cis_index] | (2 << 12));
    // len
    little_endian_store_16(buffer, 2, 0 + 4 + cis_num_channels * cis_max_octets_per_frame);
    // TimeStamp if TS flag is set
    // packet seq nr
    little_endian_store_16(buffer, 4, packet_sequence_numbers[cis_index]);
    // iso sdu len
    little_endian_store_16(buffer, 6, cis_num_channels * cis_max_octets_per_frame);
    // copy encoded payload
    uint8_t i;
    uint16_t offset = 8;
    for (i=0; i<cis_num_channels;i++) {
        memcpy(&buffer[offset], &iso_payload[i * MAX_LC3_FRAME_BYTES], cis_max_octets_per_frame);
        offset += cis_max_octets_per_frame;
    }

    // send
    hci_send_iso_packet_buffer(offset);

    packet_sequence_numbers[cis_index]++;
}

// HCI Handler

static void hci_packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    hci_con_handle_t con_handle;
    hci_con_handle_t cis_con_handle;
    uint8_t i;

    switch (packet_type) {
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case HCI_EVENT_CIS_CAN_SEND_NOW:
                    cis_con_handle = hci_event_cis_can_send_now_get_cis_con_handle(packet);
                    send_iso_packet(0);
                    hci_request_cis_can_send_now_events(cis_con_handles[0]);
                    break;
                case HCI_EVENT_META_GAP:
                    switch (hci_event_gap_meta_get_subevent_code(packet)) {
                        case GAP_SUBEVENT_CIG_CREATED:
                        MESSAGE("CIS Connection Handles: ");
                            for (i = 0; i < cig_num_cis; i++) {
                                cis_con_handles[i] = gap_subevent_cig_created_get_cis_con_handles(packet, i);
                                MESSAGE("0x%04x ", cis_con_handles[i]);
                            }
                            MESSAGE("ASCS Client: Configure QoS %u us, ASE[%d]", cig_params.sdu_interval_p_to_c,
                                    ase_index);
                            MESSAGE("       NOTE: Only one CIS supported");

                            ascs_qos_configuration.cis_id = cig_params.cis_params[0].cis_id;
                            ascs_qos_configuration.cig_id = gap_subevent_cig_created_get_cig_id(packet);

                            audio_stream_control_service_client_streamendpoint_configure_qos(ascs_cid, ase_index,
                                                                                             &ascs_qos_configuration);
                            break;
                        case GAP_SUBEVENT_CIS_CREATED:
                            if (bap_get_ase_role(ase_index) == LE_AUDIO_ROLE_SOURCE){
                                MESSAGE("CIS Established, remote role source");
                            } else {
                                MESSAGE("CIS Established, remote role sink");
                            }
                            if (response_op == BTP_LE_AUDIO_OP_ASCS_ENABLE){
                                btp_send(BTP_SERVICE_ID_LE_AUDIO, response_op, 0, 0, NULL);
                                response_op = 0;
                            }
                            break;
                        default:
                            break;
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

// ASCS
void ascs_client_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_GATTSERVICE_META) return;

    ascs_codec_configuration_t codec_configuration;
    uint8_t ase_id;
    hci_con_handle_t con_handle;
    ascs_state_t ase_state;
    uint8_t response_code;
    uint8_t reason;

    switch (hci_event_gattservice_meta_get_subevent_code(packet)){
        case GATTSERVICE_SUBEVENT_ASCS_REMOTE_SERVER_CONNECTED:
            if (gattservice_subevent_ascs_remote_server_connected_get_status(packet) != ERROR_CODE_SUCCESS){
                ascs_cid = 0;
                // TODO send error response
                MESSAGE("ASCS Client: connection failed, cid 0x%02x, con_handle 0x%02x, status 0x%02x", ascs_cid, remote_handle,
                        gattservice_subevent_ascs_remote_server_connected_get_status(packet));
                return;
            }
            ase_count = gattservice_subevent_ascs_remote_server_connected_get_num_streamendpoints(packet);
            MESSAGE("ASCS Client: connected, cid 0x%02x, num ASEs: %u", ascs_cid, ase_count);

            published_audio_capabilities_service_client_connect(&pacs_connection, remote_handle, &pacs_cid);
            break;

        case GATTSERVICE_SUBEVENT_ASCS_REMOTE_SERVER_DISCONNECTED:
            if (ascs_cid != gattservice_subevent_ascs_remote_server_disconnected_get_ascs_cid(packet)){
                return;
            }
            ascs_cid = 0;
            MESSAGE("ASCS Client: disconnected, cid 0x%02x", gattservice_subevent_ascs_remote_server_disconnected_get_ascs_cid(packet));
            break;

        case GATTSERVICE_SUBEVENT_ASCS_CODEC_CONFIGURATION:
            ase_id     = gattservice_subevent_ascs_codec_configuration_get_ase_id(packet);
            con_handle = gattservice_subevent_ascs_codec_configuration_get_con_handle(packet);

            // codec id:
            codec_configuration.coding_format =  gattservice_subevent_ascs_codec_configuration_get_coding_format(packet);;
            codec_configuration.company_id = gattservice_subevent_ascs_codec_configuration_get_company_id(packet);
            codec_configuration.vendor_specific_codec_id = gattservice_subevent_ascs_codec_configuration_get_vendor_specific_codec_id(packet);

            codec_configuration.specific_codec_configuration.codec_configuration_mask = gattservice_subevent_ascs_codec_configuration_get_specific_codec_configuration_mask(packet);
            codec_configuration.specific_codec_configuration.sampling_frequency_index = gattservice_subevent_ascs_codec_configuration_get_sampling_frequency_index(packet);
            codec_configuration.specific_codec_configuration.frame_duration_index = gattservice_subevent_ascs_codec_configuration_get_frame_duration_index(packet);
            codec_configuration.specific_codec_configuration.audio_channel_allocation_mask = gattservice_subevent_ascs_codec_configuration_get_audio_channel_allocation_mask(packet);
            codec_configuration.specific_codec_configuration.octets_per_codec_frame = gattservice_subevent_ascs_codec_configuration_get_octets_per_frame(packet);
            codec_configuration.specific_codec_configuration.codec_frame_blocks_per_sdu = gattservice_subevent_ascs_codec_configuration_get_frame_blocks_per_sdu(packet);

            MESSAGE("ASCS Client: CODEC CONFIGURATION - ase_id %d, con_handle 0x%02x", ase_id, con_handle);
            response_len = 0;
            btp_append_uint24(gattservice_subevent_ascs_codec_configuration_get_presentation_delay_min(packet));
            btp_append_uint24(gattservice_subevent_ascs_codec_configuration_get_presentation_delay_max(packet));
            btp_send(BTP_SERVICE_ID_LE_AUDIO, BTP_LE_AUDIO_OP_ASCS_CONFIGURE_CODEC, 0, response_len, response_buffer);
            break;

        case GATTSERVICE_SUBEVENT_ASCS_QOS_CONFIGURATION:
            ase_id     = gattservice_subevent_ascs_qos_configuration_get_ase_id(packet);
            con_handle = gattservice_subevent_ascs_qos_configuration_get_con_handle(packet);

            MESSAGE("ASCS Client: QOS CONFIGURATION - ase_id %d, con_handle 0x%02x", ase_id, con_handle);
            if (response_op != 0){
                btp_send(BTP_SERVICE_ID_LE_AUDIO, response_op, 0, 0, NULL);
                response_op = 0;
            }
            break;

        case GATTSERVICE_SUBEVENT_ASCS_METADATA:
            ase_id     = gattservice_subevent_ascs_metadata_get_ase_id(packet);
            con_handle = gattservice_subevent_ascs_metadata_get_con_handle(packet);

            printf("ASCS Client: METADATA UPDATE - ase_id %d, con_handle 0x%02x\n", ase_id, con_handle);
            break;

        case GATTSERVICE_SUBEVENT_ASCS_CONTROL_POINT_OPERATION_RESPONSE:
            ase_id        = gattservice_subevent_ascs_control_point_operation_response_get_ase_id(packet);
            con_handle    = gattservice_subevent_ascs_control_point_operation_response_get_con_handle(packet);
            response_code = gattservice_subevent_ascs_control_point_operation_response_get_response_code(packet);
            reason        = gattservice_subevent_ascs_control_point_operation_response_get_reason(packet);

            printf("            OPERATION STATUS - ase_id %d, response [0x%02x, 0x%02x], con_handle 0x%02x\n", ase_id, response_code, reason, con_handle);
            break;

        case GATTSERVICE_SUBEVENT_ASCS_STREAMENDPOINT_STATE:
            log_info("GATTSERVICE_SUBEVENT_ASCS_STREAMENDPOINT_STATE");
            con_handle = gattservice_subevent_ascs_streamendpoint_state_get_con_handle(packet);
            ase_id     = gattservice_subevent_ascs_streamendpoint_state_get_ase_id(packet);
            ase_state  = gattservice_subevent_ascs_streamendpoint_state_get_state(packet);

            log_info("ASCS Client: ASE STATE (%s) - ase_id %d, con_handle 0x%02x, role %s", ascs_util_ase_state2str(ase_state), ase_id, con_handle,
                     (bap_get_ase_role(ase_index) == LE_AUDIO_ROLE_SOURCE) ? "SOURCE" : "SINK" );
            // send done
            if (response_service_id == BTP_SERVICE_ID_LE_AUDIO){
                switch (response_op){
                    case BTP_LE_AUDIO_OP_ASCS_ENABLE:
                        switch (ase_state){
                            case ASCS_STATE_ENABLING:
                                log_info("Setup ISO Channel\n");
                                bap_service_client_setup_cis();
                                break;
                            case ASCS_STATE_STREAMING:
                                btp_send(BTP_SERVICE_ID_LE_AUDIO, response_op, 0, 0, NULL);
                                response_op = 0;
                                break;
                            default:
                                break;
                        }
                        break;
                    case BTP_LE_AUDIO_OP_ASCS_RECEIVER_START_READY:
                        switch (ase_state){
                            case ASCS_STATE_STREAMING:
                                btp_send(BTP_SERVICE_ID_LE_AUDIO, response_op, 0, 0, NULL);
                                response_op = 0;
                                break;
                            default:
                                break;
                        }
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
            // start sending if SINK ASE
            if ((ase_state == ASCS_STATE_STREAMING) && (bap_get_ase_role(ase_index) == LE_AUDIO_ROLE_SINK)){
                hci_request_cis_can_send_now_events(cis_con_handles[0]);
            }
            break;

        default:
            break;
    }
}

// PACS
static void pacs_client_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_GATTSERVICE_META) return;

    switch (hci_event_gattservice_meta_get_subevent_code(packet)){
        case GATTSERVICE_SUBEVENT_PACS_CONNECTED:
            if (gattservice_subevent_pacs_connected_get_status(packet) != ERROR_CODE_SUCCESS){
                ascs_cid = 0;
                // TODO send error response
                MESSAGE("PACS Client: connection failed, cid 0x%02x, con_handle 0x%02x, status 0x%02x", pacs_cid, remote_handle,
                        gattservice_subevent_pacs_connected_get_status(packet));
                return;
            }
            MESSAGE("PACS client: connected, cid 0x%02x", pacs_cid);

            // TODO: provide separate API for PACS
            uint8_t i;
            response_len = 0;
            btp_append_uint8((uint8_t) ascs_cid);
            btp_append_uint8(ase_count);
            for (i=0;i<ase_count;i++){
                btp_append_uint8(streamendpoint_characteristics[i].ase_id);
                btp_append_uint8((uint8_t) streamendpoint_characteristics[i].role);
            }
            btp_send(BTP_SERVICE_ID_LE_AUDIO, BTP_LE_AUDIO_OP_ASCS_CONNECT, 0, response_len,  response_buffer);
            break;

        case GATTSERVICE_SUBEVENT_PACS_DISCONNECTED:
            pacs_cid = 0;
            MESSAGE("PACS Client: disconnected\n");
            break;

        case GATTSERVICE_SUBEVENT_PACS_OPERATION_DONE:
            if (gattservice_subevent_pacs_operation_done_get_status(packet) == ERROR_CODE_SUCCESS){
                MESSAGE("      Operation successful");
            } else {
                MESSAGE("      Operation failed with status 0x%02X", gattservice_subevent_pacs_operation_done_get_status(packet));
            }
            break;

        case GATTSERVICE_SUBEVENT_PACS_AUDIO_LOCATIONS:
                 MESSAGE("PACS Client: %s Audio Locations 0x%04x",
                   gattservice_subevent_pacs_audio_locations_get_le_audio_role(packet) == LE_AUDIO_ROLE_SINK ? "Sink" : "Source",
                   gattservice_subevent_pacs_audio_locations_get_audio_location_mask(packet));
                if (gattservice_subevent_pacs_audio_locations_get_le_audio_role(packet) == LE_AUDIO_ROLE_SINK){
                    // 1. SINK Locations
                    pacs_audio_locations_sink = gattservice_subevent_pacs_audio_locations_get_audio_location_mask(packet);
                    // skip check for SOURCE locations if not available
                    if (pacs_connection.pacs_characteristics[(uint8_t)PACS_CLIENT_CHARACTERISTIC_INDEX_SOURCE_AUDIO_LOCATIONS].value_handle != 0) {
                        published_audio_capabilities_service_client_get_source_audio_locations(pacs_cid);
                        return;
                    }
                } else {
                    // 2. SOURCE Locations
                    pacs_audio_locations_source = gattservice_subevent_pacs_audio_locations_get_audio_location_mask(packet);
                }
                // continue ASCS Configure Codec operation
                ascs_codec_configuration_request.specific_codec_configuration.audio_channel_allocation_mask =
                        pacs_audio_locations_sink | pacs_audio_locations_source;
                audio_stream_control_service_client_streamendpoint_configure_codec(ascs_cid, ase_index, &ascs_codec_configuration_request);
            break;

        case GATTSERVICE_SUBEVENT_PACS_AVAILABLE_AUDIO_CONTEXTS:
            MESSAGE("PACS Client: Available Audio Contexts");
            MESSAGE("      Sink   0x%02X", gattservice_subevent_pacs_available_audio_contexts_get_sink_mask(packet));
            MESSAGE("      Source 0x%02X", gattservice_subevent_pacs_available_audio_contexts_get_source_mask(packet));
            break;

        case GATTSERVICE_SUBEVENT_PACS_SUPPORTED_AUDIO_CONTEXTS:
            MESSAGE("PACS Client: Supported Audio Contexts\n");
            MESSAGE("      Sink   0x%02X\n", gattservice_subevent_pacs_supported_audio_contexts_get_sink_mask(packet));
            MESSAGE("      Source 0x%02X\n", gattservice_subevent_pacs_supported_audio_contexts_get_source_mask(packet));
            break;

        case GATTSERVICE_SUBEVENT_PACS_PACK_RECORD:
            MESSAGE("PACS Client: %s PAC Record\n", gattservice_subevent_pacs_pack_record_get_le_audio_role(packet) == LE_AUDIO_ROLE_SINK ? "Sink" : "Source");
            MESSAGE("      %s PAC Record DONE\n", gattservice_subevent_pacs_pack_record_done_get_le_audio_role(packet) == LE_AUDIO_ROLE_SINK ? "Sink" : "Source");
            break;
        case GATTSERVICE_SUBEVENT_PACS_PACK_RECORD_DONE:
            MESSAGE("      %s PAC Record DONE\n", gattservice_subevent_pacs_pack_record_done_get_le_audio_role(packet) == LE_AUDIO_ROLE_SINK ? "Sink" : "Source");
            break;

        default:
            break;
    }
}

// BTP LE Audio
void btp_le_audio_handler(uint8_t opcode, uint8_t controller_index, uint16_t length, const uint8_t *data) {
    // provide op info for response
    response_len = 0;
    response_service_id = BTP_SERVICE_ID_LE_AUDIO;
    response_op = opcode;
    switch (opcode) {
        case BTP_LE_AUDIO_OP_READ_SUPPORTED_COMMANDS:
            MESSAGE("BTP_LE_AUDIO_OP_READ_SUPPOERTED_COMMANDS");
            if (controller_index == BTP_INDEX_NON_CONTROLLER) {
                uint8_t commands = 0;
                btp_send(BTP_SERVICE_ID_GATT, opcode, controller_index, 1, &commands);
            }
            break;
        case BTP_LE_AUDIO_OP_ASCS_CONNECT:
            MESSAGE("BTP_LE_AUDIO_OP_ASCS_CONNECT");
            if (controller_index == 0) {
                audio_stream_control_service_client_connect(&ascs_connection,
                                                            streamendpoint_characteristics,
                                                            ASCS_CLIENT_NUM_STREAMENDPOINTS,
                                                            remote_handle, &ascs_cid);
            }
            break;
        case BTP_LE_AUDIO_OP_ASCS_CONFIGURE_CODEC:
            if (controller_index == 0){
                // ascs_cid in data[0]
                uint8_t  ase_index         = data[1];
                uint8_t  coding_format     = data[2];
                uint32_t frequency_hz      = little_endian_read_32(data, 3);
                uint16_t frame_duration_us = little_endian_read_16(data, 7);
                uint16_t octets_per_frame  = little_endian_read_16(data, 9);
                MESSAGE("BTP_LE_AUDIO_OP_ASCS_CONFIGURE_CODEC ase %u, format %x, freq %u, duration %u, octets %u",
                        ase_index, coding_format, frequency_hz, frame_duration_us, octets_per_frame);

                ascs_specific_codec_configuration_t * sc_config = &ascs_codec_configuration_request.specific_codec_configuration;
                sc_config->codec_configuration_mask = coding_format == HCI_AUDIO_CODING_FORMAT_LC3 ? 0x3E : 0x1e;
                le_audio_codec_sampling_frequency_index_t frequency_index = 0;
                switch(frequency_hz){
                    case 8000:
                        frequency_index = LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_8000_HZ;
                        break;
                    case 16000:
                        frequency_index = LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_16000_HZ;
                        break;
                    case 24000:
                        frequency_index = LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_24000_HZ;
                        break;
                    case 32000:
                        frequency_index = LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_32000_HZ;
                        break;
                    case 44100:
                        frequency_index = LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_44100_HZ;
                        break;
                    case 48000:
                        frequency_index = LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_48000_HZ;
                        break;
                    default:
                        btstack_unreachable();
                        break;
                }
                sc_config->sampling_frequency_index = frequency_index;
                sc_config->frame_duration_index =  frame_duration_us == 7500
                                                   ? LE_AUDIO_CODEC_FRAME_DURATION_INDEX_7500US : LE_AUDIO_CODEC_FRAME_DURATION_INDEX_10000US;
                sc_config->octets_per_codec_frame = octets_per_frame;
                sc_config->audio_channel_allocation_mask = LE_AUDIO_LOCATION_MASK_FRONT_LEFT;
                sc_config->codec_frame_blocks_per_sdu = 1;

                ascs_codec_configuration_request.target_latency = LE_AUDIO_CLIENT_TARGET_LATENCY_LOW_LATENCY;
                ascs_codec_configuration_request.target_phy = LE_AUDIO_CLIENT_TARGET_PHY_BALANCED;
                ascs_codec_configuration_request.coding_format = coding_format;
                ascs_codec_configuration_request.company_id = 0;
                ascs_codec_configuration_request.vendor_specific_codec_id = 0;

                // also prepare CIG/CIS
                cis_sampling_frequency_hz = frequency_hz;
                cig_num_cis               = 1;
                cis_num_channels          = 1;

                // perform PACS query to get audio channel allocation mask first
                // audio_stream_control_service_client_streamendpoint_configure_codec(ascs_cid, ase_index, &ascs_codec_configuration_request);

                // TODO: published_audio_capabilities_service_client_get_sink_audio_locations does not return error if there's no sink audio location characteristic
                uint8_t status;
                if (pacs_connection.pacs_characteristics[(uint8_t)PACS_CLIENT_CHARACTERISTIC_INDEX_SINK_AUDIO_LOCATIONS].value_handle != 0){
                    status = published_audio_capabilities_service_client_get_sink_audio_locations(pacs_cid);
                } else {
                    // assume at least sink or source audio location characteristic available
                    status = published_audio_capabilities_service_client_get_source_audio_locations(pacs_cid);
                }
                if (status){
                    MESSAGE("PACS Client error 0x%02x", status);
                }
                break;
            }
            break;
        case BTP_LE_AUDIO_OP_ASCS_CONFIGURE_QOS:
            if (controller_index == 0){
                // ascs_cid in data[0]
                uint8_t  ase_index                = data[1];
                uint16_t sdu_interval_us          = little_endian_read_16(data, 2);
                uint8_t  framing                  = data[4];
                uint32_t max_sdu                  = little_endian_read_16(data, 5);
                uint8_t  retransmission_number    = data[7];
                uint8_t  max_transport_latency_ms = data[8];
                MESSAGE("BTP_LE_AUDIO_OP_ASCS_CONFIGURE_QOS ase %u, interval %u, framing %u, sdu_size %u, retrans %u, latency %u",
                        ase_index, sdu_interval_us, framing, max_sdu, retransmission_number, max_transport_latency_ms);

                ascs_qos_configuration.sdu_interval = sdu_interval_us;
                ascs_qos_configuration.framing = framing;
                ascs_qos_configuration.phy = LE_AUDIO_SERVER_PHY_MASK_NO_PREFERENCE;
                ascs_qos_configuration.max_sdu = max_sdu;
                ascs_qos_configuration.retransmission_number = retransmission_number;
                ascs_qos_configuration.max_transport_latency_ms = max_transport_latency_ms;
                ascs_qos_configuration.presentation_delay_us = 40000;

                // update CIG/CIS
                cig_frame_duration_us = sdu_interval_us;
                cig_framed = framing;
                cis_max_octets_per_frame = max_sdu;
                bap_service_client_setup_cig();
            }
            break;
        case BTP_LE_AUDIO_OP_ASCS_ENABLE:
            if (controller_index == 0) {
                // ascs_cid in data[0]
                uint8_t  ase_index = data[1];
                MESSAGE("BTP_LE_AUDIO_OP_ASCS_ENABLE ase %u", ase_index);
                audio_stream_control_service_client_streamendpoint_enable(ascs_cid, ase_index);
            }
            break;
        case BTP_LE_AUDIO_OP_ASCS_RECEIVER_START_READY:
            if (controller_index == 0) {
                // ascs_cid in data[0]
                uint8_t  ase_index = data[1];
                MESSAGE("btstack: add le_audio tests ase %u", ase_index);
                audio_stream_control_service_client_streamendpoint_receiver_start_ready(ascs_cid, ase_index);
            }
            break;
        case BTP_LE_AUDIO_OP_ASCS_RECEIVER_STOP_READY:
            if (controller_index == 0) {
                // ascs_cid in data[0]
                uint8_t  ase_index = data[1];
                MESSAGE("BTP_LE_AUDIO_OP_ASCS_RECEIVER_STOP_READY ase %u", ase_index);
                audio_stream_control_service_client_streamendpoint_receiver_stop_ready(ascs_cid, ase_index);
            }
            break;
        case BTP_LE_AUDIO_OP_ASCS_DISABLE:
            if (controller_index == 0) {
                // ascs_cid in data[0]
                uint8_t  ase_index = data[1];
                MESSAGE("BTP_LE_AUDIO_OP_ASCS_DISABLE ase %u", ase_index);
                audio_stream_control_service_client_streamendpoint_disable(ascs_cid, ase_index);
            }
            break;
        case BTP_LE_AUDIO_OP_ASCS_RELEASE:
            if (controller_index == 0) {
                // ascs_cid in data[0]
                uint8_t  ase_index = data[1];
                MESSAGE("BTP_LE_AUDIO_OP_ASCS_RELEASE ase %u", ase_index);
                audio_stream_control_service_client_streamendpoint_release(ascs_cid, ase_index, false);
            }
            break;
        case BTP_LE_AUDIO_OP_ASCS_UPDATE_METADATA:
            if (controller_index == 0){
                // ascs_cid in data[0]
                uint8_t  ase_index = data[1];
                MESSAGE("BTP_LE_AUDIO_OP_ASCS_UPDATE_METADATA ase %u", ase_index);
                ascs_metadata.metadata_mask = (1 << LE_AUDIO_METADATA_TYPE_PREFERRED_AUDIO_CONTEXTS) & (1 << LE_AUDIO_METADATA_TYPE_STREAMING_AUDIO_CONTEXTS);
                ascs_metadata.preferred_audio_contexts_mask = LE_AUDIO_CONTEXT_MASK_MEDIA;
                ascs_metadata.streaming_audio_contexts_mask = LE_AUDIO_CONTEXT_MASK_MEDIA;
                audio_stream_control_service_client_streamendpoint_metadata_update(ascs_cid, ase_index, &ascs_metadata);
            }
            break;
        default:
            break;
    }
}

void btp_le_audio_init(void){
    // register for HCI events
    hci_event_callback_registration.callback = &hci_packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    audio_stream_control_service_client_init(&ascs_client_event_handler);
    published_audio_capabilities_service_client_init(&pacs_client_event_handler);
}

