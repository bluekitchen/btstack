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

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <btstack_debug.h>

#include "bluetooth_data_types.h"
#include "bluetooth_company_id.h"
#include "btstack_stdin.h"
#include "btstack_event.h"
#include "gap.h"
#include "hci.h"
#include "btstack_lc3.h"
#include "btstack_lc3_google.h"
#include "le_audio_demo_util_source.h"
#include "le_audio_demo_util_sink.h"

// max config
#define MAX_CHANNELS 2
#define MAX_NUM_CIS 1


static uint8_t adv_data[] = {
        // Manufacturer Specific Data to indicate codec
        9,
        BLUETOOTH_DATA_TYPE_MANUFACTURER_SPECIFIC_DATA,
        BLUETOOTH_COMPANY_ID_BLUEKITCHEN_GMBH & 0xff,
        BLUETOOTH_COMPANY_ID_BLUEKITCHEN_GMBH >> 8,
        0, // subtype: LE Audio Connection Source
        0, // flags
        1, // num bis
        8, // sampling frequency in khz
        0, // frame duration
        26, // octets per frame
        // name
        7, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, 'S', 'o', 'u', 'r', 'c', 'e'
};

static bd_addr_t remote;

static btstack_packet_callback_registration_t hci_event_callback_registration;

static unsigned int     next_cis_index;
static hci_con_handle_t cis_con_handles[MAX_NUM_CIS];
static bool cis_established[MAX_NUM_CIS];
static uint8_t iso_frame_counter;
static uint8_t num_cis;

// time stamping
#ifdef COUNT_MODE
#define MAX_PACKET_INTERVAL_BINS_MS 50
static uint32_t send_time_bins[MAX_PACKET_INTERVAL_BINS_MS];
static uint32_t send_last_ms;
#endif

// lc3 codec config
static uint32_t sampling_frequency_hz;
static btstack_lc3_frame_duration_t frame_duration;
static uint16_t number_samples_per_frame;
static uint16_t octets_per_frame;
static uint8_t  num_channels = 1;

// codec menu
static uint8_t menu_sampling_frequency;
static uint8_t menu_variant;

// audio producer
static le_audio_demo_source_generator audio_source = AUDIO_SOURCE_MODPLAYER;

static enum {
    APP_W4_WORKING,
    APP_IDLE,
    APP_W4_CIS_COMPLETE,
    APP_STREAMING,
} app_state = APP_W4_WORKING;

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
    printf("Config '%s_%u': %u, %s ms, %u octets - %s\n",
           codec_configurations[menu_sampling_frequency].variants[menu_variant].name,
           num_channels,
           codec_configurations[menu_sampling_frequency].samplingrate_hz,
           codec_configurations[menu_sampling_frequency].variants[menu_variant].frame_duration == BTSTACK_LC3_FRAME_DURATION_7500US ? "7.5" : "10",
           codec_configurations[menu_sampling_frequency].variants[menu_variant].octets_per_frame,
           audio_source == AUDIO_SOURCE_SINE ? "Sine" : "Modplayer");
}

