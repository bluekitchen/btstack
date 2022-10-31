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
#include "btstack_run_loop.h"
#include "gap.h"
#include "hci.h"
#include "hci_cmd.h"
#include "hci_dump.h"
#include "btstack_lc3.h"
#include "btstack_lc3_google.h"

#include "hxcmod.h"
#include "mods/mod.h"

// max config
#define MAX_CHANNELS 2
#define MAX_NUM_CIS 1
#define MAX_SAMPLES_PER_FRAME 480
#define MAX_LC3_FRAME_BYTES   155


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

// input signal: pre-computed int16 sine wave, 96000 Hz at 300 Hz
static const int16_t sine_int16[] = {
        0,    643,   1286,   1929,   2571,   3212,   3851,   4489,   5126,   5760,
        6393,   7022,   7649,   8273,   8894,   9512,  10126,  10735,  11341,  11943,
        12539,  13131,  13718,  14300,  14876,  15446,  16011,  16569,  17121,  17666,
        18204,  18736,  19260,  19777,  20286,  20787,  21280,  21766,  22242,  22710,
        23170,  23620,  24062,  24494,  24916,  25329,  25732,  26126,  26509,  26882,
        27245,  27597,  27938,  28269,  28589,  28898,  29196,  29482,  29757,  30021,
        30273,  30513,  30742,  30958,  31163,  31356,  31537,  31705,  31862,  32006,
        32137,  32257,  32364,  32458,  32540,  32609,  32666,  32710,  32742,  32761,
        32767,  32761,  32742,  32710,  32666,  32609,  32540,  32458,  32364,  32257,
        32137,  32006,  31862,  31705,  31537,  31356,  31163,  30958,  30742,  30513,
        30273,  30021,  29757,  29482,  29196,  28898,  28589,  28269,  27938,  27597,
        27245,  26882,  26509,  26126,  25732,  25329,  24916,  24494,  24062,  23620,
        23170,  22710,  22242,  21766,  21280,  20787,  20286,  19777,  19260,  18736,
        18204,  17666,  17121,  16569,  16011,  15446,  14876,  14300,  13718,  13131,
        12539,  11943,  11341,  10735,  10126,   9512,   8894,   8273,   7649,   7022,
        6393,   5760,   5126,   4489,   3851,   3212,   2571,   1929,   1286,    643,
        0,   -643,  -1286,  -1929,  -2571,  -3212,  -3851,  -4489,  -5126,  -5760,
        -6393,  -7022,  -7649,  -8273,  -8894,  -9512, -10126, -10735, -11341, -11943,
        -12539, -13131, -13718, -14300, -14876, -15446, -16011, -16569, -17121, -17666,
        -18204, -18736, -19260, -19777, -20286, -20787, -21280, -21766, -22242, -22710,
        -23170, -23620, -24062, -24494, -24916, -25329, -25732, -26126, -26509, -26882,
        -27245, -27597, -27938, -28269, -28589, -28898, -29196, -29482, -29757, -30021,
        -30273, -30513, -30742, -30958, -31163, -31356, -31537, -31705, -31862, -32006,
        -32137, -32257, -32364, -32458, -32540, -32609, -32666, -32710, -32742, -32761,
        -32767, -32761, -32742, -32710, -32666, -32609, -32540, -32458, -32364, -32257,
        -32137, -32006, -31862, -31705, -31537, -31356, -31163, -30958, -30742, -30513,
        -30273, -30021, -29757, -29482, -29196, -28898, -28589, -28269, -27938, -27597,
        -27245, -26882, -26509, -26126, -25732, -25329, -24916, -24494, -24062, -23620,
        -23170, -22710, -22242, -21766, -21280, -20787, -20286, -19777, -19260, -18736,
        -18204, -17666, -17121, -16569, -16011, -15446, -14876, -14300, -13718, -13131,
        -12539, -11943, -11341, -10735, -10126,  -9512,  -8894,  -8273,  -7649,  -7022,
        -6393,  -5760,  -5126,  -4489,  -3851,  -3212,  -2571,  -1929,  -1286,   -643,
};

