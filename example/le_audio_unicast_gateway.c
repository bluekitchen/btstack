/*
 * Copyright (C) 2022 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "le_audio_unicast_source.c"

/*
 * LE Audio Unicast Source
 * Until GATT Services are available, we encode LC3 config in advertising
 */

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ad_parser.h"
#include "ble/le_device_db.h"
#include "ble/sm.h"
#include "bluetooth_company_id.h"
#include "bluetooth_data_types.h"
#include "bluetooth_gatt.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_lc3.h"
#include "btstack_lc3_google.h"
#include "btstack_run_loop.h"
#include "btstack_stdin.h"
#include "gap.h"
#include "hci.h"
#include "hci_cmd.h"
#include "hci_dump.h"
#include "hxcmod.h"
#include "l2cap.h"
#include "le-audio/gatt-service/audio_stream_control_service_client.h"
#include "le-audio/gatt-service/coordinated_set_identification_service_client.h"
#include "le-audio/gatt-service/published_audio_capabilities_service_client.h"
#include "mods/mod.h"
#include "ble/att_db_util.h"
#include "ble/att_server.h"
#include "le_audio_demo_util_source.h"

#include "le_audio_unicast_gateway.h"

// max config
#define MAX_CHANNELS 2
#define MAX_NUM_CIS 2
#define MAX_NUM_SERVERS 2
#define MAX_SAMPLES_PER_FRAME 480
#define MAX_LC3_FRAME_BYTES   155

#define NR_RSI_ENTRIES 5

#define ASCS_CLIENT_COUNT 2
#define ASCS_CLIENT_NUM_STREAMENDPOINTS 4

#define PEER_NAME_LEN 31

#define CIG_ID_INVALID 0xff

// hard-coded
#define NUM_CIS_RETRANSMISSIONS 2

static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_packet_callback_registration_t sm_event_callback_registration;

static enum {
    APP_W4_WORKING,
    APP_IDLE,
    APP_W4_UNICAST_SINK_ADV,
    APP_W4_COORDINATED_SET_INFO,
    APP_W4_COORDINATED_SET_ADV,
    APP_CONNECT_TO_NEXT_MEMBER,
    APP_W4_MEMBER_CONNECTED,
    APP_SET_CONNECTED,
    APP_CIG_CREATED,
    APP_STREAMING,
} app_state = APP_W4_WORKING;

// remote devices

typedef enum {
    SERVER_IDLE,
    SERVER_READY,
    SERVER_W4_CONNECTED,
    SERVER_W4_ENCRYPTED,
    SERVER_ENCRYPTED,
    SERVER_W4_CSIS_CONNECTED,
    SERVER_CSIS_QUERY_COMPLETED,
    SERVER_W4_ALL_MEMBERS,
    SERVER_W2_ASCS_CONNECT,
    SERVER_W4_ASCS_CONNECTED,
    SERVER_ASCS_CONNECTED,
    SERVER_W4_ASCS_CODEC_CONFIGURED,
    SERVER_ASCS_CODEC_CONFIGURED,
    SERVER_W4_ASCS_QOS_CONFIGURED,
    SERVER_ASCS_QOS_CONFIGURED,
    SERVER_ASCS_W2_ENABLE,
    SERVER_ASCS_W4_ENABLING,
    SERVER_ASCS_ENABLING,
    SERVER_ASCS_STREAMING,
    SERVER_DISCONNECT,
    SERVER_W4_DISCONNECTED
} server_state_t;

typedef struct {
    uint8_t server_id;

    bd_addr_t        remote_addr;
    bd_addr_type_t   remote_type;

    hci_con_handle_t acl_con_handle;

    server_state_t server_state;

    // csis
    csis_client_connection_t csis_connection;
    uint16_t csis_cid;
    uint8_t  sirk[16];
    uint8_t  coordinated_set_size;

    // ascs
    uint16_t ascs_cid;
    uint8_t  ase_id_sink;
    ascs_client_connection_t ascs_connection;
    ascs_streamendpoint_characteristic_t streamendpoint_characteristics[ASCS_CLIENT_NUM_STREAMENDPOINTS];

    // CIS
    uint8_t          cis_id;
    hci_con_handle_t cis_con_handle;

} server_t;

static server_t servers[MAX_NUM_SERVERS];
static uint8_t num_active_servers;

// CSIS Find Set Members
static csis_client_rsi_entry_t rsi_entries[NR_RSI_ENTRIES];

// PACS
// #define PACS_QUERY
#ifdef PACS_QUERY
static pacs_client_connection_t pacs_connection;
#endif
static uint16_t pacs_cid;

// ascs client
static ascs_client_codec_configuration_request_t ascs_codec_configuration_request;
static le_audio_metadata_t test_ascs_metadata;
static ascs_codec_configuration_t codec_configuration;

static le_audio_cig_t        cig;
static le_audio_cig_params_t cig_params;
static uint16_t              cig_id;

static hci_con_handle_t cis_con_handles[MAX_NUM_CIS];
static hci_con_handle_t acl_con_handles[MAX_NUM_CIS];
static bool cis_established[MAX_NUM_CIS];

static uint8_t source_ase_count;
static uint8_t sink_ase_count;

static ascs_qos_configuration_t ascs_qos_configuration;


// sinnk configuration
static uint8_t num_servers = 1;

// lc3 codec config
static uint32_t sampling_frequency_hz;
static btstack_lc3_frame_duration_t frame_duration;
static uint16_t octets_per_frame;
static uint8_t  num_channels = 1;

// codec menu
static uint8_t menu_sampling_frequency;
static uint8_t menu_variant;

// audio producer
static le_audio_demo_source_generator audio_source = AUDIO_SOURCE_MODPLAYER;

// enumerate default codec configs
static struct {
    uint16_t samplingrate_hz;
    uint8_t  samplingrate_index;
    uint8_t  num_variants;
    struct {
        const char * name;
        btstack_lc3_frame_duration_t frame_duration;
        uint16_t octets_per_frame;
    } variants[6];
} codec_configurations[] = {
    {
        8000, 0x01, 2,
        {
            {  "8_1",  BTSTACK_LC3_FRAME_DURATION_7500US, 26},
            {  "8_2", BTSTACK_LC3_FRAME_DURATION_10000US, 30}
        }
    },
    {
       16000, 0x03, 2,
       {
            {  "16_1",  BTSTACK_LC3_FRAME_DURATION_7500US, 30},
            {  "16_2", BTSTACK_LC3_FRAME_DURATION_10000US, 40}
       }
    },
    {
        24000, 0x05, 2,
        {
            {  "24_1",  BTSTACK_LC3_FRAME_DURATION_7500US, 45},
            {  "24_2", BTSTACK_LC3_FRAME_DURATION_10000US, 60}
       }
    },
    {
        32000, 0x06, 2,
        {
            {  "32_1",  BTSTACK_LC3_FRAME_DURATION_7500US, 60},
            {  "32_2", BTSTACK_LC3_FRAME_DURATION_10000US, 80}
        }
    },
    {
        44100, 0x07, 2,
        {
            { "441_1",  BTSTACK_LC3_FRAME_DURATION_7500US,  97},
            { "441_2", BTSTACK_LC3_FRAME_DURATION_10000US, 130}
        }
    },
    {
        48000, 0x08, 6,
        {
            {  "48_1", BTSTACK_LC3_FRAME_DURATION_7500US, 75},
            {  "48_2", BTSTACK_LC3_FRAME_DURATION_10000US, 100},
            {  "48_3", BTSTACK_LC3_FRAME_DURATION_7500US, 90},
            {  "48_4", BTSTACK_LC3_FRAME_DURATION_10000US, 120},
            {  "48_5", BTSTACK_LC3_FRAME_DURATION_7500US, 117},
            {  "48_6", BTSTACK_LC3_FRAME_DURATION_10000US, 155}
        }
    },
};

