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

#define BTSTACK_FILE__ "le_audio_unicast_sink.c"

/*
 * LE Audio Unicast Sink
 * Until GATT Services are available, we encode LC3 config in advertising
 */


#include "btstack_config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ad_parser.h"
#include "bluetooth_data_types.h"
#include "bluetooth_company_id.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_stdin.h"
#include "gap.h"
#include "hci.h"
#include "le_audio_demo_util_sink.h"
#include "le_audio_demo_util_source.h"

// max config
#define MAX_CHANNELS 2
#define MAX_SAMPLES_PER_FRAME 480

static void show_usage(void);

static enum {
    APP_W4_WORKING,
    APP_W4_SOURCE_ADV,
    APP_W4_CIG_COMPLETE,
    APP_W4_CIS_CREATED,
    APP_STREAMING,
    APP_IDLE,
} app_state = APP_W4_WORKING;

//
static btstack_packet_callback_registration_t hci_event_callback_registration;

// remote info
static char             remote_name[20];
static bd_addr_t        remote_addr;
static bd_addr_type_t   remote_type;
static hci_con_handle_t remote_handle;

static le_audio_cig_t cig;
static le_audio_cig_params_t cig_params;

// iso info
static bool framed_pdus;
static uint16_t frame_duration_us;

static uint8_t num_cis;
static hci_con_handle_t cis_con_handles[MAX_CHANNELS];
static bool cis_established[MAX_CHANNELS];

// lc3 codec config
static uint16_t sampling_frequency_hz;
static btstack_lc3_frame_duration_t frame_duration;
static uint16_t number_samples_per_frame;
static uint16_t octets_per_frame;
static uint8_t  num_channels;

// microphone
static bool microphone_enable;

static void start_scanning() {
    app_state = APP_W4_SOURCE_ADV;
    gap_set_scan_params(1, 0x30, 0x30, 0);
    gap_start_scan();
    printf("Start scan..\n");
}

static void create_cig() {
    if (sampling_frequency_hz == 44100){
        framed_pdus = 1;
        // same config as for 48k -> frame is longer by 48/44.1
        frame_duration_us = frame_duration == BTSTACK_LC3_FRAME_DURATION_7500US ? 8163 : 10884;
    } else {
        framed_pdus = 0;
        frame_duration_us = frame_duration == BTSTACK_LC3_FRAME_DURATION_7500US ? 7500 : 10000;
    }

    printf("Send: LE Set CIG Parameters\n");

    num_cis = 1;
    cig_params.cig_id = 0;
    cig_params.num_cis = 1;
    cig_params.sdu_interval_c_to_p = frame_duration_us;
    cig_params.sdu_interval_p_to_c = frame_duration_us;
    cig_params.worst_case_sca = 0; // 251 ppm to 500 ppm
    cig_params.packing = 0;        // sequential
    cig_params.framing = framed_pdus;
    cig_params.max_transport_latency_c_to_p = 40;
    cig_params.max_transport_latency_p_to_c = 40;
    uint8_t i;
    uint16_t max_sdu_c_to_p = microphone_enable ? octets_per_frame : 0;
    for (i=0; i < num_cis; i++){
        cig_params.cis_params[i].cis_id = i;
        cig_params.cis_params[i].max_sdu_c_to_p = max_sdu_c_to_p;
        cig_params.cis_params[i].max_sdu_p_to_c = num_channels * octets_per_frame;
        cig_params.cis_params[i].phy_c_to_p = 2;  // 2M
        cig_params.cis_params[i].phy_p_to_c = 2;  // 2M
        cig_params.cis_params[i].rtn_c_to_p = 2;
        cig_params.cis_params[i].rtn_p_to_c = 2;
    }

    app_state = APP_W4_CIG_COMPLETE;
    gap_cig_create(&cig, &cig_params);
}

static void enter_streaming(void){
    // init source
    if (microphone_enable){
        le_audio_demo_util_source_configure(1, 1, sampling_frequency_hz, frame_duration, octets_per_frame);
        le_audio_demo_util_source_generate_iso_frame(AUDIO_SOURCE_SINE);
    }
    // init sink
    uint16_t iso_interval_1250us = frame_duration_us / 1250;
    uint8_t  flush_timeout       = 1;
    le_audio_demo_util_sink_configure_unicast(1, num_channels, sampling_frequency_hz, frame_duration, octets_per_frame,
                                              iso_interval_1250us, flush_timeout);
    printf("Configure: %u channels, sampling rate %u, samples per frame %u\n", num_channels, sampling_frequency_hz, number_samples_per_frame);
}

