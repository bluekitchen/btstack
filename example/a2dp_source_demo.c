/*
 * Copyright (C) 2016 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "a2dp_source_demo.c"

/*
 * a2dp_source_demo.c
 */

// *****************************************************************************
/* EXAMPLE_START(a2dp_source_demo): Serve audio stream and handle remote playback control and queries.
 *
 * @text This  A2DP Source example demonstrates how to send an audio data stream 
 * to a remote A2DP Sink device and how to switch between two audio data sources.  
 * In addition, the AVRCP Target is used to answer queries on currently played media,
 * as well as to handle remote playback control, i.e. play, stop, repeat, etc.
 *
 * @test To test with a remote device, e.g. a Bluetooth speaker,
 * set the device_addr_string to the Bluetooth address of your 
 * remote device in the code, and use the UI to connect and start playback. 
 * Tap SPACE on the console to show the available commands.
 * 
 * For more info on BTstack audio, see our blog post 
 * [A2DP Sink and Source on STM32 F4 Discovery Board](http://bluekitchen-gmbh.com/a2dp-sink-and-source-on-stm32-f4-discovery-board/).
 * 
 */
// *****************************************************************************


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack.h"
#include "hxcmod.h"
#include "mods/mod.h"

// logarithmic volume reduction, samples are divided by 2^x
// #define VOLUME_REDUCTION 3
// #undef  HAVE_BTSTACK_STDIN

//#define AVRCP_BROWSING_ENABLED

#define NUM_CHANNELS                2
#define BYTES_PER_AUDIO_SAMPLE      (2*NUM_CHANNELS)
#define AUDIO_TIMEOUT_MS            10 
#define TABLE_SIZE_441HZ            100

#define SBC_STORAGE_SIZE 1030

typedef enum {
    STREAM_SINE = 0,
    STREAM_MOD,
    STREAM_PTS_TEST
} stream_data_source_t;
    
typedef struct {
    uint16_t a2dp_cid;
    uint8_t  local_seid;
    uint8_t  remote_seid;
    uint8_t  stream_opened;
    uint16_t avrcp_cid;

    uint32_t time_audio_data_sent; // ms
    uint32_t acc_num_missed_samples;
    uint32_t samples_ready;
    btstack_timer_source_t audio_timer;
    uint8_t  streaming;
    int      max_media_payload_size;
    
    uint8_t  sbc_storage[SBC_STORAGE_SIZE];
    uint16_t sbc_storage_count;
    uint8_t  sbc_ready_to_send;
} a2dp_media_sending_context_t;

static  uint8_t media_sbc_codec_capabilities[] = {
    (AVDTP_SBC_44100 << 4) | AVDTP_SBC_STEREO,
    0xFF,//(AVDTP_SBC_BLOCK_LENGTH_16 << 4) | (AVDTP_SBC_SUBBANDS_8 << 2) | AVDTP_SBC_ALLOCATION_METHOD_LOUDNESS,
    2, 53
}; 

// input signal: pre-computed int16 sine wave, 44100 Hz at 441 Hz
static const int16_t sine_int16_44100[] = {
     0,    2057,    4107,    6140,    8149,   10126,   12062,   13952,   15786,   17557,
 19260,   20886,   22431,   23886,   25247,   26509,   27666,   28714,   29648,   30466,
 31163,   31738,   32187,   32509,   32702,   32767,   32702,   32509,   32187,   31738,
 31163,   30466,   29648,   28714,   27666,   26509,   25247,   23886,   22431,   20886,
 19260,   17557,   15786,   13952,   12062,   10126,    8149,    6140,    4107,    2057,
     0,   -2057,   -4107,   -6140,   -8149,  -10126,  -12062,  -13952,  -15786,  -17557,
-19260,  -20886,  -22431,  -23886,  -25247,  -26509,  -27666,  -28714,  -29648,  -30466,
-31163,  -31738,  -32187,  -32509,  -32702,  -32767,  -32702,  -32509,  -32187,  -31738,
-31163,  -30466,  -29648,  -28714,  -27666,  -26509,  -25247,  -23886,  -22431,  -20886,
-19260,  -17557,  -15786,  -13952,  -12062,  -10126,   -8149,   -6140,   -4107,   -2057,
};

static const int num_samples_sine_int16_44100 = sizeof(sine_int16_44100) / 2;

// input signal: pre-computed int16 sine wave, 48000 Hz at 441 Hz
static const int16_t sine_int16_48000[] = {
     0,    1905,    3804,    5690,    7557,    9398,   11207,   12978,   14706,   16383,
 18006,   19567,   21062,   22486,   23834,   25101,   26283,   27376,   28377,   29282,
 30087,   30791,   31390,   31884,   32269,   32545,   32712,   32767,   32712,   32545,
 32269,   31884,   31390,   30791,   30087,   29282,   28377,   27376,   26283,   25101,
 23834,   22486,   21062,   19567,   18006,   16383,   14706,   12978,   11207,    9398,
  7557,    5690,    3804,    1905,       0,   -1905,   -3804,   -5690,   -7557,   -9398,
-11207,  -12978,  -14706,  -16384,  -18006,  -19567,  -21062,  -22486,  -23834,  -25101,
-26283,  -27376,  -28377,  -29282,  -30087,  -30791,  -31390,  -31884,  -32269,  -32545,
-32712,  -32767,  -32712,  -32545,  -32269,  -31884,  -31390,  -30791,  -30087,  -29282,
-28377,  -27376,  -26283,  -25101,  -23834,  -22486,  -21062,  -19567,  -18006,  -16384,
-14706,  -12978,  -11207,   -9398,   -7557,   -5690,   -3804,   -1905,  };

static const int num_samples_sine_int16_48000 = sizeof(sine_int16_48000) / 2;

