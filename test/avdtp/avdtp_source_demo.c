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
#include <math.h>

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
#include "stdin_support.h"
#include "avdtp_source.h"
#include "a2dp_source.h"

#include "btstack_sbc.h"
#include "sbc_encoder.h"
#include "avdtp_util.h"
#include "hxcmod.h"
#include "mod.h"

#define NUM_CHANNELS        2
#define SAMPLE_RATE         44100
#define BYTES_PER_AUDIO_SAMPLE   (2*NUM_CHANNELS)
#define LATENCY             300 // ms

#ifndef M_PI
#define M_PI  3.14159265
#endif
#define TABLE_SIZE_441HZ   100

typedef struct {
// to app
    uint32_t fill_audio_ring_buffer_timeout_ms;
    uint32_t time_audio_data_sent; // ms
    uint32_t acc_num_missed_samples;
    uint32_t samples_ready;
    btstack_timer_source_t fill_audio_ring_buffer_timer;
    btstack_ring_buffer_t sbc_ring_buffer;
    btstack_sbc_encoder_state_t sbc_encoder_state;
    
    int reconfigure;
    int num_channels;
    int sampling_frequency;
    int channel_mode;
    int block_length;
    int subbands;
    int allocation_method;
    int min_bitpool_value;
    int max_bitpool_value;
} avdtp_stream_endpoint_context_t;
static avdtp_stream_endpoint_context_t sc;

static uint8_t int_seid;
// static uint8_t acp_seid;

typedef struct {
    int16_t source[TABLE_SIZE_441HZ];
    int left_phase;
    int right_phase;
} paTestData;

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

static uint16_t avdtp_cid = 0;

static uint8_t sdp_avdtp_source_service_buffer[150];
static avdtp_stream_endpoint_t * local_stream_endpoint;

static avdtp_sep_t * active_remote_sep = NULL;

static uint8_t media_sbc_codec_configuration[4];
static uint8_t sbc_samples_storage[44100*4];


static paTestData sin_data;

static int hxcmod_initialized = 0;
static modcontext mod_context;
static tracker_buffer_state trkbuf;


typedef enum {
    STREAM_SINE,
    STREAM_MOD
} stream_data_source_t;


a2dp_state_t app_state = A2DP_IDLE;
stream_data_source_t data_source = STREAM_SINE;