static void show_usage(void);

static void print_config(void) {
    printf("Config '%s' -> %u, %s ms, %u octets - %s\n",
           codec_configurations[menu_sampling_frequency].variants[menu_variant].name,
           codec_configurations[menu_sampling_frequency].samplingrate_hz,
           codec_configurations[menu_sampling_frequency].variants[menu_variant].frame_duration == BTSTACK_LC3_FRAME_DURATION_7500US ? "7.5" : "10",
           codec_configurations[menu_sampling_frequency].variants[menu_variant].octets_per_frame,
           audio_source == AUDIO_SOURCE_SINE ? "Sine" : "Modplayer");
}

static server_t * server_for_acl_con_handle(hci_con_handle_t acl_con_handle){
    uint8_t i;
    for (i=0; i < num_active_servers; i++){
        server_t * server = &servers[i];
        if (server->acl_con_handle == acl_con_handle){
            return server;
        }
    }
    return NULL;
}

static server_t * server_for_ascs_cid(uint16_t ascs_cid){
    uint8_t i;
    for (i=0; i < num_active_servers; i++){
        server_t * server = &servers[i];
        if (server->ascs_cid == ascs_cid){
            return server;
        }
    }
    return NULL;
}

static server_t * server_for_csis_cid(uint16_t csis_cid){
    uint8_t i;
    for (i=0; i < num_active_servers; i++){
        server_t * server = &servers[i];
        if (server->csis_cid == csis_cid){
            return server;
        }
    }
    return NULL;
}

static bool servers_in_state(server_state_t state){
    uint8_t i;
    for (i=0; i < num_active_servers; i++){
        server_t * server = &servers[i];
        if (server->server_state != state){
            return false;
        }
    }
    return true;
}

static void servers_set_state(server_state_t state){
    uint8_t i;
    for (i=0; i < num_active_servers; i++){
        server_t * server = &servers[i];
        server->server_state = state;
    }
}

static void configure_stream(void) {
    // use values from table
    sampling_frequency_hz = codec_configurations[menu_sampling_frequency].samplingrate_hz;
    octets_per_frame      = codec_configurations[menu_sampling_frequency].variants[menu_variant].octets_per_frame;
    frame_duration        = codec_configurations[menu_sampling_frequency].variants[menu_variant].frame_duration;

    uint8_t num_channels_per_stream = num_channels / num_servers;
    le_audio_demo_util_source_configure(num_servers, num_channels_per_stream, sampling_frequency_hz, frame_duration, octets_per_frame);
    le_audio_demo_util_source_generate_iso_frame(audio_source);
}

static void run_for_server(server_t * server){
    uint8_t status;
    switch (server->server_state){
        case SERVER_READY:
            server->server_state = SERVER_W4_CONNECTED;
            printf("GAP Client %u: connect to %s (%s)\n",
                   server->server_id,
                   bd_addr_to_str(server->remote_addr),
                   server->remote_type == BD_ADDR_TYPE_LE_PUBLIC ? "public" : "random");
            gap_connect(server->remote_addr, server->remote_type);
            break;
        case SERVER_ENCRYPTED:
            // connect to CSIS
            printf("CSIS Client %u: connect\n", server->server_id);
            server->server_state = SERVER_W4_CSIS_CONNECTED;
            coordinated_set_identification_service_client_connect(&server->csis_connection,
                                                        server->acl_con_handle, &server->csis_cid);
            break;
        case SERVER_W2_ASCS_CONNECT:
            // connect to ASCS
            printf("ASCS Client %u: connect\n", server->server_id);
            server->server_state = SERVER_W4_ASCS_CONNECTED;
            audio_stream_control_service_client_connect(&server->ascs_connection,
                                                        server->streamendpoint_characteristics,
                                                        ASCS_CLIENT_NUM_STREAMENDPOINTS,
                                                        server->acl_con_handle, &server->ascs_cid);
            break;
        case SERVER_ASCS_CONNECTED:
            // configure codec
            ascs_codec_configuration_request.target_latency = LE_AUDIO_CLIENT_TARGET_LATENCY_LOW_LATENCY;
            ascs_codec_configuration_request.target_phy = LE_AUDIO_CLIENT_TARGET_PHY_BALANCED;
            ascs_codec_configuration_request.coding_format = HCI_AUDIO_CODING_FORMAT_LC3;
            ascs_codec_configuration_request.company_id = 0;
            ascs_codec_configuration_request.vendor_specific_codec_id = 0;
            ascs_codec_configuration_request.specific_codec_configuration.codec_configuration_mask = 0x3e;
            ascs_codec_configuration_request.specific_codec_configuration.sampling_frequency_index = codec_configurations[menu_sampling_frequency].samplingrate_index;
            ascs_codec_configuration_request.specific_codec_configuration.frame_duration_index =
                    codec_configurations[menu_sampling_frequency].variants[menu_variant].frame_duration == BTSTACK_LC3_FRAME_DURATION_7500US ?
                    LE_AUDIO_CODEC_FRAME_DURATION_INDEX_7500US : LE_AUDIO_CODEC_FRAME_DURATION_INDEX_10000US;
            ascs_codec_configuration_request.specific_codec_configuration.audio_channel_allocation_mask = num_channels == 1 ? 1 : 3;
            ascs_codec_configuration_request.specific_codec_configuration.octets_per_codec_frame = codec_configurations[menu_sampling_frequency].variants[menu_variant].octets_per_frame;
            ascs_codec_configuration_request.specific_codec_configuration.codec_frame_blocks_per_sdu = num_channels;
            status = audio_stream_control_service_client_streamendpoint_configure_codec(server->ascs_cid, server->ase_id_sink, &ascs_codec_configuration_request);
            server->server_state = SERVER_W4_ASCS_CODEC_CONFIGURED;
            btstack_assert(status == ERROR_CODE_SUCCESS);
            break;
        case SERVER_ASCS_CODEC_CONFIGURED:
            if (app_state == APP_CIG_CREATED){
                uint8_t cis_id = server->cis_id;
                uint16_t sdu_interval_us = codec_configurations[menu_sampling_frequency].variants[menu_variant].frame_duration == BTSTACK_LC3_FRAME_DURATION_7500US ? 7500 : 10000;
                uint8_t framing = 0;
                uint16_t max_sdu = num_channels *  codec_configurations[menu_sampling_frequency].variants[menu_variant].octets_per_frame;
                uint8_t retransmission_number = NUM_CIS_RETRANSMISSIONS;
                uint16_t max_transport_latency_ms = 40;

                printf("ASCS_CONFIGURE_QOS ase_id %u, cig_id %u / cis %u, interval %u, framing %u, sdu_size %u, retrans %u, latency %u\n",
                       server->ase_id_sink, cig_id, cis_id, sdu_interval_us, framing, max_sdu, retransmission_number, max_transport_latency_ms);
                ascs_qos_configuration.cig_id = cig_id;
                ascs_qos_configuration.cis_id = cis_id;
                ascs_qos_configuration.sdu_interval = sdu_interval_us;
                ascs_qos_configuration.framing = framing;
                ascs_qos_configuration.phy = LE_AUDIO_SERVER_PHY_MASK_2M;
                ascs_qos_configuration.max_sdu = max_sdu;
                ascs_qos_configuration.retransmission_number = retransmission_number;
                ascs_qos_configuration.max_transport_latency_ms = max_transport_latency_ms;
                ascs_qos_configuration.presentation_delay_us = 40000;
                server->server_state = SERVER_W4_ASCS_QOS_CONFIGURED;
                status = audio_stream_control_service_client_streamendpoint_configure_qos(server->ascs_cid, server->ase_id_sink, &ascs_qos_configuration);
                btstack_assert(status == ERROR_CODE_SUCCESS);
            }
            break;
        case SERVER_ASCS_W2_ENABLE:
            server->server_state = SERVER_ASCS_W4_ENABLING;
            status = audio_stream_control_service_client_streamendpoint_enable(server->ascs_cid, server->ase_id_sink, &test_ascs_metadata);
            btstack_assert(status == ERROR_CODE_SUCCESS);
            break;

        case SERVER_DISCONNECT:
            server->server_state = SERVER_W4_DISCONNECTED;
            gap_disconnect(server->acl_con_handle);
            break;
        default:
            break;
    }
}

