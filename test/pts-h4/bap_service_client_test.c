/*
 * Copyright (C) 2018 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "bap_service_client_test.c"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <btstack_lc3.h>

#include "btstack.h"
#include "btstack_config.h"

#define BASS_CLIENT_NUM_SOURCES 3
#define ASCS_CLIENT_NUM_STREAMENDPOINTS 4
#define MAX_CHANNELS 2

static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_packet_callback_registration_t sm_event_callback_registration;

static const char * bap_app_server_addr_string = "C007E8414591";

static bd_addr_t        bap_app_server_addr;
static hci_con_handle_t bap_app_client_con_handle;

// BASS
static bass_client_connection_t bass_connection;
static bass_client_source_t bass_sources[BASS_CLIENT_NUM_SOURCES];
static uint16_t bass_cid;

// PACS
static pacs_client_connection_t pacs_connection;
static uint16_t pacs_cid;

// ASCS
static ascs_client_connection_t ascs_connection;
static ascs_streamendpoint_characteristic_t streamendpoint_characteristics[ASCS_CLIENT_NUM_STREAMENDPOINTS];
static uint16_t ascs_cid;
static uint8_t  ase_index = 0;

// remote info
static char remote_name[20];
static bd_addr_t remote;
static bd_addr_type_t remote_type;
static uint8_t remote_sid;
static bool count_mode;
static bool pts_mode;

static char operation_cmd = 0;

// CIG/CIS
static le_audio_cig_t        cig;
static le_audio_cig_params_t cig_params;
static btstack_lc3_frame_duration_t cig_frame_duration;
static uint8_t  cig_num_cis;
static uint8_t  cis_num_channels;
static uint16_t cis_sampling_frequency_hz;
static uint16_t cis_octets_per_frame;
static hci_con_handle_t cis_con_handles[MAX_CHANNELS];
static bool cis_established[MAX_CHANNELS];
static unsigned int     cis_setup_next_index;

typedef enum {
    BAP_APP_CLIENT_STATE_IDLE = 0,
    BAP_APP_CLIENT_STATE_W4_GAP_CONNECTED,
    BAP_APP_CLIENT_STATE_W4_BASS_CONNECTED,
    BAP_APP_CLIENT_STATE_W4_PACS_CONNECTED,
    BAP_APP_CLIENT_STATE_CONNECTED,
    BAP_APP_W4_CIG_COMPLETE,
    BAP_APP_W4_CIS_CREATED,
    BAP_APP_STREAMING,
} bap_app_client_state_t; 

static bap_app_client_state_t bap_app_client_state = BAP_APP_CLIENT_STATE_IDLE;
static ascs_qos_configuration_t * bap_app_qos_configuration;

static bass_source_data_t source_data1 = {
    // address_type, address
     
    BD_ADDR_TYPE_LE_PUBLIC, {0xC0, 0x07, 0xE8, 0x7E, 0x55, 0x5F}, 
    // adv_sid
    0x01, 
    // broadcast_id
    0x000001,
    // pa_sync
    LE_AUDIO_PA_SYNC_SYNCHRONIZE_TO_PA_PAST_AVAILABLE,
    // pa_sync_state
    LE_AUDIO_PA_SYNC_STATE_NOT_SYNCHRONIZED_TO_PA,
    // pa_interval UNKNOWN
    0xFFFF,
    // subgroups_num
    1,
    {
        {
            // BIS Sync
            0xFFFFFFFF,
            // BIS sync state
            0x00000000,
            // Metadata Length
            45 - 15, 
            // Metadata
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

static ascs_client_codec_configuration_request_t codec_configuration_request_8kHz_7500us = {
    LE_AUDIO_CLIENT_TARGET_LATENCY_LOW_LATENCY,
    LE_AUDIO_CLIENT_TARGET_PHY_BALANCED,
    HCI_AUDIO_CODING_FORMAT_LC3,
    0, 0, 
    {
        // codec configuration mask
        0x3E, 
        // le_audio_codec_sampling_frequency_index_t
        LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_8000_HZ,
        // le_audio_codec_frame_duration
        LE_AUDIO_CODEC_FRAME_DURATION_MASK_7500US, 
        // audio_channel_allocation_mask (4)
        LE_AUDIO_LOCATION_MASK_FRONT_LEFT, 
        // octets_per_codec_frame (2)
        26, 
        // codec_frame_blocks_per_sdu (1)
        1
    }
};

static ascs_client_codec_configuration_request_t codec_configuration_request_16kHz_100000us = {
    LE_AUDIO_CLIENT_TARGET_LATENCY_LOW_LATENCY,
    LE_AUDIO_CLIENT_TARGET_PHY_BALANCED,
    HCI_AUDIO_CODING_FORMAT_LC3,
    0, 0, 
    {
        // codec configuration mask
        0x3E, 
        // le_audio_codec_sampling_frequency_index_t
        LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_16000_HZ,
        // le_audio_codec_frame_duration
        LE_AUDIO_CODEC_FRAME_DURATION_MASK_10000US, 
        // audio_channel_allocation_mask (4)
        LE_AUDIO_LOCATION_MASK_FRONT_LEFT, 
        // octets_per_codec_frame (2)
        40, 
        // codec_frame_blocks_per_sdu (1)
        1
    }
};

static ascs_qos_configuration_t qos_configuration_request_7500us = {
    // cig_id
    1,
    // cis_id
    1,
    // sdu_interval
    7500,
    // framing (unframed)
    0,
    // phy
    LE_AUDIO_SERVER_PHY_MASK_NO_PREFERENCE,
    // max_sdu
    255,
    // retransmission_number
    2,
    // max_transport_latency_ms
    20,
    // presentation_delay_us
    40000
};

static ascs_qos_configuration_t qos_configuration_request_10000us = {
    // cig_id
    1,
    // cis_id
    1,
    // sdu_interval
    10000,
    // framing (unframed)
    0,
    // phy
    LE_AUDIO_SERVER_PHY_MASK_NO_PREFERENCE,
    // max_sdu
    255,
    // retransmission_number
    2,
    // max_transport_latency_ms
    20,
    // presentation_delay_us
    40000
};

static le_audio_metadata_t metadata_update_request = {
    (1 << LE_AUDIO_METADATA_TYPE_PREFERRED_AUDIO_CONTEXTS) & (1 << LE_AUDIO_METADATA_TYPE_STREAMING_AUDIO_CONTEXTS),
    LE_AUDIO_CONTEXT_MASK_MEDIA,
    LE_AUDIO_CONTEXT_MASK_MEDIA,
    0, {},
    0,
    0, {},
    0, 
    0, {},
    0,0,{},
    0,0,{}
};
            
static char * bass_opcode_str[] = {
    "REMOTE_SCAN_STOPPED",
    "REMOTE_SCAN_STARTED",
    "ADD_SOURCE",
    "MODIFY_SOURCE",
    "SET_BROADCAST_CODE",
    "REMOVE_SOURCE", 
    "RFU"
};

static char * bass_opcode2str(uint8_t opcode){
    if (opcode < BASS_OPCODE_RFU){
        return bass_opcode_str[opcode];
    }
    return bass_opcode_str[(uint8_t)BASS_OPCODE_RFU];
}

static void hci_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    bd_addr_t addr;
    uint8_t status;
    uint8_t event = hci_event_packet_get_type(packet);
    uint8_t i;
    hci_con_handle_t  cis_handle;
    switch (event) {
        case SM_EVENT_JUST_WORKS_REQUEST:
            printf("Just Works requested\n");
            sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
            break;
        case SM_EVENT_NUMERIC_COMPARISON_REQUEST:
            printf("Confirming numeric comparison: %"PRIu32"\n", sm_event_numeric_comparison_request_get_passkey(packet));
            sm_numeric_comparison_confirm(sm_event_passkey_display_number_get_handle(packet));
            break;
        case SM_EVENT_PASSKEY_DISPLAY_NUMBER:
            printf("Display Passkey: %"PRIu32"\n", sm_event_passkey_display_number_get_passkey(packet));
            break;
        case SM_EVENT_IDENTITY_CREATED:
            sm_event_identity_created_get_identity_address(packet, addr);
            printf("Identity created: type %u address %s\n", sm_event_identity_created_get_identity_addr_type(packet), bd_addr_to_str(addr));
            break;
        case SM_EVENT_IDENTITY_RESOLVING_SUCCEEDED:
            sm_event_identity_resolving_succeeded_get_identity_address(packet, addr);
            printf("Identity resolved: type %u address %s\n", sm_event_identity_resolving_succeeded_get_identity_addr_type(packet), bd_addr_to_str(addr));
            break;
        case SM_EVENT_IDENTITY_RESOLVING_FAILED:
            sm_event_identity_created_get_address(packet, addr);
            printf("Identity resolving failed\n");
            break;
        case SM_EVENT_PAIRING_COMPLETE:
            switch (sm_event_pairing_complete_get_status(packet)){
                case ERROR_CODE_SUCCESS:
                    printf("Pairing complete, success\n");
                    break;
                case ERROR_CODE_CONNECTION_TIMEOUT:
                    printf("Pairing failed, timeout\n");
                    break;
                case ERROR_CODE_REMOTE_USER_TERMINATED_CONNECTION:
                    printf("Pairing faileed, disconnected\n");
                    break;
                case ERROR_CODE_AUTHENTICATION_FAILURE:
                    printf("Pairing failed, reason = %u\n", sm_event_pairing_complete_get_reason(packet));
                    break;
                default:
                    break;
            }
            break;

        case GAP_EVENT_EXTENDED_ADVERTISING_REPORT:
        {
            gap_event_extended_advertising_report_get_address(packet, source_data1.address);
            source_data1.address_type = gap_event_extended_advertising_report_get_address_type(packet);
            source_data1.adv_sid = gap_event_extended_advertising_report_get_advertising_sid(packet);

            uint8_t adv_size = gap_event_extended_advertising_report_get_data_length(packet);
            const uint8_t * adv_data = gap_event_extended_advertising_report_get_data(packet);

            ad_context_t context;
            bool found = false;
            remote_name[0] = '\0';
            uint16_t uuid;
            for (ad_iterator_init(&context, adv_size, adv_data) ; ad_iterator_has_more(&context) ; ad_iterator_next(&context)) {
                uint8_t data_type = ad_iterator_get_data_type(&context);
                uint8_t size = ad_iterator_get_data_len(&context);
                const uint8_t *data = ad_iterator_get_data(&context);
                switch (data_type){
                    case BLUETOOTH_DATA_TYPE_SERVICE_DATA_16_BIT_UUID:
                        uuid = little_endian_read_16(data, 0);
                        if (uuid == ORG_BLUETOOTH_SERVICE_BROADCAST_AUDIO_ANNOUNCEMENT_SERVICE){
                            found = true;
                        }
                        break;
                    default:
                        break;
                }
            }
            if (!found) break;
            remote_type = gap_event_extended_advertising_report_get_address_type(packet);
            remote_sid = gap_event_extended_advertising_report_get_advertising_sid(packet);
            pts_mode = strncmp("PTS-", remote_name, 4) == 0;
            count_mode = strncmp("COUNT", remote_name, 5) == 0;
            printf("Remote Broadcast sink found, addr %s, name: '%s' (pts-mode: %u, count: %u)\n", bd_addr_to_str(remote), remote_name, pts_mode, count_mode);
            gap_stop_scan();
            break;
        }

        case HCI_EVENT_LE_META:
            switch (hci_event_le_meta_get_subevent_code(packet)){
                case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
                    bap_app_client_con_handle = hci_subevent_le_connection_complete_get_connection_handle(packet);
                    bap_app_client_state      = BAP_APP_CLIENT_STATE_CONNECTED;
                    hci_subevent_le_connection_complete_get_peer_address(packet, bap_app_server_addr);
                    printf("BAP  Client: connection to %s established, con_handle 0x%02x\n", bd_addr_to_str(bap_app_server_addr), bap_app_client_con_handle);
                    break;
                default:
                    return;
            }
            break;

        case HCI_EVENT_DISCONNECTION_COMPLETE:
            bap_app_client_con_handle = HCI_CON_HANDLE_INVALID;
            bap_app_client_state      = BAP_APP_CLIENT_STATE_IDLE;
            printf("BAP  Client: disconnected from %s\n", bd_addr_to_str(bap_app_server_addr));
            break;

        case HCI_EVENT_META_GAP:
            switch (hci_event_gap_meta_get_subevent_code(packet)){
                case GAP_SUBEVENT_CIG_CREATED:
                    if (bap_app_client_state == BAP_APP_W4_CIG_COMPLETE){
                        printf("CIS Connection Handles: ");
                        for (i=0; i < cig_num_cis; i++){
                            cis_con_handles[i] = gap_subevent_cig_created_get_cis_con_handles(packet, i);
                            printf("0x%04x ", cis_con_handles[i]);
                        }
                        printf("\n");
                        printf("ASCS Client: Configure QoS %u us, ASE index %d\n", cig_params.sdu_interval_p_to_c, ase_index);
                        audio_stream_control_service_client_streamendpoint_configure_qos(ascs_cid, ase_index, bap_app_qos_configuration);
                    }
                    break;
                case GAP_SUBEVENT_CIS_CREATED:
                    // only look for cis handle
                    cis_handle = gap_subevent_cis_created_get_cis_con_handle(packet);
                    for (i=0; i < cig_num_cis; i++){
                        if (cis_handle == cis_con_handles[i]){
                            cis_established[i] = true;
                        }
                    }
                    // check for complete
                    bool complete = true;
                    for (i=0; i < cig_num_cis; i++) {
                        complete &= cis_established[i];
                    }
                    if (complete) {
                        printf("All CIS Established\n");
                        cis_setup_next_index = 0;
                        bap_app_client_state = BAP_APP_STREAMING;
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

static void bap_service_client_setup_cig(void){
    uint8_t framed_pdus;
    uint16_t frame_duration_us;
    if (cis_sampling_frequency_hz == 44100){
        framed_pdus = 1;
        // same config as for 48k -> frame is longer by 48/44.1
        frame_duration_us = cig_frame_duration == BTSTACK_LC3_FRAME_DURATION_7500US ? 8163 : 10884;
    } else {
        framed_pdus = 0;
        frame_duration_us = cig_frame_duration == BTSTACK_LC3_FRAME_DURATION_7500US ? 7500 : 10000;
    }

    printf("Create CIG\n");

    uint8_t num_cis = 1;
    cig_params.cig_id = 1;
    cig_params.num_cis = 1;
    cig_params.sdu_interval_c_to_p = frame_duration_us;
    cig_params.sdu_interval_p_to_c = frame_duration_us;
    cig_params.worst_case_sca = 0; // 251 ppm to 500 ppm
    cig_params.packing = 0;        // sequential
    cig_params.framing = framed_pdus;
    cig_params.max_transport_latency_c_to_p = 40;
    cig_params.max_transport_latency_p_to_c = 40;
    uint8_t i;
    for (i=0; i < num_cis; i++){
        cig_params.cis_params[i].cis_id = 1+i;
        cig_params.cis_params[i].max_sdu_c_to_p = cis_num_channels * cis_octets_per_frame;
        cig_params.cis_params[i].max_sdu_p_to_c = 0;
        cig_params.cis_params[i].phy_c_to_p = 2;  // 2M
        cig_params.cis_params[i].phy_p_to_c = 2;  // 2M
        cig_params.cis_params[i].rtn_c_to_p = 2;
        cig_params.cis_params[i].rtn_p_to_c = 2;
    }

    bap_app_client_state = BAP_APP_W4_CIG_COMPLETE;

    gap_cig_create(&cig, &cig_params);
}

void bap_service_client_setup_cis(void){
    uint8_t i;
    cis_setup_next_index = 0;
    printf("Create CIS\n");
    hci_con_handle_t acl_connection_handles[MAX_CHANNELS];
    for (i=0; i < cig_num_cis; i++){
        acl_connection_handles[i] = bap_app_client_con_handle;
    }
    gap_cis_create(cig_params.cig_id, cis_con_handles, acl_connection_handles);
    bap_app_client_state = BAP_APP_W4_CIS_CREATED;
}

static void bass_client_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_GATTSERVICE_META) return;

    switch (hci_event_gattservice_meta_get_subevent_code(packet)){
        case GATTSERVICE_SUBEVENT_BASS_CONNECTED:
            if (bap_app_client_con_handle != gattservice_subevent_bass_connected_get_con_handle(packet)){
                printf("BASS Client: expected con handle 0x%02x, received 0x%02x\n", bap_app_client_con_handle, gattservice_subevent_bass_connected_get_con_handle(packet));
                return;
            }

            bap_app_client_state = BAP_APP_CLIENT_STATE_CONNECTED;

            if (gattservice_subevent_bass_connected_get_status(packet) != ERROR_CODE_SUCCESS){
                printf("BASS client: connection failed, cid 0x%02x, con_handle 0x%02x, status 0x%02x\n", bass_cid, bap_app_client_con_handle, 
                    gattservice_subevent_bass_connected_get_status(packet));
                return;
            }

            printf("BASS client: connected, cid 0x%02x\n", bass_cid);
            break;

        case GATTSERVICE_SUBEVENT_BASS_DISCONNECTED:
            bass_cid = 0;
            printf("BASS Client: disconnected\n");
            break;


        case GATTSERVICE_SUBEVENT_BASS_SCAN_OPERATION_COMPLETE:
            if (bass_cid != gattservice_subevent_bass_scan_operation_complete_get_bass_cid(packet)){
                return;
            }
            if (gattservice_subevent_bass_scan_operation_complete_get_status(packet) != ERROR_CODE_SUCCESS){
                printf("BASS Client: scan operation failed, status 0x%02x\n", gattservice_subevent_bass_scan_operation_complete_get_status(packet));
                return;
            }
            printf("BASS Client: scan operation %s completed\n", bass_opcode2str(gattservice_subevent_bass_scan_operation_complete_get_opcode(packet)));
            break;

        case GATTSERVICE_SUBEVENT_BASS_SOURCE_OPERATION_COMPLETE:
            if (bass_cid != gattservice_subevent_bass_source_operation_complete_get_bass_cid(packet)){
                return;
            }

            if (gattservice_subevent_bass_source_operation_complete_get_status(packet) != ERROR_CODE_SUCCESS){
                printf("BASS Client: source operation failed, status 0x%02x\n", gattservice_subevent_bass_source_operation_complete_get_status(packet));
                return;
            }

            if ( gattservice_subevent_bass_source_operation_complete_get_opcode(packet) == (uint8_t)BASS_OPCODE_ADD_SOURCE ){
                // omit source ID, it will be received via notification
                printf("BASS Client: source operation %s completed\n", 
                    bass_opcode2str(gattservice_subevent_bass_source_operation_complete_get_opcode(packet)));
            
            } else {
                printf("BASS Client: source[%d] operation %s completed\n", 
                    gattservice_subevent_bass_source_operation_complete_get_source_id(packet),
                    bass_opcode2str(gattservice_subevent_bass_source_operation_complete_get_opcode(packet)));
            }
            break;

        case GATTSERVICE_SUBEVENT_BASS_NOTIFY_RECEIVE_STATE_BASE:
            printf("BASS Client: GATTSERVICE_SUBEVENT_BASS_NOTIFY_RECEIVE_STATE_BASE\n");
            break;
        case GATTSERVICE_SUBEVENT_BASS_NOTIFY_RECEIVE_STATE_SUBGROUP:
            printf("BASS Client: GATTSERVICE_SUBEVENT_BASS_NOTIFY_RECEIVE_STATE_SUBGROUP\n");
            break;
        case GATTSERVICE_SUBEVENT_BASS_NOTIFICATION_COMPLETE:
            printf("BASS Client: GATTSERVICE_SUBEVENT_BASS_NOTIFICATION_COMPLETE\n");
            break;
        
        default:
            btstack_assert(false);
            break;
    }
}

static void pacs_client_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_GATTSERVICE_META) return;

    switch (hci_event_gattservice_meta_get_subevent_code(packet)){
        case GATTSERVICE_SUBEVENT_PACS_CONNECTED:
            if (bap_app_client_con_handle != gattservice_subevent_pacs_connected_get_con_handle(packet)){
                printf("PACS Client: expected con handle 0x%02x, received 0x%02x\n", bap_app_client_con_handle, gattservice_subevent_pacs_connected_get_con_handle(packet));
                return;
            }

            bap_app_client_state = BAP_APP_CLIENT_STATE_CONNECTED;

            if (gattservice_subevent_pacs_connected_get_status(packet) != ERROR_CODE_SUCCESS){
                printf("PACS client: connection failed, cid 0x%02x, con_handle 0x%02x, status 0x%02x\n", pacs_cid, bap_app_client_con_handle, 
                    gattservice_subevent_pacs_connected_get_status(packet));
                return;
            }

            printf("PACS client: connected, cid 0x%02x\n", pacs_cid);
            break;

        case GATTSERVICE_SUBEVENT_PACS_DISCONNECTED:
            pacs_cid = 0;
            printf("PACS Client: disconnected\n");
            break;

        case GATTSERVICE_SUBEVENT_PACS_OPERATION_DONE:
            if (gattservice_subevent_pacs_operation_done_get_status(packet) == ERROR_CODE_SUCCESS){
                printf("      Operation successful\n");
            } else {
                printf("      Operation failed with status 0x%02X\n", gattservice_subevent_pacs_operation_done_get_status(packet));
            }
            break;

        case GATTSERVICE_SUBEVENT_PACS_AUDIO_LOCATIONS:
            printf("PACS Client: %s Audio Locations 0x%04X \n", 
                gattservice_subevent_pacs_audio_locations_get_le_audio_role(packet) == LE_AUDIO_ROLE_SINK ? "Sink" : "Source",
                gattservice_subevent_pacs_audio_locations_get_audio_location_mask(packet));
            break;

        case GATTSERVICE_SUBEVENT_PACS_AVAILABLE_AUDIO_CONTEXTS:
            printf("PACS Client: Available Audio Contexts\n");
            printf("      Sink   0x%02X\n", gattservice_subevent_pacs_available_audio_contexts_get_sink_mask(packet));
            printf("      Source 0x%02X\n", gattservice_subevent_pacs_available_audio_contexts_get_source_mask(packet));
            break;
    
        case GATTSERVICE_SUBEVENT_PACS_SUPPORTED_AUDIO_CONTEXTS:
            printf("PACS Client: Supported Audio Contexts\n");
            printf("      Sink   0x%02X\n", gattservice_subevent_pacs_supported_audio_contexts_get_sink_mask(packet));
            printf("      Source 0x%02X\n", gattservice_subevent_pacs_supported_audio_contexts_get_source_mask(packet));
            break;
            
        case GATTSERVICE_SUBEVENT_PACS_PACK_RECORD:
            printf("PACS Client: %s PAC Record\n", gattservice_subevent_pacs_pack_record_get_le_audio_role(packet) == LE_AUDIO_ROLE_SINK ? "Sink" : "Source");
            printf("      %s PAC Record DONE\n", gattservice_subevent_pacs_pack_record_done_get_le_audio_role(packet) == LE_AUDIO_ROLE_SINK ? "Sink" : "Source");
            break;
        case GATTSERVICE_SUBEVENT_PACS_PACK_RECORD_DONE:
            printf("      %s PAC Record DONE\n", gattservice_subevent_pacs_pack_record_done_get_le_audio_role(packet) == LE_AUDIO_ROLE_SINK ? "Sink" : "Source");
            break;

        default:
            break;
    }
}

static void ascs_client_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
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
        case GATTSERVICE_SUBEVENT_ASCS_CONNECTED:
            if (bap_app_client_con_handle != gattservice_subevent_ascs_connected_get_con_handle(packet)){
                printf("ASCS Client: expected con handle 0x%02x, received 0x%02x\n", bap_app_client_con_handle, gattservice_subevent_ascs_connected_get_con_handle(packet));
                return;
            }
            
            if (ascs_cid != gattservice_subevent_ascs_connected_get_ascs_cid(packet)){
                return;
            }

            bap_app_client_state = BAP_APP_CLIENT_STATE_CONNECTED;
            if (gattservice_subevent_ascs_connected_get_status(packet) != ERROR_CODE_SUCCESS){
                ascs_cid = 0;
                printf("ASCS Client: connection failed, cid 0x%02x, con_handle 0x%02x, status 0x%02x\n", ascs_cid, bap_app_client_con_handle, 
                    gattservice_subevent_ascs_connected_get_status(packet));
                return;
            }
            printf("ASCS Client: connected, cid 0x%02x\n", ascs_cid);
            break;

        case GATTSERVICE_SUBEVENT_ASCS_DISCONNECTED:
            if (ascs_cid != gattservice_subevent_ascs_disconnected_get_ascs_cid(packet)){
                return;
            }
            ascs_cid = 0;
            printf("ASCS Client: disconnected, cid 0x%02x\n", gattservice_subevent_ascs_disconnected_get_ascs_cid(packet));
            break;


        case GATTSERVICE_SUBEVENT_ASCS_CODEC_CONFIGURATION:
            ase_id     = gattservice_subevent_ascs_codec_configuration_get_ase_id(packet);
            con_handle = gattservice_subevent_ascs_codec_configuration_get_con_handle(packet);
            ase_state  =  (ascs_state_t)gattservice_subevent_ascs_codec_configuration_get_state(packet);

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

            printf("ASCS Client: ASE NOTIFICATION (%s) - ase_id %d, con_handle 0x%02x\n", ascs_util_ase_state2str(ase_state), ase_id, con_handle);
            break;

        case GATTSERVICE_SUBEVENT_ASCS_QOS_CONFIGURATION:
            ase_id     = gattservice_subevent_ascs_qos_configuration_get_ase_id(packet);
            con_handle = gattservice_subevent_ascs_qos_configuration_get_con_handle(packet);
            ase_state = (ascs_state_t)gattservice_subevent_ascs_qos_configuration_get_state(packet);

            printf("ASCS Client: ASE NOTIFICATION (%s) - ase_id %d, con_handle 0x%02x\n", ascs_util_ase_state2str(ase_state), ase_id, con_handle);
            break;

        case GATTSERVICE_SUBEVENT_ASCS_METADATA:
            ase_id     = gattservice_subevent_ascs_metadata_get_ase_id(packet);
            con_handle = gattservice_subevent_ascs_metadata_get_con_handle(packet);
            ase_state  = (ascs_state_t)gattservice_subevent_ascs_metadata_get_state(packet);

            printf("ASCS Client: ASE NOTIFICATION (%s) - ase_id %d, con_handle 0x%02x\n", ascs_util_ase_state2str(ase_state), ase_id, con_handle);
            break;

        case GATTSERVICE_SUBEVENT_ASCS_CONTROL_POINT_OPERATION:
            ase_id        = gattservice_subevent_ascs_control_point_operation_get_ase_id(packet);
            con_handle    = gattservice_subevent_ascs_control_point_operation_get_con_handle(packet);
            response_code = gattservice_subevent_ascs_control_point_operation_get_response_code(packet);
            reason        = gattservice_subevent_ascs_control_point_operation_get_reason(packet);

            printf("            OPERATION STATUS - ase_id %d, response [0x%02x, 0x%02x], con_handle 0x%02x\n", ase_id, response_code, reason, con_handle);
            break;
        
        case GATTSERVICE_SUBEVENT_ASCS_STREAMENDPOINT_STATE:
            con_handle = gattservice_subevent_ascs_streamendpoint_state_get_con_handle(packet);
            ase_id     = gattservice_subevent_ascs_streamendpoint_state_get_ase_id(packet);
            ase_state  = gattservice_subevent_ascs_streamendpoint_state_get_state(packet);
            
            printf("ASCS Client: ASE STATE (%s) - ase_id %d, con_handle 0x%02x\n", ascs_util_ase_state2str(ase_state), ase_id, con_handle);
            switch (ase_state){
                case ASCS_STATE_ENABLING:
                    printf("Setup ISO Channel (TODO: list config)\n");
                    bap_service_client_setup_cis();
                    break;
                default:
                    break;
            }
            break;

        default:
            break;
    }
}

static void show_usage(void){
    bd_addr_t      iut_address;
    gap_local_bd_addr(iut_address);
    printf("\n--- BAP Client Test Console %s ---\n", bd_addr_to_str(iut_address));
    printf("c   - connect to %s\n", bap_app_server_addr_string);       
    
    printf("\n--- BASS Client Test Console %s ---\n", bd_addr_to_str(iut_address));
    printf("b   - connect to %s\n", bap_app_server_addr_string);       
    printf("s   - scanning started\n");
    printf("S   - scanning stopped\n");
    printf("a   - add source (pa_sync %d), DO_NOT_SYNCHRONIZE_TO_PA\n", LE_AUDIO_PA_SYNC_DO_NOT_SYNCHRONIZE_TO_PA);
    printf("A   - add source (pa_sync %d), SYNCHRONIZE_TO_PA_PAST_AVAILABLE\n", LE_AUDIO_PA_SYNC_SYNCHRONIZE_TO_PA_PAST_AVAILABLE);
    printf("C   - add source (pa_sync %d), SYNCHRONIZE_TO_PA_PAST_NOT_AVAILABLE\n", LE_AUDIO_PA_SYNC_SYNCHRONIZE_TO_PA_PAST_NOT_AVAILABLE);
    
    printf("\n--- PACS Client Test Console %s ---\n", bd_addr_to_str(iut_address));
    printf("p   - connect to %s\n", bap_app_server_addr_string); 
    
    printf("e   - get sink audio locations\n");
    printf("E   - set sink audio locations\n");
    
    printf("f   - get source audio locations\n");
    printf("F   - set sink audio locations\n");
    
    printf("g   - get available audio contexts\n");
    printf("h   - get supported audio contexts\n");
    printf("i   - get sink pacs\n");
    printf("j   - get source pacs\n");

    printf("\n--- ASCS Client Test Console %s ---\n", bd_addr_to_str(iut_address));
    printf("d   - connect to %s\n", bap_app_server_addr_string);    
    printf("k   - read ASE index %d\n", ase_index);

    printf("K   - configure Codec [8kHz, 7500us], ASE index %d\n", ase_index);
    printf("l   - create CIG + configure QoS 7500us, ASE index %d\n", ase_index);
    printf("L   - configure Codec [16kHz, 10000us], ASE index %d\n", ase_index);
    printf("m   - create CIG + configure QoS 10000us, ASE index %d\n", ase_index);
    printf("M   - enable, ASE index %d\n", ase_index);
    printf("n   - start Ready, ASE index %d\n", ase_index);
    printf("N   - stop Ready, ASE index %d\n", ase_index);
    printf("r   - disable, ASE index %d\n", ase_index);
    printf("R   - release, ASE index %d\n", ase_index);
    printf("q   - released, ASE index %d\n", ase_index);
    printf("Q   - update metadata, ASE index %d\n", ase_index);

    printf(" \n");
    printf(" \n");
    printf("Ctrl-c - exit\n");
    printf("---\n");
}

static void stdin_process(char cmd){
    uint8_t status = ERROR_CODE_SUCCESS;
    operation_cmd = cmd;
    switch (cmd){
        case 'c':
            printf("Connecting...\n");
            bap_app_client_state = BAP_APP_CLIENT_STATE_W4_GAP_CONNECTED;
            status = gap_connect(bap_app_server_addr, 0);
            break;

        case 'b':
            printf("BASS: connect 0x%02x\n", bap_app_client_con_handle);
            bap_app_client_state = BAP_APP_CLIENT_STATE_W4_BASS_CONNECTED;
            status = broadcast_audio_scan_service_client_connect(&bass_connection, 
                bass_sources, BASS_CLIENT_NUM_SOURCES, bap_app_client_con_handle, &bass_cid);
            break;

        case 'd':
            printf("ASCS: connect 0x%02x\n", bap_app_client_con_handle);
            status = audio_stream_control_service_service_client_connect(&ascs_connection, 
                streamendpoint_characteristics, ASCS_CLIENT_NUM_STREAMENDPOINTS, bap_app_client_con_handle, &ascs_cid);
            break;

        case 's':
            printf("BASS: Scanning started\n");
            status = broadcast_audio_scan_service_client_scanning_started(bass_cid);
            gap_set_scan_params(1, 0x30, 0x30, 0);
            gap_start_scan();
            break;
        case 'S':
            printf("BASS: Scanning stopped\n");
            status = broadcast_audio_scan_service_client_scanning_stopped(bass_cid);
            gap_stop_scan();
            break;

        case 'a':
            printf("BASS: Add source (pa_sync %d), LE_AUDIO_PA_SYNC_DO_NOT_SYNCHRONIZE_TO_PA\n", LE_AUDIO_PA_SYNC_DO_NOT_SYNCHRONIZE_TO_PA);
            source_data1.pa_sync = LE_AUDIO_PA_SYNC_DO_NOT_SYNCHRONIZE_TO_PA;
            status = broadcast_audio_scan_service_client_add_source(bass_cid, &source_data1);
            break;
        case 'A':
            printf("BASS: Add source (pa_sync %d), LE_AUDIO_PA_SYNC_SYNCHRONIZE_TO_PA_PAST_AVAILABLE\n", LE_AUDIO_PA_SYNC_SYNCHRONIZE_TO_PA_PAST_AVAILABLE);
            source_data1.pa_sync = LE_AUDIO_PA_SYNC_SYNCHRONIZE_TO_PA_PAST_AVAILABLE;
            status = broadcast_audio_scan_service_client_add_source(bass_cid, &source_data1);
            break;
        case 'C':
            printf("BASS: Add source (pa_sync %d), LE_AUDIO_PA_SYNC_SYNCHRONIZE_TO_PA_PAST_NOT_AVAILABLE\n", LE_AUDIO_PA_SYNC_SYNCHRONIZE_TO_PA_PAST_NOT_AVAILABLE);
            source_data1.pa_sync = LE_AUDIO_PA_SYNC_SYNCHRONIZE_TO_PA_PAST_NOT_AVAILABLE;
            status = broadcast_audio_scan_service_client_add_source(bass_cid, &source_data1);
            break;
        
         case 'p':
            printf("PACS Client: Establish connection\n");
            bap_app_client_state = BAP_APP_CLIENT_STATE_W4_PACS_CONNECTED;
            status = published_audio_capabilities_service_client_connect(&pacs_connection, bap_app_client_con_handle, &pacs_cid);
            break;

        case 'e':
            printf("PACS Client: Get sink audio locations\n");
            status = published_audio_capabilities_service_client_get_sink_audio_locations(pacs_cid);
            break;

        case 'E':
            printf("PACS Client: Set sink audio locations\n");
            status = published_audio_capabilities_service_client_set_sink_audio_locations(pacs_cid, 0x00000004);
            break;

        case 'f':
            printf("PACS Client: Get source audio locations\n");
            status = published_audio_capabilities_service_client_get_source_audio_locations(pacs_cid);
            break;
        case 'F':
            printf("PACS Client: Set source audio locations\n");
            status = published_audio_capabilities_service_client_set_source_audio_locations(pacs_cid, 0x00000004);
            break;
        
        case 'g':
            printf("PACS Client: Get available audio contexts\n");
            status = published_audio_capabilities_service_client_get_available_audio_contexts(pacs_cid);
            break;
        case 'h':
            printf("PACS Client: Get supported audio contexts\n");
            status = published_audio_capabilities_service_client_get_supported_audio_contexts(pacs_cid);
            break;
        case 'i':
            printf("PACS Client: Get sink pacs\n");
            status = published_audio_capabilities_service_client_get_sink_pacs(pacs_cid);
            break;
        case 'j':
            printf("PACS Client: Get source pacs\n");
            status = published_audio_capabilities_service_client_get_source_pacs(pacs_cid);
            break;

        case 'k':
            printf("ASCS Client: Read ASE with index %d\n", ase_index);

            status = audio_stream_control_service_service_client_read_streamendpoint(ascs_cid, ase_index);
            if (ase_index < 3){
                ase_index++;
            } else {
                ase_index = 0;
            }
            break;
        
        case 'K':
            ase_index = 0;
            printf("ASCS Client: Configure Codec [Mono, 8kHz, 7500us], ASE index %d\n", ase_index);
            cig_frame_duration = BTSTACK_LC3_FRAME_DURATION_7500US;
            cig_num_cis = 1;
            cis_num_channels = 1;
            cis_octets_per_frame = 26;
            cis_sampling_frequency_hz = 8000;
            status = audio_stream_control_service_client_streamendpoint_configure_codec(ascs_cid, ase_index, &codec_configuration_request_8kHz_7500us);
            break;

        case 'l':
            bap_app_qos_configuration = &qos_configuration_request_7500us;
            bap_service_client_setup_cig();
            break;
        case 'L':
            ase_index = 0;
            printf("ASCS Client: Configure Codec [Mono, 16kHz, 10000us, 40 octets], ASE index %d\n", ase_index);
            cig_frame_duration = BTSTACK_LC3_FRAME_DURATION_10000US;
            cig_num_cis = 1;
            cis_num_channels = 1;
            cis_octets_per_frame = 40;
            cis_sampling_frequency_hz = 16000;
            status = audio_stream_control_service_client_streamendpoint_configure_codec(ascs_cid, ase_index, &codec_configuration_request_16kHz_100000us);
            break;
        case 'm':
            bap_app_qos_configuration = &qos_configuration_request_10000us;
            bap_service_client_setup_cig();
            break;
        case 'M':
            printf("ASCS Client: Enable, ASE index %d\n", ase_index);
            status = audio_stream_control_service_client_streamendpoint_enable(ascs_cid, ase_index);
            break;
        case 'n':
            printf("ASCS Client: Start Ready, ASE index %d\n", ase_index);
            status = audio_stream_control_service_client_streamendpoint_receiver_start_ready(ascs_cid, ase_index);
            break;
        case 'N':
            printf("ASCS Client: Stop Ready, ASE index %d\n", ase_index);
            status = audio_stream_control_service_client_streamendpoint_receiver_stop_ready(ascs_cid, ase_index);
            break;

        case 'r':
            printf("ASCS Client: Disable, ASE index %d\n", ase_index);
            status = audio_stream_control_service_client_streamendpoint_disable(ascs_cid, ase_index);
            break;
        case 'R':
            printf("ASCS Client: Release, ASE index %d\n", ase_index);
            status = audio_stream_control_service_client_streamendpoint_release(ascs_cid, ase_index, false);
            break;

        case 'q':
            printf("ASCS Client: Released, ASE index %d\n", ase_index);
            audio_stream_control_service_client_streamendpoint_released(ascs_cid, ase_index);
            break;

        case 'Q':
            printf("ASCS Client: Update metadata, ASE index %d\n", ase_index);
            status = audio_stream_control_service_client_streamendpoint_metadata_update(ascs_cid, ase_index, &metadata_update_request);
            break;

        case '\n':
        case '\r':
            break;
        default:
            show_usage();
            break;
    }

    if (status != ERROR_CODE_SUCCESS){
        printf("Command '%c' failed with status 0x%02x\n", cmd, status);
    }
}

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    UNUSED(argc);
    (void)argv;
    
    l2cap_init();

    gatt_client_init();

    le_device_db_init();
    
    sm_init();
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    sm_set_authentication_requirements(SM_AUTHREQ_SECURE_CONNECTION);
    sm_event_callback_registration.callback = &hci_event_handler;
    sm_add_event_handler(&sm_event_callback_registration);

    broadcast_audio_scan_service_client_init(&bass_client_event_handler);
    published_audio_capabilities_service_client_init(&pacs_client_event_handler);
    audio_stream_control_service_client_init(&ascs_client_event_handler);

    hci_event_callback_registration.callback = &hci_event_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // parse human readable Bluetooth address
    sscanf_bd_addr(bap_app_server_addr_string, bap_app_server_addr);
    btstack_stdin_setup(stdin_process);

    // turn on!
    hci_power_control(HCI_POWER_ON);

    return 0;
}
/* EXAMPLE_END */