typedef struct {
    int reconfigure;
    int num_channels;
    int sampling_frequency;
    int channel_mode;
    int block_length;
    int subbands;
    int allocation_method;
    int min_bitpool_value;
    int max_bitpool_value;
    int frames_per_buffer;
} avdtp_media_codec_configuration_sbc_t;

static btstack_packet_callback_registration_t hci_event_callback_registration;

// pts:             static const char * device_addr_string = "00:1B:DC:08:0A:A5";
// pts:             static const char * device_addr_string = "00:1B:DC:08:E2:72";
// mac 2013:        static const char * device_addr_string = "84:38:35:65:d1:15";
// phone 2013:      static const char * device_addr_string = "D8:BB:2C:DF:F0:F2";
// Minijambox:      
static const char * device_addr_string = "00:21:3C:AC:F7:38";
// Philips SHB9100: static const char * device_addr_string = "00:22:37:05:FD:E8";
// RT-B6:           static const char * device_addr_string = "00:75:58:FF:C9:7D";
// BT dongle:       static const char * device_addr_string = "00:1A:7D:DA:71:0A";
// Sony MDR-ZX330BT static const char * device_addr_string = "00:18:09:28:50:18";
// Panda (BM6)      static const char * device_addr_string = "4F:3F:66:52:8B:E0";
// BeatsX:          static const char * device_addr_string = "DC:D3:A2:89:57:FB";

static bd_addr_t device_addr;
static uint8_t sdp_a2dp_source_service_buffer[150];
static uint8_t sdp_avrcp_target_service_buffer[200];
static uint8_t sdp_avrcp_controller_service_buffer[200];

static avdtp_media_codec_configuration_sbc_t sbc_configuration;
static btstack_sbc_encoder_state_t sbc_encoder_state;

static uint8_t media_sbc_codec_configuration[4];
static a2dp_media_sending_context_t media_tracker;

static stream_data_source_t data_source;

static int sine_phase;
static int sample_rate = 44100;

static int hxcmod_initialized;
static modcontext mod_context;
static tracker_buffer_state trkbuf;

static uint16_t avrcp_controller_cid = 0;

/* AVRCP Target context START */
static const uint8_t subunit_info[] = {
    0,0,0,0,
    1,1,1,1,
    2,2,2,2,
    3,3,3,3,
    4,4,4,4,
    5,5,5,5,
    6,6,6,6,
    7,7,7,7
};

static uint32_t company_id = 0x112233;
static uint8_t companies_num = 1;
static uint8_t companies[] = {
    0x00, 0x19, 0x58 //BT SIG registered CompanyID
};

static uint8_t events_num = 13;
static uint8_t events[] = {
    AVRCP_NOTIFICATION_EVENT_PLAYBACK_STATUS_CHANGED,
    AVRCP_NOTIFICATION_EVENT_TRACK_CHANGED,
    AVRCP_NOTIFICATION_EVENT_TRACK_REACHED_END,
    AVRCP_NOTIFICATION_EVENT_TRACK_REACHED_START,
    AVRCP_NOTIFICATION_EVENT_PLAYBACK_POS_CHANGED,
    AVRCP_NOTIFICATION_EVENT_BATT_STATUS_CHANGED,
    AVRCP_NOTIFICATION_EVENT_SYSTEM_STATUS_CHANGED,
    AVRCP_NOTIFICATION_EVENT_PLAYER_APPLICATION_SETTING_CHANGED,
    AVRCP_NOTIFICATION_EVENT_NOW_PLAYING_CONTENT_CHANGED,
    AVRCP_NOTIFICATION_EVENT_AVAILABLE_PLAYERS_CHANGED,
    AVRCP_NOTIFICATION_EVENT_ADDRESSED_PLAYER_CHANGED,
    AVRCP_NOTIFICATION_EVENT_UIDS_CHANGED,
    AVRCP_NOTIFICATION_EVENT_VOLUME_CHANGED
};

typedef struct {
    uint8_t track_id[8];
    uint32_t song_length_ms;
    avrcp_playback_status_t status;
    uint32_t song_position_ms; // 0xFFFFFFFF if not supported
} avrcp_play_status_info_t;

// python -c "print('a'*512)"
static const char title[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

avrcp_track_t tracks[] = {
    {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01}, 1, "Sine", "Generated", "A2DP Source Demo", "monotone", 12345},
    {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02}, 2, "Nao-deceased", "Decease", "A2DP Source Demo", "vivid", 12345},
    {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03}, 3, (char *)title, "Decease", "A2DP Source Demo", "vivid", 12345},
};
int current_track_index;
avrcp_play_status_info_t play_info;

/* AVRCP Target context END */

/* @section Main Application Setup
 *
 * @text The Listing MainConfiguration shows how to setup AD2P Source and AVRCP Target services. 
 */

/* LISTING_START(MainConfiguration): Setup Audio Source and AVRCP Target services */
static void a2dp_source_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t * event, uint16_t event_size);
static void avrcp_target_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void avrcp_controller_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
#ifdef HAVE_BTSTACK_STDIN
static void stdin_process(char cmd);
#endif

static void a2dp_demo_reconfigure_sample_rate(int new_sample_rate){
    if (!hxcmod_initialized){
        hxcmod_initialized = hxcmod_init(&mod_context);
        if (!hxcmod_initialized) {
            printf("could not initialize hxcmod\n");
            return;
        }
    }
    sample_rate = new_sample_rate;
    media_tracker.sbc_storage_count = 0;
    media_tracker.samples_ready = 0;
    hxcmod_unload(&mod_context);
    hxcmod_setcfg(&mod_context, sample_rate, 16, 1, 1, 1);
    hxcmod_load(&mod_context, (void *) &mod_data, mod_len);
}

