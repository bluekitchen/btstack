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
#include "btp_le_audio_profile.h"
#include "btp_pacs.h"

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
static ascs_server_connection_t ascs_clients[ASCS_NUM_CLIENTS];


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
    hci_reserve_packet_buffer();
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
    uint8_t status = hci_send_iso_packet_buffer(offset);
    btstack_assert(status == ERROR_CODE_SUCCESS);

    packet_sequence_numbers[cis_index]++;
}

// PACS Server Handler
static void pacs_server_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_LEAUDIO_META) return;

    switch (hci_event_gattservice_meta_get_subevent_code(packet)){
        case LEAUDIO_SUBEVENT_PACS_SERVER_AUDIO_LOCATIONS:
            printf("PACS: audio location received\n");
            break;

        default:
            break;
    }
}

// ASCS Server Handler
static hci_con_handle_t ascs_server_current_client_con_handle = HCI_CON_HANDLE_INVALID;
static btstack_timer_source_t  ascs_server_released_timer;
static btstack_timer_source_t  ascs_server_start_ready_timer;
static le_audio_metadata_t     ascs_audio_metadata;

static const ascs_streamendpoint_characteristic_t * ascs_server_get_streamenpoint_characteristic_for_ase_id(uint8_t ase_id){
    uint8_t i;
    for (i=0;i<ASCS_NUM_STREAMENDPOINT_CHARACTERISTICS;i++){
        if (ascs_streamendpoint_characteristics[i].ase_id == ase_id){
            return &ascs_streamendpoint_characteristics[i];
        }
    }
    return NULL;
}

static void ascs_server_stop_streaming(uint8_t ase_id){
    uint8_t i;
    for (i=0;i<ASCS_NUM_CLIENTS;i++) {
        ascs_server_connection_t *connection = &ascs_clients[i];
        uint8_t j;
        for (j = 0; j < ASCS_NUM_STREAMENDPOINT_CHARACTERISTICS; j++) {
            ascs_streamendpoint_t *streamendpoint = &connection->streamendpoints[j];
            le_audio_role_t role = streamendpoint->ase_characteristic->role;
            if ((streamendpoint->ase_characteristic->ase_id == ase_id) && (role == LE_AUDIO_ROLE_SOURCE)){
                uint8_t k;
                hci_con_handle_t cis_handle = streamendpoint->cis_handle;
                for (k=0;k<cig.num_cis;k++){
                    if (cis_con_handles[k] == cis_handle){
                        cis_con_handles[k] = HCI_CON_HANDLE_INVALID;
                        MESSAGE("ASCS Server: stop streaming, cis handle %u", cis_handle);
                    }
                }
            }
        }
    }

}

static void ascs_server_released_timer_handler(btstack_timer_source_t * ts){
    UNUSED(ts);
    uint8_t i;
    for (i=0;i<ASCS_NUM_CLIENTS;i++) {
        ascs_server_connection_t *connection = &ascs_clients[i];
        uint8_t j;
        for (j = 0; j < ASCS_NUM_STREAMENDPOINT_CHARACTERISTICS; j++) {
            ascs_streamendpoint_t *streamendpoint = &connection->streamendpoints[j];
            ascs_state_t state = streamendpoint->state;
            le_audio_role_t role = streamendpoint->ase_characteristic->role;
            if (state == ASCS_STATE_RELEASING){
                uint8_t ase_id = streamendpoint->ase_characteristic->ase_id;
                MESSAGE("ASCS Server Released");
                audio_stream_control_service_server_streamendpoint_released(connection->con_handle, ase_id, true);
            }
        }
    }
}

static void ascs_server_handle_disconnect(hci_con_handle_t con_handle){
    uint8_t i;
    for (i=0;i<ASCS_NUM_CLIENTS;i++) {
        ascs_server_connection_t *connection = &ascs_clients[i];
        uint8_t j;
        for (j = 0; j < ASCS_NUM_STREAMENDPOINT_CHARACTERISTICS; j++) {
            ascs_streamendpoint_t *streamendpoint = &connection->streamendpoints[j];
            ascs_state_t state = streamendpoint->state;
            le_audio_role_t role = streamendpoint->ase_characteristic->role;
            if ((state == ASCS_STATE_STREAMING) && (streamendpoint->cis_handle == con_handle)){
                uint8_t ase_id = streamendpoint->ase_characteristic->ase_id;
                MESSAGE("HCI Disconnect for  cis_con_handle 0x%04x-> CIS lost acl handle 0x%04x, ase %u", con_handle, connection->con_handle, con_handle);
            }
        }
    }
}