static void handle_advertisement(bd_addr_type_t address_type, bd_addr_t address, uint8_t adv_size,  const uint8_t * adv_data){
    ad_context_t context;
    bool found = false;
    remote_name[0] = '\0';
    uint16_t uuid;
    uint16_t company_id;
    for (ad_iterator_init(&context, adv_size, adv_data) ; ad_iterator_has_more(&context) ; ad_iterator_next(&context)) {
        uint8_t data_type = ad_iterator_get_data_type(&context);
        uint8_t size = ad_iterator_get_data_len(&context);
        const uint8_t *data = ad_iterator_get_data(&context);
        switch (data_type){
            case BLUETOOTH_DATA_TYPE_MANUFACTURER_SPECIFIC_DATA:
                company_id = little_endian_read_16(data, 0);
                if (company_id == BLUETOOTH_COMPANY_ID_BLUEKITCHEN_GMBH){
                    // subtype = 0 -> le audio unicast source
                    uint8_t subtype = data[2];
                    if (subtype != 0) break;
                    // flags
                    uint8_t flags = data[3];
                    // num channels
                    num_channels = data[4];
                    if (num_channels > 2) break;
                    // sampling frequency
                    sampling_frequency_hz = 1000 * data[5];
                    // frame duration
                    frame_duration = data[6] == 0 ? BTSTACK_LC3_FRAME_DURATION_7500US : BTSTACK_LC3_FRAME_DURATION_10000US;
                    // octets per frame
                    octets_per_frame = data[7];
                    // done
                    found = true;
                }
                break;
            case BLUETOOTH_DATA_TYPE_SHORTENED_LOCAL_NAME:
            case BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME:
                size = btstack_min(sizeof(remote_name) - 1, size);
                memcpy(remote_name, data, size);
                remote_name[size] = 0;
                break;
            default:
                break;
        }
    }
    if (!found) return;

    memcpy(remote_addr, address, 6);
    remote_type = address_type;
    printf("Remote Unicast source found, addr %s, name: '%s'\n", bd_addr_to_str(remote_addr), remote_name);

    // stop scanning
    app_state = APP_W4_CIS_CREATED;
    gap_stop_scan();
    gap_connect(remote_addr, remote_type);
}

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    bd_addr_t event_addr;
    bd_addr_type_t event_addr_type;
    hci_con_handle_t cis_handle;
    unsigned int i;
    uint8_t status;
    if (packet_type != HCI_EVENT_PACKET) return;
    switch (packet[0]) {
        case BTSTACK_EVENT_STATE:
            switch(btstack_event_state_get_state(packet)) {
                case HCI_STATE_WORKING:
#ifdef ENABLE_DEMO_MODE
                    if (app_state != APP_W4_WORKING) break;
                    start_scanning();
#else
                    show_usage();
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
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            if (hci_event_disconnection_complete_get_connection_handle(packet) == remote_handle){
                printf("Disconnected, back to scanning\n");
                le_audio_demo_util_sink_close();
                start_scanning();
            }
            break;
        case GAP_EVENT_ADVERTISING_REPORT:
            if (app_state == APP_W4_SOURCE_ADV) {
                gap_event_advertising_report_get_address(packet, event_addr);
                event_addr_type = gap_event_advertising_report_get_address_type(packet);
                uint8_t adv_size = gap_event_advertising_report_get_data_length(packet);
                const uint8_t * adv_data = gap_event_advertising_report_get_data(packet);
                handle_advertisement(event_addr_type, event_addr, adv_size, adv_data);
            }
            break;
        case GAP_EVENT_EXTENDED_ADVERTISING_REPORT:
            if (app_state == APP_W4_SOURCE_ADV) {
                gap_event_extended_advertising_report_get_address(packet, event_addr);
                event_addr_type = gap_event_extended_advertising_report_get_address_type(packet) & 1;
                uint8_t adv_size = gap_event_extended_advertising_report_get_data_length(packet);
                const uint8_t * adv_data = gap_event_extended_advertising_report_get_data(packet);
                handle_advertisement(event_addr_type, event_addr, adv_size, adv_data);
            }
            break;
        case HCI_EVENT_META_GAP:
            switch (hci_event_gap_meta_get_subevent_code(packet)){
                case GAP_SUBEVENT_LE_CONNECTION_COMPLETE:
                    gap_subevent_le_connection_complete_get_peer_address(packet, event_addr);
                    remote_handle = gap_subevent_le_connection_complete_get_connection_handle(packet);
                    printf("Connected, remote %s, handle %04x\n", bd_addr_to_str(event_addr), remote_handle);
                    create_cig();
                    break;
                case GAP_SUBEVENT_CIG_CREATED:
                    if (app_state == APP_W4_CIG_COMPLETE){
                        printf("CIS Connection Handles: ");
                        for (i=0; i < num_cis; i++){
                            cis_con_handles[i] = gap_subevent_cig_created_get_cis_con_handles(packet, i);
                            printf("0x%04x ", cis_con_handles[i]);
                        }
                        printf("\n");
                        
                        printf("Create CIS\n");
                        hci_con_handle_t acl_connection_handles[MAX_CHANNELS];
                        for (i=0; i < num_cis; i++){
                            acl_connection_handles[i] = remote_handle;
                        }
                        gap_cis_create(cig_params.cig_id, cis_con_handles, acl_connection_handles);
                        app_state = APP_W4_CIS_CREATED;
                    }
                    break;
                case GAP_SUBEVENT_CIS_CREATED:
                    status = gap_subevent_big_created_get_status(packet);
                    cis_handle = gap_subevent_cis_created_get_cis_con_handle(packet);
                    if (status == ERROR_CODE_SUCCESS){
                        // only look for cis handle
                        for (i=0; i < num_cis; i++){
                            if (cis_handle == cis_con_handles[i]){
                                cis_established[i] = true;
                            }
                        }
                        // check for complete
                        bool complete = true;
                        for (i=0; i < num_cis; i++) {
                            complete &= cis_established[i];
                        }
                        if (complete) {
                            printf("All CIS Established\n");
                            app_state = APP_STREAMING;
                            enter_streaming();
                            // start sending
                            if (microphone_enable){
                                hci_request_cis_can_send_now_events(cis_con_handles[0]);
                            }
                        }
                    } else {
                        printf("CIS Create failed with status 0x%02x for con handle 0x%04x\n", status, cis_handle);
                    }
                    break;
                default:
                    break;
            }
            break;
        case HCI_EVENT_CIS_CAN_SEND_NOW:
            le_audio_demo_util_source_send(0, cis_con_handles[0]);
            le_audio_demo_util_source_generate_iso_frame(AUDIO_SOURCE_SINE);
            hci_request_cis_can_send_now_events(cis_con_handles[0]);
            break;
        default:
            break;
    }
}

static void iso_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    le_audio_demo_util_sink_receive(0, packet, size);
}

static void show_usage(void){
    printf("\n--- LE Audio Unicast Sink Test Console ---\n");
    printf("s - start scanning\n");
#ifdef HAVE_LC3PLUS
    printf("q - use LC3plus decoder if 10 ms ISO interval is used\n");
#endif
    printf("m - enable virtual microphone\n");
    printf("---\n");
}

static void stdin_process(char c){
    switch (c){
        case 's':
            if (app_state != APP_W4_WORKING) break;
            start_scanning();
            break;
#ifdef HAVE_LC3PLUS
        case 'q':
            printf("Use LC3 Plust for 10 ms frames\n");
            le_audio_demo_util_sink_enable_lc3plus(true);
            break;
#endif
        case 'm':
            printf("Enable virtual microphone\n");
            microphone_enable = true;
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

    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // register for ISO Packet
    hci_register_iso_packet_handler(&iso_packet_handler);

    // setup audio processing
    le_audio_demo_util_sink_init("le_audio_unicast_sink.wav");
    le_audio_demo_util_source_init();

    // turn on!
    hci_power_control(HCI_POWER_ON);

    btstack_stdin_setup(stdin_process);
    return 0;
}