static int a2dp_source_and_avrcp_services_init(void){

    // request role change on reconnecting headset to always use them in slave mode
    hci_set_master_slave_policy(0);

    l2cap_init();
    // Initialize  A2DP Source.
    a2dp_source_init();
    a2dp_source_register_packet_handler(&a2dp_source_packet_handler);

    // Create stream endpoint.
    avdtp_stream_endpoint_t * local_stream_endpoint = a2dp_source_create_stream_endpoint(AVDTP_AUDIO, AVDTP_CODEC_SBC, media_sbc_codec_capabilities, sizeof(media_sbc_codec_capabilities), media_sbc_codec_configuration, sizeof(media_sbc_codec_configuration));
    if (!local_stream_endpoint){
        printf("A2DP Source: not enough memory to create local stream endpoint\n");
        return 1;
    }
    media_tracker.local_seid = avdtp_local_seid(local_stream_endpoint);
    avdtp_source_register_delay_reporting_category(media_tracker.local_seid);

    // Initialize AVRCP Target.
    avrcp_target_init();
    avrcp_target_register_packet_handler(&avrcp_target_packet_handler);
    // Initialize AVRCP Controller
    avrcp_controller_init();
    avrcp_controller_register_packet_handler(&avrcp_controller_packet_handler);

    // Initialize SDP, 
    sdp_init();
    
    // Create  A2DP Source service record and register it with SDP.
    memset(sdp_a2dp_source_service_buffer, 0, sizeof(sdp_a2dp_source_service_buffer));
    a2dp_source_create_sdp_record(sdp_a2dp_source_service_buffer, 0x10002, 1, NULL, NULL);
    sdp_register_service(sdp_a2dp_source_service_buffer);
    
    // Create AVRCP target service record and register it with SDP.
    memset(sdp_avrcp_target_service_buffer, 0, sizeof(sdp_avrcp_target_service_buffer));
    uint16_t supported_features = (1 << AVRCP_TARGET_SUPPORTED_FEATURE_CATEGORY_PLAYER_OR_RECORDER);
#ifdef AVRCP_BROWSING_ENABLED
    supported_features |= (1 << AVRCP_TARGET_SUPPORTED_FEATURE_BROWSING);
#endif
    avrcp_target_create_sdp_record(sdp_avrcp_target_service_buffer, 0x10001, supported_features, NULL, NULL);
    sdp_register_service(sdp_avrcp_target_service_buffer);

    // setup AVRCP Controller
    memset(sdp_avrcp_controller_service_buffer, 0, sizeof(sdp_avrcp_controller_service_buffer));
    uint16_t controller_supported_features = (1 << AVRCP_CONTROLLER_SUPPORTED_FEATURE_CATEGORY_MONITOR_OR_AMPLIFIER);
    avrcp_controller_create_sdp_record(sdp_avrcp_controller_service_buffer, 0x10002, controller_supported_features, NULL, NULL);
    sdp_register_service(sdp_avrcp_controller_service_buffer);

    // Set local name with a template Bluetooth address, that will be automatically
    // replaced with a actual address once it is available, i.e. when BTstack boots
    // up and starts talking to a Bluetooth module.
    gap_set_local_name("A2DP Source 00:00:00:00:00:00");
    gap_discoverable_control(1);
    gap_set_class_of_device(0x200408);
    
    // Register for HCI events.
    hci_event_callback_registration.callback = &a2dp_source_packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    a2dp_demo_reconfigure_sample_rate(sample_rate);
    
    // Parse human readable Bluetooth address.
    sscanf_bd_addr(device_addr_string, device_addr);

#ifdef HAVE_BTSTACK_STDIN
    btstack_stdin_setup(stdin_process);
#endif
    return 0;
}
/* LISTING_END */

static void a2dp_demo_send_media_packet(void){
    int num_bytes_in_frame = btstack_sbc_encoder_sbc_buffer_length();
    int bytes_in_storage = media_tracker.sbc_storage_count;
    uint8_t num_frames = bytes_in_storage / num_bytes_in_frame;
    a2dp_source_stream_send_media_payload(media_tracker.a2dp_cid, media_tracker.local_seid, media_tracker.sbc_storage, bytes_in_storage, num_frames, 0);
    media_tracker.sbc_storage_count = 0;
    media_tracker.sbc_ready_to_send = 0;
}

static void produce_sine_audio(int16_t * pcm_buffer, int num_samples_to_write){
    int count;
    for (count = 0; count < num_samples_to_write ; count++){
        switch (sample_rate){
            case 44100:
                pcm_buffer[count * 2]     = sine_int16_44100[sine_phase];
                pcm_buffer[count * 2 + 1] = sine_int16_44100[sine_phase];
                sine_phase++;
                if (sine_phase >= num_samples_sine_int16_44100){
                    sine_phase -= num_samples_sine_int16_44100;
                }
                break;
            case 48000:
                pcm_buffer[count * 2]     = sine_int16_48000[sine_phase];
                pcm_buffer[count * 2 + 1] = sine_int16_48000[sine_phase];
                sine_phase++;
                if (sine_phase >= num_samples_sine_int16_48000){
                    sine_phase -= num_samples_sine_int16_48000;
                }
                break;
            default:
                break;
        }
        
        
    }
}

static void produce_mod_audio(int16_t * pcm_buffer, int num_samples_to_write){
    hxcmod_fillbuffer(&mod_context, (unsigned short *) &pcm_buffer[0], num_samples_to_write, &trkbuf);
}

static void produce_audio(int16_t * pcm_buffer, int num_samples){
    switch (data_source){
        case STREAM_SINE:
            produce_sine_audio(pcm_buffer, num_samples);
            break;
        case STREAM_MOD:
            produce_mod_audio(pcm_buffer, num_samples);
            break;
        default:
            break;
    }    
#ifdef VOLUME_REDUCTION
    int i;
    for (i=0;i<num_samples*2;i++){
        if (pcm_buffer[i] > 0){
            pcm_buffer[i] =     pcm_buffer[i]  >> VOLUME_REDUCTION;
        } else {
            pcm_buffer[i] = -((-pcm_buffer[i]) >> VOLUME_REDUCTION);
        }
    }
#endif
}

