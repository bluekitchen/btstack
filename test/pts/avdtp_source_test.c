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

#include "btstack.h"

// #define NUM_CHANNELS                2
// #define A2DP_SAMPLE_RATE            44100
// #define BYTES_PER_AUDIO_SAMPLE      (2*NUM_CHANNELS)
#define AUDIO_TIMEOUT_MS            10 
#define TABLE_SIZE_441HZ            100
#define AVDTP_MEDIA_PAYLOAD_HEADER_SIZE 12

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

typedef struct {
    // bitmaps
    uint8_t sampling_frequency_bitmap;
    uint8_t channel_mode_bitmap;
    uint8_t block_length_bitmap;
    uint8_t subbands_bitmap;
    uint8_t allocation_method_bitmap;
    uint8_t min_bitpool_value;
    uint8_t max_bitpool_value;
} adtvp_media_codec_information_sbc_t;

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

#ifdef HAVE_BTSTACK_STDIN
// mac 2011:    static const char * device_addr_string = "04:0C:CE:E4:85:D3";
// pts:         static const char * device_addr_string = "00:1B:DC:08:E2:72";
// mac 2013:    static const char * device_addr_string = "84:38:35:65:d1:15";
// phone 2013:  static const char * device_addr_string = "D8:BB:2C:DF:F0:F2";
// minijambox:  static const char * device_addr_string = "00:21:3C:AC:F7:38";
// head phones: static const char * device_addr_string = "00:18:09:28:50:18";
// BT dongle:   static const char * device_addr_string = "00:15:83:5F:9D:46";
// BT dongle:   static const char * device_addr_string = "00:1A:7D:DA:71:0A";

static const char * device_addr_string = "00:02:72:DC:31:C1";
#endif

typedef struct {
    uint16_t avdtp_cid;
    uint8_t  local_seid;
    uint8_t  remote_seid;

    uint32_t time_audio_data_sent; // ms
    uint32_t acc_num_missed_samples;
    uint32_t samples_ready;
    btstack_timer_source_t audio_timer;
    uint8_t  streaming;
    int      max_media_payload_size;
    
    uint8_t  sbc_storage[1030];
    uint16_t sbc_storage_count;
    uint8_t  sbc_ready_to_send;
} a2dp_media_sending_context_t;

a2dp_media_sending_context_t media_tracker;

static bd_addr_t device_addr;
static int sine_phase;

static uint8_t sdp_avdtp_source_service_buffer[150];

static avdtp_stream_endpoint_context_t sc;

static uint16_t remote_configuration_bitmap;
static avdtp_capabilities_t remote_configuration;
static avdtp_context_t a2dp_source_context;
static btstack_sbc_encoder_state_t sbc_encoder_state;
static uint8_t is_cmd_triggered_localy = 0;

static btstack_packet_callback_registration_t hci_event_callback_registration;

static int bytes_per_audio_sample(void){
    return sc.num_channels * 2;
}

static int a2dp_sample_rate(void){
    return sc.sampling_frequency;
}


static void a2dp_source_setup_media_header(uint8_t * media_packet, int size, int *offset, uint8_t marker, uint16_t sequence_number){
    if (size < AVDTP_MEDIA_PAYLOAD_HEADER_SIZE){
        log_error("small outgoing buffer");
        return;
    }

    uint8_t  rtp_version = 2;
    uint8_t  padding = 0;
    uint8_t  extension = 0;
    uint8_t  csrc_count = 0;
    uint8_t  payload_type = 1; //0x60;
    // uint16_t sequence_number = stream_endpoint->sequence_number;
    uint32_t timestamp = btstack_run_loop_get_time_ms();
    uint32_t ssrc = 0x11223344;

    // rtp header (min size 12B)
    int pos = 0;
    // int mtu = l2cap_get_remote_mtu_for_local_cid(stream_endpoint->l2cap_media_cid);

    media_packet[pos++] = (rtp_version << 6) | (padding << 5) | (extension << 4) | csrc_count;
    media_packet[pos++] = (marker << 1) | payload_type;
    big_endian_store_16(media_packet, pos, sequence_number);
    pos += 2;
    big_endian_store_32(media_packet, pos, timestamp);
    pos += 4;
    big_endian_store_32(media_packet, pos, ssrc); // only used for multicast
    pos += 4;
    *offset = pos;
}