static btstack_packet_callback_registration_t hci_event_callback_registration;

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    bd_addr_t event_addr;

    switch (packet_type) {
 
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case HCI_EVENT_PIN_CODE_REQUEST:
                    // inform about pin code request
                    printf("Pin code request - using '0000'\n");
                    hci_event_pin_code_request_get_bd_addr(packet, event_addr);
                    hci_send_cmd(&hci_pin_code_request_reply, &event_addr, 4, "0000");
                    break;
                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    // connection closed -> quit test app
                    printf("\n --- application --- HCI_EVENT_DISCONNECTION_COMPLETE ---\n");
                    break;
                case HCI_EVENT_AVDTP_META:
                    switch (packet[2]){
                        case AVDTP_SUBEVENT_STREAMING_CAN_SEND_MEDIA_PACKET_NOW: {
                            avdtp_source_stream_send_media_payload(local_stream_endpoint->l2cap_media_cid, &sc.sbc_ring_buffer, 0);
                            if (btstack_ring_buffer_bytes_available(&sc.sbc_ring_buffer)){
                                avdtp_source_stream_endpoint_request_can_send_now(local_stream_endpoint);
                            }
                            break;
                        }
                        default:
                            app_state = A2DP_IDLE;
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
    printf("d      - discover stream endpoints\n");
    
    if (!active_remote_sep){
        printf("Ctrl-c - exit\n");
        printf("---\n");
        return;
    }
    printf("m      - start stream with %d\n", active_remote_sep->seid);
    printf("x      - start streaming sine\n");
    if (hxcmod_initialized){
        printf("z      - start streaming '%s'\n", mod_name);
    }
    printf("X      - stop streaming\n");
    printf("A      - abort stream with %d\n", active_remote_sep->seid);
    printf("S      - stop stream with %d\n", active_remote_sep->seid);
    printf("P      - suspend stream with %d\n", active_remote_sep->seid);
    printf("Ctrl-c - exit\n");
    printf("---\n");
}

static  uint8_t media_sbc_codec_capabilities[] = {
    0xFF,//(AVDTP_SBC_44100 << 4) | AVDTP_SBC_STEREO,
    0xFF,//(AVDTP_SBC_BLOCK_LENGTH_16 << 4) | (AVDTP_SBC_SUBBANDS_8 << 2) | AVDTP_SBC_ALLOCATION_METHOD_LOUDNESS,
    2, 53
}; 


static void produce_sine_audio(int16_t * pcm_buffer, void *user_data, int num_samples_to_write){
    paTestData *data = (paTestData*)user_data;
    int count;
    for (count = 0; count < num_samples_to_write ; count++){
        pcm_buffer[count * 2]     = data->source[data->left_phase];
        pcm_buffer[count * 2 + 1] = data->source[data->right_phase];

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

static int fill_sbc_ring_buffer(void){
    // performe sbc encoding
    int total_num_bytes_read = 0;
    int num_audio_samples_per_sbc_buffer = btstack_sbc_encoder_num_audio_frames();
    // int audio_bytes_to_read = num_audio_samples_per_sbc_buffer * BYTES_PER_AUDIO_SAMPLE; 

    while (sc.samples_ready >= num_audio_samples_per_sbc_buffer
        && btstack_ring_buffer_bytes_free(&sc.sbc_ring_buffer) >= btstack_sbc_encoder_sbc_buffer_length()){ 

        uint8_t pcm_frame[256*BYTES_PER_AUDIO_SAMPLE];

        produce_audio((int16_t *) pcm_frame, num_audio_samples_per_sbc_buffer);
        btstack_sbc_encoder_process_data((int16_t *) pcm_frame);
        
        uint16_t sbc_frame_size = btstack_sbc_encoder_sbc_buffer_length(); 
        uint8_t * sbc_frame = btstack_sbc_encoder_sbc_buffer();
        
        total_num_bytes_read += num_audio_samples_per_sbc_buffer;
        
        if (btstack_ring_buffer_bytes_free(&sc.sbc_ring_buffer) >= sbc_frame_size ){
            uint8_t size_buffer = sbc_frame_size;
            btstack_ring_buffer_write(&sc.sbc_ring_buffer, &size_buffer, 1);
            btstack_ring_buffer_write(&sc.sbc_ring_buffer, sbc_frame, sbc_frame_size);
        } else {
            printf("No space in sbc buffer\n");
        }
    }
    return total_num_bytes_read;
}

static void avdtp_fill_audio_ring_buffer_timeout_handler(btstack_timer_source_t * timer){
    UNUSED(timer);
    btstack_run_loop_set_timer(&sc.fill_audio_ring_buffer_timer, sc.fill_audio_ring_buffer_timeout_ms); // 2 seconds timeout
    btstack_run_loop_add_timer(&sc.fill_audio_ring_buffer_timer);
    uint32_t now = btstack_run_loop_get_time_ms();

    uint32_t update_period_ms = sc.fill_audio_ring_buffer_timeout_ms;
    if (sc.time_audio_data_sent > 0){
        update_period_ms = now - sc.time_audio_data_sent;
    } 
    uint32_t num_samples = (update_period_ms * 44100) / 1000;
    sc.acc_num_missed_samples += (update_period_ms * 44100) % 1000;

    if (sc.acc_num_missed_samples >= 1000){
        num_samples++;
        sc.acc_num_missed_samples -= 1000;
    }

    sc.time_audio_data_sent = now;
    sc.samples_ready += num_samples;

    int total_num_bytes_read = fill_sbc_ring_buffer();
     // schedule sending
    if (total_num_bytes_read != 0){
        avdtp_source_stream_endpoint_request_can_send_now(local_stream_endpoint);
    }
}

static void avdtp_fill_audio_ring_buffer_timer_start(void){
    btstack_run_loop_remove_timer(&sc.fill_audio_ring_buffer_timer);
    btstack_run_loop_set_timer_handler(&sc.fill_audio_ring_buffer_timer, avdtp_fill_audio_ring_buffer_timeout_handler);
    // btstack_run_loop_set_timer_context(&sc.fill_audio_ring_buffer_timer, stream_endpoint);
    btstack_run_loop_set_timer(&sc.fill_audio_ring_buffer_timer, sc.fill_audio_ring_buffer_timeout_ms); // 50 ms timeout
    btstack_run_loop_add_timer(&sc.fill_audio_ring_buffer_timer);
}

static void avdtp_fill_audio_ring_buffer_timer_stop(void){
    btstack_run_loop_remove_timer(&sc.fill_audio_ring_buffer_timer);
} 

static void avdtp_source_stream_data_start(void){
    if (!avdtp_source_stream_endpoint_ready(local_stream_endpoint)) return;
    avdtp_fill_audio_ring_buffer_timer_start();
}

static void avdtp_source_stream_data_stop(){
    if (!avdtp_source_stream_endpoint_ready(local_stream_endpoint)) return;
    avdtp_fill_audio_ring_buffer_timer_stop(); 
}


static void stdin_process(btstack_data_source_t *ds, btstack_data_source_callback_type_t callback_type){
    UNUSED(ds);
    UNUSED(callback_type);

    int cmd = btstack_stdin_read();

    switch (cmd){
        case 'c':
            printf("Creating L2CAP Connection to %s, PSM_AVDTP\n", bd_addr_to_str(remote));
            a2dp_source_connect(remote, int_seid);
            break;
        case 'C':
            printf("Disconnect not implemented\n");
            a2dp_source_disconnect(avdtp_cid);
            break;
        case 'x':
            // a2dp_source_start_stream(avdtp_cid, int_seid, acp_seid);
            avdtp_source_stream_data_start();
            break;
        case 'X':
            // a2dp_source_stop_stream(avdtp_cid, int_seid, acp_seid);
            avdtp_source_stream_data_stop();
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
    int_seid = a2dp_source_create_stream_endpoint(AVDTP_AUDIO, AVDTP_CODEC_SBC, media_sbc_codec_capabilities, sizeof(media_sbc_codec_capabilities), media_sbc_codec_configuration, sizeof(media_sbc_codec_configuration));

    // Initialize SDP 
    sdp_init();
    memset(sdp_avdtp_source_service_buffer, 0, sizeof(sdp_avdtp_source_service_buffer));
    a2dp_source_create_sdp_record(sdp_avdtp_source_service_buffer, 0x10002, 1, NULL, NULL);
    sdp_register_service(sdp_avdtp_source_service_buffer);
    
    gap_set_local_name(device_name);
    gap_discoverable_control(1);
    gap_set_class_of_device(0x200408);
    
    memset(sbc_samples_storage, 0, sizeof(sbc_samples_storage));
    btstack_ring_buffer_init(&sc.sbc_ring_buffer, sbc_samples_storage, sizeof(sbc_samples_storage));
    sc.fill_audio_ring_buffer_timeout_ms = 50;

    /* initialise sinusoidal wavetable */
    int i;
    for (i=0; i<TABLE_SIZE_441HZ; i++){
        sin_data.source[i] = sin(((double)i/(double)TABLE_SIZE_441HZ) * M_PI * 2.)*32767;
    }
    sin_data.left_phase = sin_data.right_phase = 0;

    hxcmod_initialized = hxcmod_init(&mod_context);
    if (hxcmod_initialized){
        hxcmod_setcfg(&mod_context, 44100, 16, 1, 1, 1);
        hxcmod_load(&mod_context, (void *) &mod_data, mod_len);
        printf("loaded mod '%s', size %u\n", mod_name, mod_len);
    }

    // turn on!
    hci_power_control(HCI_POWER_ON);

    btstack_stdin_setup(stdin_process);
    return 0;
}
