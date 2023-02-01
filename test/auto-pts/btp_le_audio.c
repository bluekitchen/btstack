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
#include "gatt_profiles_bap.h"

#define ASCS_CLIENT_NUM_STREAMENDPOINTS 4
#define ASCS_CLIENT_COUNT 2
#define PACS_CLIENT_COUNT 2

#define MAX_CHANNELS 2
#define MAX_NUM_CIS  2
#define MAX_LC3_FRAME_BYTES   155
static btstack_packet_callback_registration_t hci_event_callback_registration;


// ASCS Client
static ascs_client_connection_t ascs_connections[ASCS_CLIENT_COUNT];
static ascs_streamendpoint_characteristic_t streamendpoint_characteristics[ASCS_CLIENT_COUNT * ASCS_CLIENT_NUM_STREAMENDPOINTS];

// valid only during current request
// - ascs connect
static uint8_t ascs_index;
static uint8_t source_ase_count;
static uint8_t sink_ase_count;
// - ascs configure codec
static uint8_t ase_id_used_by_configure_codec = 0;
static ascs_client_codec_configuration_request_t ascs_codec_configuration_request;

// - ascs configure qos
static ascs_qos_configuration_t ascs_qos_configuration;
static le_audio_metadata_t ascs_metadata;


// ASCS Server
#define ASCS_NUM_STREAMENDPOINT_CHARACTERISTICS 5
#define ASCS_NUM_CLIENTS 3

static ascs_streamendpoint_characteristic_t ascs_streamendpoint_characteristics[ASCS_NUM_STREAMENDPOINT_CHARACTERISTICS];
static ascs_remote_client_t ascs_clients[ASCS_NUM_CLIENTS];


// PACS Client
static pacs_client_connection_t pacs_connections[PACS_CLIENT_COUNT];

// PACS Server
static ascs_codec_configuration_t ascs_codec_configuration = {
        0x01, 0x00, 0x00, 0x0FA0,
        0x000005, 0x00FFAA, 0x000000, 0x000000,
        HCI_AUDIO_CODING_FORMAT_LC3, 0x0000, 0x0000,
        {   0x1E,
            0x03, 0x01, 0x00000001, 0x0028, 0x00
        }
};

static pacs_record_t sink_pac_records[] = {
        // sink_record_0
        {
                // codec ID
                {HCI_AUDIO_CODING_FORMAT_LC3, 0x0000, 0x0000},
                // capabilities
                {
                        0x3E,
                        LE_AUDIO_CODEC_SAMPLING_FREQUENCY_MASK_8000_HZ |
                        LE_AUDIO_CODEC_SAMPLING_FREQUENCY_MASK_16000_HZ |
                        LE_AUDIO_CODEC_SAMPLING_FREQUENCY_MASK_24000_HZ |
                        LE_AUDIO_CODEC_SAMPLING_FREQUENCY_MASK_32000_HZ |
                        LE_AUDIO_CODEC_SAMPLING_FREQUENCY_MASK_44100_HZ |
                        LE_AUDIO_CODEC_SAMPLING_FREQUENCY_MASK_48000_HZ,
                        LE_AUDIO_CODEC_FRAME_DURATION_MASK_7500US | LE_AUDIO_CODEC_FRAME_DURATION_MASK_10000US,
                        LE_AUDIO_CODEC_AUDIO_CHANNEL_COUNT_MASK_1 | LE_AUDIO_CODEC_AUDIO_CHANNEL_COUNT_MASK_2 | LE_AUDIO_CODEC_AUDIO_CHANNEL_COUNT_MASK_4 | LE_AUDIO_CODEC_AUDIO_CHANNEL_COUNT_MASK_8,
                        26,
                        155,
                        2
                },
                // metadata length
                45,
                // metadata
                {
                        // all metadata set
                        0x0FFE,
                        // (2) preferred_audio_contexts_mask
                        LE_AUDIO_CONTEXT_MASK_UNSPECIFIED,
                        // (2) streaming_audio_contexts_mask
                        LE_AUDIO_CONTEXT_MASK_UNSPECIFIED,
                        // program_info
                        3, {0xA1, 0xA2, 0xA3},
                        // language_code
                        0x0000DE,
                        // ccids_num
                        3, {0xB1, 0xB2, 0xB3},
                        // parental_rating
                        LE_AUDIO_PARENTAL_RATING_NO_RATING,
                        // program_info_uri_length
                        3, {0xC1, 0xC2, 0xC3},
                        // extended_metadata_type
                        0x0001, 3, {0xD1, 0xD2, 0xD3},
                        // vendor_specific_metadata_type
                        0x0002, 3, {0xE1, 0xE2, 0xE3},
                }
        }
};