static void trigger_next_connect(void){
    // trigger next idle server
    btstack_assert(app_state == APP_CONNECT_TO_NEXT_MEMBER);
    uint8_t i;
    for (i=0;i<num_active_servers;i++){
        if (servers[i].server_state == SERVER_IDLE){
            printf("GAP Client %u: trigger connect\n", servers[i].server_id);
            servers[i].server_state = SERVER_READY;
            app_state = APP_W4_MEMBER_CONNECTED;
            break;
        }
    }
}

static void all_members_connected(void){
    // all connected
    app_state = APP_SET_CONNECTED;
    servers_set_state(SERVER_W2_ASCS_CONNECT);
    printf("[-] CSIS - all members connected\n");
}

static void app_run(void){
    // run main
    uint8_t i;
    switch (app_state){
        case APP_W4_COORDINATED_SET_INFO:
            if (servers[0].server_state == SERVER_CSIS_QUERY_COMPLETED){
                servers[0].server_state = SERVER_W4_ALL_MEMBERS;
                uint8_t set_size = servers[0].coordinated_set_size;
                // select mode based on coordinated set size
                switch (set_size){
                    case 1:
                        printf("CAP client %u: select stereo speaker mode\n", servers[0].server_id);
                        num_servers  = 1;
                        num_channels = 2;
                        break;
                    case 2:
                        printf("CAP client %u: select true-wireless mode\n", servers[0].server_id);
                        num_servers  = 2;
                        num_channels = 2;
                        break;
                    default:
                        printf("CAP client %u: set size %u not handled\n", servers[0].server_id, set_size);
                        num_servers = 1;
                        break;
                }
                if (num_servers == 2){
                    // need more servers
                    app_state = APP_W4_COORDINATED_SET_ADV;
                    coordinated_set_identification_service_client_find_members(rsi_entries, NR_RSI_ENTRIES, servers[0].sirk);
                    printf("CAP client %u: look for %u more Set Members\n", servers[0].server_id, set_size - 1);
                    // start scanning
                    gap_start_scan();
                } else {
                    all_members_connected();
                }
            }
            break;
        case APP_W4_MEMBER_CONNECTED:
            // check if CSIS client connected
            for (i=0;i<num_active_servers;i++){
                if (servers[i].server_state == SERVER_CSIS_QUERY_COMPLETED){
                    servers[i].server_state = SERVER_W4_ALL_MEMBERS;
                    // all connected?
                    if (servers_in_state(SERVER_W4_ALL_MEMBERS)){
                        all_members_connected();
                    } else {
                        // trigger next idle server
                        app_state = APP_CONNECT_TO_NEXT_MEMBER;
                        trigger_next_connect();
                    }
                }
            }
            break;
        case APP_CONNECT_TO_NEXT_MEMBER:
            trigger_next_connect();
            break;
        default:
            break;
    }

    // run servers
    for (i=0; i < num_active_servers; i++){
        run_for_server(&servers[i]);
    }
}

static void rsi_match_handler(bd_addr_type_t addr_type, bd_addr_t addr){
    // add coordinated set member
    printf("[-] CSIS add device %s\n", bd_addr_to_str(addr));
    memset(&servers[num_active_servers], 0, sizeof(servers[0]));
    servers[num_active_servers].server_id = num_active_servers;
    servers[num_active_servers].server_state = SERVER_IDLE;
    servers[num_active_servers].remote_type = addr_type;
    memcpy(servers[num_active_servers].remote_addr, addr, 6);
    num_active_servers++;
    printf("[-] Num active server %u\n",num_active_servers);

    // all found?
    if (num_active_servers >= num_servers){
        // stop scanning and start connecting
        app_state = APP_CONNECT_TO_NEXT_MEMBER;
        gap_stop_scan();
        app_run();
    }
}

static bool all_cis_established(void) {
    // check for complete
    uint8_t i;
    for (i=0; i < cig_params.num_cis; i++) {
        if (cis_established[i] == false){
            return false;
        }
    }
    return true;
}

static void try_transition_to_streaming() {
    // CIS Ready?
    if (all_cis_established() == false) {
        printf("Transition_to_streaming: not all cis establisehd\n");
        return;
    }
    // Server Ready?
    if (servers_in_state(SERVER_ASCS_STREAMING) == false){
        printf("Transition_to_streaming: not all server ready\n");
        return;
    }

    printf("All CIS Established and IO Paths setup, start streaming\n");

    app_state = APP_STREAMING;
    uint8_t i;
    for (i=0; i < cig_params.num_cis; i++) {
        hci_request_cis_can_send_now_events(cis_con_handles[i]);
    }
}

static void le_audio_unicast_source_handle_adv(bd_addr_type_t adv_addr_type, bd_addr_t adv_addr, const uint8_t * adv_data, uint16_t adv_size){
    switch (app_state) {
        case APP_W4_COORDINATED_SET_ADV:
            coordinated_set_identification_service_client_check_advertisement(adv_addr, adv_addr_type, adv_data, adv_size);
            return;
        case APP_W4_UNICAST_SINK_ADV:
            break;
        default:
            return;
    }

    ad_context_t context;
    bool match = false;
    uint8_t i;
    char remote_name[PEER_NAME_LEN];
    uint8_t rsi[6];
    memset(rsi, 0, sizeof(rsi));
    memset(remote_name, 0, sizeof(remote_name));
    for (ad_iterator_init(&context, adv_size, adv_data); ad_iterator_has_more(&context); ad_iterator_next(
            &context)) {
        uint8_t data_type = ad_iterator_get_data_type(&context);
        uint8_t data_len  = ad_iterator_get_data_len(&context);
        const uint8_t *data = ad_iterator_get_data(&context);
        switch (data_type) {
            case BLUETOOTH_DATA_TYPE_SHORTENED_LOCAL_NAME:
            case BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME:
                data_len = btstack_min(sizeof(remote_name) - 1, data_len);
                memcpy(remote_name, data, data_len);
                remote_name[data_len] = 0;
                printf("%s - '%s'\n", remote_name, bd_addr_to_str(adv_addr));
                break;
            case BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS:
            case BLUETOOTH_DATA_TYPE_INCOMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS:
                for (i=0;(i+1) < data_len; i++){
                    if (little_endian_read_16(data, i) == ORG_BLUETOOTH_SERVICE_AUDIO_STREAM_CONTROL_SERVICE){
                        match = true;
                    }
                }
                break;
            default:
                break;
        }
    }
    // done if not match expectation
    if (!match) return;

    // if already in server list, skip
    for (i=0; i < num_active_servers; i++){
        if (servers[i].remote_type != adv_addr_type)                     continue;
        if (memcmp(servers[i].remote_addr, adv_addr, 6) != 0) continue;
        return;
    }

    printf("- Remote Unicast Sink found, addr %s, name: '%s'\n", bd_addr_to_str(adv_addr), remote_name);

    // use as first server
    memset(&servers[0], 0, sizeof(servers[0]));
    servers[0].server_state = SERVER_READY;
    servers[0].remote_type = adv_addr_type;
    servers[0].server_id = 0;
    memcpy(servers[0].remote_addr, adv_addr, 6);
    num_active_servers = 1;
    // stop scanning and start connecting
    app_state = APP_W4_COORDINATED_SET_INFO;
    gap_stop_scan();
    app_run();
}