static int a2dp_demo_fill_sbc_audio_buffer(a2dp_media_sending_context_t * context){
    // perform sbc encodin
    int total_num_bytes_read = 0;
    unsigned int num_audio_samples_per_sbc_buffer = btstack_sbc_encoder_num_audio_frames();
    while (context->samples_ready >= num_audio_samples_per_sbc_buffer
        && (context->max_media_payload_size - context->sbc_storage_count) >= btstack_sbc_encoder_sbc_buffer_length()){

        int16_t pcm_frame[256*NUM_CHANNELS];

        produce_audio(pcm_frame, num_audio_samples_per_sbc_buffer);
        btstack_sbc_encoder_process_data(pcm_frame);
        
        uint16_t sbc_frame_size = btstack_sbc_encoder_sbc_buffer_length(); 
        uint8_t * sbc_frame = btstack_sbc_encoder_sbc_buffer();
        
        total_num_bytes_read += num_audio_samples_per_sbc_buffer;
        memcpy(&context->sbc_storage[context->sbc_storage_count], sbc_frame, sbc_frame_size);
        context->sbc_storage_count += sbc_frame_size;
        context->samples_ready -= num_audio_samples_per_sbc_buffer;
    }
    return total_num_bytes_read;
}

static void a2dp_demo_audio_timeout_handler(btstack_timer_source_t * timer){
    a2dp_media_sending_context_t * context = (a2dp_media_sending_context_t *) btstack_run_loop_get_timer_context(timer);
    btstack_run_loop_set_timer(&context->audio_timer, AUDIO_TIMEOUT_MS); 
    btstack_run_loop_add_timer(&context->audio_timer);
    uint32_t now = btstack_run_loop_get_time_ms();

    uint32_t update_period_ms = AUDIO_TIMEOUT_MS;
    if (context->time_audio_data_sent > 0){
        update_period_ms = now - context->time_audio_data_sent;
    } 

    uint32_t num_samples = (update_period_ms * sample_rate) / 1000;
    context->acc_num_missed_samples += (update_period_ms * sample_rate) % 1000;
    
    while (context->acc_num_missed_samples >= 1000){
        num_samples++;
        context->acc_num_missed_samples -= 1000;
    }
    context->time_audio_data_sent = now;
    context->samples_ready += num_samples;

    if (context->sbc_ready_to_send) return;

    a2dp_demo_fill_sbc_audio_buffer(context);

    if ((context->sbc_storage_count + btstack_sbc_encoder_sbc_buffer_length()) > context->max_media_payload_size){
        // schedule sending
        context->sbc_ready_to_send = 1;
        a2dp_source_stream_endpoint_request_can_send_now(context->a2dp_cid, context->local_seid);
    }
}

static void a2dp_demo_timer_start(a2dp_media_sending_context_t * context){
    context->max_media_payload_size = btstack_min(a2dp_max_media_payload_size(context->a2dp_cid, context->local_seid), SBC_STORAGE_SIZE);
    context->sbc_storage_count = 0;
    context->sbc_ready_to_send = 0;
    context->streaming = 1;
    btstack_run_loop_remove_timer(&context->audio_timer);
    btstack_run_loop_set_timer_handler(&context->audio_timer, a2dp_demo_audio_timeout_handler);
    btstack_run_loop_set_timer_context(&context->audio_timer, context);
    btstack_run_loop_set_timer(&context->audio_timer, AUDIO_TIMEOUT_MS); 
    btstack_run_loop_add_timer(&context->audio_timer);
}

static void a2dp_demo_timer_stop(a2dp_media_sending_context_t * context){
    context->time_audio_data_sent = 0;
    context->acc_num_missed_samples = 0;
    context->samples_ready = 0;
    context->streaming = 1;
    context->sbc_storage_count = 0;
    context->sbc_ready_to_send = 0;
    btstack_run_loop_remove_timer(&context->audio_timer);
} 

static void dump_sbc_configuration(avdtp_media_codec_configuration_sbc_t * configuration){
    printf("Received media codec configuration:\n");
    printf("    - num_channels: %d\n", configuration->num_channels);
    printf("    - sampling_frequency: %d\n", configuration->sampling_frequency);
    printf("    - channel_mode: %d\n", configuration->channel_mode);
    printf("    - block_length: %d\n", configuration->block_length);
    printf("    - subbands: %d\n", configuration->subbands);
    printf("    - allocation_method: %d\n", configuration->allocation_method);
    printf("    - bitpool_value [%d, %d] \n", configuration->min_bitpool_value, configuration->max_bitpool_value);
}

static void a2dp_source_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    uint8_t status;
    uint8_t local_seid;
    bd_addr_t address;
    uint16_t cid;

    if (packet_type != HCI_EVENT_PACKET) return;

#ifndef HAVE_BTSTACK_STDIN
    if (hci_event_packet_get_type(packet) == BTSTACK_EVENT_STATE){
        if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING) return;
        printf("Create A2DP Source connection to addr %s.\n", bd_addr_to_str(device_addr));
        status = a2dp_source_establish_stream(device_addr, media_tracker.local_seid, &media_tracker.a2dp_cid);
        if (status != ERROR_CODE_SUCCESS){
            printf("Could not perform command, status 0x%2x\n", status);
        }
        return;
    }