static pacs_record_t source_pac_records[] = {
        // sink_record_0
        {
                // codec ID
                {HCI_AUDIO_CODING_FORMAT_LC3, 0x0000, 0x0000},
                // capabilities
                {
                        0x3E,
                        LE_AUDIO_CODEC_SAMPLING_FREQUENCY_MASK_8000_HZ |
                        LE_AUDIO_CODEC_SAMPLING_FREQUENCY_MASK_16000_HZ |
                        LE_AUDIO_CODEC_SAMPLING_FREQUENCY_MASK_24000_HZ |
                        LE_AUDIO_CODEC_SAMPLING_FREQUENCY_MASK_32000_HZ |
                        LE_AUDIO_CODEC_SAMPLING_FREQUENCY_MASK_44100_HZ |
                        LE_AUDIO_CODEC_SAMPLING_FREQUENCY_MASK_48000_HZ,
                        LE_AUDIO_CODEC_FRAME_DURATION_MASK_7500US | LE_AUDIO_CODEC_FRAME_DURATION_MASK_10000US,
                        LE_AUDIO_CODEC_AUDIO_CHANNEL_COUNT_MASK_1 | LE_AUDIO_CODEC_AUDIO_CHANNEL_COUNT_MASK_2 | LE_AUDIO_CODEC_AUDIO_CHANNEL_COUNT_MASK_4 | LE_AUDIO_CODEC_AUDIO_CHANNEL_COUNT_MASK_8,
                        26,
                        155,
                        2
                },
                // metadata length
                45,
                // metadata
                {
                        // all metadata set
                        0x0FFE,
                        // (2) preferred_audio_contexts_mask
                        LE_AUDIO_CONTEXT_MASK_UNSPECIFIED,
                        // (2) streaming_audio_contexts_mask
                        LE_AUDIO_CONTEXT_MASK_UNSPECIFIED,
                        // program_info
                        3, {0xA1, 0xA2, 0xA3},
                        // language_code
                        0x0000DE,
                        // ccids_num
                        3, {0xB1, 0xB2, 0xB3},
                        // parental_rating
                        LE_AUDIO_PARENTAL_RATING_NO_RATING,
                        // program_info_uri_length
                        3, {0xC1, 0xC2, 0xC3},
                        // extended_metadata_type
                        0x0001, 3, {0xD1, 0xD2, 0xD3},
                        // vendor_specific_metadata_type
                        0x0002, 3, {0xE1, 0xE2, 0xE3},
                }
        }
};

static pacs_streamendpoint_t sink_node;
static pacs_streamendpoint_t source_node;


// CIG/CIS
static le_audio_cig_t        cig;
static le_audio_cig_params_t cig_params;
static uint8_t  cis_sdu_size;
static hci_con_handle_t cis_con_handles[MAX_CHANNELS];
static uint16_t packet_sequence_numbers[MAX_NUM_CIS];
static uint8_t iso_payload[MAX_CHANNELS * MAX_LC3_FRAME_BYTES];

static void send_iso_packet(uint8_t cis_index){
    bool ok = hci_reserve_packet_buffer();
    btstack_assert(ok);
    uint8_t * buffer = hci_get_outgoing_packet_buffer();
    // complete SDU, no TimeStamp
    little_endian_store_16(buffer, 0, cis_con_handles[cis_index] | (2 << 12));
    // len
    little_endian_store_16(buffer, 2, 0 + 4 + cis_sdu_size);
    // TimeStamp if TS flag is set
    // packet seq nr
    little_endian_store_16(buffer, 4, packet_sequence_numbers[cis_index]);
    // iso sdu len
    little_endian_store_16(buffer, 6, cis_sdu_size);
    // copy encoded payload
    uint8_t i;
    uint16_t offset = 8;
    memcpy(&buffer[offset], &iso_payload[i * MAX_LC3_FRAME_BYTES], cis_sdu_size);
    offset += cis_sdu_size;

    // send
    hci_send_iso_packet_buffer(offset);

    packet_sequence_numbers[cis_index]++;
}

// PACS Server Handler
static void pacs_server_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_GATTSERVICE_META) return;

    switch (hci_event_gattservice_meta_get_subevent_code(packet)){
        case GATTSERVICE_SUBEVENT_PACS_AUDIO_LOCATION_RECEIVED:
            printf("PACS: audio location received\n");
            break;

        default:
            break;
    }
}

// ASCS Server Handler
static uint8_t ase_id = 0;
static hci_con_handle_t ascs_server_current_client_con_handle = HCI_CON_HANDLE_INVALID;