static void create_cig(void){
    btstack_assert((num_servers == 1) || (num_channels == 2));

    uint8_t i;

    configure_stream();

    // select cig id
    cig_id = 1;

    // TODO: setup CIG
    cig_params.cig_id =  cig_id;
    cig_params.num_cis = num_servers;
    cig_params.sdu_interval_c_to_p = codec_configuration.specific_codec_configuration.frame_duration_index == LE_AUDIO_CODEC_FRAME_DURATION_INDEX_7500US ? 7500 : 10000;
    cig_params.sdu_interval_p_to_c = codec_configuration.specific_codec_configuration.frame_duration_index == LE_AUDIO_CODEC_FRAME_DURATION_INDEX_7500US ? 7500 : 10000;
    cig_params.worst_case_sca = 0; // 251 ppm to 500 ppm
    cig_params.packing = 0;        // sequential
    cig_params.framing =0;
    cig_params.max_transport_latency_c_to_p = 40;
    cig_params.max_transport_latency_p_to_c = 40;

    printf("CIG_CREATE cig %u, num cis %u, interval %u us\n", cig_params.cig_id, cig_params.num_cis, cig_params.sdu_interval_c_to_p );
    for (i = 0; i < cig_params.num_cis; i++) {
        uint8_t cis_id = 1 + i;

        cig_params.cis_params[i].cis_id = cis_id;
        cig_params.cis_params[i].max_sdu_c_to_p = codec_configuration.specific_codec_configuration.octets_per_codec_frame * num_channels / num_servers;
        cig_params.cis_params[i].max_sdu_p_to_c = 0;
        cig_params.cis_params[i].phy_c_to_p = LE_AUDIO_SERVER_PHY_MASK_2M;
        cig_params.cis_params[i].phy_p_to_c = LE_AUDIO_SERVER_PHY_MASK_2M;
        cig_params.cis_params[i].rtn_c_to_p = NUM_CIS_RETRANSMISSIONS;
        cig_params.cis_params[i].rtn_p_to_c = NUM_CIS_RETRANSMISSIONS;
        printf("CIG_CREATE -- cis %u, sdu_c_to_p %u, sdu_p_to_c %u\n",
               cig_params.cis_params[i].cis_id,
               cig_params.cis_params[i].max_sdu_c_to_p,
               cig_params.cis_params[i].max_sdu_p_to_c);

        servers[i].cis_id = cis_id;
        servers[i].cis_con_handle = HCI_CON_HANDLE_INVALID;
    }

    // ascs configured
    uint8_t status = gap_cig_create(&cig, &cig_params);
    btstack_assert(status == ERROR_CODE_SUCCESS);
}

static void hci_packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    if (packet_type != HCI_EVENT_PACKET) return;

    hci_con_handle_t cis_con_handle;
    hci_con_handle_t acl_con_handle;
    uint8_t i;
    uint8_t status;
    bd_addr_t adv_addr;
    bd_addr_type_t adv_addr_type;
    uint8_t adv_size;
    const uint8_t *adv_data;

    switch (packet[0]) {
        case BTSTACK_EVENT_STATE:
            switch(btstack_event_state_get_state(packet)) {
                case HCI_STATE_WORKING:
                    app_state = APP_IDLE;
#ifdef ENABLE_DEMO_MODE
                    // start unicast automatically, mod player
                    start_unicast();
#else
                    show_usage();
                    printf("Please select sample frequency and variation, then start scanning for headphones\n");
#endif
                    break;
                case HCI_STATE_OFF:
                    printf("Goodbye\n");
                    exit(0);
                    break;
                default:
                    break;
            }
            break;
        case GAP_EVENT_ADVERTISING_REPORT:
            adv_size = gap_event_advertising_report_get_data_length(packet);
            adv_data = gap_event_advertising_report_get_data(packet);
            gap_event_advertising_report_get_address(packet, adv_addr);
            adv_addr_type = gap_event_advertising_report_get_address_type(packet);
            le_audio_unicast_source_handle_adv(adv_addr_type, adv_addr, adv_data, adv_size);
            break;
        case GAP_EVENT_EXTENDED_ADVERTISING_REPORT:
            adv_size = gap_event_extended_advertising_report_get_data_length(packet);
            adv_data = gap_event_extended_advertising_report_get_data(packet);
            gap_event_extended_advertising_report_get_address(packet, adv_addr);
            adv_addr_type = gap_event_extended_advertising_report_get_address_type(packet) & 1;
            le_audio_unicast_source_handle_adv(adv_addr_type, adv_addr, adv_data, adv_size);
            break;
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            // identify server / cis
            acl_con_handle = hci_event_disconnection_complete_get_connection_handle(packet);
            for (i=0; i < num_active_servers; i++) {
                server_t *server = &servers[i];
                if (server->acl_con_handle == acl_con_handle) {
                    printf("GAP Client %u: Disconnected\n", server->server_id);
                    server->acl_con_handle = hci_subevent_le_connection_complete_get_connection_handle(packet);
                    server->server_state = SERVER_IDLE;
                    sm_request_pairing(server->acl_con_handle);
                }
            }
            //
            if (servers_in_state(SERVER_IDLE)){
                printf("Headset(s) disconnected\n");
                app_state = APP_IDLE;
                // gap_cig_remove(cig_id);
            }
            break;
        case HCI_EVENT_META_GAP:
            switch (hci_event_gap_meta_get_subevent_code(packet)) {
                case GAP_SUBEVENT_LE_CONNECTION_COMPLETE:
                    if (gap_subevent_le_connection_complete_get_status(packet) == ERROR_CODE_SUCCESS){
                        // find server
                        for (i=0; i < num_active_servers; i++){
                            server_t * server = &servers[i];
                            if (server->server_state == SERVER_W4_CONNECTED){
                                printf("GAP Client %u: Connected\n", server->server_id);
                                server->acl_con_handle = hci_subevent_le_connection_complete_get_connection_handle(packet);
                                server->server_state = SERVER_W4_ENCRYPTED;
                                sm_request_pairing(server->acl_con_handle);
                            }
                        }
                        break;
                    }
                    break;
                case GAP_SUBEVENT_CIG_CREATED:
                    status = gap_subevent_cig_created_get_status(packet);
                    printf("CIG Created 0x%02x\n", status);
                    if (status == ERROR_CODE_SUCCESS){
                        cig_id = gap_subevent_cig_created_get_cig_id(packet);
                        printf("CIS Connection Handles: \n");
                        for (i = 0; i < cig.num_cis; i++) {
                            cis_con_handle = gap_subevent_cig_created_get_cis_con_handles(packet, i);
                            cis_con_handles[i]        = cis_con_handle;
                            servers[i].cis_con_handle = cis_con_handle;
                            printf("- server %u, ASCS Client %u: 0x%04x \n", i, servers[i].server_id, cis_con_handles[i]);
                        }
                    }
                    app_state = APP_CIG_CREATED;
                    break;
                case GAP_SUBEVENT_CIS_CREATED:
                    cis_con_handle = gap_subevent_cis_created_get_cis_con_handle(packet);
                    status = gap_subevent_cis_created_get_status(packet);
                    printf("GAP_SUBEVENT_CIS_CREATED: cis con handle 0x%04x, status 0x%02x\n", cis_con_handle, status);
                    if (status == ERROR_CODE_SUCCESS){
                        for (i=0; i < cig_params.num_cis; i++){
                            if (cis_con_handle == cis_con_handles[i]){
                                printf("CIS id %u with con handle 0x%04x established\n", i, cis_con_handle);
                                cis_established[i] = true;
                            }
                        }
                        try_transition_to_streaming();
                    break;
                }
                default:
                    break;
            }
            break;
        case HCI_EVENT_CIS_CAN_SEND_NOW:
            cis_con_handle = hci_event_cis_can_send_now_get_cis_con_handle(packet);
            log_info("ISO can send now %04x", cis_con_handle);
            for (i=0;i<cig_params.num_cis;i++){
                if (cis_con_handle == cis_con_handles[i]){
                    // allow to send
                    le_audio_demo_util_source_send(i, cis_con_handle);
                    // TODO: replace this quick fix
                    if (i == cig_params.num_cis-1){
                        le_audio_demo_util_source_generate_iso_frame(audio_source);
                    }
                    hci_request_cis_can_send_now_events(cis_con_handle);
                }
            }
            break;

        default:
            break;
    }

    app_run();
}