static bd_addr_t remote;
static hci_con_handle_t remote_handle;

static btstack_packet_callback_registration_t hci_event_callback_registration;

static unsigned int     next_cis_index;
static hci_con_handle_t cis_con_handles[MAX_NUM_CIS];
static uint16_t packet_sequence_numbers[MAX_NUM_CIS];
static uint8_t framed_pdus;
static bool cis_established[MAX_NUM_CIS];
static uint8_t iso_frame_counter;
static uint16_t frame_duration_us;
static uint8_t num_cis;
static uint8_t iso_payload[MAX_CHANNELS * MAX_LC3_FRAME_BYTES];

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

// lc3 encoder
static const btstack_lc3_encoder_t * lc3_encoder;
static btstack_lc3_encoder_google_t encoder_contexts[MAX_CHANNELS];
static int16_t pcm[MAX_CHANNELS * MAX_SAMPLES_PER_FRAME];
static uint32_t time_generation_ms;

// codec menu
static uint8_t menu_sampling_frequency;
static uint8_t menu_variant;

// mod player
static int hxcmod_initialized;
static modcontext mod_context;
static tracker_buffer_state trkbuf;

// sine generator
static uint8_t  sine_step;
static uint16_t sine_phases[MAX_CHANNELS];

// audio producer
static enum {
    AUDIO_SOURCE_SINE,
    AUDIO_SOURCE_MODPLAYER
} audio_source = AUDIO_SOURCE_MODPLAYER;

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

static void setup_lc3_encoder(void){
    uint8_t channel;
    for (channel = 0 ; channel < num_channels ; channel++){
        btstack_lc3_encoder_google_t * context = &encoder_contexts[channel];
        lc3_encoder = btstack_lc3_encoder_google_init_instance(context);
        lc3_encoder->configure(context, sampling_frequency_hz, frame_duration, octets_per_frame);
    }
    number_samples_per_frame = btstack_lc3_samples_per_frame(sampling_frequency_hz, frame_duration);
    btstack_assert(number_samples_per_frame <= MAX_SAMPLES_PER_FRAME);
    printf("LC3 Encoder config: %u hz, frame duration %s ms, num samples %u, num octets %u\n",
           sampling_frequency_hz, frame_duration == BTSTACK_LC3_FRAME_DURATION_7500US ? "7.5" : "10",
           number_samples_per_frame, octets_per_frame);
}

static void setup_mod_player(void){
    if (!hxcmod_initialized) {
        hxcmod_initialized = hxcmod_init(&mod_context);
        btstack_assert(hxcmod_initialized != 0);
    }
    hxcmod_unload(&mod_context);
    hxcmod_setcfg(&mod_context, sampling_frequency_hz, 16, 1, 1, 1);
    hxcmod_load(&mod_context, (void *) &mod_data, mod_len);
}

static void generate_audio(void){
    uint32_t start_ms = btstack_run_loop_get_time_ms();
    uint16_t sample;
    switch (audio_source) {
        case AUDIO_SOURCE_SINE:
            // generate sine wave for all channels
            for (sample = 0 ; sample < number_samples_per_frame ; sample++){
                uint8_t channel;
                for (channel = 0; channel < num_channels; channel++) {
                    int16_t value = sine_int16[sine_phases[channel]] / 4;
                    pcm[sample * num_channels + channel] = value;
                    sine_phases[channel] += sine_step * (1+channel);    // second channel, double frequency
                    if (sine_phases[channel] >= (sizeof(sine_int16) / sizeof(int16_t))) {
                        sine_phases[channel] = 0;
                    }
                }
            }
            break;
        case AUDIO_SOURCE_MODPLAYER:
            // mod player configured for stereo
            hxcmod_fillbuffer(&mod_context, (unsigned short *) pcm, number_samples_per_frame, &trkbuf);
            if (num_channels == 1) {
                // stereo -> mono
                uint16_t i;
                for (i=0;i<number_samples_per_frame;i++){
                    pcm[i] = (pcm[2*i] / 2) + (pcm[2*i+1] / 2);
                }
            }
            break;
        default:
            btstack_unreachable();
            break;
    }
    time_generation_ms = btstack_run_loop_get_time_ms() - start_ms;
    iso_frame_counter++;
}