static void ascs_server_start_ready_timer_handler(btstack_timer_source_t * ts){
    bool more_tasks = false;

    // if AES Sink and CIS established, transition to streaming
    uint8_t i;
    for (i=0;i<ASCS_NUM_CLIENTS;i++) {
        ascs_server_connection_t *connection = &ascs_clients[i];
        uint8_t j;
        for (j = 0; j < ASCS_NUM_STREAMENDPOINT_CHARACTERISTICS; j++) {
            ascs_streamendpoint_t *streamendpoint = &connection->streamendpoints[j];
            ascs_state_t state = streamendpoint->state;
            uint8_t ase_id = streamendpoint->ase_characteristic->ase_id;
            le_audio_role_t role = streamendpoint->ase_characteristic->role;
            if ((state == ASCS_STATE_ENABLING) && (role == LE_AUDIO_ROLE_SINK)){
                if (streamendpoint->cis_handle == HCI_CON_HANDLE_INVALID){
                    more_tasks = true;
                } else {
                    MESSAGE("ASE %u, Role %u, State %u, CIS 0x%04x -> ReceiverStart Ready", ase_id, role, streamendpoint->state, streamendpoint->cis_handle);
                    audio_stream_control_service_server_streamendpoint_receiver_start_ready(connection->con_handle, ase_id);
                }
            }
        }
    }

    if (more_tasks){
        btstack_run_loop_set_timer(ts, 100);
        btstack_run_loop_add_timer(ts);
    }
}