#endif
    if (hci_event_packet_get_type(packet) == HCI_EVENT_PIN_CODE_REQUEST) {
        printf("Pin code request - using '0000'\n");
        hci_event_pin_code_request_get_bd_addr(packet, address);
        gap_pin_code_response(address, "0000");
        return;
    }
    
    if (hci_event_packet_get_type(packet) != HCI_EVENT_A2DP_META) return;

    switch (hci_event_a2dp_meta_get_subevent_code(packet)){
        case A2DP_SUBEVENT_SIGNALING_CONNECTION_ESTABLISHED:
            a2dp_subevent_signaling_connection_established_get_bd_addr(packet, address);
            cid = a2dp_subevent_signaling_connection_established_get_a2dp_cid(packet);
            status = a2dp_subevent_signaling_connection_established_get_status(packet);

            if (status != ERROR_CODE_SUCCESS){
                printf("A2DP Source: Connection failed, status 0x%02x, cid 0x%02x, a2dp_cid 0x%02x \n", status, cid, media_tracker.a2dp_cid);
                media_tracker.a2dp_cid = 0;
                break;
            }
            media_tracker.a2dp_cid = cid;
            printf("A2DP Source: Connected to address %s, a2dp cid 0x%02x, local seid %d.\n", bd_addr_to_str(address), media_tracker.a2dp_cid, media_tracker.local_seid);
            break;

         case A2DP_SUBEVENT_SIGNALING_MEDIA_CODEC_SBC_CONFIGURATION:{
            cid  = avdtp_subevent_signaling_media_codec_sbc_configuration_get_avdtp_cid(packet);
            if (cid != media_tracker.a2dp_cid) return;
            media_tracker.remote_seid = a2dp_subevent_signaling_media_codec_sbc_configuration_get_acp_seid(packet);
            
            sbc_configuration.reconfigure = a2dp_subevent_signaling_media_codec_sbc_configuration_get_reconfigure(packet);
            sbc_configuration.num_channels = a2dp_subevent_signaling_media_codec_sbc_configuration_get_num_channels(packet);
            sbc_configuration.sampling_frequency = a2dp_subevent_signaling_media_codec_sbc_configuration_get_sampling_frequency(packet);
            sbc_configuration.channel_mode = a2dp_subevent_signaling_media_codec_sbc_configuration_get_channel_mode(packet);
            sbc_configuration.block_length = a2dp_subevent_signaling_media_codec_sbc_configuration_get_block_length(packet);
            sbc_configuration.subbands = a2dp_subevent_signaling_media_codec_sbc_configuration_get_subbands(packet);
            sbc_configuration.allocation_method = a2dp_subevent_signaling_media_codec_sbc_configuration_get_allocation_method(packet);
            sbc_configuration.min_bitpool_value = a2dp_subevent_signaling_media_codec_sbc_configuration_get_min_bitpool_value(packet);
            sbc_configuration.max_bitpool_value = a2dp_subevent_signaling_media_codec_sbc_configuration_get_max_bitpool_value(packet);
            sbc_configuration.frames_per_buffer = sbc_configuration.subbands * sbc_configuration.block_length;
            printf("A2DP Source: Received SBC codec configuration, sampling frequency %u, a2dp_cid 0x%02x, local seid %d (expected %d), remote seid %d .\n", 
                sbc_configuration.sampling_frequency, cid,
                a2dp_subevent_signaling_media_codec_sbc_configuration_get_int_seid(packet),
                media_tracker.local_seid,
                media_tracker.remote_seid);
            
            // Adapt Bluetooth spec definition to SBC Encoder expected input
            sbc_configuration.allocation_method -= 1;
            sbc_configuration.num_channels = 2;
            switch (sbc_configuration.channel_mode){
                case AVDTP_SBC_JOINT_STEREO:
                    sbc_configuration.channel_mode = 3;
                    break;
                case AVDTP_SBC_STEREO:
                    sbc_configuration.channel_mode = 2;
                    break;
                case AVDTP_SBC_DUAL_CHANNEL:
                    sbc_configuration.channel_mode = 1;
                    break;
                case AVDTP_SBC_MONO:
                    sbc_configuration.channel_mode = 0;
                    sbc_configuration.num_channels = 1;
                    break;
            }
            dump_sbc_configuration(&sbc_configuration);

            btstack_sbc_encoder_init(&sbc_encoder_state, SBC_MODE_STANDARD, 
                sbc_configuration.block_length, sbc_configuration.subbands, 
                sbc_configuration.allocation_method, sbc_configuration.sampling_frequency, 
                sbc_configuration.max_bitpool_value,
                sbc_configuration.channel_mode);
            break;
        }  

        case A2DP_SUBEVENT_SIGNALING_DELAY_REPORTING_CAPABILITY:
            printf("A2DP Source: remote supports delay report, remote seid %d\n", 
                avdtp_subevent_signaling_delay_reporting_capability_get_remote_seid(packet));
            break;
        case A2DP_SUBEVENT_SIGNALING_CAPABILITIES_DONE:
            printf("A2DP Source: All capabilities reported, remote seid %d\n", 
                avdtp_subevent_signaling_capabilities_done_get_remote_seid(packet));
            break;

        case A2DP_SUBEVENT_SIGNALING_DELAY_REPORT:
            printf("A2DP Source: Received delay report of %d.%0d ms, local seid %d\n", 
                avdtp_subevent_signaling_delay_report_get_delay_100us(packet)/10, avdtp_subevent_signaling_delay_report_get_delay_100us(packet)%10,
                avdtp_subevent_signaling_delay_report_get_local_seid(packet));
            break;
       
        case A2DP_SUBEVENT_STREAM_ESTABLISHED:
            a2dp_subevent_stream_established_get_bd_addr(packet, address);
            status = a2dp_subevent_stream_established_get_status(packet);
            if (status){
                printf("A2DP Source: Stream failed, status 0x%02x.\n", status);
                break;
            }
            
            local_seid = a2dp_subevent_stream_established_get_local_seid(packet);
            cid = a2dp_subevent_stream_established_get_a2dp_cid(packet);
            printf("A2DP_SUBEVENT_STREAM_ESTABLISHED:  a2dp_cid [expected 0x%02x, received 0x%02x], local_seid %d (expected %d), remote_seid %d (expected %d)\n",
                     media_tracker.a2dp_cid, cid, 
                     local_seid, media_tracker.local_seid, 
                     a2dp_subevent_stream_established_get_remote_seid(packet), media_tracker.remote_seid);
            
            if (local_seid != media_tracker.local_seid){
                printf("A2DP Source: Stream failed, wrong local seid %d, expected %d.\n", local_seid, media_tracker.local_seid);
                break;    
            }
            printf("A2DP Source: Stream established, address %s, a2dp cid 0x%02x, local seid %d, remote seid %d.\n", bd_addr_to_str(address),
                media_tracker.a2dp_cid, media_tracker.local_seid, a2dp_subevent_stream_established_get_remote_seid(packet));
            media_tracker.stream_opened = 1;
            data_source = STREAM_MOD;
            status = a2dp_source_start_stream(media_tracker.a2dp_cid, media_tracker.local_seid);
            break;

        case A2DP_SUBEVENT_STREAM_RECONFIGURED:
            status = a2dp_subevent_stream_reconfigured_get_status(packet);
            local_seid = a2dp_subevent_stream_reconfigured_get_local_seid(packet);
            cid = a2dp_subevent_stream_reconfigured_get_a2dp_cid(packet);

            printf("A2DP Source: Reconfigured: a2dp_cid [expected 0x%02x, received 0x%02x], local_seid [expected %d, received %d]\n", media_tracker.a2dp_cid, cid, media_tracker.local_seid, local_seid);
            printf("Status 0x%02x\n", status);
            break;

        case A2DP_SUBEVENT_STREAM_STARTED:
            local_seid = a2dp_subevent_stream_started_get_local_seid(packet);
            cid = a2dp_subevent_stream_started_get_a2dp_cid(packet);
            
            play_info.status = AVRCP_PLAYBACK_STATUS_PLAYING;
            if (media_tracker.avrcp_cid){
                avrcp_target_set_now_playing_info(media_tracker.avrcp_cid, &tracks[data_source], sizeof(tracks)/sizeof(avrcp_track_t));
                avrcp_target_set_playback_status(media_tracker.avrcp_cid, AVRCP_PLAYBACK_STATUS_PLAYING);
            }
            a2dp_demo_timer_start(&media_tracker);
            printf("A2DP Source: Stream started: a2dp_cid [expected 0x%02x, received 0x%02x], local_seid [expected %d, received %d]\n", media_tracker.a2dp_cid, cid, media_tracker.local_seid, local_seid);
            break;

        case A2DP_SUBEVENT_STREAMING_CAN_SEND_MEDIA_PACKET_NOW:
            local_seid = a2dp_subevent_streaming_can_send_media_packet_now_get_local_seid(packet);
            cid = a2dp_subevent_signaling_media_codec_sbc_configuration_get_a2dp_cid(packet);
            // printf("A2DP Source: can send media packet: a2dp_cid [expected 0x%02x, received 0x%02x], local_seid [expected %d, received %d]\n", media_tracker.a2dp_cid, cid, media_tracker.local_seid, local_seid);
            a2dp_demo_send_media_packet();
            break;        

        case A2DP_SUBEVENT_STREAM_SUSPENDED:
            local_seid = a2dp_subevent_stream_suspended_get_local_seid(packet);
            cid = a2dp_subevent_stream_suspended_get_a2dp_cid(packet);
            
            play_info.status = AVRCP_PLAYBACK_STATUS_PAUSED;
            if (media_tracker.avrcp_cid){
                avrcp_target_set_playback_status(media_tracker.avrcp_cid, AVRCP_PLAYBACK_STATUS_PAUSED);
            }
            printf("A2DP Source: Stream paused: a2dp_cid [expected 0x%02x, received 0x%02x], local_seid [expected %d, received %d]\n", media_tracker.a2dp_cid, cid, media_tracker.local_seid, local_seid);
            
            a2dp_demo_timer_stop(&media_tracker);
            break;

        case A2DP_SUBEVENT_STREAM_RELEASED:
            play_info.status = AVRCP_PLAYBACK_STATUS_STOPPED;
            cid = a2dp_subevent_stream_released_get_a2dp_cid(packet);
            local_seid = a2dp_subevent_stream_released_get_local_seid(packet);
            
            printf("A2DP Source: Stream released: a2dp_cid [expected 0x%02x, received 0x%02x], local_seid [expected %d, received %d]\n", media_tracker.a2dp_cid, cid, media_tracker.local_seid, local_seid);

            if (cid == media_tracker.a2dp_cid) {
                media_tracker.stream_opened = 0;
                printf("A2DP Source: Stream released.\n");
            }
            if (media_tracker.avrcp_cid){
                avrcp_target_set_now_playing_info(media_tracker.avrcp_cid, NULL, sizeof(tracks)/sizeof(avrcp_track_t));
                avrcp_target_set_playback_status(media_tracker.avrcp_cid, AVRCP_PLAYBACK_STATUS_STOPPED);
            }

            a2dp_demo_timer_stop(&media_tracker);
            break;
        case A2DP_SUBEVENT_SIGNALING_CONNECTION_RELEASED:
            cid = a2dp_subevent_signaling_connection_released_get_a2dp_cid(packet);
            if (cid == media_tracker.a2dp_cid) {
                media_tracker.avrcp_cid = 0;
                media_tracker.a2dp_cid = 0;
                printf("A2DP Source: Signaling released.\n\n");
            }
            break;
        default:
            break; 
    }
}

