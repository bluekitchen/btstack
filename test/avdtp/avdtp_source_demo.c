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


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "btstack_config.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_memory.h"
#include "btstack_run_loop.h"
#include "gap.h"
#include "hci.h"
#include "hci_cmd.h"
#include "hci_dump.h"
#include "l2cap.h"
#include "btstack_stdin.h"
#include "avdtp_source.h"
#include "a2dp_source.h"

#include "btstack_sbc.h"
#include "sbc_encoder.h"
#include "avdtp_util.h"
#include "hxcmod.h"
#include "mod.h"

#define NUM_CHANNELS        2
#define A2DP_SAMPLE_RATE         44100
#define BYTES_PER_AUDIO_SAMPLE   (2*NUM_CHANNELS)
#define FILL_AUDIO_BUFFER_TIMEOUT_MS 10 

#ifndef M_PI
#define M_PI  3.14159265
#endif
#define TABLE_SIZE_441HZ   100

typedef struct {
    int left_phase;
    int right_phase;
} paTestData;

typedef enum {
    STREAM_SINE,
    STREAM_MOD
} stream_data_source_t;

static  uint8_t media_sbc_codec_capabilities[] = {
    (AVDTP_SBC_44100 << 4) | AVDTP_SBC_STEREO,
    0xFF,//(AVDTP_SBC_BLOCK_LENGTH_16 << 4) | (AVDTP_SBC_SUBBANDS_8 << 2) | AVDTP_SBC_ALLOCATION_METHOD_LOUDNESS,
    2, 53
}; 