static void ascs_server_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_LEAUDIO_META) return;

    hci_con_handle_t con_handle;
    hci_con_handle_t cis_handle;
    uint8_t status;
    uint8_t ase_id;

    ascs_codec_configuration_t codec_configuration;
    ascs_qos_configuration_t   qos_configuration;

    switch (hci_event_gattservice_meta_get_subevent_code(packet)){
        case LEAUDIO_SUBEVENT_ASCS_SERVER_CONNECTED:
            con_handle = leaudio_subevent_ascs_server_connected_get_con_handle(packet);
            status =     leaudio_subevent_ascs_server_connected_get_status(packet);
            btstack_assert(status == ERROR_CODE_SUCCESS);
            ascs_server_current_client_con_handle = con_handle;
            MESSAGE("ASCS Server: connected, con_handle 0x%02x", con_handle);
            break;
        case LEAUDIO_SUBEVENT_ASCS_SERVER_DISCONNECTED:
            con_handle = leaudio_subevent_ascs_server_disconnected_get_con_handle(packet);
            MESSAGE("ASCS Server: disconnected, con_handle 0x%02xn", con_handle);
            break;
        case LEAUDIO_SUBEVENT_ASCS_SERVER_CODEC_CONFIGURATION:
            ase_id = leaudio_subevent_ascs_server_codec_configuration_get_ase_id(packet);
            con_handle = leaudio_subevent_ascs_server_codec_configuration_get_con_handle(packet);

            // use framing for 441
            codec_configuration.framing = leaudio_subevent_ascs_server_codec_configuration_get_sampling_frequency_index(packet) == LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_44100_HZ ? 1 : 0;

            codec_configuration.preferred_phy = LE_AUDIO_SERVER_PHY_MASK_NO_PREFERENCE;
            codec_configuration.preferred_retransmission_number = 0;
            codec_configuration.max_transport_latency_ms = 0x0FA0;
            codec_configuration.presentation_delay_min_us = 0x0005;
            codec_configuration.presentation_delay_max_us = 0xFFAA;
            codec_configuration.preferred_presentation_delay_min_us = 0;
            codec_configuration.preferred_presentation_delay_max_us = 0;

            // codec id:
            codec_configuration.coding_format =  leaudio_subevent_ascs_server_codec_configuration_get_coding_format(packet);;
            codec_configuration.company_id = leaudio_subevent_ascs_server_codec_configuration_get_company_id(packet);
            codec_configuration.vendor_specific_codec_id = leaudio_subevent_ascs_server_codec_configuration_get_vendor_specific_codec_id(packet);

            codec_configuration.specific_codec_configuration.codec_configuration_mask = leaudio_subevent_ascs_server_codec_configuration_get_specific_codec_configuration_mask(packet);
            codec_configuration.specific_codec_configuration.sampling_frequency_index = leaudio_subevent_ascs_server_codec_configuration_get_sampling_frequency_index(packet);
            codec_configuration.specific_codec_configuration.frame_duration_index = leaudio_subevent_ascs_server_codec_configuration_get_frame_duration_index(packet);
            codec_configuration.specific_codec_configuration.audio_channel_allocation_mask = leaudio_subevent_ascs_server_codec_configuration_get_audio_channel_allocation_mask(packet);
            codec_configuration.specific_codec_configuration.octets_per_codec_frame = leaudio_subevent_ascs_server_codec_configuration_get_octets_per_frame(packet);
            codec_configuration.specific_codec_configuration.codec_frame_blocks_per_sdu = leaudio_subevent_ascs_server_codec_configuration_get_frame_blocks_per_sdu(packet);

            MESSAGE("ASCS: CODEC_CONFIGURATION_RECEIVED ase_id %d, codec_configuration_mask %02x, con_handle 0x%02x", ase_id,
                    codec_configuration.specific_codec_configuration.codec_configuration_mask, con_handle);
            audio_stream_control_service_server_streamendpoint_configure_codec(con_handle, ase_id, &codec_configuration);
            break;
        case LEAUDIO_SUBEVENT_ASCS_SERVER_QOS_CONFIGURATION:
            ase_id = leaudio_subevent_ascs_server_qos_configuration_get_ase_id(packet);
            con_handle = leaudio_subevent_ascs_server_qos_configuration_get_con_handle(packet);

            qos_configuration.cig_id = leaudio_subevent_ascs_server_qos_configuration_get_cig_id(packet);
            qos_configuration.cis_id = leaudio_subevent_ascs_server_qos_configuration_get_cis_id(packet);
            qos_configuration.sdu_interval = leaudio_subevent_ascs_server_qos_configuration_get_sdu_interval(packet);
            qos_configuration.framing = leaudio_subevent_ascs_server_qos_configuration_get_framing(packet);
            qos_configuration.phy = leaudio_subevent_ascs_server_qos_configuration_get_phy(packet);
            qos_configuration.max_sdu = leaudio_subevent_ascs_server_qos_configuration_get_max_sdu(packet);
            qos_configuration.retransmission_number = leaudio_subevent_ascs_server_qos_configuration_get_retransmission_number(packet);
            qos_configuration.max_transport_latency_ms = leaudio_subevent_ascs_server_qos_configuration_get_max_transport_latency(packet);
            qos_configuration.presentation_delay_us = leaudio_subevent_ascs_server_qos_configuration_get_presentation_delay_us(packet);

            MESSAGE("ASCS: QOS_CONFIGURATION_RECEIVED ase_id %d, cig_id %u, cis_id %u", ase_id, qos_configuration.cig_id, qos_configuration.cis_id);
            audio_stream_control_service_server_streamendpoint_configure_qos(con_handle, ase_id, &qos_configuration);
            // HACK: start counting CIS
            cig.num_cis = 0;
            break;
        case LEAUDIO_SUBEVENT_ASCS_SERVER_ENABLE:
            ase_id = leaudio_subevent_ascs_server_enable_get_ase_id(packet);
            con_handle = leaudio_subevent_ascs_server_enable_get_con_handle(packet);
            MESSAGE("ASCS: ENABLE ase_id %d", ase_id);
            le_audio_util_metadata_using_mask_from_enable_event(packet, size, &ascs_audio_metadata);
            audio_stream_control_service_server_streamendpoint_enable(con_handle, ase_id, &ascs_audio_metadata);

            // trigger (potential) Receiver Start Ready
            btstack_run_loop_remove_timer(&ascs_server_start_ready_timer);
            btstack_run_loop_set_timer_handler(&ascs_server_start_ready_timer,
                                               &ascs_server_start_ready_timer_handler);
            btstack_run_loop_set_timer(&ascs_server_start_ready_timer, 100);
            btstack_run_loop_add_timer(&ascs_server_start_ready_timer);
            break;
        case LEAUDIO_SUBEVENT_ASCS_SERVER_METADATA:
            con_handle = leaudio_subevent_ascs_server_metadata_get_con_handle(packet);
            ase_id = leaudio_subevent_ascs_server_metadata_get_ase_id(packet);
            MESSAGE("ASCS: METADATA_RECEIVED ase_id %d", ase_id);
            le_audio_util_metadata_using_mask_from_metadata_event(packet, size, &ascs_audio_metadata);
            audio_stream_control_service_server_streamendpoint_metadata_update(con_handle, ase_id, &ascs_audio_metadata);
            break;
        case LEAUDIO_SUBEVENT_ASCS_SERVER_START_READY:
            ase_id = leaudio_subevent_ascs_server_start_ready_get_ase_id(packet);
            con_handle = leaudio_subevent_ascs_server_start_ready_get_con_handle(packet);
            MESSAGE("ASCS: START_READY ase_id %d", ase_id);
            audio_stream_control_service_server_streamendpoint_receiver_start_ready(con_handle, ase_id);
            // if this is a SOURCE ASE, start streaming
            cis_handle = audio_stream_control_service_server_streamendpoint_cis_get_handle(con_handle, ase_id);
            MESSAGE("CSI: request to send ase_id %u, cis handle 0x%04x", ase_id, cis_handle);
            cis_con_handles[cig.num_cis++] = cis_handle;
            hci_request_cis_can_send_now_events(cis_handle);
            break;
        case LEAUDIO_SUBEVENT_ASCS_SERVER_STOP_READY:
            ase_id = leaudio_subevent_ascs_server_stop_ready_get_ase_id(packet);
            con_handle = leaudio_subevent_ascs_server_stop_ready_get_con_handle(packet);
            MESSAGE("ASCS: STOP_READY ase_id %d", ase_id);
            audio_stream_control_service_server_streamendpoint_receiver_stop_ready(con_handle, ase_id);
            break;
        case LEAUDIO_SUBEVENT_ASCS_SERVER_DISABLE:
            ase_id = leaudio_subevent_ascs_server_disable_get_ase_id(packet);
            con_handle = leaudio_subevent_ascs_server_disable_get_con_handle(packet);
            MESSAGE("ASCS: DISABLING ase_id %d", ase_id);
            audio_stream_control_service_server_streamendpoint_disable(con_handle, ase_id);
            ascs_server_stop_streaming(ase_id);
            break;
        case LEAUDIO_SUBEVENT_ASCS_SERVER_RELEASE:
            ase_id            = leaudio_subevent_ascs_server_release_get_ase_id(packet);
            con_handle = leaudio_subevent_ascs_server_release_get_con_handle(packet);
            MESSAGE("ASCS: RELEASE ase_id %d", ase_id);
            audio_stream_control_service_server_streamendpoint_release(con_handle, ase_id);
            // Client request: Release. Accept (enter Releasing State), and trigger Releasing
            // TODO: find better approach
            btstack_run_loop_remove_timer(&ascs_server_released_timer);
            btstack_run_loop_set_timer_handler(&ascs_server_released_timer, &ascs_server_released_timer_handler);
            btstack_run_loop_set_timer(&ascs_server_released_timer, 500);
            btstack_run_loop_add_timer(&ascs_server_released_timer);
            break;

        case LEAUDIO_SUBEVENT_ASCS_SERVER_RELEASED:
            ase_id = leaudio_subevent_ascs_server_released_get_ase_id(packet);
            con_handle = leaudio_subevent_ascs_server_released_get_con_handle(packet);
            MESSAGE("ASCS: RELEASED ase_id %d", ase_id);
            audio_stream_control_service_server_streamendpoint_released(con_handle, ase_id, true);
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
    uint8_t cig_id;
    uint8_t cis_id;
    uint8_t i;

    switch (packet_type) {
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    for (i=0;i<MAX_NUM_CIS;i++){
                        con_handle = hci_event_disconnection_complete_get_connection_handle(packet);
                        if (cis_con_handles[i] == con_handle){
                            MESSAGE("HCI Disconnect for cis_con_handle 0x%04x, with index %u", cis_con_handle, i);
                            cis_con_handles[i] = HCI_CON_HANDLE_INVALID;
                        }
                    }
                    ascs_server_handle_disconnect(hci_event_disconnection_complete_get_connection_handle(packet));
                    break;
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
                        case HCI_SUBEVENT_LE_CIS_REQUEST:
                            con_handle     = hci_subevent_le_cis_request_get_acl_connection_handle(packet);
                            cis_con_handle = hci_subevent_le_cis_request_get_cis_connection_handle(packet);
                            cig_id = hci_subevent_le_cis_request_get_cig_id(packet);
                            cis_id = hci_subevent_le_cis_request_get_cis_id(packet);
                            gap_cis_accept(cis_con_handle);
                            MESSAGE("Accept cig_id %u/cis_id %u with con handle 0x%04x", cig_id, cis_id, cis_con_handle);
                            audio_stream_control_service_server_streamendpoint_cis_accepted(con_handle, cis_con_handle, cig_id, cis_id);
                            break;
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
                            cis_con_handle = gap_subevent_cis_created_get_cis_con_handle(packet);
                            MESSAGE("CIS Created 0x%04x", cis_con_handle);
                            // TODO: check if all CIS have been created in both modes
                            if (response_op == BTP_LE_AUDIO_OP_CIS_CREATE){
                                response_op = 0;
                                btp_send(BTP_SERVICE_ID_LE_AUDIO, BTP_LE_AUDIO_OP_CIS_CREATE, 0, 0, NULL);
                            } else {
                                // TODO: move into ASCS Server
                                // lookup ASE by { CIS Handle, Role == SINK } and report as ready
                                for (i=0;i<ASCS_NUM_CLIENTS;i++){
                                    ascs_server_connection_t * connection = &ascs_clients[i];
                                    uint8_t j;
                                    for (j=0;j<ASCS_NUM_STREAMENDPOINT_CHARACTERISTICS;j++){
                                        ascs_streamendpoint_t * streamendpoint = &connection->streamendpoints[j];
                                        uint8_t ase_id = streamendpoint->ase_characteristic->ase_id;
                                        le_audio_role_t role = streamendpoint->ase_characteristic->role;
                                        switch (streamendpoint->state){
                                            case ASCS_STATE_ENABLING:
                                                if (streamendpoint->cis_handle == cis_con_handle){
                                                    if (role == LE_AUDIO_ROLE_SINK){
                                                        MESSAGE("ASE %u, Role %u, State %u, CIS 0x%04x -> Start Ready", ase_id, role, streamendpoint->state, streamendpoint->cis_handle);
                                                        audio_stream_control_service_server_streamendpoint_receiver_start_ready(connection->con_handle, ase_id);
                                                    }
                                                }
                                                break;
                                            default:
                                                break;
                                        }
                                    }
                                }
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

static void ascs_client_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_LEAUDIO_META) return;

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
        case LEAUDIO_SUBEVENT_ASCS_CLIENT_CONNECTED:
            ascs_cid = leaudio_subevent_ascs_client_connected_get_ascs_cid(packet);
            con_handle = leaudio_subevent_ascs_client_connected_get_con_handle(packet);
            if (leaudio_subevent_ascs_client_connected_get_status(packet) != ERROR_CODE_SUCCESS){
                // TODO send error response
                MESSAGE("ASCS Client: connection failed, cid 0x%02x, con_handle 0x%04x, status 0x%02x", ascs_cid, con_handle,
                        leaudio_subevent_ascs_client_connected_get_status(packet));
                return;
            }
            source_ase_count = leaudio_subevent_ascs_client_connected_get_source_ase_num(packet);
            sink_ase_count = leaudio_subevent_ascs_client_connected_get_sink_ase_num(packet);
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

        case LEAUDIO_SUBEVENT_ASCS_CLIENT_DISCONNECTED:
            MESSAGE("ASCS Client: disconnected, cid 0x%02x", leaudio_subevent_ascs_client_disconnected_get_ascs_cid(packet));
            break;

        case LEAUDIO_SUBEVENT_ASCS_CLIENT_CODEC_CONFIGURATION:
            ase_id = leaudio_subevent_ascs_client_codec_configuration_get_ase_id(packet);
            ascs_cid = leaudio_subevent_ascs_client_codec_configuration_get_ascs_cid(packet);

            // codec id:
            codec_configuration.coding_format =  leaudio_subevent_ascs_client_codec_configuration_get_coding_format(packet);;
            codec_configuration.company_id = leaudio_subevent_ascs_client_codec_configuration_get_company_id(packet);
            codec_configuration.vendor_specific_codec_id = leaudio_subevent_ascs_client_codec_configuration_get_vendor_specific_codec_id(packet);

            codec_configuration.specific_codec_configuration.codec_configuration_mask = leaudio_subevent_ascs_client_codec_configuration_get_specific_codec_configuration_mask(packet);
            codec_configuration.specific_codec_configuration.sampling_frequency_index = leaudio_subevent_ascs_client_codec_configuration_get_sampling_frequency_index(packet);
            codec_configuration.specific_codec_configuration.frame_duration_index = leaudio_subevent_ascs_client_codec_configuration_get_frame_duration_index(packet);
            codec_configuration.specific_codec_configuration.audio_channel_allocation_mask = leaudio_subevent_ascs_client_codec_configuration_get_audio_channel_allocation_mask(packet);
            codec_configuration.specific_codec_configuration.octets_per_codec_frame = leaudio_subevent_ascs_client_codec_configuration_get_octets_per_frame(packet);
            codec_configuration.specific_codec_configuration.codec_frame_blocks_per_sdu = leaudio_subevent_ascs_client_codec_configuration_get_frame_blocks_per_sdu(packet);

            MESSAGE("ASCS Client: CODEC CONFIGURATION - ase_id %d, ascs_cid 0x%02x", ase_id, ascs_cid);
            response_len = 0;
            btp_append_uint24(leaudio_subevent_ascs_client_codec_configuration_get_presentation_delay_min(packet));
            btp_append_uint24(leaudio_subevent_ascs_client_codec_configuration_get_presentation_delay_max(packet));
            btp_send(BTP_SERVICE_ID_LE_AUDIO, BTP_LE_AUDIO_OP_ASCS_CONFIGURE_CODEC, 0, response_len, response_buffer);
            break;

        case LEAUDIO_SUBEVENT_ASCS_CLIENT_QOS_CONFIGURATION:
            ase_id   = leaudio_subevent_ascs_client_qos_configuration_get_ase_id(packet);
            ascs_cid = leaudio_subevent_ascs_client_qos_configuration_get_ascs_cid(packet);

            MESSAGE("ASCS Client: QOS CONFIGURATION - ase_id %d, ascs_cid 0x%02x", ase_id, ascs_cid);
            if (response_op != 0){
                btp_send(BTP_SERVICE_ID_LE_AUDIO, response_op, 0, 0, NULL);
                response_op = 0;
            }
            break;

        case LEAUDIO_SUBEVENT_ASCS_CLIENT_METADATA:
            ase_id  = leaudio_subevent_ascs_client_metadata_get_ase_id(packet);
            ascs_cid = leaudio_subevent_ascs_client_metadata_get_ascs_cid(packet);

            printf("ASCS Client: METADATA UPDATE - ase_id %d, ascs_cid 0x%02x\n", ase_id, ascs_cid);
            break;

        case LEAUDIO_SUBEVENT_ASCS_CLIENT_CONTROL_POINT_OPERATION_RESPONSE:
            ase_id        = leaudio_subevent_ascs_client_control_point_operation_response_get_ase_id(packet);
            ascs_cid      = leaudio_subevent_ascs_client_control_point_operation_response_get_ascs_cid(packet);
            response_code = leaudio_subevent_ascs_client_control_point_operation_response_get_response_code(packet);
            reason        = leaudio_subevent_ascs_client_control_point_operation_response_get_reason(packet);
            opcode        = leaudio_subevent_ascs_client_control_point_operation_response_get_opcode(packet);
            printf("ASCS Client: Operation complete - ase_id %d, opcode %u, response [0x%02x, 0x%02x], ascs_cid 0x%02x\n", ase_id, opcode, response_code, reason, ascs_cid);
            if ((opcode == ASCS_OPCODE_RECEIVER_START_READY) && (response_op != 0)){
                MESSAGE("Receiver Start Ready completed");
                btp_send(BTP_SERVICE_ID_LE_AUDIO, response_op, 0, 0, NULL);
                response_op = 0;
            }
            break;

        case LEAUDIO_SUBEVENT_ASCS_CLIENT_STREAMENDPOINT_STATE:
            log_info("LEAUDIO_SUBEVENT_ASCS_STREAMENDPOINT_STATE");
            ascs_cid   = leaudio_subevent_ascs_client_streamendpoint_state_get_ascs_cid(packet);
            ase_id     = leaudio_subevent_ascs_client_streamendpoint_state_get_ase_id(packet);
            ase_state  = leaudio_subevent_ascs_client_streamendpoint_state_get_state(packet);
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

        default:
            break;
    }
}