static void avrcp_target_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    bd_addr_t event_addr;
    uint16_t local_cid;
    uint8_t  status = ERROR_CODE_SUCCESS;

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_AVRCP_META) return;
    
    switch (packet[2]){
        case AVRCP_SUBEVENT_CONNECTION_ESTABLISHED: {
            local_cid = avrcp_subevent_connection_established_get_avrcp_cid(packet);
            // if (avrcp_cid != 0 && avrcp_cid != local_cid) {
            //     printf("AVRCP Target: Connection failed, expected 0x%02X l2cap cid, received 0x%02X\n", avrcp_cid, local_cid);
            //     return;
            // }
            // if (avrcp_cid != local_cid) break;
            
            status = avrcp_subevent_connection_established_get_status(packet);
            if (status != ERROR_CODE_SUCCESS){
                printf("AVRCP Target: Connection failed, status 0x%02x\n", status);
                return;
            }
            media_tracker.avrcp_cid = local_cid;
            avrcp_subevent_connection_established_get_bd_addr(packet, event_addr);
            printf("AVRCP Target: Connected to %s, avrcp_cid 0x%02x\n", bd_addr_to_str(event_addr), local_cid);
            
            avrcp_target_set_now_playing_info(media_tracker.avrcp_cid, NULL, sizeof(tracks)/sizeof(avrcp_track_t));
            avrcp_target_set_unit_info(media_tracker.avrcp_cid, AVRCP_SUBUNIT_TYPE_AUDIO, company_id);
            avrcp_target_set_subunit_info(media_tracker.avrcp_cid, AVRCP_SUBUNIT_TYPE_AUDIO, (uint8_t *)subunit_info, sizeof(subunit_info));
            return;
        }
        case AVRCP_SUBEVENT_NOTIFICATION_VOLUME_CHANGED:
            printf("AVRCP Target: new volume %d\n", avrcp_subevent_notification_volume_changed_get_absolute_volume(packet));
            break;
        case AVRCP_SUBEVENT_EVENT_IDS_QUERY:
            status = avrcp_target_supported_events(media_tracker.avrcp_cid, events_num, events, sizeof(events));
            break;
        case AVRCP_SUBEVENT_COMPANY_IDS_QUERY:
            status = avrcp_target_supported_companies(media_tracker.avrcp_cid, companies_num, companies, sizeof(companies));
            break;
        case AVRCP_SUBEVENT_PLAY_STATUS_QUERY:
            status = avrcp_target_play_status(media_tracker.avrcp_cid, play_info.song_length_ms, play_info.song_position_ms, play_info.status);            
            break;
        // case AVRCP_SUBEVENT_NOW_PLAYING_INFO_QUERY:
        //     status = avrcp_target_now_playing_info(avrcp_cid);
        //     break;
        case AVRCP_SUBEVENT_OPERATION:{
            avrcp_operation_id_t operation_id = avrcp_subevent_operation_get_operation_id(packet);
            switch (operation_id){
                case AVRCP_OPERATION_ID_PLAY:
                    printf("AVRCP Target: PLAY\n");
                    status = a2dp_source_start_stream(media_tracker.a2dp_cid, media_tracker.local_seid);
                    break;
                case AVRCP_OPERATION_ID_PAUSE:
                    printf("AVRCP Target: PAUSE\n");
                    status = a2dp_source_pause_stream(media_tracker.a2dp_cid, media_tracker.local_seid);
                    break;
                case AVRCP_OPERATION_ID_STOP:
                    printf("AVRCP Target: STOP\n");
                    status = a2dp_source_disconnect(media_tracker.a2dp_cid);
                    break;
                default:
                    printf("AVRCP Target: operation 0x%2x is not handled\n", operation_id);
                    return;
            }
            break;
        }
        case AVRCP_SUBEVENT_CONNECTION_RELEASED:
            printf("AVRCP Target: Disconnected, avrcp_cid 0x%02x\n", avrcp_subevent_connection_released_get_avrcp_cid(packet));
            media_tracker.avrcp_cid = 0;
            return;
        default:
            break;
    }

    if (status != ERROR_CODE_SUCCESS){
        printf("Responding to event 0x%02x failed with status 0x%02x\n", packet[2], status);
    }
}