static void sm_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    uint32_t passkey;
    bd_addr_type_t addr_type;
    bd_addr_t addr;
    server_t * server = NULL;

    if (packet_type != HCI_EVENT_PACKET) return;

    switch (hci_event_packet_get_type(packet)) {
        case SM_EVENT_JUST_WORKS_REQUEST:
            server = server_for_acl_con_handle(sm_event_just_works_request_get_handle(packet));
            printf("GAP Client %u: Just works requested - auto-accept\n", server->server_id);
            sm_just_works_confirm(server->acl_con_handle);
            break;
        case SM_EVENT_NUMERIC_COMPARISON_REQUEST:
            server = server_for_acl_con_handle(sm_event_numeric_comparison_request_get_handle(packet));
            printf("GAP Client %u: Confirming numeric comparison: %"PRIu32" - auto-accept\n", server->server_id, sm_event_numeric_comparison_request_get_passkey(packet));
            sm_numeric_comparison_confirm(server->acl_con_handle);
            break;
        case SM_EVENT_PASSKEY_DISPLAY_NUMBER:
            server = server_for_acl_con_handle(sm_event_passkey_display_number_get_handle(packet));
            passkey = sm_event_passkey_display_number_get_passkey(packet);
            printf("GAP Client %u: Display Passkey: %"PRIu32"\n", server->server_id, passkey);
            break;
        case SM_EVENT_PASSKEY_INPUT_NUMBER:
            server = server_for_acl_con_handle(sm_event_passkey_input_number_get_handle(packet));
            printf("GAP Client %u: Passkey Input requested\n", server->server_id);
            // MESSAGE("Sending fixed passkey %"PRIu32"\n", FIXED_PASSKEY);
            // sm_passkey_input(sm_event_passkey_input_number_get_handle(packet), FIXED_PASSKEY);
            break;
        case SM_EVENT_PAIRING_COMPLETE:
            server = server_for_acl_con_handle(sm_event_pairing_complete_get_handle(packet));
            btstack_assert(server != NULL);
            switch (sm_event_pairing_complete_get_status(packet)){
                case ERROR_CODE_SUCCESS:
                    printf("GAP Client %u: Pairing complete, success\n", server->server_id);
                    server->server_state = SERVER_ENCRYPTED;
                    break;
                case ERROR_CODE_CONNECTION_TIMEOUT:
                    printf("GAP Client %u: Pairing failed, timeout\n", server->server_id);
                    break;
                case ERROR_CODE_REMOTE_USER_TERMINATED_CONNECTION:
                    printf("GAP Client %u: Pairing failed, disconnected\n", server->server_id);
                    break;
                case ERROR_CODE_AUTHENTICATION_FAILURE:
                    printf("GAP Client %u: Pairing failed, reason = %u\n", server->server_id, sm_event_pairing_complete_get_reason(packet));
                    break;
                default:
                    break;
            }
            break;
        case SM_EVENT_REENCRYPTION_COMPLETE:
            server = server_for_acl_con_handle(sm_event_pairing_complete_get_handle(packet));
            btstack_assert(server != NULL);
            switch (sm_event_reencryption_complete_get_status(packet)){
                case ERROR_CODE_SUCCESS:
                    printf("GAP Client %u: Re-encryption complete, success\n", server->server_id);
                    server->server_state = SERVER_ENCRYPTED;
                    break;
                case ERROR_CODE_CONNECTION_TIMEOUT:
                    printf("GAP Client %u: Re-encryption failed, timeout\n", server->server_id);
                    break;
                case ERROR_CODE_REMOTE_USER_TERMINATED_CONNECTION:
                    printf("GAP Client %u: Re-encryption failed, disconnected\n", server->server_id);
                    break;
                case ERROR_CODE_PIN_OR_KEY_MISSING:
                    printf("GAP Client %u: Re-encryption failed, bonding information missing\n", server->server_id);
                    printf("GAP Client %u: Assuming remote lost bonding information\n", server->server_id);
                    printf("GAP Client %u: Deleting local bonding information and start new pairing...\n", server->server_id);
                    sm_event_reencryption_complete_get_address(packet, addr);
                    addr_type = sm_event_reencryption_started_get_addr_type(packet);
                    gap_delete_bonding(addr_type, addr);
                    sm_request_pairing(sm_event_reencryption_complete_get_handle(packet));
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }

    app_run();
}

static void pacs_client_next_query(void){
    static uint8_t step = 0;
    uint8_t status=ERROR_CODE_SUCCESS;
    switch (step++){
        case 0:
            printf("PACS Client: Get sink audio locations\n");
            status = published_audio_capabilities_service_client_get_sink_audio_locations(pacs_cid);
            break;
        case 1:
            printf("PACS Client: Get source audio locations\n");
            status = published_audio_capabilities_service_client_get_source_audio_locations(pacs_cid);
            break;
        case 2:
            printf("PACS Client: Get available audio contexts\n");
            status = published_audio_capabilities_service_client_get_available_audio_contexts(pacs_cid);
            break;
        case 3:
            printf("PACS Client: Get supported audio contexts\n");
            status = published_audio_capabilities_service_client_get_supported_audio_contexts(pacs_cid);
            break;
        case 4:
            printf("PACS Client: Get sink pacs\n");
            status = published_audio_capabilities_service_client_get_sink_pacs(pacs_cid);
            break;
        case 5:
            printf("PACS Client: Get source pacs\n");
            status = published_audio_capabilities_service_client_get_source_pacs(pacs_cid);
            break;
        case 6:
            printf("PACS DONE\n");
#ifdef PACS_QUERY
            printf("Connect to ASCS\n");
            status = audio_stream_control_service_client_connect(&ascs_connections[ascs_index],
                                                                         &streamendpoint_characteristics[ascs_index * ASCS_CLIENT_NUM_STREAMENDPOINTS],
                                                                         ASCS_CLIENT_NUM_STREAMENDPOINTS,
                                                                         acl_con_handle, &ascs_cid);
#endif

            break;
        default:
            break;
    }
    if (status != ERROR_CODE_SUCCESS){
        printf("[!] PACS Client: status 0x%02x\n", status);
    }

    app_run();
}