static void a2dp_source_copy_media_payload(uint8_t * media_packet, int size, int * offset, uint8_t * storage, int num_bytes_to_copy, uint8_t num_frames){
    if (size < num_bytes_to_copy + 1){
        log_error("small outgoing buffer: buffer size %u, but need %u", size, num_bytes_to_copy + 1);
        return;
    }
    
    int pos = *offset;
    media_packet[pos++] = num_frames; // (fragmentation << 7) | (starting_packet << 6) | (last_packet << 5) | num_frames;
    memcpy(media_packet + pos, storage, num_bytes_to_copy);
    pos += num_bytes_to_copy;
    *offset = pos;
}


static void a2dp_demo_send_media_packet(void){
    int num_bytes_in_frame = btstack_sbc_encoder_sbc_buffer_length();
    int bytes_in_storage = media_tracker.sbc_storage_count;
    uint8_t num_frames = bytes_in_storage / num_bytes_in_frame;
    
    avdtp_source_stream_send_media_payload(media_tracker.avdtp_cid, media_tracker.local_seid, media_tracker.sbc_storage, bytes_in_storage, num_frames, 0);
    media_tracker.sbc_storage_count = 0;
    media_tracker.sbc_ready_to_send = 0;
}

static void produce_sine_audio(int16_t * pcm_buffer, int num_samples_to_write){
    int count;
    for (count = 0; count < num_samples_to_write ; count++){
        pcm_buffer[count * 2]     = sine_int16[sine_phase];
        pcm_buffer[count * 2 + 1] = sine_int16[sine_phase];
        sine_phase++;
        if (sine_phase >= TABLE_SIZE_441HZ){
            sine_phase -= TABLE_SIZE_441HZ;
        }
    }
}