static void avrcp_controller_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    uint16_t local_cid;
    uint8_t  status = 0xFF;
    bd_addr_t adress;
    
    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_AVRCP_META) return;
    switch (packet[2]){
        case AVRCP_SUBEVENT_CONNECTION_ESTABLISHED: {
            local_cid = avrcp_subevent_connection_established_get_avrcp_cid(packet);
            if (avrcp_controller_cid != 0 && avrcp_controller_cid != local_cid) {
                printf("AVRCP Controller: Connection failed, expected 0x%02X l2cap cid, received 0x%02X\n", avrcp_controller_cid, local_cid);
                return;
            }

            status = avrcp_subevent_connection_established_get_status(packet);
            if (status != ERROR_CODE_SUCCESS){
                printf("AVRCP Controller: Connection failed: status 0x%02x\n", status);
                avrcp_controller_cid = 0;
                return;
            }
            
            avrcp_controller_cid = local_cid;
            avrcp_subevent_connection_established_get_bd_addr(packet, adress);
            printf("AVRCP Controller: Channel successfully opened: %s, avrcp_controller_cid 0x%02x\n", bd_addr_to_str(adress), avrcp_controller_cid);

            // automatically enable notifications
            avrcp_controller_enable_notification(avrcp_controller_cid, AVRCP_NOTIFICATION_EVENT_VOLUME_CHANGED);
            return;
        }
        case AVRCP_SUBEVENT_CONNECTION_RELEASED:
            printf("AVRCP Controller: Channel released: avrcp_controller_cid 0x%02x\n", avrcp_subevent_connection_released_get_avrcp_cid(packet));
            avrcp_controller_cid = 0;
            return;
        default:
            break;
    }

    status = packet[5];
    if (!avrcp_controller_cid) return;

    // ignore INTERIM status
    if (status == AVRCP_CTYPE_RESPONSE_INTERIM) return;
            
    switch (packet[2]){
        case AVRCP_SUBEVENT_NOTIFICATION_VOLUME_CHANGED:{
            int volume_percentage = avrcp_subevent_notification_volume_changed_get_absolute_volume(packet) * 100 / 127;
            printf("AVRCP Controller: notification absolute volume changed %d %%\n", volume_percentage);
            return;
        }
        default:
            break;
    }  
}