static void start_unicast() {
    // use values from table
    sampling_frequency_hz = codec_configurations[menu_sampling_frequency].samplingrate_hz;
    octets_per_frame      = codec_configurations[menu_sampling_frequency].variants[menu_variant].octets_per_frame;
    frame_duration        = codec_configurations[menu_sampling_frequency].variants[menu_variant].frame_duration;

    le_audio_demo_util_source_configure(1, num_channels, sampling_frequency_hz, frame_duration, octets_per_frame);
    le_audio_demo_util_source_generate_iso_frame(audio_source);

    // update adv / BASE
    adv_data[4] = 0; // subtype
    adv_data[5] = 0; // flags
    adv_data[6] = num_channels;
    adv_data[7] = sampling_frequency_hz / 1000;
    adv_data[8] = frame_duration == BTSTACK_LC3_FRAME_DURATION_7500US ? 0 : 1;
    adv_data[9] = octets_per_frame;

    // setup advertisements
    uint16_t adv_int_min = 0x0030;
    uint16_t adv_int_max = 0x0030;
    uint8_t adv_type = 0;
    bd_addr_t null_addr;
    memset(null_addr, 0, 6);
    gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);
    gap_advertisements_set_data(sizeof(adv_data), adv_data);
    gap_advertisements_enable(1);
    num_cis = 1;
    app_state = APP_W4_CIS_COMPLETE;
}

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    if (packet_type != HCI_EVENT_PACKET) return;

    hci_con_handle_t cis_con_handle;
    uint8_t i;

    switch (packet[0]) {
        case BTSTACK_EVENT_STATE:
            switch(btstack_event_state_get_state(packet)) {
                case HCI_STATE_WORKING:
                    app_state = APP_IDLE;
#ifdef ENABLE_DEMO_MODE
                    // start unicast automatically, mod player, 48_5_2
                    num_channels = 2;
                    menu_sampling_frequency = 5;
                    menu_variant = 4;
                    start_unicast();
#else
                    show_usage();
                    printf("Please select sample frequency and variation, then start advertising\n");
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
            cis_con_handle = hci_event_disconnection_complete_get_connection_handle(packet);
            for (i=0; i < num_cis; i++){
                if (cis_con_handle == cis_con_handles[i]){
                    le_audio_demo_util_sink_close();
                }
            }
            break;
        case HCI_EVENT_LE_META:
            switch(hci_event_le_meta_get_subevent_code(packet)){
                case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
                    next_cis_index = 0;
                    break;
                case HCI_SUBEVENT_LE_CIS_REQUEST:
                    cis_con_handles[next_cis_index] = hci_subevent_le_cis_request_get_cis_connection_handle(packet);
                    gap_cis_accept(cis_con_handles[next_cis_index]);
                    next_cis_index++;
                    break;
                default:
                    break;
            }
            break;
        case HCI_EVENT_META_GAP:
            switch (hci_event_gap_meta_get_subevent_code(packet)) {
                case GAP_SUBEVENT_CIS_CREATED: {
                    cis_con_handle = gap_subevent_cis_created_get_cis_con_handle(packet);
                    for (i=0; i < num_cis; i++){
                        if (cis_con_handle == cis_con_handles[i]){
                            cis_established[i] = true;
                        }
                    }
                    // check for complete
                    bool complete = true;
                    for (i=0; i < num_cis; i++) {
                        complete &= cis_established[i];
                    }
                    // ready to send
                    if (complete) {
                        printf("All CIS Established and ISO Path setup\n");

                        // init sink
                        uint16_t iso_interval_1250us = gap_subevent_cis_created_get_iso_interval_1250us(packet);
                        uint8_t  flush_timeout       = gap_subevent_cis_created_get_flush_timeout_c_to_p(packet);
                        le_audio_demo_util_sink_configure_unicast(1, 1, sampling_frequency_hz, frame_duration,
                                                                  octets_per_frame,
                                                                  iso_interval_1250us, flush_timeout);

                        next_cis_index = 0;
                        app_state = APP_STREAMING;

                        hci_request_cis_can_send_now_events(cis_con_handles[0]);
                    }
                    break;
                }
            }
            break;
        case HCI_EVENT_CIS_CAN_SEND_NOW:
            cis_con_handle = hci_event_cis_can_send_now_get_cis_con_handle(packet);
            for (i=0;i<num_cis;i++){
                if (cis_con_handle == cis_con_handles[i]){
                    // allow to send
                    le_audio_demo_util_source_send(i, cis_con_handle);
                    le_audio_demo_util_source_generate_iso_frame(audio_source);
                    hci_request_cis_can_send_now_events(cis_con_handle);
                }
            }
            break;

        default:
            break;
    }
}

static void iso_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    le_audio_demo_util_sink_receive(0, packet, size);
}

static void show_usage(void){
    printf("\n--- LE Audio Unicast Source Test Console ---\n");
    print_config();
    printf("---\n");
    printf("c - toggle channels\n");
    printf("f - next sampling frequency\n");
    printf("v - next codec variant\n");
    printf("t - toggle sine / modplayer\n");
    printf("s - start advertising\n");
    printf("x - shutdown\n");
    printf("---\n");
}

static void stdin_process(char c){
    switch (c){
        case 'c':
            if (app_state != APP_IDLE){
                printf("Codec configuration can only be changed in idle state\n");
                break;
            }
            num_channels = 3 - num_channels;
            print_config();
            break;
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
                printf("Cannot start advertising - not in idle state\n");
                break;
            }
            start_unicast();

            break;
        case 't':
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
    le_audio_demo_util_sink_init("le_audio_unicast_source.wav");
    le_audio_demo_util_source_init();

    // turn on!
    hci_power_control(HCI_POWER_ON);

    btstack_stdin_setup(stdin_process);
    return 0;
}