static int fill_sbc_audio_buffer(a2dp_media_sending_context_t * context){
    // perform sbc encodin
    int total_num_bytes_read = 0;
    int num_audio_samples_per_sbc_buffer = btstack_sbc_encoder_num_audio_frames();
    
    while (context->samples_ready >= num_audio_samples_per_sbc_buffer
        && (context->max_media_payload_size - context->sbc_storage_count) >= btstack_sbc_encoder_sbc_buffer_length()){

        uint8_t pcm_frame[ 256 * bytes_per_audio_sample()];

        produce_sine_audio((int16_t *) pcm_frame, num_audio_samples_per_sbc_buffer);
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

static void avdtp_audio_timeout_handler(btstack_timer_source_t * timer){
    a2dp_media_sending_context_t * context = (a2dp_media_sending_context_t *) btstack_run_loop_get_timer_context(timer);
    btstack_run_loop_set_timer(&context->audio_timer, AUDIO_TIMEOUT_MS); 
    btstack_run_loop_add_timer(&context->audio_timer);
    uint32_t now = btstack_run_loop_get_time_ms();

    uint32_t update_period_ms = AUDIO_TIMEOUT_MS;
    if (context->time_audio_data_sent > 0){
        update_period_ms = now - context->time_audio_data_sent;
    } 

    uint32_t num_samples = (update_period_ms * a2dp_sample_rate()) / 1000;
    context->acc_num_missed_samples += (update_period_ms * a2dp_sample_rate()) % 1000;
    
    while (context->acc_num_missed_samples >= 1000){
        num_samples++;
        context->acc_num_missed_samples -= 1000;
    }
    context->time_audio_data_sent = now;
    context->samples_ready += num_samples;

    if (context->sbc_ready_to_send) return;

    fill_sbc_audio_buffer(context);

    if ((context->sbc_storage_count + btstack_sbc_encoder_sbc_buffer_length()) > context->max_media_payload_size){
        // schedule sending
        context->sbc_ready_to_send = 1;

        // a2dp_source_stream_endpoint_request_can_send_now(context->local_seid);
        avdtp_stream_endpoint_t * stream_endpoint = avdtp_stream_endpoint_for_seid(context->local_seid, &a2dp_source_context);
        if (!stream_endpoint) {
            printf("no stream_endpoint for seid %d\n", context->local_seid);
            return;
        }
        stream_endpoint->send_stream = 1;
        if (stream_endpoint->connection){
            avdtp_request_can_send_now_initiator(stream_endpoint->connection, stream_endpoint->l2cap_media_cid);
        }
    }
}

// static int avdtp_max_media_payload_size(uint8_t local_seid){
//     avdtp_stream_endpoint_t * stream_endpoint = avdtp_stream_endpoint_for_seid(local_seid, &a2dp_source_context);
//     if (!stream_endpoint) {
//         log_error("no stream_endpoint found for seid %d", local_seid);
//         return 0;
//     }
//     if (stream_endpoint->l2cap_media_cid == 0){
//         log_error("no media cid found for seid %d", local_seid);
//         return 0;
//     }  
//     return l2cap_get_remote_mtu_for_local_cid(stream_endpoint->l2cap_media_cid) - AVDTP_MEDIA_PAYLOAD_HEADER_SIZE;
// }


static void a2dp_demo_timer_start(a2dp_media_sending_context_t * context){
    context->max_media_payload_size = 0x290;// avdtp_max_media_payload_size(context->local_seid);
    context->sbc_storage_count = 0;
    context->sbc_ready_to_send = 0;
    context->streaming = 1;
    btstack_run_loop_remove_timer(&context->audio_timer);
    btstack_run_loop_set_timer_handler(&context->audio_timer, avdtp_audio_timeout_handler);
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

static void a2dp_demo_timer_pause(a2dp_media_sending_context_t * context){
    btstack_run_loop_remove_timer(&context->audio_timer);
} 

static void initialize_streaming_context(const uint8_t * configuration){
    sc.channel_mode       =  configuration[0] & 0x0F;
    sc.sampling_frequency =  configuration[0] >> 4;
    sc.block_length       =  configuration[1] >> 4;
    sc.subbands           = (configuration[1] & 0x0F) >> 1;
    sc.allocation_method  = (configuration[1] & 0x03) >> 1;
    sc.max_bitpool_value  =  configuration[3];
    
    if (sc.channel_mode & AVDTP_SBC_JOINT_STEREO){
        sc.channel_mode = 3;
    } else if (sc.channel_mode & AVDTP_SBC_STEREO){
        sc.channel_mode = 2;
    } else if (sc.channel_mode & AVDTP_SBC_DUAL_CHANNEL){
        sc.channel_mode = 1;
    } else if (sc.channel_mode & AVDTP_SBC_MONO){
        sc.channel_mode = 0;
    } 

    if (sc.sampling_frequency & AVDTP_SBC_16000){
        sc.sampling_frequency  = 16000;
    }
    if (sc.sampling_frequency & AVDTP_SBC_32000){
        sc.sampling_frequency = 32000;
    }
    if (sc.sampling_frequency & AVDTP_SBC_44100){
        sc.sampling_frequency  = 44100;
    }
    if (sc.sampling_frequency & AVDTP_SBC_48000){
        sc.sampling_frequency  = 48000;
    }

    if (sc.subbands & AVDTP_SBC_SUBBANDS_4){
        sc.subbands = 4;
    }
    if (sc.subbands & AVDTP_SBC_SUBBANDS_8){
        sc.subbands = 8;
    }

    if (sc.block_length & AVDTP_SBC_BLOCK_LENGTH_4){
        sc.block_length = 4;
    }
    if (sc.block_length & AVDTP_SBC_BLOCK_LENGTH_8){
        sc.block_length = 8;
    }
    if (sc.block_length & AVDTP_SBC_BLOCK_LENGTH_12){
        sc.block_length = 12;
    }
    if (sc.block_length & AVDTP_SBC_BLOCK_LENGTH_16){
        sc.block_length = 16;
    }
}

static void initialize_sbc_encoder(void){
    printf("streaming_context: block length %d, subbands %d, allocation_method %d, sampling_frequency %d, max_bitpool_value %d, channel_mode %d\n", 
                        sc.block_length, sc.subbands, 
                        sc.allocation_method, sc.sampling_frequency, 
                        sc.max_bitpool_value, sc.channel_mode);

    btstack_sbc_encoder_init(&sbc_encoder_state, SBC_MODE_STANDARD, 
                        sc.block_length, sc.subbands, 
                        sc.allocation_method, sc.sampling_frequency, 
                        sc.max_bitpool_value, sc.channel_mode);
}

static void dump_sbc_capability(adtvp_media_codec_information_sbc_t media_codec_sbc){
    printf("    - sampling_frequency: 0x%02x\n", media_codec_sbc.sampling_frequency_bitmap);
    printf("    - channel_mode: 0x%02x\n", media_codec_sbc.channel_mode_bitmap);
    printf("    - block_length: 0x%02x\n", media_codec_sbc.block_length_bitmap);
    printf("    - subbands: 0x%02x\n", media_codec_sbc.subbands_bitmap);
    printf("    - allocation_method: 0x%02x\n", media_codec_sbc.allocation_method_bitmap);
    printf("    - bitpool_value [%d, %d] \n", media_codec_sbc.min_bitpool_value, media_codec_sbc.max_bitpool_value);
}

static void dump_sbc_configuration(avdtp_media_codec_configuration_sbc_t configuration){
    printf("Received media codec configuration:\n");
    printf("    - num_channels: %d\n", configuration.num_channels);
    printf("    - sampling_frequency: %d\n", configuration.sampling_frequency);
    printf("    - channel_mode: %d\n", configuration.channel_mode);
    printf("    - block_length: %d\n", configuration.block_length);
    printf("    - subbands: %d\n", configuration.subbands);
    printf("    - allocation_method: %d\n", configuration.allocation_method);
    printf("    - bitpool_value [%d, %d] \n", configuration.min_bitpool_value, configuration.max_bitpool_value);
}

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_AVDTP_META) return;
    UNUSED(channel);
    UNUSED(size);
    uint8_t signal_identifier;
    uint8_t status;
    avdtp_sep_t sep;
    uint16_t avdtp_cid;

    switch (packet[2]){
        case AVDTP_SUBEVENT_SIGNALING_CONNECTION_ESTABLISHED:
            avdtp_cid = avdtp_subevent_signaling_connection_established_get_avdtp_cid(packet);
            status = avdtp_subevent_signaling_connection_established_get_status(packet);
            if (status != 0){
                printf("AVDTP source signaling connection failed: status %d\n", status);
                break;
            }
            media_tracker.avdtp_cid = avdtp_subevent_signaling_connection_established_get_avdtp_cid(packet);
            printf("AVDTP source signaling connection established: avdtp_cid 0x%02x\n", avdtp_cid);
            break;
        
        case AVDTP_SUBEVENT_STREAMING_CONNECTION_ESTABLISHED:
            status = avdtp_subevent_streaming_connection_established_get_status(packet);
            if (status != 0){
                printf("Streaming connection failed: status %d\n", status);
                break;
            }
            avdtp_cid = avdtp_subevent_streaming_connection_established_get_avdtp_cid(packet);
            printf("Streaming connection established, avdtp_cid 0x%02x\n", avdtp_cid);
            break;

        case AVDTP_SUBEVENT_SIGNALING_SEP_FOUND:
            sep.seid = avdtp_subevent_signaling_sep_found_get_remote_seid(packet);;
            sep.in_use = avdtp_subevent_signaling_sep_found_get_in_use(packet);
            sep.media_type = avdtp_subevent_signaling_sep_found_get_media_type(packet);
            sep.type = avdtp_subevent_signaling_sep_found_get_sep_type(packet);
            printf("Found sep: seid %u, in_use %d, media type %d, sep type %d (1-SNK)\n", sep.seid, sep.in_use, sep.media_type, sep.type);
            break;
        
        case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_SBC_CAPABILITY:{
            printf("CAPABILITY - MEDIA_CODEC_SBC: \n");
            adtvp_media_codec_information_sbc_t   sbc_capability;
            sbc_capability.sampling_frequency_bitmap = avdtp_subevent_signaling_media_codec_sbc_capability_get_sampling_frequency_bitmap(packet);
            sbc_capability.channel_mode_bitmap = avdtp_subevent_signaling_media_codec_sbc_capability_get_channel_mode_bitmap(packet);
            sbc_capability.block_length_bitmap = avdtp_subevent_signaling_media_codec_sbc_capability_get_block_length_bitmap(packet);
            sbc_capability.subbands_bitmap = avdtp_subevent_signaling_media_codec_sbc_capability_get_subbands_bitmap(packet);
            sbc_capability.allocation_method_bitmap = avdtp_subevent_signaling_media_codec_sbc_capability_get_allocation_method_bitmap(packet);
            sbc_capability.min_bitpool_value = avdtp_subevent_signaling_media_codec_sbc_capability_get_min_bitpool_value(packet);
            sbc_capability.max_bitpool_value = avdtp_subevent_signaling_media_codec_sbc_capability_get_max_bitpool_value(packet);
            dump_sbc_capability(sbc_capability);
            break;
        }
        case AVDTP_SUBEVENT_SIGNALING_MEDIA_TRANSPORT_CAPABILITY:
            printf("CAPABILITY - MEDIA_TRANSPORT supported on remote.\n");
            break;
        case AVDTP_SUBEVENT_SIGNALING_REPORTING_CAPABILITY:
            printf("CAPABILITY - REPORTING supported on remote.\n");
            break;
        case AVDTP_SUBEVENT_SIGNALING_RECOVERY_CAPABILITY:
            printf("CAPABILITY - RECOVERY supported on remote: \n");
            printf("    - recovery_type                %d\n", avdtp_subevent_signaling_recovery_capability_get_recovery_type(packet));
            printf("    - maximum_recovery_window_size %d\n", avdtp_subevent_signaling_recovery_capability_get_maximum_recovery_window_size(packet));
            printf("    - maximum_number_media_packets %d\n", avdtp_subevent_signaling_recovery_capability_get_maximum_number_media_packets(packet));
            break;
        case AVDTP_SUBEVENT_SIGNALING_CONTENT_PROTECTION_CAPABILITY:
            printf("CAPABILITY - CONTENT_PROTECTION supported on remote: \n");
            printf("    - cp_type           %d\n", avdtp_subevent_signaling_content_protection_capability_get_cp_type(packet));
            printf("    - cp_type_value_len %d\n", avdtp_subevent_signaling_content_protection_capability_get_cp_type_value_len(packet));
            printf("    - cp_type_value     \'%s\'\n", avdtp_subevent_signaling_content_protection_capability_get_cp_type_value(packet));
            break;
        case AVDTP_SUBEVENT_SIGNALING_MULTIPLEXING_CAPABILITY:
            printf("CAPABILITY - MULTIPLEXING supported on remote: \n");
            printf("    - fragmentation                  %d\n", avdtp_subevent_signaling_multiplexing_capability_get_fragmentation(packet));
            printf("    - transport_identifiers_num      %d\n", avdtp_subevent_signaling_multiplexing_capability_get_transport_identifiers_num(packet));
            printf("    - transport_session_identifier_1 %d\n", avdtp_subevent_signaling_multiplexing_capability_get_transport_session_identifier_1(packet));
            printf("    - transport_session_identifier_2 %d\n", avdtp_subevent_signaling_multiplexing_capability_get_transport_session_identifier_2(packet));
            printf("    - transport_session_identifier_3 %d\n", avdtp_subevent_signaling_multiplexing_capability_get_transport_session_identifier_3(packet));
            printf("    - tcid_1                         %d\n", avdtp_subevent_signaling_multiplexing_capability_get_tcid_1(packet));
            printf("    - tcid_2                         %d\n", avdtp_subevent_signaling_multiplexing_capability_get_tcid_2(packet));
            printf("    - tcid_3                         %d\n", avdtp_subevent_signaling_multiplexing_capability_get_tcid_3(packet));
            break;
        case AVDTP_SUBEVENT_SIGNALING_DELAY_REPORTING_CAPABILITY:
            printf("CAPABILITY - DELAY_REPORTING supported on remote.\n");
            break;
        case AVDTP_SUBEVENT_SIGNALING_DELAY_REPORT:
            printf("DELAY_REPORT received: %d.%0d ms, local seid %d\n", 
                avdtp_subevent_signaling_delay_report_get_delay_100us(packet)/10, avdtp_subevent_signaling_delay_report_get_delay_100us(packet)%10,
                avdtp_subevent_signaling_delay_report_get_local_seid(packet));
            break;
        case AVDTP_SUBEVENT_SIGNALING_HEADER_COMPRESSION_CAPABILITY:
            printf("CAPABILITY - HEADER_COMPRESSION supported on remote: \n");
            printf("    - back_ch   %d\n", avdtp_subevent_signaling_header_compression_capability_get_back_ch(packet));
            printf("    - media     %d\n", avdtp_subevent_signaling_header_compression_capability_get_media(packet));
            printf("    - recovery  %d\n", avdtp_subevent_signaling_header_compression_capability_get_recovery(packet));
            break;

        case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_SBC_CONFIGURATION:{
            avdtp_media_codec_configuration_sbc_t sbc_configuration;
            sbc_configuration.reconfigure = avdtp_subevent_signaling_media_codec_sbc_configuration_get_reconfigure(packet);
            sbc_configuration.num_channels = avdtp_subevent_signaling_media_codec_sbc_configuration_get_num_channels(packet);
            sbc_configuration.sampling_frequency = avdtp_subevent_signaling_media_codec_sbc_configuration_get_sampling_frequency(packet);
            sbc_configuration.channel_mode = avdtp_subevent_signaling_media_codec_sbc_configuration_get_channel_mode(packet);
            sbc_configuration.block_length = avdtp_subevent_signaling_media_codec_sbc_configuration_get_block_length(packet);
            sbc_configuration.subbands = avdtp_subevent_signaling_media_codec_sbc_configuration_get_subbands(packet);
            sbc_configuration.allocation_method = avdtp_subevent_signaling_media_codec_sbc_configuration_get_allocation_method(packet);
            sbc_configuration.min_bitpool_value = avdtp_subevent_signaling_media_codec_sbc_configuration_get_min_bitpool_value(packet);
            sbc_configuration.max_bitpool_value = avdtp_subevent_signaling_media_codec_sbc_configuration_get_max_bitpool_value(packet);
            sbc_configuration.frames_per_buffer = sbc_configuration.subbands * sbc_configuration.block_length;
            dump_sbc_configuration(sbc_configuration);
            printf("sbc config remote %d, local %d\n", avdtp_subevent_signaling_media_codec_sbc_configuration_get_remote_seid(packet), avdtp_subevent_signaling_media_codec_sbc_configuration_get_local_seid(packet));
            media_tracker.remote_seid = avdtp_subevent_signaling_media_codec_sbc_configuration_get_remote_seid(packet);
            sc.block_length = sbc_configuration.block_length;
            sc.subbands     = sbc_configuration.subbands;
            sc.allocation_method = sbc_configuration.allocation_method - 1;
            sc.max_bitpool_value = sbc_configuration.max_bitpool_value;
            sc.sampling_frequency = sbc_configuration.sampling_frequency;
            sc.num_channels = 2;
            switch (sbc_configuration.channel_mode){
                case AVDTP_SBC_JOINT_STEREO:
                    sc.channel_mode = 3;
                    break;
                case AVDTP_SBC_STEREO:
                    sc.channel_mode = 2;
                    break;
                case AVDTP_SBC_DUAL_CHANNEL:
                    sc.channel_mode = 1;
                    break;
                case AVDTP_SBC_MONO:
                    sc.channel_mode = 0;
                    sc.num_channels = 1;
                    break;
            }
            break;
        }  

        case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_OTHER_CAPABILITY:
            printf("Received non SBC codec: not parsed\n");
            break;
        
        case AVDTP_SUBEVENT_STREAMING_CAN_SEND_MEDIA_PACKET_NOW:
            a2dp_demo_send_media_packet();
            break;  


        case AVDTP_SUBEVENT_SIGNALING_ACCEPT:
            // TODO check cid
            signal_identifier = avdtp_subevent_signaling_accept_get_signal_identifier(packet);
            if (is_cmd_triggered_localy){
                is_cmd_triggered_localy = 0;
                printf("AVDTP Source command accepted\n");
            }
            
            switch (signal_identifier){
                case AVDTP_SI_OPEN:
                    initialize_sbc_encoder();  
                    break;
                case AVDTP_SI_SET_CONFIGURATION:
                    break;
                case  AVDTP_SI_START:
                    printf("Stream started.\n");
                    a2dp_demo_timer_start(&media_tracker);
                    break;
                case AVDTP_SI_SUSPEND:
                    printf("Stream paused.\n");
                    a2dp_demo_timer_pause(&media_tracker);
                    break;
                case AVDTP_SI_ABORT:
                case AVDTP_SI_CLOSE:
                    printf("Stream released.\n");
                    a2dp_demo_timer_stop(&media_tracker);
                    break;
                default:
                    break;
            }
            break;

        case AVDTP_SUBEVENT_SIGNALING_REJECT:
            signal_identifier = avdtp_subevent_signaling_reject_get_signal_identifier(packet);
            printf("Rejected %s\n", avdtp_si2str(signal_identifier));
            break;
        case AVDTP_SUBEVENT_SIGNALING_GENERAL_REJECT:
            signal_identifier = avdtp_subevent_signaling_general_reject_get_signal_identifier(packet);
            printf("Rejected %s\n", avdtp_si2str(signal_identifier));
            break;
        case AVDTP_SUBEVENT_STREAMING_CONNECTION_RELEASED:
            a2dp_demo_timer_stop(&media_tracker);
            printf("Streaming connection released.\n");
            break;
        case AVDTP_SUBEVENT_SIGNALING_CONNECTION_RELEASED:
            a2dp_demo_timer_stop(&media_tracker);
            printf("Signaling connection released.\n");
            break;
        default:
            printf("AVDTP Source PTS test: event not parsed 0x%02x\n", packet[2]);
            break; 
    }
}


static const uint8_t media_sbc_codec_capabilities[] = {
    0xFF,//(AVDTP_SBC_44100 << 4) | AVDTP_SBC_STEREO,
    0xFF,//(AVDTP_SBC_BLOCK_LENGTH_16 << 4) | (AVDTP_SBC_SUBBANDS_8 << 2) | AVDTP_SBC_ALLOCATION_METHOD_LOUDNESS,
    2, 53
}; 

static const uint8_t media_sbc_codec_configuration[] = {
    (AVDTP_SBC_44100 << 4) | AVDTP_SBC_STEREO,
    (AVDTP_SBC_BLOCK_LENGTH_16 << 4) | (AVDTP_SBC_SUBBANDS_8 << 2) | AVDTP_SBC_ALLOCATION_METHOD_LOUDNESS,
    2, 53
}; 

static const uint8_t media_sbc_codec_reconfiguration[] = {
    (AVDTP_SBC_44100 << 4) | AVDTP_SBC_STEREO,
    (AVDTP_SBC_BLOCK_LENGTH_16 << 4) | (AVDTP_SBC_SUBBANDS_8 << 2) | AVDTP_SBC_ALLOCATION_METHOD_SNR,
    2, 53
}; 



#ifdef HAVE_BTSTACK_STDIN

static void show_usage(void){
    bd_addr_t      iut_address;
    gap_local_bd_addr(iut_address);
    printf("\n--- Bluetooth AVDTP SOURCE Test Console %s ---\n", bd_addr_to_str(iut_address));
    printf("c      - create connection to addr %s\n", device_addr_string);
    printf("C      - disconnect\n");
    printf("d      - discover stream endpoints\n");
    printf("g      - get capabilities\n");
    printf("a      - get all capabilities\n");
    printf("s      - set configuration\n");
    printf("f      - get configuration\n");
    printf("R      - reconfigure stream with %d\n", media_tracker.remote_seid);
    printf("o      - open stream with seid %d\n", media_tracker.remote_seid);
    printf("m      - start stream with %d\n", media_tracker.remote_seid);
    printf("A      - abort stream with %d\n", media_tracker.remote_seid);
    printf("S      - stop stream with %d\n", media_tracker.remote_seid);
    printf("P      - suspend stream with %d\n", media_tracker.remote_seid);
    printf("X      - stop streaming sine\n");
    printf("Ctrl-c - exit\n");
    printf("---\n");
}


static void stdin_process(char cmd){
    uint8_t status = ERROR_CODE_SUCCESS;
    is_cmd_triggered_localy = 1;
    switch (cmd){
        case 'c':
            printf("Establish AVDTP Source connection to %s\n", device_addr_string);
            status = avdtp_source_connect(device_addr, &media_tracker.avdtp_cid);
            break;
        case 'C':
            printf("Disconnect AVDTP Source\n");
            status = avdtp_source_disconnect(media_tracker.avdtp_cid);
            break;
        case 'd':
            printf("Discover stream endpoints of %s\n", device_addr_string);
            status = avdtp_source_discover_stream_endpoints(media_tracker.avdtp_cid);
            break;
        case 'g':
            printf("Get capabilities of stream endpoint with seid %d\n", media_tracker.remote_seid);
            status = avdtp_source_get_capabilities(media_tracker.avdtp_cid, media_tracker.remote_seid);
            break;
        case 'a':
            printf("Get all capabilities of stream endpoint with seid %d\n", media_tracker.remote_seid);
            status = avdtp_source_get_all_capabilities(media_tracker.avdtp_cid, media_tracker.remote_seid);
            break;
        case 'f':
            printf("Get configuration of stream endpoint with seid %d\n", media_tracker.remote_seid);
            status = avdtp_source_get_configuration(media_tracker.avdtp_cid, media_tracker.remote_seid);
            break;
        case 's':
            printf("Set configuration of stream endpoint with seid %d\n", media_tracker.remote_seid);
            remote_configuration_bitmap = store_bit16(remote_configuration_bitmap, AVDTP_MEDIA_CODEC, 1);
            remote_configuration.media_codec.media_type = AVDTP_AUDIO;
            remote_configuration.media_codec.media_codec_type = AVDTP_CODEC_SBC;
            remote_configuration.media_codec.media_codec_information_len = sizeof(media_sbc_codec_configuration);
            remote_configuration.media_codec.media_codec_information = (uint8_t *)media_sbc_codec_configuration;

            initialize_streaming_context(media_sbc_codec_configuration);
            status = avdtp_source_set_configuration(media_tracker.avdtp_cid, media_tracker.local_seid, media_tracker.remote_seid, remote_configuration_bitmap, remote_configuration);
            break;
        case 'R':
            printf("Reconfigure stream endpoint with seid %d\n", media_tracker.remote_seid);
            remote_configuration_bitmap = store_bit16(remote_configuration_bitmap, AVDTP_MEDIA_CODEC, 1);
            remote_configuration.media_codec.media_type = AVDTP_AUDIO;
            remote_configuration.media_codec.media_codec_type = AVDTP_CODEC_SBC;
            remote_configuration.media_codec.media_codec_information_len = sizeof(media_sbc_codec_reconfiguration);
            remote_configuration.media_codec.media_codec_information = (uint8_t *)media_sbc_codec_reconfiguration;
            
            initialize_streaming_context(media_sbc_codec_reconfiguration);
            status = avdtp_source_reconfigure(media_tracker.avdtp_cid, media_tracker.local_seid, media_tracker.remote_seid, remote_configuration_bitmap, remote_configuration);
            break;
        case 'o':
            printf("Establish stream between local %d and remote %d seid\n", media_tracker.local_seid, media_tracker.remote_seid);
            initialize_sbc_encoder();
            status = avdtp_source_open_stream(media_tracker.avdtp_cid, media_tracker.local_seid, media_tracker.remote_seid);
            break;
        case 'm': 
            printf("Start stream between local %d and remote %d seid, \n", media_tracker.local_seid, media_tracker.remote_seid);
            status = avdtp_source_start_stream(media_tracker.avdtp_cid, media_tracker.local_seid);
            break;
        case 'A':
            printf("Abort stream between local %d and remote %d seid\n", media_tracker.local_seid, media_tracker.remote_seid);
            status = avdtp_source_abort_stream(media_tracker.avdtp_cid, media_tracker.local_seid);
            break;
        case 'S':
            printf("Release stream between local %d and remote %d seid\n", media_tracker.local_seid, media_tracker.remote_seid);
            status = avdtp_source_stop_stream(media_tracker.avdtp_cid, media_tracker.local_seid);
            break;
        case 'P':
            printf("Susspend stream between local %d and remote %d seid\n", media_tracker.local_seid, media_tracker.remote_seid);
            status = avdtp_source_suspend(media_tracker.avdtp_cid, media_tracker.local_seid);
            break;
        case 'X':
            printf("Stop streaming\n");
            status = avdtp_source_stop_stream(media_tracker.avdtp_cid, media_tracker.local_seid);
            break;
            
        case '\n':
        case '\r':
            break;
        default:
            show_usage();
            break;

    }
    if (status != ERROR_CODE_SUCCESS){
        printf("AVDTP Sink cmd \'%c\' failed, status 0x%02x\n", cmd, status);
    }
}
#endif


int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    UNUSED(argc);
    (void)argv;

    /* Register for HCI events */
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    l2cap_init();
    // Initialize AVDTP Sink
    avdtp_source_init(&a2dp_source_context);
    avdtp_source_register_packet_handler(&packet_handler);

    sc.local_stream_endpoint = a2dp_source_create_stream_endpoint(AVDTP_AUDIO, AVDTP_CODEC_SBC, (uint8_t *) media_sbc_codec_capabilities, sizeof(media_sbc_codec_capabilities), (uint8_t*) media_sbc_codec_configuration, sizeof(media_sbc_codec_configuration));
    media_tracker.local_seid = avdtp_local_seid(sc.local_stream_endpoint);
    media_tracker.remote_seid = 1;
    
    avdtp_source_register_delay_reporting_category(media_tracker.local_seid);

    // Initialize SDP 
    sdp_init();
    memset(sdp_avdtp_source_service_buffer, 0, sizeof(sdp_avdtp_source_service_buffer));
    a2dp_source_create_sdp_record(sdp_avdtp_source_service_buffer, 0x10002, 1, NULL, NULL);
    sdp_register_service(sdp_avdtp_source_service_buffer);
    
    gap_set_local_name("BTstack AVDTP Source PTS 00:00:00:00:00:00");
    gap_discoverable_control(1);
    gap_set_class_of_device(0x200408);

#ifdef HAVE_BTSTACK_STDIN
    // parse human readable Bluetooth address
    sscanf_bd_addr(device_addr_string, device_addr);
    btstack_stdin_setup(stdin_process);
#endif

    // turn on!
    hci_power_control(HCI_POWER_ON);
    return 0;
}