static const int16_t sine_int16[] = {
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

static char * device_name = "A2DP Source BTstack";

// mac 2011: static bd_addr_t remote = {0x04, 0x0C, 0xCE, 0xE4, 0x85, 0xD3};
// pts: static bd_addr_t remote = {0x00, 0x1B, 0xDC, 0x08, 0x0A, 0xA5};
// mac 2013: static bd_addr_t remote = {0x84, 0x38, 0x35, 0x65, 0xd1, 0x15};
// phone 2013: static bd_addr_t remote = {0xD8, 0xBB, 0x2C, 0xDF, 0xF0, 0xF2};
// minijambox: 
static bd_addr_t remote = {0x00, 0x21, 0x3c, 0xac, 0xf7, 0x38};
// head phones: static bd_addr_t remote = {0x00, 0x18, 0x09, 0x28, 0x50, 0x18};
// bt dongle: -u 02-04-01 
// static bd_addr_t remote = {0x00, 0x15, 0x83, 0x5F, 0x9D, 0x46};


static uint8_t sdp_avdtp_source_service_buffer[150];
static uint8_t media_sbc_codec_configuration[4];
    
typedef struct {
    uint16_t a2dp_cid;
    uint8_t  local_seid;
    
    uint32_t time_audio_data_sent; // ms
    uint32_t acc_num_missed_samples;
    uint32_t samples_ready;
    btstack_timer_source_t fill_audio_buffer_timer;
    uint8_t  streaming;
                            
    int      max_media_payload_size;
    
    uint8_t  sbc_storage[1030];
    uint16_t sbc_storage_count;
    uint8_t  sbc_ready_to_send;

} a2dp_media_sending_context_t;

static a2dp_media_sending_context_t media_tracker;

static paTestData sin_data;

static int hxcmod_initialized = 0;
static modcontext mod_context;
static tracker_buffer_state trkbuf;

static uint8_t local_seid = 0;
stream_data_source_t data_source = STREAM_SINE;

static btstack_packet_callback_registration_t hci_event_callback_registration;

static void a2dp_fill_audio_buffer_timer_start(a2dp_media_sending_context_t * context);
static void a2dp_fill_audio_buffer_timer_stop(a2dp_media_sending_context_t * context);
static void a2dp_fill_audio_buffer_timer_pause(a2dp_media_sending_context_t * context);

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    switch (packet_type) {
 
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case HCI_EVENT_A2DP_META:
                    switch (packet[2]){
                        case A2DP_SUBEVENT_STREAM_ESTABLISHED:
                            media_tracker.local_seid = a2dp_subevent_stream_established_get_local_seid(packet);
                            media_tracker.a2dp_cid = a2dp_subevent_stream_established_get_a2dp_cid(packet);
                            printf(" --- application --- A2DP_SUBEVENT_STREAM_ESTABLISHED, a2dp_cid 0x%02x, local seid %d, remote seid %d\n", 
                                media_tracker.a2dp_cid, media_tracker.local_seid, a2dp_subevent_stream_established_get_remote_seid(packet));
                            break;
                        
                        case A2DP_SUBEVENT_STREAM_START_ACCEPTED:
                            if (local_seid != media_tracker.local_seid) break;
                            if (!a2dp_source_stream_endpoint_ready(media_tracker.local_seid)) break;
                            a2dp_fill_audio_buffer_timer_start(&media_tracker);
                            printf(" --- application ---  A2DP_SUBEVENT_STREAM_START_ACCEPTED, local seid %d\n", media_tracker.local_seid);
                            break;
                        
                        case A2DP_SUBEVENT_STREAMING_CAN_SEND_MEDIA_PACKET_NOW:{
                            if (local_seid != media_tracker.local_seid) break;

                            int num_bytes_in_frame = btstack_sbc_encoder_sbc_buffer_length();
                            int bytes_in_storage = media_tracker.sbc_storage_count;
                            uint8_t num_frames = bytes_in_storage / num_bytes_in_frame;
                            
                            a2dp_source_stream_send_media_payload(media_tracker.local_seid, media_tracker.sbc_storage, bytes_in_storage, num_frames, 0);
                            media_tracker.sbc_storage_count = 0;
                            media_tracker.sbc_ready_to_send = 0;
                            break;        
                        }
                        case A2DP_SUBEVENT_STREAM_SUSPENDED:
                            printf(" --- application ---  A2DP_SUBEVENT_STREAM_SUSPENDED, local seid %d\n", media_tracker.local_seid);
                            a2dp_fill_audio_buffer_timer_pause(&media_tracker);
                            break;

                        case A2DP_SUBEVENT_STREAM_RELEASED:
                            printf(" --- application ---  A2DP_SUBEVENT_STREAM_RELEASED, local seid %d\n", media_tracker.local_seid);
                            a2dp_fill_audio_buffer_timer_stop(&media_tracker);
                            break;
                        default:
                            printf(" --- application ---  not implemented\n");
                            break; 
                    }
                    break;   
                default:
                    break;
            }
            break;
        default:
            // other packet type
            break;
    }    
}

static void show_usage(void){
    bd_addr_t      iut_address;
    gap_local_bd_addr(iut_address);
    printf("\n--- Bluetooth AVDTP SOURCE Test Console %s ---\n", bd_addr_to_str(iut_address));
    printf("c      - create connection to addr %s\n", bd_addr_to_str(remote));
    printf("C      - disconnect\n");
    printf("x      - start streaming sine\n");
    if (hxcmod_initialized){
        printf("z      - start streaming '%s'\n", mod_name);
    }
    printf("p      - pause streaming\n");
    printf("X      - stop streaming\n");
    printf("Ctrl-c - exit\n");
    printf("---\n");
}

static void produce_sine_audio(int16_t * pcm_buffer, void *user_data, int num_samples_to_write){
    paTestData *data = (paTestData*)user_data;
    int count;
    for (count = 0; count < num_samples_to_write ; count++){
        pcm_buffer[count * 2]     = sine_int16[data->left_phase];
        pcm_buffer[count * 2 + 1] = sine_int16[data->right_phase];

        data->left_phase += 1;
        if (data->left_phase >= TABLE_SIZE_441HZ){
            data->left_phase -= TABLE_SIZE_441HZ;
        }
        data->right_phase += 1; 
        if (data->right_phase >= TABLE_SIZE_441HZ){
            data->right_phase -= TABLE_SIZE_441HZ;
        } 
    }
}