static void ascs_server_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_GATTSERVICE_META) return;

    hci_con_handle_t con_handle;
    uint8_t status;

    ascs_codec_configuration_t codec_configuration;
    ascs_qos_configuration_t   qos_configuration;

    switch (hci_event_gattservice_meta_get_subevent_code(packet)){

        case GATTSERVICE_SUBEVENT_ASCS_REMOTE_CLIENT_CONNECTED:
            con_handle = gattservice_subevent_ascs_remote_client_connected_get_con_handle(packet);
            status =     gattservice_subevent_ascs_remote_client_connected_get_status(packet);
            btstack_assert(status == ERROR_CODE_SUCCESS);
            ascs_server_current_client_con_handle = con_handle;
            MESSAGE("ASCS Server: connected, con_handle 0x%02x", con_handle);
            break;

        case GATTSERVICE_SUBEVENT_ASCS_REMOTE_CLIENT_DISCONNECTED:
            con_handle = gattservice_subevent_ascs_remote_client_disconnected_get_con_handle(packet);
            MESSAGE("ASCS Server: disconnected, con_handle 0x%02xn", con_handle);
            break;

        case GATTSERVICE_SUBEVENT_ASCS_CODEC_CONFIGURATION_REQUEST:
            ase_id = gattservice_subevent_ascs_codec_configuration_request_get_ase_id(packet);
            con_handle = gattservice_subevent_ascs_codec_configuration_request_get_con_handle(packet);

            // use framing for 441
            codec_configuration.framing = gattservice_subevent_ascs_codec_configuration_request_get_sampling_frequency_index(packet) == LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_44100_HZ ? 1 : 0;

            codec_configuration.preferred_phy = LE_AUDIO_SERVER_PHY_MASK_NO_PREFERENCE;
            codec_configuration.preferred_retransmission_number = 0;
            codec_configuration.max_transport_latency_ms = 0x0FA0;
            codec_configuration.presentation_delay_min_us = 0x0005;
            codec_configuration.presentation_delay_max_us = 0xFFAA;
            codec_configuration.preferred_presentation_delay_min_us = 0;
            codec_configuration.preferred_presentation_delay_max_us = 0;

            // codec id:
            codec_configuration.coding_format =  gattservice_subevent_ascs_codec_configuration_request_get_coding_format(packet);;
            codec_configuration.company_id = gattservice_subevent_ascs_codec_configuration_request_get_company_id(packet);
            codec_configuration.vendor_specific_codec_id = gattservice_subevent_ascs_codec_configuration_request_get_vendor_specific_codec_id(packet);

            codec_configuration.specific_codec_configuration.codec_configuration_mask = gattservice_subevent_ascs_codec_configuration_request_get_specific_codec_configuration_mask(packet);
            codec_configuration.specific_codec_configuration.sampling_frequency_index = gattservice_subevent_ascs_codec_configuration_request_get_sampling_frequency_index(packet);
            codec_configuration.specific_codec_configuration.frame_duration_index = gattservice_subevent_ascs_codec_configuration_request_get_frame_duration_index(packet);
            codec_configuration.specific_codec_configuration.audio_channel_allocation_mask = gattservice_subevent_ascs_codec_configuration_request_get_audio_channel_allocation_mask(packet);
            codec_configuration.specific_codec_configuration.octets_per_codec_frame = gattservice_subevent_ascs_codec_configuration_request_get_octets_per_frame(packet);
            codec_configuration.specific_codec_configuration.codec_frame_blocks_per_sdu = gattservice_subevent_ascs_codec_configuration_request_get_frame_blocks_per_sdu(packet);

            MESSAGE("ASCS: CODEC_CONFIGURATION_RECEIVED ase_id %d, con_handle 0x%02x", ase_id, con_handle);
            audio_stream_control_service_server_streamendpoint_configure_codec(con_handle, ase_id, codec_configuration);
            break;

        // TODO: will change to get_con_handle
        // TODO: use gatt_service_subevent_ascs_qos_configuration_request_...
        case GATTSERVICE_SUBEVENT_ASCS_QOS_CONFIGURATION:
        case GATTSERVICE_SUBEVENT_ASCS_QOS_CONFIGURATION_REQUEST:
            ase_id = gattservice_subevent_ascs_qos_configuration_get_ase_id(packet);
            con_handle = gattservice_subevent_ascs_qos_configuration_get_ascs_cid(packet);

            qos_configuration.cig_id = gattservice_subevent_ascs_qos_configuration_get_cig_id(packet);
            qos_configuration.cis_id = gattservice_subevent_ascs_qos_configuration_get_cis_id(packet);
            qos_configuration.sdu_interval = gattservice_subevent_ascs_qos_configuration_get_sdu_interval(packet);
            qos_configuration.framing = gattservice_subevent_ascs_qos_configuration_get_framing(packet);
            qos_configuration.phy = gattservice_subevent_ascs_qos_configuration_get_phy(packet);
            qos_configuration.max_sdu = gattservice_subevent_ascs_qos_configuration_get_max_sdu(packet);
            qos_configuration.retransmission_number = gattservice_subevent_ascs_qos_configuration_get_retransmission_number(packet);
            qos_configuration.max_transport_latency_ms = gattservice_subevent_ascs_qos_configuration_get_max_transport_latency(packet);
            qos_configuration.presentation_delay_us = gattservice_subevent_ascs_qos_configuration_get_presentation_delay_us(packet);

            MESSAGE("ASCS: QOS_CONFIGURATION_RECEIVED ase_id %d", ase_id);
            audio_stream_control_service_server_streamendpoint_configure_qos(con_handle, ase_id, qos_configuration);
            break;

        // TODO: will change to get_con_handle
        // TODO: use gatt_service_subevent_ascs_metadata_request_...
        case GATTSERVICE_SUBEVENT_ASCS_METADATA:
        case GATTSERVICE_SUBEVENT_ASCS_METADATA_REQUEST:
            ase_id = gattservice_subevent_ascs_metadata_get_ase_id(packet);
            // TODO: will change to get_con_handle
            // TODO: use gatt_service_subevent_ascs_metadata_request_...
            con_handle = gattservice_subevent_ascs_metadata_get_ascs_cid(packet);
            MESSAGE("ASCS: METADATA_RECEIVED ase_id %d", ase_id);
            audio_stream_control_service_server_streamendpoint_enable(con_handle, ase_id);
            break;
        case GATTSERVICE_SUBEVENT_ASCS_CLIENT_START_READY:
            ase_id = gattservice_subevent_ascs_client_start_ready_get_ase_id(packet);
            con_handle = gattservice_subevent_ascs_client_start_ready_get_con_handle(packet);
            MESSAGE("ASCS: START_READY ase_id %d", ase_id);
            audio_stream_control_service_server_streamendpoint_receiver_start_ready(con_handle, ase_id);
            break;
        case GATTSERVICE_SUBEVENT_ASCS_CLIENT_STOP_READY:
            ase_id = gattservice_subevent_ascs_client_stop_ready_get_ase_id(packet);
            con_handle = gattservice_subevent_ascs_client_stop_ready_get_con_handle(packet);
            MESSAGE("ASCS: STOP_READY ase_id %d", ase_id);
            audio_stream_control_service_server_streamendpoint_receiver_stop_ready(con_handle, ase_id);
            break;
        case GATTSERVICE_SUBEVENT_ASCS_CLIENT_DISABLING:
            ase_id = gattservice_subevent_ascs_client_disabling_get_ase_id(packet);
            con_handle = gattservice_subevent_ascs_client_disabling_get_con_handle(packet);
            MESSAGE("ASCS: DISABLING ase_id %d", ase_id);
            audio_stream_control_service_server_streamendpoint_disable(con_handle, ase_id);
            break;
        case GATTSERVICE_SUBEVENT_ASCS_CLIENT_RELEASING:
            ase_id = gattservice_subevent_ascs_client_releasing_get_ase_id(packet);
            con_handle = gattservice_subevent_ascs_client_releasing_get_con_handle(packet);
            MESSAGE("ASCS: RELEASING ase_id %d", ase_id);
            audio_stream_control_service_server_streamendpoint_release(con_handle, ase_id);
            break;

        case GATTSERVICE_SUBEVENT_ASCS_CLIENT_RELEASED:
            ase_id = gattservice_subevent_ascs_client_released_get_ase_id(packet);
            con_handle = gattservice_subevent_ascs_client_released_get_con_handle(packet);
            MESSAGE("ASCS: RELEASED ase_id %d", ase_id);
            audio_stream_control_service_server_streamendpoint_released(con_handle, ase_id, false);
            break;

        default:
            break;
    }
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
                    for (i = 0; i < cig.num_cis; i++) {
                        if (cis_con_handle == cis_con_handles[i]){
                            send_iso_packet(i);
                            hci_request_cis_can_send_now_events(cis_con_handle);
                        }
                    }
                    break;
                case HCI_EVENT_LE_META:
                    switch (hci_event_le_meta_get_subevent_code(packet)){
                        default:
                            break;
                    }
                    break;
                case HCI_EVENT_META_GAP:
                    switch (hci_event_gap_meta_get_subevent_code(packet)) {
                        case GAP_SUBEVENT_CIG_CREATED:
                            MESSAGE("CIG Created: CIS Connection Handles: ");
                            for (i = 0; i < cig.num_cis; i++) {
                                cis_con_handles[i] = gap_subevent_cig_created_get_cis_con_handles(packet, i);
                                MESSAGE("0x%04x ", cis_con_handles[i]);
                            }
                            if (response_op == BTP_LE_AUDIO_OP_CIG_CREATE){
                                btp_send(BTP_SERVICE_ID_LE_AUDIO, BTP_LE_AUDIO_OP_CIG_CREATE, 0, 0, NULL);
                                response_op = 0;
                            }
                            break;
                        case GAP_SUBEVENT_CIS_CREATED:
                            MESSAGE("CIS Created");
                            if (response_op == BTP_LE_AUDIO_OP_CIS_CREATE){
                                response_op = 0;
                                btp_send(BTP_SERVICE_ID_LE_AUDIO, BTP_LE_AUDIO_OP_CIS_CREATE, 0, 0, NULL);
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
static uint16_t ascs_get_cid_for_con_handle(hci_con_handle_t con_handle){
    uint8_t i;
    for(i=0;i<ASCS_CLIENT_COUNT;i++){
        if ((ascs_connections[i].state != ASCS_STATE_IDLE) &&
             (ascs_connections[i].con_handle == con_handle)){
            return ascs_connections[i].cid;
        }
    }
    btstack_assert(false);
    return 0;
}

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

hci_con_handle_t get_cis_con_handle_for_cig_cis_id(uint8_t cig_id, uint8_t cis_id) {
    btstack_assert(cig_id == cig.cig_id);
    hci_con_handle_t cis_con_handle = HCI_CON_HANDLE_INVALID;
    uint8_t i;
    uint8_t num_cis = cig.num_cis;
    for (i=0; i < num_cis; i++){
        if (cig.params->cis_params[i].cis_id == cis_id){
            return cig.cis_con_handles[i];
        }
    }
    btstack_assert(false);
    return HCI_CON_HANDLE_INVALID;
}

void ascs_client_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
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

    switch (hci_event_gattservice_meta_get_subevent_code(packet)){
        case GATTSERVICE_SUBEVENT_ASCS_REMOTE_SERVER_CONNECTED:
            ascs_cid = gattservice_subevent_ascs_remote_server_connected_get_ascs_cid(packet);
            con_handle = gattservice_subevent_ascs_remote_server_connected_get_con_handle(packet);
            if (gattservice_subevent_ascs_remote_server_connected_get_status(packet) != ERROR_CODE_SUCCESS){
                // TODO send error response
                MESSAGE("ASCS Client: connection failed, cid 0x%02x, con_handle 0x%04x, status 0x%02x", ascs_cid, con_handle,
                        gattservice_subevent_ascs_remote_server_connected_get_status(packet));
                return;
            }
            source_ase_count = gattservice_subevent_ascs_remote_server_connected_get_source_ase_num(packet);
            sink_ase_count = gattservice_subevent_ascs_remote_server_connected_get_sink_ase_num(packet);
            num_ases = sink_ase_count + source_ase_count;
            MESSAGE("ASCS Client: connected, cid 0x%02x, con_handle 0x%04x, num Sink ASEs: %u, num Source ASEs: %u",
                    ascs_cid, con_handle, sink_ase_count, source_ase_count);

            response_len = 0;
            btp_append_uint8((uint8_t) ascs_cid);
            btp_append_uint8(num_ases);
            uint8_t offset = ascs_index * ASCS_CLIENT_NUM_STREAMENDPOINTS;
            for (i=0;i<num_ases;i++){
                btp_append_uint8(streamendpoint_characteristics[offset+i].ase_id);
                btp_append_uint8((uint8_t) streamendpoint_characteristics[offset+i].role);
            }
            btp_send(BTP_SERVICE_ID_LE_AUDIO, BTP_LE_AUDIO_OP_ASCS_CONNECT, 0, response_len,  response_buffer);
            break;

        case GATTSERVICE_SUBEVENT_ASCS_REMOTE_SERVER_DISCONNECTED:
            MESSAGE("ASCS Client: disconnected, cid 0x%02x", gattservice_subevent_ascs_remote_server_disconnected_get_ascs_cid(packet));
            break;

        case GATTSERVICE_SUBEVENT_ASCS_CODEC_CONFIGURATION:
            ase_id = gattservice_subevent_ascs_codec_configuration_get_ase_id(packet);
            ascs_cid = gattservice_subevent_ascs_codec_configuration_get_ascs_cid(packet);

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

            MESSAGE("ASCS Client: CODEC CONFIGURATION - ase_id %d, ascs_cid 0x%02x", ase_id, ascs_cid);
            response_len = 0;
            btp_append_uint24(gattservice_subevent_ascs_codec_configuration_get_presentation_delay_min(packet));
            btp_append_uint24(gattservice_subevent_ascs_codec_configuration_get_presentation_delay_max(packet));
            btp_send(BTP_SERVICE_ID_LE_AUDIO, BTP_LE_AUDIO_OP_ASCS_CONFIGURE_CODEC, 0, response_len, response_buffer);
            break;

        case GATTSERVICE_SUBEVENT_ASCS_QOS_CONFIGURATION:
            ase_id  = gattservice_subevent_ascs_qos_configuration_get_ase_id(packet);
            ascs_cid = gattservice_subevent_ascs_qos_configuration_get_ascs_cid(packet);

            MESSAGE("ASCS Client: QOS CONFIGURATION - ase_id %d, ascs_cid 0x%02x", ase_id, ascs_cid);
            if (response_op != 0){
                btp_send(BTP_SERVICE_ID_LE_AUDIO, response_op, 0, 0, NULL);
                response_op = 0;
            }
            break;

        case GATTSERVICE_SUBEVENT_ASCS_METADATA:
            ase_id  = gattservice_subevent_ascs_metadata_get_ase_id(packet);
            ascs_cid = gattservice_subevent_ascs_metadata_get_ascs_cid(packet);

            printf("ASCS Client: METADATA UPDATE - ase_id %d, ascs_cid 0x%02x\n", ase_id, ascs_cid);
            break;

        case GATTSERVICE_SUBEVENT_ASCS_CONTROL_POINT_OPERATION_RESPONSE:
            ase_id  = gattservice_subevent_ascs_control_point_operation_response_get_ase_id(packet);
            ascs_cid    = gattservice_subevent_ascs_control_point_operation_response_get_ascs_cid(packet);
            response_code = gattservice_subevent_ascs_control_point_operation_response_get_response_code(packet);
            reason        = gattservice_subevent_ascs_control_point_operation_response_get_reason(packet);
            opcode        = gattservice_subevent_ascs_control_point_operation_response_get_opcode(packet);
            printf("ASCS Client: Operation complete - ase_id %d, opcode %u, response [0x%02x, 0x%02x], ascs_cid 0x%02x\n", ase_id, opcode, response_code, reason, ascs_cid);
            if ((opcode == ASCS_OPCODE_RECEIVER_START_READY) && (response_op != 0)){
                MESSAGE("Receiver Start Ready completed");
                btp_send(BTP_SERVICE_ID_LE_AUDIO, response_op, 0, 0, NULL);
                response_op = 0;
            }
            break;

        case GATTSERVICE_SUBEVENT_ASCS_STREAMENDPOINT_STATE:
            log_info("GATTSERVICE_SUBEVENT_ASCS_STREAMENDPOINT_STATE");
            ascs_cid   = gattservice_subevent_ascs_streamendpoint_state_get_ascs_cid(packet);
            ase_id     = gattservice_subevent_ascs_streamendpoint_state_get_ase_id(packet);
            ase_state  = gattservice_subevent_ascs_streamendpoint_state_get_state(packet);
            log_info("ASCS Client: ASE STATE (%s) - ase_id %d, ascs_cid 0x%02x, role %s", ascs_util_ase_state2str(ase_state), ase_id, ascs_cid,
                     (audio_stream_control_service_client_get_ase_role(ascs_cid, ase_id) == LE_AUDIO_ROLE_SOURCE) ? "SOURCE" : "SINK" );
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

    uint32_t audio_allocation_mask;
    uint16_t ascs_cid;
    uint16_t pacs_cid;
    hci_con_handle_t con_handle;
    uint8_t i;
    uint8_t status;

    switch (hci_event_gattservice_meta_get_subevent_code(packet)){
        case GATTSERVICE_SUBEVENT_PACS_CONNECTED:
            pacs_cid = gattservice_subevent_pacs_connected_get_pacs_cid(packet);
            con_handle = gattservice_subevent_pacs_connected_get_con_handle(packet);
            status = gattservice_subevent_pacs_connected_get_status(packet);
            if (status != ERROR_CODE_SUCCESS){
                // TODO send error response
                MESSAGE("PACS Client: connection failed, cid 0x%02x, con_handle 0x%02x, status 0x%02x", pacs_cid, con_handle, status);
                return;
            }
            ascs_cid = ascs_get_cid_for_con_handle(con_handle);
            uint8_t num_ases = sink_ase_count + source_ase_count;
            MESSAGE("PACS client: connected, cid 0x%02x, con_handle 0x04%x -> ascs cid 0x%02x", pacs_cid, con_handle, ascs_cid);

            // TODO: provide separate API for PACS
            // TODO: use ASE info (id, type) from event instead of direct ASE access
            response_len = 0;
            btp_append_uint8((uint8_t) ascs_cid);
            btp_append_uint8(num_ases);
            uint8_t offset = ascs_index * ASCS_CLIENT_NUM_STREAMENDPOINTS;
            for (i=0;i<num_ases;i++){
                btp_append_uint8(streamendpoint_characteristics[offset+i].ase_id);
                btp_append_uint8((uint8_t) streamendpoint_characteristics[offset+i].role);
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
            audio_allocation_mask = gattservice_subevent_pacs_audio_locations_get_audio_location_mask(packet);
            MESSAGE("PACS Client: %s Audio Locations 0x%04x",
                gattservice_subevent_pacs_audio_locations_get_le_audio_role(packet) == LE_AUDIO_ROLE_SINK ? "Sink" : "Source", audio_allocation_mask);
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

static void expect_status_no_error(uint8_t status){
    if (status != ERROR_CODE_SUCCESS){
        MESSAGE("STATUS = 0x%02x -> ABORT", status);
    }
    btstack_assert(status == ERROR_CODE_SUCCESS);
}

// BTP LE Audio
void btp_le_audio_handler(uint8_t opcode, uint8_t controller_index, uint16_t length, const uint8_t *data) {
    // provide op info for response
    response_len = 0;
    response_service_id = BTP_SERVICE_ID_LE_AUDIO;
    response_op = opcode;
    uint8_t ase_id;
    switch (opcode) {
        case BTP_LE_AUDIO_OP_READ_SUPPORTED_COMMANDS:
            MESSAGE("BTP_LE_AUDIO_OP_READ_SUPPOERTED_COMMANDS");
            if (controller_index == BTP_INDEX_NON_CONTROLLER) {
                uint8_t commands = 0;
                btp_send(BTP_SERVICE_ID_GATT, opcode, controller_index, 1, &commands);
            }
            break;
        case BTP_LE_AUDIO_OP_ASCS_CONNECT:
            if (controller_index == 0) {
                bd_addr_t bd_addr;
                bd_addr_type_t  bd_addr_type = (bd_addr_type_t) data[0];
                reverse_bd_addr(&data[1], bd_addr);
                const hci_connection_t * hci_connection = hci_connection_for_bd_addr_and_type(bd_addr, bd_addr_type);
                btstack_assert(hci_connection != NULL);
                hci_con_handle_t con_handle = hci_connection->con_handle;
                MESSAGE("BTP_LE_AUDIO_OP_ASCS_CONNECT to %s, con handle 0x%04x", bd_addr_to_str(bd_addr), con_handle);
                ascs_index = ascs_get_free_slot();
                uint16_t ascs_cid;
                audio_stream_control_service_client_connect(&ascs_connections[ascs_index],
                                                            &streamendpoint_characteristics[ascs_index * ASCS_CLIENT_NUM_STREAMENDPOINTS],
                                                            ASCS_CLIENT_NUM_STREAMENDPOINTS,
                                                            con_handle, &ascs_cid);
            }
            break;
        case BTP_LE_AUDIO_OP_ASCS_CONFIGURE_CODEC:
            if (controller_index == 0){
                uint16_t ascs_cid              = data[0];
                ase_id_used_by_configure_codec = data[1];
                uint8_t  coding_format     = data[2];
                uint32_t frequency_hz      = little_endian_read_32(data, 3);
                uint16_t frame_duration_us = little_endian_read_16(data, 7);
                uint32_t audio_locations   = little_endian_read_32(data, 9);
                uint16_t octets_per_frame  = little_endian_read_16(data, 13);
                MESSAGE("BTP_LE_AUDIO_OP_ASCS_CONFIGURE_CODEC ase_id %u, format %x, freq %u, duration %u, locations 0x%0x, octets %u",
                        ase_id_used_by_configure_codec, coding_format, frequency_hz, frame_duration_us, audio_locations, octets_per_frame);

                ascs_specific_codec_configuration_t * sc_config = NULL;
                ascs_codec_configuration_t ascs_server_codec_configuration;
                if (ascs_cid > 0) {
                    sc_config = &ascs_codec_configuration_request.specific_codec_configuration;
                } else {
                    sc_config = &ascs_server_codec_configuration.specific_codec_configuration;
                }

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

                sc_config->codec_configuration_mask = coding_format == HCI_AUDIO_CODING_FORMAT_LC3 ? 0x3E : 0x1e;
                sc_config->sampling_frequency_index = frequency_index;
                sc_config->frame_duration_index =  frame_duration_us == 7500
                                                   ? LE_AUDIO_CODEC_FRAME_DURATION_INDEX_7500US : LE_AUDIO_CODEC_FRAME_DURATION_INDEX_10000US;
                sc_config->octets_per_codec_frame = octets_per_frame;
                sc_config->audio_channel_allocation_mask = audio_locations;
                sc_config->codec_frame_blocks_per_sdu = 1;

                if (ascs_cid > 0){
                    ascs_codec_configuration_request.target_latency = LE_AUDIO_CLIENT_TARGET_LATENCY_LOW_LATENCY;
                    ascs_codec_configuration_request.target_phy = LE_AUDIO_CLIENT_TARGET_PHY_BALANCED;
                    ascs_codec_configuration_request.coding_format = coding_format;
                    ascs_codec_configuration_request.company_id = 0;
                    ascs_codec_configuration_request.vendor_specific_codec_id = 0;

                    uint8_t status = audio_stream_control_service_client_streamendpoint_configure_codec(ascs_cid, ase_id_used_by_configure_codec, &ascs_codec_configuration_request);
                    expect_status_no_error(status);
                } else {

                    ascs_server_codec_configuration.coding_format = coding_format;
                    ascs_server_codec_configuration.company_id = 0;
                    ascs_server_codec_configuration.vendor_specific_codec_id = 0;
                    ascs_server_codec_configuration.preferred_phy = LE_AUDIO_SERVER_PHY_MASK_NO_PREFERENCE;
                    ascs_server_codec_configuration.preferred_retransmission_number = 0;
                    ascs_server_codec_configuration.max_transport_latency_ms = 0x0FA0;
                    ascs_server_codec_configuration.presentation_delay_min_us = 0x0005;
                    ascs_server_codec_configuration.presentation_delay_max_us = 0xFFAA;
                    ascs_server_codec_configuration.preferred_presentation_delay_min_us = 0;
                    ascs_server_codec_configuration.preferred_presentation_delay_max_us = 0;

                    ascs_server_codec_configuration.framing = frequency_index == LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_44100_HZ ? 1 : 0;
                    audio_stream_control_service_server_streamendpoint_configure_codec(ascs_server_current_client_con_handle,
                                                                                       ase_id_used_by_configure_codec, ascs_server_codec_configuration);

                    response_len = 0;
                    btp_append_uint24(0);
                    btp_append_uint24(0);
                    btp_send(BTP_SERVICE_ID_LE_AUDIO, BTP_LE_AUDIO_OP_ASCS_CONFIGURE_CODEC, 0, response_len, response_buffer);
                }
            }
            break;
        case BTP_LE_AUDIO_OP_ASCS_CONFIGURE_QOS:
            if (controller_index == 0){
                uint16_t ascs_cid                 = data[0];
                ase_id                            = data[1];
                uint8_t  cig_id                   = data[2];
                uint8_t  cis_id                   = data[3];
                uint16_t sdu_interval_us          = little_endian_read_16(data, 4);
                uint8_t  framing                  = data[6];
                uint32_t max_sdu                  = little_endian_read_16(data, 7);
                uint8_t  retransmission_number    = data[9];
                uint8_t  max_transport_latency_ms = data[10];

                MESSAGE("BTP_LE_AUDIO_OP_ASCS_CONFIGURE_QOS ase_id %u, cis %u, interval %u, framing %u, sdu_size %u, retrans %u, latency %u",
                        ase_id, cis_id, sdu_interval_us, framing, max_sdu, retransmission_number, max_transport_latency_ms);
                ascs_qos_configuration.cig_id = cig_id;
                ascs_qos_configuration.cis_id = cis_id;
                ascs_qos_configuration.sdu_interval = sdu_interval_us;
                ascs_qos_configuration.framing = framing;
                ascs_qos_configuration.phy = LE_AUDIO_SERVER_PHY_MASK_NO_PREFERENCE;
                ascs_qos_configuration.max_sdu = max_sdu;
                ascs_qos_configuration.retransmission_number = retransmission_number;
                ascs_qos_configuration.max_transport_latency_ms = max_transport_latency_ms;
                ascs_qos_configuration.presentation_delay_us = 40000;
                uint8_t status = audio_stream_control_service_client_streamendpoint_configure_qos(ascs_cid, ase_id, &ascs_qos_configuration);
                expect_status_no_error(status);

                // TODO:
                cis_sdu_size = max_sdu;
            }
            break;
        case BTP_LE_AUDIO_OP_ASCS_ENABLE:
            if (controller_index == 0) {
                uint16_t ascs_cid = data[0];
                ase_id = data[1];
                MESSAGE("BTP_LE_AUDIO_OP_ASCS_ENABLE ase_id %u", ase_id);
                uint8_t status = audio_stream_control_service_client_streamendpoint_enable(ascs_cid, ase_id);
                expect_status_no_error(status);
            }
            break;
        case BTP_LE_AUDIO_OP_ASCS_RECEIVER_START_READY:
            if (controller_index == 0) {
                uint16_t ascs_cid = data[0];
                ase_id = data[1];
                MESSAGE("BTP_LE_AUDIO_OP_ASCS_RECEIVER_START_READY: ase_id %u", ase_id);
                uint8_t status = audio_stream_control_service_client_streamendpoint_receiver_start_ready(ascs_cid, ase_id);
                expect_status_no_error(status);
            }
            break;
        case BTP_LE_AUDIO_OP_ASCS_RECEIVER_STOP_READY:
            if (controller_index == 0) {
                uint16_t ascs_cid = data[0];
                ase_id = data[1];
                MESSAGE("BTP_LE_AUDIO_OP_ASCS_RECEIVER_STOP_READY ase_id %u", ase_id);
                uint8_t status = audio_stream_control_service_client_streamendpoint_receiver_stop_ready(ascs_cid, ase_id);
                expect_status_no_error(status);
            }
            break;
        case BTP_LE_AUDIO_OP_ASCS_DISABLE:
            if (controller_index == 0) {
                uint16_t ascs_cid = data[0];
                ase_id = data[1];
                MESSAGE("BTP_LE_AUDIO_OP_ASCS_DISABLE ase_id %u", ase_id);
                uint8_t status = audio_stream_control_service_client_streamendpoint_disable(ascs_cid, ase_id);
                expect_status_no_error(status);
            }
            break;
        case BTP_LE_AUDIO_OP_ASCS_RELEASE:
            if (controller_index == 0) {
                uint16_t ascs_cid = data[0];
                ase_id = data[1];
                MESSAGE("BTP_LE_AUDIO_OP_ASCS_RELEASE ase_id %u", ase_id);
                uint8_t status = audio_stream_control_service_client_streamendpoint_release(ascs_cid, ase_id, false);
                expect_status_no_error(status);
            }
            break;
        case BTP_LE_AUDIO_OP_ASCS_UPDATE_METADATA:
            if (controller_index == 0){
                uint16_t ascs_cid = data[0];
                ase_id = data[1];
                MESSAGE("BTP_LE_AUDIO_OP_ASCS_UPDATE_METADATA ase_id %u", ase_id);
                ascs_metadata.metadata_mask = (1 << LE_AUDIO_METADATA_TYPE_PREFERRED_AUDIO_CONTEXTS) & (1 << LE_AUDIO_METADATA_TYPE_STREAMING_AUDIO_CONTEXTS);
                ascs_metadata.preferred_audio_contexts_mask = LE_AUDIO_CONTEXT_MASK_MEDIA;
                ascs_metadata.streaming_audio_contexts_mask = LE_AUDIO_CONTEXT_MASK_MEDIA;
                uint8_t status = audio_stream_control_service_client_streamendpoint_metadata_update(ascs_cid, ase_id, &ascs_metadata);
                expect_status_no_error(status);
            }
            break;
        case BTP_LE_AUDIO_OP_CIG_CREATE:
            if (controller_index == 0){
                uint16_t offset = 0;
                cig_params.cig_id =  data[offset++];
                cig_params.num_cis = data[offset++];
                cig_params.sdu_interval_c_to_p = little_endian_read_16(data, offset);
                offset += 2;
                cig_params.sdu_interval_p_to_c = little_endian_read_16(data, offset);
                offset += 2;
                cig_params.worst_case_sca = 0; // 251 ppm to 500 ppm
                cig_params.packing = 0;        // sequential
                cig_params.framing = data[offset++];
                cig_params.max_transport_latency_c_to_p = 40;
                cig_params.max_transport_latency_p_to_c = 40;
                uint8_t i;
                MESSAGE("BTP_LE_AUDIO_OP_CIG_CREATE cig %u, num cis %u, interval %u us", cig_params.cig_id, cig_params.num_cis, cig_params.sdu_interval_c_to_p );
                for (i = 0; i < cig_params.num_cis; i++) {
                    cig_params.cis_params[i].cis_id = data[offset++];
                    cig_params.cis_params[i].max_sdu_c_to_p = little_endian_read_16(data, offset);
                    offset += 2;
                    cig_params.cis_params[i].max_sdu_p_to_c = little_endian_read_16(data, offset);
                    offset += 2;
                    cig_params.cis_params[i].phy_c_to_p = 2;  // 2M
                    cig_params.cis_params[i].phy_p_to_c = 2;  // 2M
                    cig_params.cis_params[i].rtn_c_to_p = 2;
                    cig_params.cis_params[i].rtn_p_to_c = 2;
                    MESSAGE("BTP_LE_AUDIO_OP_CIG_CREATE cis %u, sdu_c_to_p %u, sdu_p_to_c %u",
                            cig_params.cis_params[i].cis_id,
                            cig_params.cis_params[i].max_sdu_c_to_p,
                            cig_params.cis_params[i].max_sdu_p_to_c);
                }

                uint8_t status = gap_cig_create(&cig, &cig_params);
                expect_status_no_error(status);
            }
            break;
        case BTP_LE_AUDIO_OP_CIS_CREATE:
            if (controller_index == 0) {
                bd_addr_t bd_addr;
                uint16_t pos = 0;
                uint8_t cig_cid = data[pos++];
                MESSAGE("BTP_LE_AUDIO_OP_CIS_CREATE cig_id %u", cig_cid);
                btstack_assert(cig_cid == cig.cig_id);
                // TODO handle partial CIS setup - requires support from stack, too
                uint8_t num_cis = data[pos++];
                btstack_assert(num_cis == cig.num_cis);
                uint8_t i;
                hci_con_handle_t acl_connection_handles[MAX_CHANNELS];
                hci_con_handle_t cis_connection_handles[MAX_CHANNELS];
                for (i=0;i<num_cis;i++){
                    uint8_t cis_id = data[pos++];
                    // find cis index for cis_id
                    uint8_t j;
                    bool found = false;
                    for (j=0;j<num_cis;j++){
                        if (cig.params->cis_params[j].cis_id == cis_id){
                            cis_connection_handles[i] = cig.cis_con_handles[j];
                            found = true;
                            break;
                        }
                    }
                    btstack_assert(found);
                    bd_addr_type_t  bd_addr_type = (bd_addr_type_t) data[pos++];
                    reverse_bd_addr(&data[pos], bd_addr);
                    pos += 6;
                    const hci_connection_t * hci_connection = hci_connection_for_bd_addr_and_type(bd_addr, bd_addr_type);
                    btstack_assert(hci_connection != NULL);
                    acl_connection_handles[i] = hci_connection->con_handle;
                }
                uint8_t status = gap_cis_create(cig_cid, cis_connection_handles, acl_connection_handles);
                expect_status_no_error(status);
            }
            break;
        case BTP_LE_AUDIO_OP_CIS_START_STREAMING:
            if (controller_index == 0) {
                bd_addr_t bd_addr;
                uint16_t pos = 0;
                uint8_t cig_id = data[pos++];
                uint8_t cis_id = data[pos++];
                MESSAGE("BTP_LE_AUDIO_OP_CIS_START_STREAMING: cig %u, cis %u", cig_id, cis_id);
                hci_con_handle_t cis_con_handle = get_cis_con_handle_for_cig_cis_id(cig_id, cis_id);
                MESSAGE("Request can send now 0x%04x", cis_con_handle);
                hci_request_cis_can_send_now_events(cis_con_handle);
                btp_send(BTP_SERVICE_ID_LE_AUDIO, BTP_LE_AUDIO_OP_CIS_START_STREAMING,
                         controller_index, 0, NULL);
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

    // Clients
    audio_stream_control_service_client_init(&ascs_client_event_handler);
    published_audio_capabilities_service_client_init(&pacs_client_event_handler);

    // setup ATT server
    att_server_init(profile_data, NULL, NULL);

    // Servers
    // TODO: initialize all servers until BTP API to enable/disable is available

    // PACS Server
    sink_node.records = &sink_pac_records[0];
    sink_node.records_num = 1;
    sink_node.audio_locations_mask = LE_AUDIO_LOCATION_MASK_FRONT_RIGHT;
    sink_node.available_audio_contexts_mask = LE_AUDIO_CONTEXT_MASK_UNSPECIFIED;
    sink_node.supported_audio_contexts_mask = LE_AUDIO_CONTEXT_MASK_UNSPECIFIED;

    source_node.records = &source_pac_records[0];
    source_node.records_num = 1;
    source_node.audio_locations_mask = LE_AUDIO_LOCATION_MASK_FRONT_RIGHT;
    source_node.available_audio_contexts_mask = LE_AUDIO_CONTEXT_MASK_UNSPECIFIED;
    source_node.supported_audio_contexts_mask = LE_AUDIO_CONTEXT_MASK_UNSPECIFIED;

    published_audio_capabilities_service_server_init(&sink_node, &source_node);
    published_audio_capabilities_service_server_register_packet_handler(&pacs_server_packet_handler);

    // ASCS Server
    audio_stream_control_service_server_init(ASCS_NUM_STREAMENDPOINT_CHARACTERISTICS, ascs_streamendpoint_characteristics, ASCS_NUM_CLIENTS, ascs_clients);
    audio_stream_control_service_server_register_packet_handler(&ascs_server_packet_handler);
}