#ifdef HAVE_BTSTACK_STDIN
static void show_usage(void){
    bd_addr_t      iut_address;
    gap_local_bd_addr(iut_address);
    printf("\n--- Bluetooth  A2DP Source/AVRCP Target Demo %s ---\n", bd_addr_to_str(iut_address));
    printf("b      - A2DP Source create connection to addr %s\n", device_addr_string);
    printf("B      - A2DP Source disconnect\n");
    printf("c      - AVRCP Target create connection to addr %s\n", device_addr_string);
    printf("C      - AVRCP Target disconnect\n");

    printf("x      - start streaming sine\n");
    if (hxcmod_initialized){
        printf("z      - start streaming '%s'\n", mod_name);
    }
    printf("p      - pause streaming\n");
    printf("w      - reconfigure stream for 44100 Hz\n");
    printf("e      - reconfigure stream for 48000 Hz\n");
    printf("t      - volume up\n");
    printf("T      - volume down\n");
    printf("v      - absolute volume of 50 percent\n");

    printf("\n--- Bluetooth  AVRCP Target Commands %s ---\n", bd_addr_to_str(iut_address));
    printf("---\n");
}

static void stdin_process(char cmd){
    uint8_t status = ERROR_CODE_SUCCESS;
    switch (cmd){
        case 'b':
            status = a2dp_source_establish_stream(device_addr, media_tracker.local_seid, &media_tracker.a2dp_cid);
            printf("%c - Create A2DP Source connection to addr %s, cid 0x%02x.\n", cmd, bd_addr_to_str(device_addr), media_tracker.a2dp_cid);
            break;
        case 'B':
            printf("%c - A2DP Source Disconnect from cid 0x%2x\n", cmd, media_tracker.a2dp_cid);
            status = a2dp_source_disconnect(media_tracker.a2dp_cid);
            break;
        case 'c':
            printf("%c - Create AVRCP Target connection to addr %s.\n", cmd, bd_addr_to_str(device_addr));
            status = avrcp_target_connect(device_addr, &media_tracker.avrcp_cid);
            break;
        case 'C':
            printf("%c - AVRCP Target disconnect\n", cmd);
            status = avrcp_target_disconnect(media_tracker.avrcp_cid);
            break;

        case '\n':
        case '\r':
            break;

        case 't':
            printf(" - volume up\n");
            status = avrcp_controller_volume_up(avrcp_controller_cid);
            break;
        case 'T':
            printf(" - volume down\n");
            status = avrcp_controller_volume_down(avrcp_controller_cid);
            break;
        case 'v':
            printf(" - absolute volume of 50%% (64)\n");
            status = avrcp_controller_set_absolute_volume(avrcp_controller_cid, 64);
            break;

        case 'x':
            if (media_tracker.avrcp_cid){
                avrcp_target_set_now_playing_info(media_tracker.avrcp_cid, &tracks[data_source], sizeof(tracks)/sizeof(avrcp_track_t));
            }
            printf("%c - Play sine.\n", cmd);
            data_source = STREAM_SINE;
            if (!media_tracker.stream_opened) break;
            status = a2dp_source_start_stream(media_tracker.a2dp_cid, media_tracker.local_seid);
            break;
        case 'z':
            if (media_tracker.avrcp_cid){
                avrcp_target_set_now_playing_info(media_tracker.avrcp_cid, &tracks[data_source], sizeof(tracks)/sizeof(avrcp_track_t));
            }
            printf("%c - Play mod.\n", cmd);
            data_source = STREAM_MOD;
            if (!media_tracker.stream_opened) break;
            status = a2dp_source_start_stream(media_tracker.a2dp_cid, media_tracker.local_seid);
            if (status == ERROR_CODE_SUCCESS){
                a2dp_demo_reconfigure_sample_rate(sample_rate);
            }
            break;
        
        case 'p':
            if (!media_tracker.stream_opened) break;
            printf("%c - Pause stream.\n", cmd);
            status = a2dp_source_pause_stream(media_tracker.a2dp_cid, media_tracker.local_seid);
            break;
        
        case 'w':
            if (!media_tracker.stream_opened) break;
            if (play_info.status == AVRCP_PLAYBACK_STATUS_PLAYING){
                printf("Stream cannot be reconfigured while playing, please pause stream first\n");
                break;
            }
            printf("%c - Reconfigure for %d Hz.\n", cmd, sample_rate);
            status = a2dp_source_reconfigure_stream_sampling_frequency(media_tracker.a2dp_cid, 44100);
            if (status == ERROR_CODE_SUCCESS){
                a2dp_demo_reconfigure_sample_rate(44100);
            }
            break;

        case 'e':
            if (!media_tracker.stream_opened) break;
            if (play_info.status == AVRCP_PLAYBACK_STATUS_PLAYING){
                printf("Stream cannot be reconfigured while playing, please pause stream first\n");
                break;
            }
            printf("%c - Reconfigure for %d Hz.\n", cmd, sample_rate);
            status = a2dp_source_reconfigure_stream_sampling_frequency(media_tracker.a2dp_cid, 48000);
            if (status == ERROR_CODE_SUCCESS){
                a2dp_demo_reconfigure_sample_rate(48000);
            }
            break;

        default:
            show_usage();
            return;
    }
    if (status != ERROR_CODE_SUCCESS){
        printf("Could not perform command \'%c\', status 0x%2x\n", cmd, status);
    }
}
#endif


int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    (void)argc;
    (void)argv;
    
    int err = a2dp_source_and_avrcp_services_init();
    if (err) return err;
    // turn on!
    hci_power_control(HCI_POWER_ON);
    return 0;
}
/* EXAMPLE_END */