// PACS
static void pacs_client_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_LEAUDIO_META) return;

    uint32_t audio_allocation_mask;
    uint16_t ascs_cid;
    uint16_t pacs_cid;
    hci_con_handle_t con_handle;
    uint8_t i;
    uint8_t status;

    switch (hci_event_gattservice_meta_get_subevent_code(packet)){
        case LEAUDIO_SUBEVENT_PACS_CLIENT_CONNECTED:
            pacs_cid = leaudio_subevent_pacs_client_connected_get_pacs_cid(packet);
            con_handle = leaudio_subevent_pacs_client_connected_get_con_handle(packet);
            status = leaudio_subevent_pacs_client_connected_get_status(packet);
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

        case LEAUDIO_SUBEVENT_PACS_CLIENT_DISCONNECTED:
            pacs_cid = 0;
            MESSAGE("PACS Client: disconnected\n");
            break;

        case LEAUDIO_SUBEVENT_PACS_CLIENT_OPERATION_DONE:
            if (leaudio_subevent_pacs_client_operation_done_get_status(packet) == ERROR_CODE_SUCCESS){
                MESSAGE("      Operation successful");
            } else {
                MESSAGE("      Operation failed with status 0x%02X", leaudio_subevent_pacs_client_operation_done_get_status(packet));
            }
            break;

        case LEAUDIO_SUBEVENT_PACS_CLIENT_AUDIO_LOCATIONS:
            audio_allocation_mask = leaudio_subevent_pacs_client_audio_locations_get_audio_locations_mask(packet);
            MESSAGE("PACS Client: %s Audio Locations 0x%04x",
                leaudio_subevent_pacs_client_audio_locations_get_le_audio_role(packet) == LE_AUDIO_ROLE_SINK ? "Sink" : "Source", audio_allocation_mask);
            break;

        case LEAUDIO_SUBEVENT_PACS_CLIENT_AVAILABLE_AUDIO_CONTEXTS:
            MESSAGE("PACS Client: Available Audio Contexts");
            MESSAGE("      Sink   0x%02X", leaudio_subevent_pacs_client_available_audio_contexts_get_sink_mask(packet));
            MESSAGE("      Source 0x%02X", leaudio_subevent_pacs_client_available_audio_contexts_get_source_mask(packet));
            break;

        case LEAUDIO_SUBEVENT_PACS_CLIENT_SUPPORTED_AUDIO_CONTEXTS:
            MESSAGE("PACS Client: Supported Audio Contexts\n");
            MESSAGE("      Sink   0x%02X\n", leaudio_subevent_pacs_client_supported_audio_contexts_get_sink_mask(packet));
            MESSAGE("      Source 0x%02X\n", leaudio_subevent_pacs_client_supported_audio_contexts_get_source_mask(packet));
            break;

        case LEAUDIO_SUBEVENT_PACS_CLIENT_PACK_RECORD:
            MESSAGE("PACS Client: %s PAC Record\n", leaudio_subevent_pacs_client_pack_record_get_le_audio_role(packet) == LE_AUDIO_ROLE_SINK ? "Sink" : "Source");
            MESSAGE("      %s PAC Record DONE\n", leaudio_subevent_pacs_client_pack_record_done_get_le_audio_role(packet) == LE_AUDIO_ROLE_SINK ? "Sink" : "Source");
            break;
        case LEAUDIO_SUBEVENT_PACS_CLIENT_PACK_RECORD_DONE:
            MESSAGE("      %s PAC Record DONE\n", leaudio_subevent_pacs_client_pack_record_done_get_le_audio_role(packet) == LE_AUDIO_ROLE_SINK ? "Sink" : "Source");
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
                                                                                       ase_id_used_by_configure_codec, &ascs_server_codec_configuration);

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
                uint8_t status = audio_stream_control_service_client_streamendpoint_enable(ascs_cid, ase_id, &ascs_metadata);
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
                if (ascs_cid > 0){
                    uint8_t status = audio_stream_control_service_client_streamendpoint_disable(ascs_cid, ase_id);
                    expect_status_no_error(status);
                } else {
                    audio_stream_control_service_server_streamendpoint_disable(ascs_server_current_client_con_handle, ase_id);
                    response_len = 0;
                    btp_send(BTP_SERVICE_ID_LE_AUDIO, BTP_LE_AUDIO_OP_ASCS_DISABLE, 0, response_len, response_buffer);
                }
            }
            break;
        case BTP_LE_AUDIO_OP_ASCS_RELEASE:
            if (controller_index == 0) {
                uint16_t ascs_cid = data[0];
                ase_id = data[1];
                MESSAGE("BTP_LE_AUDIO_OP_ASCS_RELEASE ase_id %u", ase_id);
                if (ascs_cid > 0){
                    uint8_t status = audio_stream_control_service_client_streamendpoint_release(ascs_cid, ase_id, false);
                    expect_status_no_error(status);
                } else {
                    audio_stream_control_service_server_streamendpoint_release(ascs_server_current_client_con_handle, ase_id);
                    response_len = 0;
                    btp_send(BTP_SERVICE_ID_LE_AUDIO, BTP_LE_AUDIO_OP_ASCS_RELEASE, 0, response_len, response_buffer);
                    // PTS is waiting for RELEASED operation
                    btstack_run_loop_remove_timer(&ascs_server_released_timer);
                    btstack_run_loop_set_timer_handler(&ascs_server_released_timer, &ascs_server_released_timer_handler);
                    btstack_run_loop_set_timer(&ascs_server_released_timer, 100);
                    btstack_run_loop_add_timer(&ascs_server_released_timer);
                }
            }
            break;
        case BTP_LE_AUDIO_OP_ASCS_UPDATE_METADATA:
            if (controller_index == 0){
                uint16_t ascs_cid = data[0];
                ase_id = data[1];
                MESSAGE("BTP_LE_AUDIO_OP_ASCS_UPDATE_METADATA ase_id %u", ase_id);
                ascs_metadata.metadata_mask = (1 << LE_AUDIO_METADATA_TYPE_PREFERRED_AUDIO_CONTEXTS) &
                                              (1 << LE_AUDIO_METADATA_TYPE_STREAMING_AUDIO_CONTEXTS);
                ascs_metadata.preferred_audio_contexts_mask = LE_AUDIO_CONTEXT_MASK_MEDIA;
                ascs_metadata.streaming_audio_contexts_mask = LE_AUDIO_CONTEXT_MASK_MEDIA;
                if (ascs_cid > 0) {
                    uint8_t status = audio_stream_control_service_client_streamendpoint_metadata_update(ascs_cid,
                                                                                                        ase_id,
                                                                                                        &ascs_metadata);
                    expect_status_no_error(status);
                } else {
                    audio_stream_control_service_server_streamendpoint_metadata_update(ascs_server_current_client_con_handle, ase_id, &ascs_metadata);
                    response_len = 0;
                    btp_send(BTP_SERVICE_ID_LE_AUDIO, BTP_LE_AUDIO_OP_ASCS_UPDATE_METADATA, 0, response_len, response_buffer);
                }
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
                btp_send(BTP_SERVICE_ID_LE_AUDIO, BTP_LE_AUDIO_OP_CIS_START_STREAMING,
                         controller_index, 0, NULL);
                hci_con_handle_t cis_con_handle = get_cis_con_handle_for_cig_cis_id(cig_id, cis_id);
                MESSAGE("Request can send now 0x%04x", cis_con_handle);
                hci_request_cis_can_send_now_events(cis_con_handle);
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
    sink_node.audio_locations_mask = LE_AUDIO_LOCATION_MASK_FRONT_LEFT | LE_AUDIO_LOCATION_MASK_FRONT_RIGHT;
    sink_node.available_audio_contexts_mask = LE_AUDIO_CONTEXT_MASK_UNSPECIFIED;
    sink_node.supported_audio_contexts_mask = LE_AUDIO_CONTEXT_MASK_UNSPECIFIED;

    source_node.records = &source_pac_records[0];
    source_node.records_num = 1;
    source_node.audio_locations_mask = LE_AUDIO_LOCATION_MASK_FRONT_LEFT | LE_AUDIO_LOCATION_MASK_FRONT_RIGHT;
    source_node.available_audio_contexts_mask = LE_AUDIO_CONTEXT_MASK_UNSPECIFIED;
    source_node.supported_audio_contexts_mask = LE_AUDIO_CONTEXT_MASK_UNSPECIFIED;

    published_audio_capabilities_service_server_init(&sink_node, &source_node);
    published_audio_capabilities_service_server_register_packet_handler(&pacs_server_packet_handler);

    // ASCS Server
    audio_stream_control_service_server_init(ASCS_NUM_STREAMENDPOINT_CHARACTERISTICS, ascs_streamendpoint_characteristics, ASCS_NUM_CLIENTS, ascs_clients);
    audio_stream_control_service_server_register_packet_handler(&ascs_server_packet_handler);

    // CIS
    uint8_t i;
    for (i=0;i<MAX_NUM_CIS;i++){
        cis_con_handles[i] = HCI_CON_HANDLE_INVALID;
    }
}