static void produce_mod_audio(int16_t * pcm_buffer, int num_samples_to_write){
    hxcmod_fillbuffer(&mod_context, (unsigned short *) &pcm_buffer[0], num_samples_to_write, &trkbuf);
}

static void produce_audio(int16_t * pcm_buffer, int num_samples){
    switch (data_source){
        case STREAM_SINE:
            produce_sine_audio(pcm_buffer, &sin_data, num_samples);
            break;
        case STREAM_MOD:
            produce_mod_audio(pcm_buffer, num_samples);
            break;
    }    
}

static int fill_sbc_audio_buffer(a2dp_media_sending_context_t * context){
    // perform sbc encodin
    int total_num_bytes_read = 0;
    int num_audio_samples_per_sbc_buffer = btstack_sbc_encoder_num_audio_frames();
    while (context->samples_ready >= num_audio_samples_per_sbc_buffer
        && (context->max_media_payload_size - context->sbc_storage_count) >= btstack_sbc_encoder_sbc_buffer_length()){

        uint8_t pcm_frame[256*BYTES_PER_AUDIO_SAMPLE];

        produce_audio((int16_t *) pcm_frame, num_audio_samples_per_sbc_buffer);
        btstack_sbc_encoder_process_data((int16_t *) pcm_frame);
        
        uint16_t sbc_frame_size = btstack_sbc_encoder_sbc_buffer_length(); 
        uint8_t * sbc_frame = btstack_sbc_encoder_sbc_buffer();
        
        total_num_bytes_read += num_audio_samples_per_sbc_buffer;
        memcpy(&context->sbc_storage[context->sbc_storage_count], sbc_frame, sbc_frame_size);
        context->sbc_storage_count += sbc_frame_size;
        context->samples_ready -= num_audio_samples_per_sbc_buffer;
    }
    return total_num_bytes_read;
}

static void avdtp_fill_audio_buffer_timeout_handler(btstack_timer_source_t * timer){
    log_info("timer");
    a2dp_media_sending_context_t * context = (a2dp_media_sending_context_t *) btstack_run_loop_get_timer_context(timer);
    btstack_run_loop_set_timer(&context->fill_audio_buffer_timer, FILL_AUDIO_BUFFER_TIMEOUT_MS); 
    btstack_run_loop_add_timer(&context->fill_audio_buffer_timer);
    uint32_t now = btstack_run_loop_get_time_ms();

    uint32_t update_period_ms = FILL_AUDIO_BUFFER_TIMEOUT_MS;
    if (context->time_audio_data_sent > 0){
        update_period_ms = now - context->time_audio_data_sent;
    } 

    uint32_t num_samples = (update_period_ms * A2DP_SAMPLE_RATE) / 1000;
    context->acc_num_missed_samples += (update_period_ms * A2DP_SAMPLE_RATE) % 1000;
    
    while (context->acc_num_missed_samples >= 1000){
        num_samples++;
        context->acc_num_missed_samples -= 1000;
    }
    context->time_audio_data_sent = now;
    context->samples_ready += num_samples;

    if (!context->sbc_ready_to_send){
        fill_sbc_audio_buffer(context);
    }

    if ((context->sbc_storage_count + btstack_sbc_encoder_sbc_buffer_length()) > context->max_media_payload_size){
        // schedule sending
        context->sbc_ready_to_send = 1;
        a2dp_source_stream_endpoint_request_can_send_now(context->local_seid);
    }
}