static void pacs_client_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_GATTSERVICE_META) return;

    switch (hci_event_gattservice_meta_get_subevent_code(packet)){
        case GATTSERVICE_SUBEVENT_PACS_CLIENT_CONNECTED:
            if (gattservice_subevent_pacs_client_connected_get_status(packet) != ERROR_CODE_SUCCESS){
                printf("PACS client: connection failed, cid 0x%04x, con_handle 0x%04x, status 0x%02x\n", pacs_cid,
                       gattservice_subevent_pacs_client_connected_get_con_handle(packet),
                       gattservice_subevent_pacs_client_connected_get_status(packet));
                return;
            }
            printf("PACS client: connected, cid 0x%04x\n", pacs_cid);
            pacs_client_next_query();
            break;

        case GATTSERVICE_SUBEVENT_PACS_CLIENT_DISCONNECTED:
            pacs_cid = 0;
            printf("PACS Client: disconnected\n");
            break;

        case GATTSERVICE_SUBEVENT_PACS_CLIENT_OPERATION_DONE:
            if (gattservice_subevent_pacs_client_operation_done_get_status(packet) == ERROR_CODE_SUCCESS){
                printf("      Operation successful\n");
            } else {
                printf("      Operation failed with status 0x%02x\n", gattservice_subevent_pacs_client_operation_done_get_status(packet));
            }
            pacs_client_next_query();
            break;

        case GATTSERVICE_SUBEVENT_PACS_CLIENT_AUDIO_LOCATIONS:
            printf("PACS Client: %s Audio Locations - 0x%04x \n",
                   gattservice_subevent_pacs_client_audio_locations_get_le_audio_role(packet) == LE_AUDIO_ROLE_SINK ? "Sink" : "Source",
                   gattservice_subevent_pacs_client_audio_locations_get_audio_locations_mask(packet));
            break;

        case GATTSERVICE_SUBEVENT_PACS_CLIENT_AVAILABLE_AUDIO_CONTEXTS:
            printf("PACS Client: Available Audio Contexts:\n");
            printf("      Sink   0x%02x\n", gattservice_subevent_pacs_client_available_audio_contexts_get_sink_mask(packet));
            printf("      Source 0x%02x\n", gattservice_subevent_pacs_client_available_audio_contexts_get_source_mask(packet));
            break;

        case GATTSERVICE_SUBEVENT_PACS_CLIENT_SUPPORTED_AUDIO_CONTEXTS:
            printf("PACS Client: Supported Audio Contexts:\n");
            printf("      Sink   0x%02x\n", gattservice_subevent_pacs_client_supported_audio_contexts_get_sink_mask(packet));
            printf("      Source 0x%02x\n", gattservice_subevent_pacs_client_supported_audio_contexts_get_source_mask(packet));
            break;

        case GATTSERVICE_SUBEVENT_PACS_CLIENT_PACK_RECORD:
            printf("PACS Client: %s PAC Record\n", gattservice_subevent_pacs_client_pack_record_get_le_audio_role(packet) == LE_AUDIO_ROLE_SINK ? "Sink" : "Source");
            printf("              supported_sampling_frequencies_mask 0x%04x\n", gattservice_subevent_pacs_client_pack_record_get_supported_sampling_frequencies_mask(packet));
            printf("              supported_frame_durations_mask      0x%04x\n", gattservice_subevent_pacs_client_pack_record_get_supported_frame_durations_mask(packet));
            printf("              supported_audio_channel_counts_mask 0x%04x\n", gattservice_subevent_pacs_client_pack_record_get_supported_audio_channel_counts_mask(packet));
            printf("              referred_audio_contexts_mask        0x%04x\n", gattservice_subevent_pacs_client_pack_record_get_preferred_audio_contexts_mask(packet));
            printf("              streaming_audio_contexts_mask       0x%04x\n", gattservice_subevent_pacs_client_pack_record_get_streaming_audio_contexts_mask(packet));
            printf("              supported_octets_per_frame_min_num  %4u\n", gattservice_subevent_pacs_client_pack_record_get_supported_octets_per_frame_min_num(packet));
            printf("              supported_octets_per_frame_max_num  %4u\n", gattservice_subevent_pacs_client_pack_record_get_supported_octets_per_frame_max_num(packet));
            printf("              supported_max_codec_frames_per_sdu  %4u\n", gattservice_subevent_pacs_client_pack_record_get_supported_max_codec_frames_per_sdu(packet));
            printf("              ccids_num                           %4u\n", gattservice_subevent_pacs_client_pack_record_get_ccids_num(packet));
            break;
        case GATTSERVICE_SUBEVENT_PACS_CLIENT_PACK_RECORD_DONE:
            printf("      %s PAC Record DONE\n", gattservice_subevent_pacs_client_pack_record_done_get_le_audio_role(packet) == LE_AUDIO_ROLE_SINK ? "Sink" : "Source");
            break;

        default:
            break;
    }

    app_run();
}

