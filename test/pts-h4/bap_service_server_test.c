/*
 * Copyright (C) 2014 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "bap_service_server_test.c"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "bap_service_server_test.h"
#include "btstack.h"

#define CSIS_COORDINATORS_MAX_NUM  3 

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static uint16_t att_read_callback(hci_con_handle_t con_handle, uint16_t att_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size);
static int att_write_callback(hci_con_handle_t con_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size);

static int  le_notification_enabled;
static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_packet_callback_registration_t sm_event_callback_registration;
static le_audio_metadata_t     ascs_server_audio_metadata;

typedef enum {
    BAP_APP_SERVER_STATE_IDLE = 0,
    BAP_APP_SERVER_STATE_CONNECTED
} bap_app_server_state_t; 

static bap_app_server_state_t  bap_app_server_state      = BAP_APP_SERVER_STATE_IDLE;
static hci_con_handle_t        bap_app_server_con_handle = HCI_CON_HANDLE_INVALID;
static hci_con_handle_t        bap_app_server_cis_handle = HCI_CON_HANDLE_INVALID;
static bd_addr_t               bap_app_client_addr;

#define APP_AD_FLAGS 0x06

static uint8_t adv_data[] = {
    // Flags general discoverable
    0x02, BLUETOOTH_DATA_TYPE_FLAGS, APP_AD_FLAGS,
    // RSI
    0x07, BLUETOOTH_DATA_TYPE_RESOLVABLE_SET_IDENTIFIER, 0x28, 0x31, 0xB6, 0x4C, 0x39, 0xCC,
    // ASCS
    0x09, BLUETOOTH_DATA_TYPE_SERVICE_DATA_16_BIT_UUID, 0x4E, 0x18, 1,  0, 0, 0, 0,  0,
    // CSIS
    0x03, BLUETOOTH_DATA_TYPE_INCOMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS, 0x46, 0x18,
    // Name
    0x04, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, 'I', 'U', 'T',
};

static const uint8_t adv_sid = 0;
static le_advertising_set_t le_advertising_set;
static uint8_t adv_handle = 0;

const uint8_t adv_data_len = sizeof(adv_data);

static const le_extended_advertising_parameters_t extended_params = {
        .advertising_event_properties = 1,  // connectable
        .primary_advertising_interval_min = 0x4b0, // 750 ms
        .primary_advertising_interval_max = 0x4b0, // 750 ms
        .primary_advertising_channel_map = 7,
        .own_address_type = 1,
        .peer_address_type = 0,
        .peer_address =  { 0 },
        .advertising_filter_policy = 0,
        .advertising_tx_power = 10, // 10 dBm
        .primary_advertising_phy = 1, // LE 1M PHY
        .secondary_advertising_max_skip = 0,
        .secondary_advertising_phy = 1, // LE 1M PHY
        .advertising_sid = adv_sid,
        .scan_request_notification_enable = 0,
};


static void setup_advertising(void) {
    bd_addr_t random_address = { 0xC1, 0x01, 0x01, 0x01, 0x01, 0x01 };
    gap_extended_advertising_setup(&le_advertising_set, &extended_params, &adv_handle);
    gap_extended_advertising_set_adv_data(adv_handle, sizeof(adv_data), adv_data);
    gap_extended_advertising_set_random_address(adv_handle, random_address);
    gap_extended_advertising_start(adv_handle, 0, 0);
}

static ascs_codec_configuration_t ascs_codec_configuration = {
    0x01, 0x00, 0x00, 0x0FA0,
    0x000005, 0x00FFAA, 0x000000, 0x000000,
    HCI_AUDIO_CODING_FORMAT_LC3, 0x0000, 0x0000,
    {   0x1E,
        0x03, 0x01, 0x00000001, 0x0028, 0x00
    }
};

// PACS
static pacs_record_t sink_pac_records[] = {
    // sink_record_0
    {
        // codec ID
        {HCI_AUDIO_CODING_FORMAT_LC3, 0x0000, 0x0000},
        // capabilities
        {
            0x3E,
            LE_AUDIO_CODEC_SAMPLING_FREQUENCY_MASK_16000_HZ | LE_AUDIO_CODEC_SAMPLING_FREQUENCY_MASK_44100_HZ,
            LE_AUDIO_CODEC_FRAME_DURATION_MASK_7500US | LE_AUDIO_CODEC_FRAME_DURATION_MASK_10000US,
            LE_AUDIO_CODEC_AUDIO_CHANNEL_COUNT_MASK_1 | LE_AUDIO_CODEC_AUDIO_CHANNEL_COUNT_MASK_2 | LE_AUDIO_CODEC_AUDIO_CHANNEL_COUNT_MASK_4 | LE_AUDIO_CODEC_AUDIO_CHANNEL_COUNT_MASK_8,
            0x11AA,
            0xBB22,
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
            LE_AUDIO_CODEC_SAMPLING_FREQUENCY_MASK_16000_HZ | LE_AUDIO_CODEC_SAMPLING_FREQUENCY_MASK_44100_HZ,
            LE_AUDIO_CODEC_FRAME_DURATION_MASK_7500US | LE_AUDIO_CODEC_FRAME_DURATION_MASK_10000US,
            LE_AUDIO_CODEC_AUDIO_CHANNEL_COUNT_MASK_1 | LE_AUDIO_CODEC_AUDIO_CHANNEL_COUNT_MASK_2 | LE_AUDIO_CODEC_AUDIO_CHANNEL_COUNT_MASK_4 | LE_AUDIO_CODEC_AUDIO_CHANNEL_COUNT_MASK_8,
            0x11AA,
            0xBB22,
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

// BASS
#define BASS_NUM_CLIENTS 1
#define BASS_NUM_SOURCES 1
static bass_server_source_t bass_sources[] = {
    { 
        // bass_source_data_t
        {
            // address_type, address
            BD_ADDR_TYPE_LE_PUBLIC, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 
            // adv_sid
            0, 
            // broadcast_id
            0, 
            // pa_sync
            LE_AUDIO_PA_SYNC_DO_NOT_SYNCHRONIZE_TO_PA,
            // pa_sync_state
            LE_AUDIO_PA_SYNC_STATE_NOT_SYNCHRONIZED_TO_PA,
            // pa_interval
            0,
            // subgroups_num
            1,
            {
                // bass_subgroup_t
                {
                    // bis_sync
                    0,
                    // bis_sync_state
                    0,
                    // metadata_length
                    0,
                    // le_audio_metadata_t
                    {0}
                }
            }
        },
        
    
        // update_counter, source_id, in_use
        0, 0, false, 
        
        // big_encryption
        LE_AUDIO_BIG_ENCRYPTION_NOT_ENCRYPTED,
        // bad_code
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},  
        // bass_receive_state_handle
        0x00, 
        // bass_receive_state_client_configuration_handle
        0x00, 
        // bass_receive_state_client_configuration
        0x00, 
    }
};

static bass_source_data_t source_data1 = {
    // address_type, address
     
    BD_ADDR_TYPE_LE_PUBLIC, {0xC0, 0x07, 0xE8, 0x7E, 0x55, 0x5F}, 
    // adv_sid
    0x01, 
    // broadcast_id
    0, 
    // pa_sync
    LE_AUDIO_PA_SYNC_DO_NOT_SYNCHRONIZE_TO_PA,
    // pa_sync_state
    LE_AUDIO_PA_SYNC_STATE_NOT_SYNCHRONIZED_TO_PA,
    // pa_interval
    0,
    // subgroups_num
    1,
    {
        {
            0,
            0, 
            45 - 15, 
            {
                // all metadata set
                (1 << LE_AUDIO_METADATA_TYPE_STREAMING_AUDIO_CONTEXTS), 
                // (2) preferred_audio_contexts_mask
                LE_AUDIO_CONTEXT_MASK_UNSPECIFIED,
                // (2) streaming_audio_contexts_mask
                0x4000,
                // program_info
                0, {0},
                // language_code
                0x00000,
                // ccids_num
                0, {0},
                // parental_rating
                LE_AUDIO_PARENTAL_RATING_NO_RATING,
                // program_info_uri_length
                0, {0},
                // extended_metadata_type
                0x0001, 0, {0},
                // vendor_specific_metadata_type
                0x0002, 0, {0},
            }

        }
    }
    
};

static csis_server_connection_t csis_coordinators[CSIS_COORDINATORS_MAX_NUM];
static gap_privacy_client_t csis_privacy_client;
static bool csis_privacy_update_pending;

static uint8_t bad_code[] = {0x01, 0x02, 0x68, 0x05, 0x53, 0xF1, 0x41, 0x5A, 0xA2, 0x65, 0xBB, 0xAF, 0xC6, 0xEA, 0x03, 0xB8};
static bass_server_connection_t bass_clients[BASS_NUM_CLIENTS];

static le_audio_pa_sync_state_t pa_sync_state = LE_AUDIO_PA_SYNC_STATE_NOT_SYNCHRONIZED_TO_PA;
static uint8_t source_id = 0;
static uint8_t ase_id = 0;

// ASCS
#define ASCS_NUM_STREAMENDPOINT_CHARACTERISTICS 5
#define ASCS_NUM_CLIENTS 3

static ascs_streamendpoint_characteristic_t ascs_streamendpoint_characteristics[ASCS_NUM_STREAMENDPOINT_CHARACTERISTICS];
static ascs_server_connection_t ascs_clients[ASCS_NUM_CLIENTS];

// PSPX_sirk
static uint8_t sirk[] = {
    0x83, 0x8E, 0x68, 0x05, 0x53, 0xF1, 0x41, 0x5A,
    0xA2, 0x65, 0xBB, 0xAF, 0xC6, 0xEA, 0x03, 0xB8   
};

static void enter_streaming(void){
    printf("TODO enter streaming\n");
}

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;

    switch (hci_event_packet_get_type(packet)) {
        case ATT_EVENT_CAN_SEND_NOW:
            // att_server_notify(con_handle, ATT_CHARACTERISTIC_0000FF11_0000_1000_8000_00805F9B34FB_01_VALUE_HANDLE, (uint8_t*) counter_string, counter_string_len);
            break;
       case SM_EVENT_JUST_WORKS_REQUEST:
            printf("Accept Just Works\n");
            sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
            break;
       case SM_EVENT_NUMERIC_COMPARISON_REQUEST:
            printf("Confirming numeric comparison: %"PRIu32"\n", sm_event_numeric_comparison_request_get_passkey(packet));
            sm_numeric_comparison_confirm(sm_event_passkey_display_number_get_handle(packet));
            break;

       case HCI_EVENT_LE_META:
            switch(hci_event_le_meta_get_subevent_code(packet)) {
                case HCI_SUBEVENT_LE_CIS_REQUEST:
                    bap_app_server_cis_handle = hci_subevent_le_cis_request_get_cis_connection_handle(packet);
                    gap_cis_accept(bap_app_server_cis_handle);
                    break;
                default:
                    break;
            }
            break;

       case HCI_EVENT_META_GAP:
            switch (hci_event_gap_meta_get_subevent_code(packet)){
                case GAP_SUBEVENT_LE_CONNECTION_COMPLETE:
                    bap_app_server_con_handle = gap_subevent_le_connection_complete_get_connection_handle(packet);
                    bap_app_server_state      = BAP_APP_SERVER_STATE_CONNECTED;
                    gap_subevent_le_connection_complete_get_peer_address(packet, bap_app_client_addr);
                    printf("BAP Server: Connection to %s established\n", bd_addr_to_str(bap_app_client_addr));
                    break;
                case GAP_SUBEVENT_CIS_CREATED:
                    // only look for cis handle
                    enter_streaming();
                    audio_stream_control_service_server_streamendpoint_receiver_start_ready(bap_app_server_con_handle, 1);
                    break;
                default:
                    return;
            }
            break;

       case HCI_EVENT_DISCONNECTION_COMPLETE:
            bap_app_server_con_handle = HCI_CON_HANDLE_INVALID;
            bap_app_server_state      = BAP_APP_SERVER_STATE_IDLE;

            le_notification_enabled = 0;

            printf("BAP Server: Disconnected from %s\n", bd_addr_to_str(bap_app_client_addr));

            gap_extended_advertising_start(adv_handle, 0, 0);

            break;

       default:
            break;
    }
}


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

static void bass_server_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_LEAUDIO_META) return;
    
    switch (hci_event_gattservice_meta_get_subevent_code(packet)){
        case LEAUDIO_SUBEVENT_BASS_SERVER_SCAN_STOPPED:
            printf("BASS: remote scan stopped\n");
            break;
        case LEAUDIO_SUBEVENT_BASS_SERVER_SCAN_STARTED:
            printf("BASS: remote scan started\n");
            break;
        
        case LEAUDIO_SUBEVENT_BASS_SERVER_SOURCE_ADDED:
            source_id = leaudio_subevent_bass_server_source_added_get_source_id(packet);
            printf("BASS: source[%d] added, py_sync %d\n", source_id, leaudio_subevent_bass_server_source_added_get_pa_sync(packet));
            btstack_assert(source_id < BASS_NUM_SOURCES);

            switch (leaudio_subevent_bass_server_source_added_get_pa_sync(packet)){
                case LE_AUDIO_PA_SYNC_DO_NOT_SYNCHRONIZE_TO_PA:
                    bass_sources[source_id].data.pa_sync_state = LE_AUDIO_PA_SYNC_STATE_NOT_SYNCHRONIZED_TO_PA;
                    break;
                case LE_AUDIO_PA_SYNC_SYNCHRONIZE_TO_PA_PAST_AVAILABLE:
                case LE_AUDIO_PA_SYNC_SYNCHRONIZE_TO_PA_PAST_NOT_AVAILABLE:
                    bass_sources[source_id].data.pa_sync_state = LE_AUDIO_PA_SYNC_STATE_SYNCHRONIZED_TO_PA;
                    break;
                default:
                    btstack_assert(false);
                    return;
            }
            broadcast_audio_scan_service_server_set_pa_sync_state(source_id, bass_sources[source_id].data.pa_sync_state);
            printf("BASS: source[%d], py_sync_state %d\n", source_id, bass_sources[source_id].data.pa_sync_state);
            break;
        
        case LEAUDIO_SUBEVENT_BASS_SERVER_SOURCE_MODIFIED:
            source_id = leaudio_subevent_bass_server_source_modified_get_source_id(packet);
            printf("BASS: source[%d] modified, py_sync %d\n", source_id, leaudio_subevent_bass_server_source_added_get_pa_sync(packet));
            break;

        case LEAUDIO_SUBEVENT_BASS_SERVER_SOURCE_DELETED:
            source_id = leaudio_subevent_bass_server_source_deleted_get_source_id(packet);
            printf("BASS: source deleted %d, %d\n", source_id, pa_sync_state);
            btstack_assert(source_id < BASS_NUM_SOURCES);
            break;

        default:
            break;
    }
}

static void ascs_server_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_LEAUDIO_META) return;

    hci_con_handle_t con_handle;
    uint8_t status;

    ascs_codec_configuration_t codec_configuration;
    ascs_qos_configuration_t   qos_configuration;


    switch (hci_event_gattservice_meta_get_subevent_code(packet)){
        
        case LEAUDIO_SUBEVENT_ASCS_SERVER_CONNECTED:
            con_handle = leaudio_subevent_ascs_server_connected_get_con_handle(packet);
            status =     leaudio_subevent_ascs_server_connected_get_status(packet);

            if (status != ERROR_CODE_SUCCESS){
                printf("ASCS Server: connection to client failed, con_handle 0x%02x, status 0x%02x\n", con_handle, status);
                return;
            }
            printf("ASCS Server: connected, con_handle 0x%02x\n", con_handle);
            break;

        case LEAUDIO_SUBEVENT_ASCS_SERVER_DISCONNECTED:
            con_handle = leaudio_subevent_ascs_server_disconnected_get_con_handle(packet);
            printf("ASCS Server: disconnected, con_handle 0x%02x\n", con_handle);
            break;

        case LEAUDIO_SUBEVENT_ASCS_SERVER_CODEC_CONFIGURATION:
            ase_id = leaudio_subevent_ascs_server_codec_configuration_get_ase_id(packet);
            con_handle = leaudio_subevent_ascs_server_codec_configuration_get_con_handle(packet);
            codec_configuration.framing = LE_AUDIO_UNFRAMED_ISOAL_MASK_PDUS_SUPPORTED;
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

            printf("ASCS: CODEC_CONFIGURATION_RECEIVED ase_id %d, con_handle 0x%02x\n", ase_id, con_handle);
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

            printf("ASCS: QOS_CONFIGURATION_RECEIVED ase_id %d\n", ase_id);
            audio_stream_control_service_server_streamendpoint_configure_qos(con_handle, ase_id, &qos_configuration);
            break;

        case LEAUDIO_SUBEVENT_ASCS_SERVER_ENABLE:
            ase_id = leaudio_subevent_ascs_server_disable_get_ase_id(packet);
            con_handle = leaudio_subevent_ascs_server_disable_get_con_handle(packet);
            printf("ASCS: ENABLE ase_id %d\n", ase_id);
            le_audio_util_metadata_using_mask_from_enable_event(packet, size, &ascs_server_audio_metadata);
            audio_stream_control_service_server_streamendpoint_enable(con_handle, ase_id, &ascs_server_audio_metadata);
            break;

        case LEAUDIO_SUBEVENT_ASCS_SERVER_METADATA:
            ase_id = leaudio_subevent_ascs_server_metadata_get_ase_id(packet);
            con_handle = leaudio_subevent_ascs_server_metadata_get_con_handle(packet);
            printf("ASCS: METADATA_RECEIVED ase_id %d\n", ase_id);
            le_audio_util_metadata_using_mask_from_metadata_event(packet, size, &ascs_server_audio_metadata);
            audio_stream_control_service_server_streamendpoint_enable(con_handle, ase_id, &ascs_server_audio_metadata);
            break;
            
        case LEAUDIO_SUBEVENT_ASCS_SERVER_START_READY:
            ase_id = leaudio_subevent_ascs_server_start_ready_get_ase_id(packet);
            con_handle = leaudio_subevent_ascs_server_start_ready_get_con_handle(packet);
            printf("ASCS: START_READY ase_id %d\n", ase_id);
            audio_stream_control_service_server_streamendpoint_receiver_start_ready(con_handle, ase_id);
            break;
        case LEAUDIO_SUBEVENT_ASCS_SERVER_STOP_READY:
            ase_id = leaudio_subevent_ascs_server_stop_ready_get_ase_id(packet);
            con_handle = leaudio_subevent_ascs_server_stop_ready_get_con_handle(packet);
            printf("ASCS: STOP_READY ase_id %d\n", ase_id);
            audio_stream_control_service_server_streamendpoint_receiver_stop_ready(con_handle, ase_id);
            break;
        case LEAUDIO_SUBEVENT_ASCS_SERVER_DISABLE:
            ase_id = leaudio_subevent_ascs_server_disable_get_ase_id(packet);
            con_handle = leaudio_subevent_ascs_server_disable_get_con_handle(packet);
            printf("ASCS: DISABLE ase_id %d\n", ase_id);
            audio_stream_control_service_server_streamendpoint_disable(con_handle, ase_id);
            break;
        case LEAUDIO_SUBEVENT_ASCS_SERVER_RELEASE:
            ase_id = leaudio_subevent_ascs_server_release_get_ase_id(packet);
            con_handle = leaudio_subevent_ascs_server_release_get_con_handle(packet);
            printf("ASCS: RELEASE ase_id %d\n", ase_id);
            audio_stream_control_service_server_streamendpoint_release(con_handle, ase_id);
            break;

        case LEAUDIO_SUBEVENT_ASCS_SERVER_RELEASED:
            ase_id = leaudio_subevent_ascs_server_released_get_ase_id(packet);
            con_handle = leaudio_subevent_ascs_server_released_get_con_handle(packet);
            printf("ASCS: RELEASED ase_id %d\n", ase_id);
            audio_stream_control_service_server_streamendpoint_released(con_handle, ase_id, false);
            break;

        default:
            break;
    }
}

static void csis_privacy_handler(gap_privacy_client_t * client, bd_addr_t random_addr){
    printf("Privacy: new random address %s\n", bd_addr_to_str(random_addr));
    csis_privacy_update_pending = true;
    coordinated_set_identification_service_server_generate_rsi();
}

static void csis_server_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_LEAUDIO_META) return;

    hci_con_handle_t con_handle;
    uint8_t status;
    uint8_t ris[6];

    switch (hci_event_gattservice_meta_get_subevent_code(packet)){
        
        case LEAUDIO_SUBEVENT_CSIS_SERVER_CONNECTED:
            con_handle = leaudio_subevent_csis_server_connected_get_con_handle(packet);
            status =     leaudio_subevent_csis_server_connected_get_status(packet);

            if (status != ERROR_CODE_SUCCESS){
                printf("CSIS Server: connection to coordinator failed, con_handle 0x%02x, status 0x%02x\n", con_handle, status);
                return;
            }
            printf("CSIS Server: connected, con_handle 0x%02x\n", con_handle);
            break;

        case LEAUDIO_SUBEVENT_CSIS_SERVER_RSI:
            printf("CSIS Server: RSI \n");
            leaudio_subevent_csis_server_rsi_get_rsi(packet, ris);
            reverse_48(ris, &adv_data[5]);
            printf_hexdump(ris, 6);
            //
            printf("Setup advertising\n");
            setup_advertising();
            // report done
            if (csis_privacy_update_pending){
                csis_privacy_update_pending = false;
                gap_privacy_client_ready(&csis_privacy_client);
            }
            break;

        case LEAUDIO_SUBEVENT_CSIS_SERVER_DISCONNECTED:
            con_handle = leaudio_subevent_csis_server_disconnected_get_con_handle(packet);
            printf("CSIS Server: RELEASED\n");
            break;

        default:
            break;
    }
}

// ATT Client Read Callback for Dynamic Data
// - if buffer == NULL, don't copy data, just return size of value
// - if buffer != NULL, copy data and return number bytes copied
// @param offset defines start of attribute value
static uint16_t att_read_callback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
    UNUSED(connection_handle);
    UNUSED(att_handle);
    UNUSED(offset);
    UNUSED(buffer);
    UNUSED(buffer_size);

    return 0;
}

static int att_write_callback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
    UNUSED(connection_handle);
    UNUSED(att_handle);
    UNUSED(transaction_mode);
    UNUSED(offset);
    UNUSED(buffer);
    UNUSED(buffer_size);
    return 0;
}

static void show_usage(void){
    uint8_t   iut_address_type;
    bd_addr_t iut_address;
    gap_le_get_own_address(&iut_address_type, iut_address);
    printf("TSPX_bd_addr_iut: ");
    for (uint8_t i=0;i<6;i++) printf("%02x", iut_address[i]);
    printf("\n");

    printf("## PACS\n");
    printf("a - trigger Sink PAC record notification\n");
    printf("A - trigger Source PAC record notification\n");

    printf("b - set Sink audio locations\n");
    printf("B - set Source audio locations\n");
    
    printf("c - set available audio contexts\n");
    printf("d - set supported audio contexts\n");

    printf("\n## BASS\n");
    printf("0 - set pa_sync_state[%d] to NOT_SYNCHRONIZED_TO_PA\n", source_id);
    printf("1 - set pa_sync_state[%d] to SYNCINFO_REQUEST\n", source_id);
    printf("2 - set pa_sync_state[%d] to SYNCHRONIZED_TO_PA\n", source_id);
    printf("3 - set pa_sync_state[%d] to FAILED_TO_SYNCHRONIZE_TO_PA\n", source_id);
    printf("4 - set pa_sync_state[%d] to NO_PAST\n", source_id);

    printf("r - set bis_sync[%d] to 0x00000000\n", source_id);
    printf("s - set bis_sync[%d] to 0x00000001\n", source_id);
    printf("m - set big_encryption[%d] to 0x01\n", source_id);
    printf("n - set big_encryption[%d] to 0x02\n", source_id);
    printf("y - set big_encryption[%d] to 0x03\n", source_id);
    printf("o - add source\n");
    printf("x - ???\n");

    printf("\n## ASCS\n");
    printf("e - set Codec Config, SNK 1\n");
    printf("E - set Codec Config, SRC 3\n");
    printf("f - disable SNK 1\n");
    printf("F - disable SRC 3\n");
    printf("R - release SNK 1\n");
    printf("S - release SRC 3\n");
    printf("M - released SNK 1\n");
    printf("N - released SRC 3\n");
    printf("t - set ADV data to Targeted Announcement\n");

    printf("\n## CSIS\n");
    printf("g - set OOB Sirk only\n");
    printf("G - set Encrypted Sirk\n");
    printf("h - Simulate server locked by another remote coordinator\n");
    printf("H - Simulate server unlock by another remote coordinator\n");
    printf("i - generate RSI for plaintext SIRK and start advertising with public address\n");
    printf("I - generate RSI for encrypted SIRK and start advertising with public address\n");
    printf("j - generate RSI for plaintext SIRK and start advertising with random address\n");
}

static void stdin_process(char cmd){
    printf("%c\n", cmd);
    uint8_t status = ERROR_CODE_SUCCESS;

    switch (cmd){
        case 'a':
            printf("Trigger Sink PAC record notification\n");
            sink_pac_records[0].codec_capability.sampling_frequency_mask |= LE_AUDIO_CODEC_SAMPLING_FREQUENCY_MASK_48000_HZ;
            published_audio_capabilities_service_server_sink_pac_modified();
            break;
        case 'A':
            printf("Trigger Source PAC record notification\n");
            source_pac_records[0].codec_capability.sampling_frequency_mask |= LE_AUDIO_CODEC_SAMPLING_FREQUENCY_MASK_48000_HZ;
            published_audio_capabilities_service_server_source_pac_modified();
            break;


        case 'b':
            printf("Set Sink audio locations\n");
            published_audio_capabilities_service_server_set_sink_audio_locations(LE_AUDIO_LOCATION_MASK_BOTTOM_FRONT_LEFT | LE_AUDIO_LOCATION_MASK_BOTTOM_FRONT_RIGHT);
            break;
        case 'B':
            printf("Set Source audio locations\n");
            published_audio_capabilities_service_server_set_source_audio_locations(LE_AUDIO_LOCATION_MASK_BOTTOM_FRONT_LEFT | LE_AUDIO_LOCATION_MASK_BOTTOM_FRONT_RIGHT);
            break;

        case 'c':
            printf("Set available audio contexts\n");
            published_audio_capabilities_service_server_set_available_audio_contexts(LE_AUDIO_CONTEXT_MASK_MEDIA, LE_AUDIO_CONTEXT_MASK_GAME);
            break;
        case 'd':
            printf("Set supported audio contexts\n");
            published_audio_capabilities_service_server_set_supported_audio_contexts(LE_AUDIO_CONTEXT_MASK_UNSPECIFIED|LE_AUDIO_CONTEXT_MASK_MEDIA, LE_AUDIO_CONTEXT_MASK_GAME);
            break;

        case '0':
            printf("set pa_sync_state[%d] to LE_AUDIO_PA_SYNC_STATE_NOT_SYNCHRONIZED_TO_PA\n", source_id);
            broadcast_audio_scan_service_server_set_pa_sync_state(source_id, LE_AUDIO_PA_SYNC_STATE_NOT_SYNCHRONIZED_TO_PA);
            break;

        case '1':
            printf("set pa_sync_state[%d] to LE_AUDIO_PA_SYNC_STATE_SYNCINFO_REQUEST\n", source_id);
            broadcast_audio_scan_service_server_set_pa_sync_state(source_id, LE_AUDIO_PA_SYNC_STATE_SYNCINFO_REQUEST);
            break;
        
        case '2':
            printf("set pa_sync_state[%d] to LE_AUDIO_PA_SYNC_STATE_SYNCHRONIZED_TO_PA\n", source_id);
            broadcast_audio_scan_service_server_set_pa_sync_state(source_id, LE_AUDIO_PA_SYNC_STATE_SYNCHRONIZED_TO_PA);
            break;

        case '3':
            printf("set pa_sync_state[%d] to LE_AUDIO_PA_SYNC_STATE_FAILED_TO_SYNCHRONIZE_TO_PA\n", source_id);
            broadcast_audio_scan_service_server_set_pa_sync_state(source_id, LE_AUDIO_PA_SYNC_STATE_FAILED_TO_SYNCHRONIZE_TO_PA);
            break;
        
        case '4':
            printf("set pa_sync_state[%d] to LE_AUDIO_PA_SYNC_STATE_NO_PAST\n", source_id);
            broadcast_audio_scan_service_server_set_pa_sync_state(source_id, LE_AUDIO_PA_SYNC_STATE_NO_PAST);
            break;
        
        case 'r':
            printf("Set bis_sync[%d] to 0x00000000\n", source_id);
            bass_sources[0].data.subgroups[0].bis_sync_state = 0;
            break;
        
        case 's':
            printf("Set bis_sync[%d] to 0x00000001\n", source_id);
            bass_sources[0].data.subgroups[0].bis_sync_state = 0x00000001;
            break;

        case 'm':
            printf("Set big_encryption[%d] to 0x%02x\n", source_id, LE_AUDIO_BIG_ENCRYPTION_BROADCAST_CODE_REQUIRED);
            bass_sources[0].big_encryption = LE_AUDIO_BIG_ENCRYPTION_BROADCAST_CODE_REQUIRED;
            break;

        case 'n':
            printf("Set big_encryption[%d] to 0x%02x\n", source_id, LE_AUDIO_BIG_ENCRYPTION_DECRYPTING);
            bass_sources[0].big_encryption = LE_AUDIO_BIG_ENCRYPTION_DECRYPTING;
            break;

        case 'y':
            printf("Set big_encryption[%d] to 0x%02x\n", source_id, LE_AUDIO_BIG_ENCRYPTION_BAD_CODE);
            bass_sources[0].big_encryption = LE_AUDIO_BIG_ENCRYPTION_BAD_CODE;
            memcpy(bass_sources[0].bad_code, bad_code, sizeof(bad_code));
            bass_sources[0].data.subgroups[0].bis_sync_state = 0x00000000;
            break;

        case 'o':
            broadcast_audio_scan_service_server_add_source(&source_data1, &source_id);
            break;

        case 'x':
            broadcast_audio_scan_service_server_add_source(&source_data1, &source_id);
            bass_sources[0].big_encryption = LE_AUDIO_BIG_ENCRYPTION_BROADCAST_CODE_REQUIRED;
            break;

        case 'e':
            printf("Set Codec Config SNK, 0x%02X\n", bap_app_server_con_handle);
            audio_stream_control_service_server_streamendpoint_configure_codec(bap_app_server_con_handle, 1, &ascs_codec_configuration);
            break;
        case 'E':
            printf("Set Codec Config SRC, 0x%02X\n", bap_app_server_con_handle);
            audio_stream_control_service_server_streamendpoint_configure_codec(bap_app_server_con_handle, 3, &ascs_codec_configuration);
            break;
        case 'f':
            printf("Disable SNK 1, 0x%02X\n", bap_app_server_con_handle);
            audio_stream_control_service_server_streamendpoint_disable(bap_app_server_con_handle, 1);
            break;
        case 'F':
            printf("Disable SRC 3, 0x%02X\n", bap_app_server_con_handle);
            audio_stream_control_service_server_streamendpoint_disable(bap_app_server_con_handle, 3);
            break;
        case 'R':
            printf("Release SNK 1, 0x%02X\n", bap_app_server_con_handle);
            audio_stream_control_service_server_streamendpoint_release(bap_app_server_con_handle, 1);
            break;
        case 'S':
            printf("Release SRC 3, 0x%02X\n", bap_app_server_con_handle);
            audio_stream_control_service_server_streamendpoint_release(bap_app_server_con_handle, 3);
            break;
        case 'M':
            printf("Released SNK 1, 0x%02X\n", bap_app_server_con_handle);
            audio_stream_control_service_server_streamendpoint_released(bap_app_server_con_handle, 1, false);
            break;
        case 'N':
            printf("Released SRC 3, 0x%02X\n", bap_app_server_con_handle);
            audio_stream_control_service_server_streamendpoint_released(bap_app_server_con_handle, 3, false);
            break;
        case 't':
            printf("Set Announcement type to 1\n");
            adv_data[15] = 1;
            gap_extended_advertising_set_adv_data(adv_handle, sizeof(adv_data), adv_data);
            break;
        case 'g':
            printf("Set OOB Sirk only\n");
            coordinated_set_identification_service_server_set_sirk(CSIS_SIRK_TYPE_PUBLIC, &sirk[0], true);
            break;
        case 'G':
            printf("Set Encrypted Sirk\n");
            coordinated_set_identification_service_server_set_sirk(CSIS_SIRK_TYPE_ENCRYPTED, &sirk[0], false);
            break;
        case 'h':{
            printf("Simulate server locked by another remote member\n");
            hci_con_handle_t con_handle = 0x00AA;
            coordinated_set_identification_service_server_simulate_member_connected(con_handle);
            status = coordinated_set_identification_service_server_simulate_set_lock(con_handle, CSIS_MEMBER_LOCKED);
            if (status == ERROR_CODE_SUCCESS){
                printf("Server locked by member with con_handle 0x%02x\n", con_handle);
                break;
            }        
            printf("Server lock failed, status 0x%02x\n", status);
            break;
        }
        case 'H':{
            printf("Simulate server unlock by another remote member\n");
            hci_con_handle_t con_handle = 0x00AA;
            status = coordinated_set_identification_service_server_simulate_set_lock(con_handle, CSIS_MEMBER_UNLOCKED);
            if (status == ERROR_CODE_SUCCESS){
                printf("Server unlocked by member with con_handle 0x%02x\n", con_handle);
                break;
            }        
        
            printf("Server unlock failed, status 0x%02x\n", status);
            break;
        }
        
        case 'i':
            coordinated_set_identification_service_server_set_sirk(CSIS_SIRK_TYPE_PUBLIC, sirk, false);
            coordinated_set_identification_service_server_generate_rsi();
            break;

        case 'I':
            coordinated_set_identification_service_server_set_sirk(CSIS_SIRK_TYPE_ENCRYPTED, sirk, false);
            coordinated_set_identification_service_server_generate_rsi();
            break;

        case 'j':
            gap_random_address_set_mode(GAP_RANDOM_ADDRESS_RESOLVABLE);
            coordinated_set_identification_service_server_set_sirk(CSIS_SIRK_TYPE_PUBLIC, sirk, false);
            coordinated_set_identification_service_server_generate_rsi();
            break;

        case '\n':
        case '\r':
            break;
        default:
            show_usage();
            break;
    }

    if (status != ERROR_CODE_SUCCESS){
        printf("Command '%c' could not be performed, status 0x%02x\n", cmd, status);
    }
}

int btstack_main(void);
int btstack_main(void)
{
    
    l2cap_init();

    // setup le device db
    le_device_db_init();

    // setup SM: Display only
    sm_init();
    // register for SM events
    sm_event_callback_registration.callback = &packet_handler;
    sm_add_event_handler(&sm_event_callback_registration);
    sm_set_authentication_requirements(SM_AUTHREQ_BONDING);
    sm_allow_ltk_reconstruction_without_le_device_db_entry(0);
    att_server_init(profile_data, att_read_callback, att_write_callback);

    // setup PACS
    sink_node.records = &sink_pac_records[0];
    sink_node.records_num = 1;
    sink_node.audio_locations_mask = LE_AUDIO_LOCATION_MASK_FRONT_RIGHT;

    source_node.records = &source_pac_records[0];
    source_node.records_num = 1;
    source_node.audio_locations_mask = LE_AUDIO_LOCATION_MASK_FRONT_RIGHT;

    published_audio_capabilities_service_server_init(&sink_node, &source_node);
    published_audio_capabilities_service_server_register_packet_handler(&pacs_server_packet_handler);

    // setup BASS
    broadcast_audio_scan_service_server_init(BASS_NUM_SOURCES, bass_sources, BASS_NUM_CLIENTS, bass_clients);
    broadcast_audio_scan_service_server_register_packet_handler(&bass_server_packet_handler);

    // setup ASCS
    audio_stream_control_service_server_init(ASCS_NUM_STREAMENDPOINT_CHARACTERISTICS, ascs_streamendpoint_characteristics, ASCS_NUM_CLIENTS, ascs_clients);
    audio_stream_control_service_server_register_packet_handler(&ascs_server_packet_handler);

    // setup CSIS
    coordinated_set_identification_service_server_init(CSIS_COORDINATORS_MAX_NUM, &csis_coordinators[0], CSIS_COORDINATORS_MAX_NUM, 1);
    coordinated_set_identification_service_server_register_packet_handler(&csis_server_packet_handler);
    csis_privacy_client.callback = csis_privacy_handler;
    gap_privacy_client_register(&csis_privacy_client);

    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // setup advertisements
    gap_periodic_advertising_sync_transfer_set_default_parameters(2, 0, 0x2000, 0);

    btstack_stdin_setup(stdin_process);

    // turn on!
	hci_power_control(HCI_POWER_ON);
	    
    return 0;
}