static void a2dp_fill_audio_buffer_timer_start(a2dp_media_sending_context_t * context){
    context->max_media_payload_size = a2dp_max_media_payload_size(context->local_seid);
    context->sbc_storage_count = 0;
    context->sbc_ready_to_send = 0;
    context->streaming = 1;
    btstack_run_loop_remove_timer(&context->fill_audio_buffer_timer);
    btstack_run_loop_set_timer_handler(&context->fill_audio_buffer_timer, avdtp_fill_audio_buffer_timeout_handler);
    btstack_run_loop_set_timer_context(&context->fill_audio_buffer_timer, context);
    btstack_run_loop_set_timer(&context->fill_audio_buffer_timer, FILL_AUDIO_BUFFER_TIMEOUT_MS); 
    btstack_run_loop_add_timer(&context->fill_audio_buffer_timer);
}

static void a2dp_fill_audio_buffer_timer_stop(a2dp_media_sending_context_t * context){
    context->time_audio_data_sent = 0;
    context->acc_num_missed_samples = 0;
    context->samples_ready = 0;
    context->streaming = 1;
    context->sbc_storage_count = 0;
    context->sbc_ready_to_send = 0;
    btstack_run_loop_remove_timer(&context->fill_audio_buffer_timer);
} 

static void a2dp_fill_audio_buffer_timer_pause(a2dp_media_sending_context_t * context){
    // context->time_audio_data_sent = 0;
    btstack_run_loop_remove_timer(&context->fill_audio_buffer_timer);
} 

static void stdin_process(char cmd){
    switch (cmd){
        case 'c':
            printf("Creating L2CAP Connection to %s, PSM_AVDTP\n", bd_addr_to_str(remote));
            a2dp_source_establish_stream(remote, local_seid);
            break;
        case 'C':
            printf("Disconnect\n");
            a2dp_source_disconnect(media_tracker.a2dp_cid);
            break;
        case 'x':
            printf("Stream sine, local seid %d\n", media_tracker.local_seid);
            data_source = STREAM_SINE;
            a2dp_source_start_stream(media_tracker.local_seid);
            break;
        case 'z':
            printf("Stream mode, local seid %d\n", media_tracker.local_seid);
            data_source = STREAM_MOD;
            a2dp_source_start_stream(media_tracker.local_seid);
            break;
        case 'p':
            printf("Pause stream, local seid %d\n", media_tracker.local_seid);
            a2dp_source_pause_stream(media_tracker.local_seid);
            break;
        case 'X':
            printf("Close stream, local seid %d\n", media_tracker.local_seid);
            a2dp_source_release_stream(media_tracker.local_seid);
            break;
        default:
            show_usage();
            break;
    }
}

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    UNUSED(argc);
    (void)argv;

    /* Register for HCI events */
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    l2cap_init();
    // Initialize AVDTP Sink
    a2dp_source_init();
    a2dp_source_register_packet_handler(&packet_handler);

    //#ifndef SMG_BI
    local_seid = a2dp_source_create_stream_endpoint(AVDTP_AUDIO, AVDTP_CODEC_SBC, media_sbc_codec_capabilities, sizeof(media_sbc_codec_capabilities), media_sbc_codec_configuration, sizeof(media_sbc_codec_configuration));

    // Initialize SDP 
    sdp_init();
    memset(sdp_avdtp_source_service_buffer, 0, sizeof(sdp_avdtp_source_service_buffer));
    a2dp_source_create_sdp_record(sdp_avdtp_source_service_buffer, 0x10002, 1, NULL, NULL);
    sdp_register_service(sdp_avdtp_source_service_buffer);
    
    gap_set_local_name(device_name);
    gap_discoverable_control(1);
    gap_set_class_of_device(0x200408);
    
    hxcmod_initialized = hxcmod_init(&mod_context);
    if (hxcmod_initialized){
        hxcmod_setcfg(&mod_context, A2DP_SAMPLE_RATE, 16, 1, 1, 1);
        hxcmod_load(&mod_context, (void *) &mod_data, mod_len);
        printf("loaded mod '%s', size %u\n", mod_name, mod_len);
    }

    // turn on!
    hci_power_control(HCI_POWER_ON);

    btstack_stdin_setup(stdin_process);
    return 0;
}