static void csis_client_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_GATTSERVICE_META) return;

    uint8_t status;
    server_t * server = NULL;

    switch (hci_event_gattservice_meta_get_subevent_code(packet)){
        case GATTSERVICE_SUBEVENT_CSIS_CLIENT_CONNECTED:
            server = server_for_csis_cid(gattservice_subevent_csis_client_connected_get_csis_cid(packet));
            if (server != NULL){

                if (gattservice_subevent_csis_client_connected_get_status(packet) != ERROR_CODE_SUCCESS){
                    printf("CSIS client %u: connection failed, cid 0x%04x, con_handle 0x%04x, status 0x%02x\n",
                           server->server_id, server->csis_cid, server->acl_con_handle,
                           gattservice_subevent_csis_client_connected_get_status(packet));
                    printf("TODO: handle CSIS connection failure\n");
                    return;
                }

                printf("CSIS client %u: connected, cid 0x%04x\n", server->server_id, server->csis_cid);

                // get coordinated set size
                coordinated_set_identification_service_client_read_coordinated_set_size(server->csis_cid);
            }
            break;

        case GATTSERVICE_SUBEVENT_CSIS_CLIENT_COORDINATED_SET_SIZE:
            server = server_for_csis_cid(gattservice_subevent_csis_client_coordinated_set_size_get_csis_cid(packet));
            status = gattservice_subevent_csis_client_coordinated_set_size_get_status(packet);
            if (status != ERROR_CODE_SUCCESS){
                printf("CSIS client %u: read coordinated set size failed, 0x%02x\n", server->server_id, status);
            } else {
                uint8_t set_size = gattservice_subevent_csis_client_coordinated_set_size_get_coordinated_set_size(packet);
                server->coordinated_set_size = set_size;
                printf("CSIS client %u: coordinated_set_size %u\n", server->server_id, set_size);
            }
            coordinated_set_identification_service_client_read_sirk(server->csis_cid);
            break;

        case GATTSERVICE_SUBEVENT_CSIS_CLIENT_SIRK:
            server = server_for_csis_cid(gattservice_subevent_csis_client_sirk_get_csis_cid(packet));
            status = gattservice_subevent_csis_client_sirk_get_status(packet);
            if (status != ERROR_CODE_SUCCESS){
                printf("CSIS client %u: read SIRK failed, 0x%02x\n", server->server_id, status);
                printf("TODO: handle get sirk failed\n");
                break;
            }
            gattservice_subevent_csis_client_sirk_get_sirk(packet, server->sirk);
            printf("CSIS client %u: remote SIRK (%s) ", server->server_id,
                   (csis_sirk_type_t)gattservice_subevent_csis_client_sirk_get_sirk_type(packet) == CSIS_SIRK_TYPE_ENCRYPTED ? "ENCRYPTED" : "PLAIN_TEXT");
            printf_hexdump(server->sirk, sizeof(server->sirk));

            server->server_state = SERVER_CSIS_QUERY_COMPLETED;
            break;

        case GATTSERVICE_SUBEVENT_CSIS_RSI_MATCH:
            if (gattservice_subevent_csis_rsi_match_get_match(packet) != 0){
                bd_addr_t addr;
                gattservice_subevent_csis_rsi_match_get_source_address(packet, addr);
                rsi_match_handler(gattservice_subevent_csis_rsi_match_get_source_address_type(packet), addr);
            }
            break;

        case GATTSERVICE_SUBEVENT_CSIS_CLIENT_LOCK:
            server = server_for_csis_cid(gattservice_subevent_csis_client_lock_get_csis_cid(packet));
            status = gattservice_subevent_csis_client_lock_get_status(packet);
            if (status != ERROR_CODE_SUCCESS){
                printf("CSIS client %u: read LOCK failed, 0x%02x\n", server->server_id, status);
                break;
            }
            if (server != NULL){
                printf("CSIS client %u: remote LOCK %u\n", server->server_id, gattservice_subevent_csis_client_lock_get_lock(packet));

                // set locked
                printf("CSIS client %u: set client LOCK\n", server->server_id);
                coordinated_set_identification_service_client_write_member_lock(server->csis_cid, CSIS_MEMBER_LOCKED);
            }
            break;

        case GATTSERVICE_SUBEVENT_CSIS_CLIENT_RANK:
            server = server_for_csis_cid(gattservice_subevent_csis_client_rank_get_csis_cid(packet));
            status = gattservice_subevent_csis_client_rank_get_status(packet);
            if (status != ERROR_CODE_SUCCESS){
                printf("CSIS client %u: read RANK failed, 0x%02x\n", status, server->server_id);
                break;
            }
            printf("CSIS client %u: remote member RANK %u\n", server->server_id, gattservice_subevent_csis_client_rank_get_rank(packet));
            break;

        case GATTSERVICE_SUBEVENT_CSIS_CLIENT_LOCK_WRITE_COMPLETE:
            server = server_for_csis_cid(gattservice_subevent_csis_client_lock_write_complete_get_csis_cid(packet));
            status = gattservice_subevent_csis_client_lock_write_complete_get_status(packet);
            if (status != ERROR_CODE_SUCCESS){
                printf("CSIS client %u: write LOCK failed, 0x%02x\n", server->server_id, status);
            } else {
                if (server != NULL){
                    csis_member_lock_t csis_member_lock = (csis_member_lock_t) gattservice_subevent_csis_client_lock_write_complete_get_lock(packet);
                    printf("CSIS client %u: remote member %s\n", server->server_id, csis_member_lock == CSIS_MEMBER_UNLOCKED ? "UNLOCKED" : "LOCKED");
                }
            }
            break;

        case GATTSERVICE_SUBEVENT_CSIS_CLIENT_DISCONNECTED:
            server = server_for_csis_cid(gattservice_subevent_csis_client_disconnected_get_csis_cid(packet));
            if (server != NULL){
                server->csis_cid = 0;
                printf("CSIS Client %u: disconnected\n", server->server_id);
            }
            break;

        default:
            break;
    }

    app_run();
}

void ascs_client_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_GATTSERVICE_META) return;

    hci_con_handle_t con_handle;
    ascs_state_t ase_state;
    uint8_t response_code;
    uint8_t reason;
    // uint8_t ase_id;
    uint8_t opcode;
    uint8_t i;
    uint8_t status;
    server_t * server = NULL;

    switch (hci_event_gattservice_meta_get_subevent_code(packet)){
        case GATTSERVICE_SUBEVENT_ASCS_CLIENT_CONNECTED:
            con_handle = gattservice_subevent_ascs_client_connected_get_con_handle(packet);
            server = server_for_acl_con_handle(con_handle);
            if (server != NULL){
                btstack_assert(server->server_state == SERVER_W4_ASCS_CONNECTED);
                if (gattservice_subevent_ascs_client_connected_get_status(packet) != ERROR_CODE_SUCCESS){
                    // TODO send error response
                    printf("ASCS Client %u: connection failed, con_handle 0x%04x, status 0x%02x\n", server->server_id, con_handle,
                           gattservice_subevent_ascs_client_connected_get_status(packet));
                    return;
                }
                source_ase_count = gattservice_subevent_ascs_client_connected_get_source_ase_num(packet);
                sink_ase_count = gattservice_subevent_ascs_client_connected_get_sink_ase_num(packet);
                printf("ASCS Client %u: connected, con_handle 0x%04x, num Sink ASEs: %u, num Source ASEs: %u\n",
                       server->server_id, con_handle, sink_ase_count, source_ase_count);

                // dump ASEs
                for (i=0;i<source_ase_count+sink_ase_count;i++){
                    ascs_streamendpoint_characteristic_t * characteristic = server->ascs_connection.streamendpoints[i].ase_characteristic;
                    printf("ASCS Client %u: ASE ID: %u - role %s\n", server->server_id, characteristic->ase_id, characteristic->role == LE_AUDIO_ROLE_SOURCE ? "Source" : "Sink");
                    if (characteristic->role == LE_AUDIO_ROLE_SINK){
                        server->ase_id_sink = characteristic->ase_id;
                        printf("ASCS Client %u: Using ASE ID %u for audio\n", server->server_id, server->ase_id_sink);
                        break;
                    }
                }
                server->server_state = SERVER_ASCS_CONNECTED;
            }
            break;

        case GATTSERVICE_SUBEVENT_ASCS_CLIENT_DISCONNECTED:
            server = server_for_ascs_cid(gattservice_subevent_ascs_client_disconnected_get_ascs_cid(packet));
            if (server != NULL) {
                printf("ASCS Client %u: disconnected\n", server->server_id);
            }
            break;

        case GATTSERVICE_SUBEVENT_ASCS_CLIENT_CODEC_CONFIGURATION:
            server = server_for_ascs_cid(gattservice_subevent_ascs_client_codec_configuration_get_ascs_cid(packet));
            if (server != NULL){
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
                printf("ASCS Client %u: CODEC CONFIGURATION - ase_id %d,\n", server->server_id, server->ase_id_sink);
                server->server_state = SERVER_ASCS_CODEC_CONFIGURED;
            }
            break;

        case GATTSERVICE_SUBEVENT_ASCS_CLIENT_QOS_CONFIGURATION:
            server = server_for_ascs_cid(gattservice_subevent_ascs_client_qos_configuration_get_ascs_cid(packet));
            if (server != NULL){
                printf("ASCS Client %u: QOS CONFIGURATION - ase_id %u\n", server->server_id,
                    gattservice_subevent_ascs_server_qos_configuration_get_ase_id(packet));
            }
            break;

        case GATTSERVICE_SUBEVENT_ASCS_CLIENT_METADATA:
            server = server_for_ascs_cid(gattservice_subevent_ascs_client_metadata_get_ascs_cid(packet));
            if (server != NULL){
                printf("ASCS Client %u: METADATA UPDATE - ase_id %u\n", server->server_id,
                       gattservice_subevent_ascs_client_metadata_get_ase_id(packet));
            }
            break;

        case GATTSERVICE_SUBEVENT_ASCS_CLIENT_CONTROL_POINT_OPERATION_RESPONSE:
            server = server_for_ascs_cid(gattservice_subevent_ascs_client_control_point_operation_response_get_ascs_cid(packet));
            if (server != NULL){
                response_code = gattservice_subevent_ascs_client_control_point_operation_response_get_response_code(packet);
                reason        = gattservice_subevent_ascs_client_control_point_operation_response_get_reason(packet);
                opcode        = gattservice_subevent_ascs_client_control_point_operation_response_get_opcode(packet);
                printf("ASCS Client %u: Operation complete - ase_id %d, opcode %u, response [0x%02x, 0x%02x]\n",
                       server->server_id,
                       gattservice_subevent_ascs_client_control_point_operation_response_get_ase_id(packet),
                       opcode, response_code, reason);
            }
            break;

        case GATTSERVICE_SUBEVENT_ASCS_CLIENT_STREAMENDPOINT_STATE:
            server = server_for_ascs_cid(gattservice_subevent_ascs_client_streamendpoint_state_get_ascs_cid(packet));
            if (server != NULL){
                ase_state  = gattservice_subevent_ascs_client_streamendpoint_state_get_state(packet);
                printf("ASCS Client %u: ASE STATE (%s) - ase_id %d, role %s\n", server->server_id,
                       ascs_util_ase_state2str(ase_state), server->ase_id_sink,
                       (audio_stream_control_service_client_get_ase_role(server->ascs_cid, server->ase_id_sink) == LE_AUDIO_ROLE_SOURCE) ? "SOURCE" : "SINK" );
                switch (ase_state) {
                    case ASCS_STATE_CODEC_CONFIGURED:
                        // trigger cig creation if all are configured
                        if (servers_in_state(SERVER_ASCS_CODEC_CONFIGURED)){
                            create_cig();
                        }
                        break;
                    case ASCS_STATE_QOS_CONFIGURED:
                        if (app_state == APP_CIG_CREATED){
                            // set metadata: streaming audio context = audo context unspecified
                            test_ascs_metadata.metadata_mask |= 1 << LE_AUDIO_METADATA_TYPE_STREAMING_AUDIO_CONTEXTS;
                            test_ascs_metadata.streaming_audio_contexts_mask = LE_AUDIO_CONTEXT_MASK_UNSPECIFIED;
                            server->server_state = SERVER_ASCS_QOS_CONFIGURED;
                            if (servers_in_state(SERVER_ASCS_QOS_CONFIGURED)){
                                servers_set_state(SERVER_ASCS_W2_ENABLE);
                            }
                        }
                        break;
                    case ASCS_STATE_ENABLING:
                        if (server->server_state == SERVER_ASCS_W4_ENABLING){
                            server->server_state = SERVER_ASCS_ENABLING;
                            if (servers_in_state(SERVER_ASCS_ENABLING)){
                                // all servers are in state enabling, create all cis
                                printf("Create all CIS\n");
                                for (i=0;i<cig_params.num_cis;i++){
                                    acl_con_handles[i] = servers[i].acl_con_handle;
                                }
                                status = gap_cis_create(cig_id, cis_con_handles, acl_con_handles);
                                btstack_assert(status == ERROR_CODE_SUCCESS);
                            }
                        }
                        break;
                    case ASCS_STATE_STREAMING:
                        if (server->server_state == SERVER_ASCS_ENABLING){
                            server->server_state = SERVER_ASCS_STREAMING;
                        }
                        try_transition_to_streaming();
                        break;
                    default:
                        break;
                }
            }
            break;

        default:
            break;
    }

    app_run();
}