// PACS
void btp_pacs_handler(uint8_t opcode, uint8_t controller_index, uint16_t length, const uint8_t *data) {
    // provide op info for response
    response_len = 0;
    response_service_id = BTP_SERVICE_ID_PACS;
    response_op = opcode;
    switch (opcode) {
        case BTP_PACS_READ_SUPPORTED_COMMANDS:
        MESSAGE("BTP_ASCS_READ_SUPPORTED_COMMANDS");
            if (controller_index == BTP_INDEX_NON_CONTROLLER) {
                uint8_t commands = 0;
                btp_send(response_service_id, opcode, controller_index, 1, &commands);
            }
            break;
        case BTP_PACS_SET_AVAILABLE_CONTEXTS:
            if (controller_index == 0){
                /*
                uint16_t sink_contexts;
                uint16_t source_contexts;
                */
                uint8_t offset = 0;
                uint16_t sink_available_contexts = little_endian_read_16(data, offset);
                offset += 2;
                uint16_t source_available_context = little_endian_read_16(data, offset);
                published_audio_capabilities_service_server_set_available_audio_contexts(sink_available_contexts, source_available_context);
                audio_stream_control_service_server_set_available_audio_contexts(sink_available_contexts, source_available_context);
                btp_send(response_service_id, opcode, controller_index, 0, NULL);
                MESSAGE("BTP_PACS_SET_AVAILABLE_CONTEXTS: sink 0x%04x,source 0x%04x", sink_available_contexts, source_available_context);
            }
            break;
        case BTP_PACS_SET_SUPPORTED_CONTEXTS:
            if (controller_index == 0){
                /*
                uint16_t sink_contexts;
                uint16_t source_contexts;
                */
                uint8_t offset = 0;
                uint16_t sink_supported_contexts = little_endian_read_16(data, offset);
                offset += 2;
                uint16_t source_supported_context = little_endian_read_16(data, offset);
                published_audio_capabilities_service_server_set_supported_audio_contexts(sink_supported_contexts, source_supported_context);
                btp_send(response_service_id, opcode, controller_index, 0, NULL);
                MESSAGE("BTP_PACS_SET_SUPPORTED_CONTEXTS: sink 0x%04x,source 0x%04x", sink_supported_contexts, source_supported_context);
            }
            break;
        default:
        MESSAGE("BTP PACS Operation 0x%02x not implemented", opcode);
            btstack_assert(false);
            break;
    }
}

void btp_pacs_init(void) {
}