static void encode(uint8_t channel){
    // encode as lc3
    lc3_encoder->encode_signed_16(&encoder_contexts[channel], &pcm[channel], num_channels, &iso_payload[channel * MAX_LC3_FRAME_BYTES]);
}

static void send_iso_packet(uint8_t cis_index){

#ifdef COUNT_MODE
    if (bis_index == 0) {
        uint32_t now = btstack_run_loop_get_time_ms();
        if (send_last_ms != 0) {
            uint16_t send_interval_ms = now - send_last_ms;
            if (send_interval_ms >= MAX_PACKET_INTERVAL_BINS_MS) {
                printf("ERROR: send interval %u\n", send_interval_ms);
            } else {
                send_time_bins[send_interval_ms]++;
            }
        }
        send_last_ms = now;
    }
#endif
    bool ok = hci_reserve_packet_buffer();
    btstack_assert(ok);
    uint8_t * buffer = hci_get_outgoing_packet_buffer();
    // complete SDU, no TimeStamp
    little_endian_store_16(buffer, 0, cis_con_handles[cis_index] | (2 << 12));
    // len
    little_endian_store_16(buffer, 2, 0 + 4 + num_channels * octets_per_frame);
    // TimeStamp if TS flag is set
    // packet seq nr
    little_endian_store_16(buffer, 4, packet_sequence_numbers[cis_index]);
    // iso sdu len
    little_endian_store_16(buffer, 6, num_channels * octets_per_frame);
#ifdef COUNT_MODE
    // test data: bis_index, counter
    buffer[8] = bis_index;
    memset(&buffer[9], iso_frame_counter, num_channels * octets_per_frame - 1);
#else
    // copy encoded payload
    uint8_t i;
    uint16_t offset = 8;
    for (i=0; i<num_channels;i++) {
        memcpy(&buffer[offset], &iso_payload[i * MAX_LC3_FRAME_BYTES], octets_per_frame);
        offset += octets_per_frame;
    }
#endif

    // send
    hci_send_iso_packet_buffer(offset);

    if (((packet_sequence_numbers[cis_index] & 0x7f) == 0) && (cis_index == 0)) {
        printf("Encoding time: %u\n", time_generation_ms);
    }

    packet_sequence_numbers[cis_index]++;
}

static void generate_audio_and_encode(uint8_t cis_index){
    generate_audio();
    uint8_t i;
    for (i=0; i<num_channels;i++) {
        encode(i);
    }
}

static void start_unicast() {// use values from table
    sampling_frequency_hz = codec_configurations[menu_sampling_frequency].samplingrate_hz;
    octets_per_frame      = codec_configurations[menu_sampling_frequency].variants[menu_variant].octets_per_frame;
    frame_duration        = codec_configurations[menu_sampling_frequency].variants[menu_variant].frame_duration;

    // get num samples per frame
    setup_lc3_encoder();

    // setup mod player
    setup_mod_player();

    // setup sine generator
    if (sampling_frequency_hz == 44100){
        sine_step = 2;
    } else {
        sine_step = 96000 / sampling_frequency_hz;
    }

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

    bd_addr_t event_addr;
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
                        next_cis_index = 0;
                        app_state = APP_STREAMING;
                        generate_audio_and_encode(0);
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
                    send_iso_packet(i);
                    generate_audio_and_encode(0);
                    hci_request_cis_can_send_now_events(cis_con_handles[0]);
                }
            }
            break;

        default:
            break;
    }
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
            audio_source = 1 - audio_source;
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

    // turn on!
    hci_power_control(HCI_POWER_ON);

    btstack_stdin_setup(stdin_process);
    return 0;
}