static uint16_t att_read_callback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
    UNUSED(connection_handle);
    printf("ATT Read, handle %04x\n", att_handle);
    return 0;
}

static int att_write_callback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
    UNUSED(transaction_mode);
    UNUSED(offset);
    UNUSED(buffer_size);
    server_t * server = server_for_acl_con_handle(connection_handle);
    if (server != NULL){
        printf("ATT Write Client %u, handle %04x\n", server->server_id, att_handle);
    }
    return 0;
}

static void show_usage(void){
    printf("\n--- LE Audio Unicast Source Test Console ---\n");
    print_config();
    printf("---\n");
    printf("f - next sampling frequency\n");
    printf("v - next codec variant\n");
    printf("x - toggle sine / modplayer\n");
    printf("s - start scanning for headphones\n");
    printf("q - disconnect\n");
    printf("---\n");
}

static void stdin_process(char c){
    switch (c){
        case 'f':
            if (app_state != APP_IDLE){
                printf("Codec configuration can only be changed in idle state\n");
                break;
            }
            menu_sampling_frequency++;
            if (menu_sampling_frequency >= 6){
                menu_sampling_frequency = 0;
            }
            if (menu_variant >= codec_configurations[menu_sampling_frequency].num_variants){
                menu_variant = 0;
            }
            print_config();
            break;
        case 'v':
            if (app_state != APP_IDLE){
                printf("Codec configuration can only be changed in idle state\n");
                break;
            }
            menu_variant++;
            if (menu_variant >= codec_configurations[menu_sampling_frequency].num_variants){
                menu_variant = 0;
            }
            print_config();
            break;
        case 's':
            if (app_state != APP_IDLE){
                printf("Cannot start scanning - not in idle state\n");
                break;
            }
            num_active_servers = 0;
            cig_id = CIG_ID_INVALID;
            app_state = APP_W4_UNICAST_SINK_ADV;
            // start scanning
            gap_start_scan();
            break;
        case 'x':
            switch (audio_source){
                case AUDIO_SOURCE_MODPLAYER:
                    audio_source = AUDIO_SOURCE_SINE;
                    break;
                case AUDIO_SOURCE_SINE:
                    audio_source = AUDIO_SOURCE_MODPLAYER;
                    break;
                default:
                    btstack_unreachable();
                    break;
            }
            print_config();
            break;
        case 'q':
            if (cig_id != CIG_ID_INVALID){
                gap_cig_remove(cig_id);
            }
            servers_set_state(SERVER_DISCONNECT);
            app_run();
            break;
        case '\n':
        case '\r':
            break;
        default:
            show_usage();
            break;
    }
}

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    (void) argv;
    (void) argc;

    l2cap_init();

    le_device_db_init();
    sm_init();

    gatt_client_init();

    // SM
    sm_set_authentication_requirements(SM_AUTHREQ_SECURE_CONNECTION | SM_AUTHREQ_BONDING);
    sm_set_io_capabilities(IO_CAPABILITY_KEYBOARD_DISPLAY);

    // GATT Server
    att_db_util_init();
    att_server_init(profile_data, att_read_callback, att_write_callback);

    // register for HCI events
    hci_event_callback_registration.callback = &hci_packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // register for SM events
    sm_event_callback_registration.callback = &sm_packet_handler;
    sm_add_event_handler(&sm_event_callback_registration);

    // LE Audio Service Clients
    audio_stream_control_service_client_init(&ascs_client_event_handler);
    coordinated_set_identification_service_client_init(&csis_client_event_handler);
    published_audio_capabilities_service_client_init(&pacs_client_event_handler);

    // setup audio processing
    le_audio_demo_util_source_init();

    // init servers
    uint8_t i;
    for (i=0;i<MAX_NUM_SERVERS;i++){
        servers[i].server_id = i;
    }

    // default config
    menu_sampling_frequency = 5;
    menu_variant = 3;

    // turn on!
    hci_power_control(HCI_POWER_ON);

    btstack_stdin_setup(stdin_process);
    return 0;
}