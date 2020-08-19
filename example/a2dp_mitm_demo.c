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

#define BTSTACK_FILE__ "a2dp_mitm_demo.c"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
#include "classic/a2dp_sink.h"
#include "classic/a2dp_source.h"
#include "classic/avdtp_sink.h"
#include "classic/avdtp_source.h"
#include "classic/avdtp_util.h"
#include "classic/btstack_sbc.h"


#ifdef HAVE_BTSTACK_STDIN
#include "btstack_stdin.h"
#endif

#define NUM_SBC_FRAMES_PER_PACKET 5
#define MAX_SBC_FRAME_SIZE 120
#define MAX_NUM_SBC_FRAMES 100
#define SBC_STORAGE_SIZE 1030
#define PREBUFFER_BYTES 10000
#define FILL_AUDIO_BUFFER_TIMEOUT_MS 5 
#define A2DP_SAMPLE_RATE         44100

// configuration

// iPhone SE
static const char * smartphone_addr_string = "BC:EC:5D:E6:15:03";
// Jawbone
static const char * headset_addr_string    = "00:21:3C:AC:F7:38";
// Sony MDR330 static const char * headset_addr_string    = "00:18:09:28:50:18";

static uint8_t media_sbc_codec_capabilities[] = {
        (AVDTP_SBC_44100 << 4) | AVDTP_SBC_STEREO,
        0xFF,//(AVDTP_SBC_BLOCK_LENGTH_16 << 4) | (AVDTP_SBC_SUBBANDS_8 << 2) | AVDTP_SBC_ALLOCATION_METHOD_LOUDNESS,
        2, 53
};

// gap
static btstack_packet_callback_registration_t hci_event_callback_registration;
static bd_addr_t smartphone_addr;
static bd_addr_t headset_addr;

// sdp records
static uint8_t sdp_avdtp_sink_service_buffer[150];
static uint8_t sdp_avdtp_source_service_buffer[150];

// stored sbc audio frames
static btstack_ring_buffer_t ring_buffer;
static uint8_t ring_buffer_storage[MAX_NUM_SBC_FRAMES * MAX_SBC_FRAME_SIZE];

// prepare sbc packet
static uint8_t  sbc_storage[1030];

// application context
typedef struct {
    uint16_t a2dp_sink_cid;
    uint8_t  a2dp_sink_local_seid;
    hci_con_handle_t a2dp_sink_con_handle;

    uint16_t a2dp_source_cid;
    uint8_t  a2dp_source_local_seid;
    hci_con_handle_t a2dp_source_con_handle;

    uint16_t sbc_frame_size;
    uint16_t num_sbc_frames_in_ring_buffer;
    int      max_media_payload_size;

    bool headset_stream_ready;

#ifdef USE_TIMER_FOR_SENDING
    bool forward_active;
    bool sbc_ready_to_send;
    uint32_t time_audio_data_sent; // ms
    uint32_t acc_num_missed_samples;
    uint32_t samples_ready;
    btstack_timer_source_t fill_audio_buffer_timer;
#endif

} mitm_context_t;

static mitm_context_t mitm_context;

#ifdef HAVE_BTSTACK_STDIN
static uint8_t media_sbc_codec_configuration[] = {
    (AVDTP_SBC_44100 << 4) | AVDTP_SBC_STEREO,
    (AVDTP_SBC_BLOCK_LENGTH_16 << 4) | (AVDTP_SBC_SUBBANDS_8 << 2) | AVDTP_SBC_ALLOCATION_METHOD_LOUDNESS,
    2, 53
}; 

// code start

static void handle_l2cap_media_data_packet(uint8_t seid, uint8_t *packet, uint16_t size);

static void dump_sbc_configuration(const uint8_t * packet){
    printf("    - num_channels: %d\n", a2dp_subevent_signaling_media_codec_sbc_configuration_get_num_channels(packet));
    printf("    - sampling_frequency: %d\n", a2dp_subevent_signaling_media_codec_sbc_configuration_get_sampling_frequency(packet));
    printf("    - channel_mode: %d\n", a2dp_subevent_signaling_media_codec_sbc_configuration_get_channel_mode(packet));
    printf("    - block_length: %d\n", a2dp_subevent_signaling_media_codec_sbc_configuration_get_block_length(packet));
    printf("    - subbands: %d\n", a2dp_subevent_signaling_media_codec_sbc_configuration_get_subbands(packet));
    printf("    - allocation_method: %d\n", a2dp_subevent_signaling_media_codec_sbc_configuration_get_allocation_method(packet));
    printf("    - bitpool_value [%d, %d] \n", a2dp_subevent_signaling_media_codec_sbc_configuration_get_min_bitpool_value(packet),
            a2dp_subevent_signaling_media_codec_sbc_configuration_get_max_bitpool_value(packet));
}

static int ring_buffer_init(void){
    memset(ring_buffer_storage, 0, sizeof(ring_buffer_storage));
    btstack_ring_buffer_init(&ring_buffer, ring_buffer_storage, sizeof(ring_buffer_storage));
    mitm_context.num_sbc_frames_in_ring_buffer = 0;
    return 0;
}

#ifdef USE_TIMER_FOR_SENDING
static void a2dp_fill_audio_buffer_timeout_handler(btstack_timer_source_t * timer){
    mitm_context_t * context = (mitm_context_t *) btstack_run_loop_get_timer_context(timer);
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

    if (context->sbc_ready_to_send) return;

    // do we have enough samples to send?

    // calculate num sbc frames per outgoing packet
    uint16_t payload_max_size = a2dp_max_media_payload_size(context->a2dp_source_cid, context->a2dp_source_local_seid);
    // a2dp_max_media_payload_size(media_tracker.source_local_seid);
    

    int sbc_frames_per_packet = payload_max_size / mitm_context.sbc_frame_size;

    // calculate num frames "ready" - using hard-coded
    int num_samples_per_frame = 128;
    int sbc_frames_ready = context->samples_ready / num_samples_per_frame;

    // log_info("Samples ready %u - Frames ready %u / frames per packet %u\n", context->samples_ready, sbc_frames_ready, sbc_frames_per_packet);

    if (sbc_frames_ready >= sbc_frames_per_packet){
        // schedule sending
        context->sbc_ready_to_send = true;
        a2dp_source_stream_endpoint_request_can_send_now(context->a2dp_source_cid, context->a2dp_source_local_seid);
    }
}

static void a2dp_fill_audio_buffer_timer_start(mitm_context_t * context){
    // context->max_media_payload_size = a2dp_max_media_payload_size(context->source_local_seid);
    context->max_media_payload_size = btstack_min(a2dp_max_media_payload_size(context->a2dp_source_cid, context->a2dp_source_local_seid), SBC_STORAGE_SIZE);

    context->sbc_ready_to_send = false;
    btstack_run_loop_remove_timer(&context->fill_audio_buffer_timer);
    btstack_run_loop_set_timer_handler(&context->fill_audio_buffer_timer, a2dp_fill_audio_buffer_timeout_handler);
    btstack_run_loop_set_timer_context(&context->fill_audio_buffer_timer, context);
    btstack_run_loop_set_timer(&context->fill_audio_buffer_timer, FILL_AUDIO_BUFFER_TIMEOUT_MS); 
    btstack_run_loop_add_timer(&context->fill_audio_buffer_timer);
}

static void a2dp_fill_audio_buffer_timer_stop(mitm_context_t * context){
    context->time_audio_data_sent = 0;
    context->acc_num_missed_samples = 0;
    context->samples_ready = 0;
    context->sbc_ready_to_send = false;
    btstack_run_loop_remove_timer(&context->fill_audio_buffer_timer);
}

static void a2dp_mitm_try_send(void){
    if (!mitm_context.forward_active && btstack_ring_buffer_bytes_available(&ring_buffer) > PREBUFFER_BYTES){
        mitm_context.forward_active = true;
        a2dp_fill_audio_buffer_timer_start(&mitm_context);
    }
}
#endif

#ifndef USE_TIMER_FOR_SENDING
static void a2dp_mitm_try_send(void){
    if (mitm_context.num_sbc_frames_in_ring_buffer < NUM_SBC_FRAMES_PER_PACKET) return;
    // schedule sending
    a2dp_source_stream_endpoint_request_can_send_now(mitm_context.a2dp_source_cid, mitm_context.a2dp_source_local_seid);
}
#endif

static void handle_l2cap_media_data_packet(uint8_t seid, uint8_t *packet, uint16_t size){
    UNUSED(seid);
    // if no sink, drop data
    if (!mitm_context.headset_stream_ready) {
        printf("DEMO: Audio from smartphone, but headset not ready -> dropping\n");
        return;
    }

    UNUSED(size);

    int pos = 0;
    
    avdtp_media_packet_header_t media_header;
    media_header.version = packet[pos] & 0x03;
    media_header.padding = get_bit16(packet[pos],2);
    media_header.extension = get_bit16(packet[pos],3);
    media_header.csrc_count = (packet[pos] >> 4) & 0x0F;

    pos++;

    media_header.marker = get_bit16(packet[pos],0);
    media_header.payload_type  = (packet[pos] >> 1) & 0x7F;
    pos++;

    media_header.sequence_number = big_endian_read_16(packet, pos);
    pos+=2;

    media_header.timestamp = big_endian_read_32(packet, pos);
    pos+=4;

    media_header.synchronization_source = big_endian_read_32(packet, pos);
    pos+=4;

    UNUSED(media_header);

    // TODO: read csrc list
    
    // printf_hexdump( packet, pos );
    // printf("MEDIA HEADER: %u timestamp, version %u, padding %u, extension %u, csrc_count %u\n", 
    //     media_header.timestamp, media_header.version, media_header.padding, media_header.extension, media_header.csrc_count);
    // printf("MEDIA HEADER: marker %02x, payload_type %02x, sequence_number %u, synchronization_source %u\n", 
    //     media_header.marker, media_header.payload_type, media_header.sequence_number, media_header.synchronization_source);
    
    avdtp_sbc_codec_header_t sbc_header;
    sbc_header.fragmentation = get_bit16(packet[pos], 7);
    sbc_header.starting_packet = get_bit16(packet[pos], 6);
    sbc_header.last_packet = get_bit16(packet[pos], 5);
    sbc_header.num_frames = packet[pos] & 0x0f;
    pos++;

    UNUSED(sbc_header);
    // printf("SBC HEADER: num_frames %u, fragmented %u, start %u, stop %u\n", sbc_header.num_frames, sbc_header.fragmentation, sbc_header.starting_packet, sbc_header.last_packet);
    // printf_hexdump( packet+pos, size-pos );

    // num frames
    uint16_t num_frames = sbc_header.num_frames;

    // sbc frame size
    mitm_context.sbc_frame_size = (size - pos) / sbc_header.num_frames;
    
    // log_info("IN: Ringbuffer: %u bytes free", btstack_ring_buffer_bytes_free(&ring_buffer));

    // store individual frames: { len_16, frame }
    int i;
    for (i=0;i<num_frames;i++){
        uint16_t frame_size = mitm_context.sbc_frame_size;
        if (btstack_ring_buffer_bytes_free(&ring_buffer) < (2 + frame_size)){
            log_info("RING: cannot store frame of size %u, only %u free", frame_size,
                     btstack_ring_buffer_bytes_free(&ring_buffer));
            break;
        }
        // log_info("RING: store frame of size %u", frame_size);
        btstack_ring_buffer_write(&ring_buffer, (uint8_t*) &frame_size, 2);
        btstack_ring_buffer_write(&ring_buffer, packet+pos, mitm_context.sbc_frame_size);
        pos += mitm_context.sbc_frame_size;
        mitm_context.num_sbc_frames_in_ring_buffer++;
    }

    a2dp_mitm_try_send();
}

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    bd_addr_t event_addr;

    if (packet_type != HCI_EVENT_PACKET) return;
 
    switch (hci_event_packet_get_type(packet)) {
        case HCI_EVENT_PIN_CODE_REQUEST:
            // inform about pin code request
            printf("Pin code request - using '0000'\n");
            hci_event_pin_code_request_get_bd_addr(packet, event_addr);
            gap_pin_code_response(event_addr, "0000");
            break;
        case HCI_EVENT_CONNECTION_COMPLETE:
            hci_event_connection_complete_get_bd_addr(packet, event_addr);
            if (bd_addr_cmp(event_addr, smartphone_addr) == 0){
                mitm_context.a2dp_sink_con_handle = hci_event_connection_complete_get_connection_handle(packet);
            }
            if (bd_addr_cmp(event_addr, headset_addr) == 0){
                mitm_context.a2dp_source_con_handle = hci_event_connection_complete_get_connection_handle(packet);
            }
            break;
        default:
            break;
    }
}

static void a2dp_sink_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    bd_addr_t event_addr;
    uint8_t status;

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_A2DP_META) return;

    switch (packet[2]){
         case A2DP_SUBEVENT_SIGNALING_CONNECTION_ESTABLISHED:
             a2dp_subevent_signaling_connection_established_get_bd_addr(packet, event_addr);
             if (memcmp(smartphone_addr, event_addr,  6) != 0) break;
             status = a2dp_subevent_signaling_connection_established_get_status(packet);
             if (status != ERROR_CODE_SUCCESS){
                 printf("A2DP Sink: Connection failed with status 0x%02x\n", status);
                 break;
             }
             mitm_context.a2dp_sink_cid = a2dp_subevent_signaling_connection_established_get_a2dp_cid(packet);
             printf("A2DP Sink: Connected to device with addr %s, a2dp cid 0x%02x.\n", bd_addr_to_str(event_addr), mitm_context.a2dp_sink_cid);
             break;

        case A2DP_SUBEVENT_SIGNALING_MEDIA_CODEC_SBC_CONFIGURATION:{
            printf("A2DP Sink: received SBC codec configuration.\n");
            dump_sbc_configuration(packet);

            // init ring buffer to store sbc frames
            ring_buffer_init();
            break;
        }  
        case A2DP_SUBEVENT_SIGNALING_MEDIA_CODEC_OTHER_CONFIGURATION:
            printf("A2DP Sink: received non SBC codec. It will be ignored.\n");
            break;
       
        case A2DP_SUBEVENT_STREAM_ESTABLISHED:
            mitm_context.a2dp_sink_local_seid = a2dp_subevent_stream_established_get_local_seid(packet);
            mitm_context.a2dp_sink_cid = a2dp_subevent_stream_established_get_a2dp_cid(packet);
            printf("A2DP Sink: stream established a2dp_cid 0x%02x, local seid %d, remote seid %d\n",
                   mitm_context.a2dp_sink_cid, mitm_context.a2dp_sink_local_seid, a2dp_subevent_stream_established_get_remote_seid(packet));
            break;

        case A2DP_SUBEVENT_STREAM_STARTED:
            printf("A2DP Sink: phone stream started\n");
            break;

        case A2DP_SUBEVENT_STREAM_SUSPENDED:
            printf("A2DP Sink: phone stream paused\n");
            break;

        case A2DP_SUBEVENT_STREAM_RELEASED:
            printf("A2DP Sink: phone stream released\n");
            break;

        case A2DP_SUBEVENT_SIGNALING_CONNECTION_RELEASED:
            mitm_context.a2dp_sink_cid = 0;
            printf("A2DP Sink: signaling connection released\n");
            break;
        default:
            // printf("A2DP Sink: event %d not parsed\n", packet[2]);
            break; 
    }
}

static void a2dp_source_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    bd_addr_t event_addr;
    uint8_t status;

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_A2DP_META) return;

    switch (packet[2]){
        case A2DP_SUBEVENT_SIGNALING_CONNECTION_ESTABLISHED:
            a2dp_subevent_signaling_connection_established_get_bd_addr(packet, event_addr);
            if (memcmp(headset_addr, event_addr,  6) != 0) break;
            status = a2dp_subevent_signaling_connection_established_get_status(packet);
            if (status != ERROR_CODE_SUCCESS){
                printf("A2DP Sink: Connection failed with status 0x%02x\n", status);
                break;
            }
            mitm_context.a2dp_source_cid = a2dp_subevent_signaling_connection_established_get_a2dp_cid(packet);
            printf("A2DP Source: Connected to device with addr %s, a2dp_cid 0x%02x.\n", bd_addr_to_str(event_addr), mitm_context.a2dp_source_cid);

            // check role
            if (gap_get_role(mitm_context.a2dp_source_con_handle) == HCI_ROLE_SLAVE){
                printf("A2DP Source: In slave role, request master role\n");
                gap_request_role(headset_addr, HCI_ROLE_MASTER);
            }

            break;

         case A2DP_SUBEVENT_SIGNALING_MEDIA_CODEC_SBC_CONFIGURATION:{
            printf("A2DP Source: Received SBC codec configuration.\n");
            dump_sbc_configuration(packet);
            break;
        }  

        case A2DP_SUBEVENT_STREAM_ESTABLISHED:
            printf("A2DP Source: stream established a2dp_cid 0x%02x, local seid %d, remote seid %d\n",
                   mitm_context.a2dp_source_cid, mitm_context.a2dp_source_local_seid, a2dp_subevent_stream_established_get_remote_seid(packet));
            printf("A2DP Source: start stream to headset\n");
            a2dp_source_start_stream(mitm_context.a2dp_source_cid, mitm_context.a2dp_source_local_seid);
            break;
        
        case A2DP_SUBEVENT_STREAM_STARTED:
            mitm_context.headset_stream_ready = 1;
            printf("A2DP Source: headset stream started\n");
            break;

        case A2DP_SUBEVENT_STREAM_SUSPENDED:
            printf("A2DP Source: headset stream paused\n");
            break;

        case A2DP_SUBEVENT_STREAM_RELEASED:
            printf("A2DP Source: headset stream released\n");
            break;

        case A2DP_SUBEVENT_STREAMING_CAN_SEND_MEDIA_PACKET_NOW: {

            uint16_t num_frames;
            uint16_t len = 0;
            uint32_t bytes_read = 0;
            int pos = 0;

            // calculate max num sbc frames for outgoing earlier
            uint16_t payload_max_size = a2dp_max_media_payload_size(mitm_context.a2dp_source_cid, mitm_context.a2dp_source_local_seid);
            int max_sbc_frames = btstack_min( payload_max_size / mitm_context.sbc_frame_size, NUM_SBC_FRAMES_PER_PACKET);

            for (num_frames = 0 ; num_frames < max_sbc_frames && mitm_context.num_sbc_frames_in_ring_buffer > 0; num_frames++){
                btstack_ring_buffer_read(&ring_buffer, (uint8_t*) &len, 2, &bytes_read);
                // log_info("RING: read frame of size %u", len);
                btstack_ring_buffer_read(&ring_buffer, &sbc_storage[pos], len, &bytes_read);
                pos += len;
                mitm_context.num_sbc_frames_in_ring_buffer--;
            }

            a2dp_source_stream_send_media_payload(mitm_context.a2dp_source_cid, mitm_context.a2dp_source_local_seid, sbc_storage, pos, num_frames, 0);

#ifdef USE_TIMER_FOR_SENDING
            mitm_context.sbc_ready_to_send = false;

            int num_samples_per_frame = 128;
            int samples_consumed = num_frames * num_samples_per_frame;
            mitm_context.samples_ready -= samples_consumed;

            // printf("DEMO: audio packet sent, ring buffer %u\n", btstack_ring_buffer_bytes_available(&ring_buffer));
            log_info("OUT: Ringbuffer: %u bytes available", btstack_ring_buffer_bytes_available(&ring_buffer));
            if (btstack_ring_buffer_bytes_available(&ring_buffer) == 0){
                log_info("Stream dried out. stop");
                printf("Stream dried out. restart\n");
                mitm_context.forward_active = false;
                a2dp_fill_audio_buffer_timer_stop(&mitm_context);
                break;
            }
#endif
            a2dp_mitm_try_send();
            break;

        }

        case A2DP_SUBEVENT_SIGNALING_CONNECTION_RELEASED:
            mitm_context.a2dp_source_cid = 0;
            printf("A2DP Source: signaling connection released\n");
            break;

        default:
            break;
    }
       
}

#ifdef HAVE_BTSTACK_STDIN
static const char * get_role_for_connection(hci_con_handle_t con_handle){
    hci_role_t role = HCI_ROLE_INVALID;
    if (con_handle != HCI_CON_HANDLE_INVALID) {
        role = gap_get_role(con_handle);
    }
    switch (role){
        case HCI_ROLE_INVALID:
            return "";
        case HCI_ROLE_MASTER:
            return "/ role: master";
        case HCI_ROLE_SLAVE:
            return "/ role: slave";
    }
}

static void show_status(void){
    // get pairing state
    bool smartphone_paired = false;
    bool headset_paired = false;
    bd_addr_t addr;
    link_key_t link_key;
    link_key_type_t  type;
    btstack_link_key_iterator_t it;
    gap_link_key_iterator_init(&it);
    while (gap_link_key_iterator_get_next(&it, addr, link_key, &type)){
        if (memcmp(addr, smartphone_addr, 6) == 0){
            smartphone_paired = true;
        }
        if (memcmp(addr, headset_addr, 6) == 0){
            headset_paired = true;
        }
    }
    printf("--- Status ---\n");
    printf("- Paired with smartphone: %s\n", smartphone_paired ? "yes" : "no");
    printf("- Paired with headset:    %s\n", headset_paired ? "yes" : "no");
    printf("- Smartphone connected:   %s %s\n", mitm_context.a2dp_sink_cid   ? "yes" : "no", get_role_for_connection(mitm_context.a2dp_sink_con_handle));
    printf("- Headset connected:      %s %s\n", mitm_context.a2dp_source_cid ? "yes" : "no", get_role_for_connection(mitm_context.a2dp_source_con_handle));
    printf("- Headset ready:          %s\n", mitm_context.headset_stream_ready ? "yes" : "no");
    printf("\n");
}

static void show_usage(void){
    bd_addr_t      iut_address;
    gap_local_bd_addr(iut_address);
    printf("\n");
    printf("--- A2DP MITM Console on %s ---\n", bd_addr_to_str(iut_address));
    printf("p      - create connection to smartphone %s\n", bd_addr_to_str(smartphone_addr));
    printf("h      - create connection to headset    %s\n", bd_addr_to_str(headset_addr));
    printf("s      - show status\n");
    printf("d      - disconnect all\n");
    printf("c      - delete all link keys\n");
    printf("\nTo setup MITM, please connect to Headphone using 'h',\n");
    printf("then connect from smartphone/laptop to BTstack.\n");
    printf("Ctrl-c - exit\n");
    printf("---\n");
}
#endif

static void stdin_process(char cmd){
    uint8_t status = ERROR_CODE_SUCCESS;
    switch (cmd){
        case 's':
            show_status();
            break;
        case 'p':
            // allow role switches for outgoing connections to smartphone
            gap_set_allow_role_switch(true);
            status = a2dp_sink_establish_stream(smartphone_addr, mitm_context.a2dp_sink_local_seid, &mitm_context.a2dp_sink_cid);
            printf("Creating A2DP Connection to remote audio source (smartphone) %s, a2dp sink cid 0x%02x\n",
                    bd_addr_to_str(smartphone_addr), mitm_context.a2dp_sink_cid);
            break;
        case 'h':
            // don't allow role switches for outgoing connections to headset
            gap_set_allow_role_switch(false);
            status = a2dp_source_establish_stream(headset_addr, mitm_context.a2dp_source_local_seid, &mitm_context.a2dp_source_cid);
            printf("Creating A2DP Connection to remote audio sink (headset) %s, a2dp source cid 0x%02x\n",
                    bd_addr_to_str(headset_addr), mitm_context.a2dp_source_cid);
            break;
        case 'd':
            printf("Disconnect all\n");
            a2dp_sink_disconnect(mitm_context.a2dp_sink_cid);
            a2dp_source_disconnect(mitm_context.a2dp_source_cid);
            break;
        case 'c':
            printf("Deleting all link keys\n");
            gap_delete_all_link_keys();
            break;
        case 'w':
            printf("Pause stream.\n");
            status = a2dp_source_pause_stream(mitm_context.a2dp_source_cid, mitm_context.a2dp_source_local_seid);
            break;
        default:
            show_usage();
            break;

    }

    if (status != ERROR_CODE_SUCCESS){
        printf("A2DP: Could not perform command \'%c\', status 0x%2x\n", cmd, status);
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

    // assume Display and Yes/No buttons
    gap_ssp_set_io_capability(SSP_IO_CAPABILITY_DISPLAY_YES_NO);

    // allow for role switches
    gap_set_default_link_policy_settings(LM_LINK_POLICY_ENABLE_ROLE_SWITCH);

    // try to become master on incoming connections
    hci_set_master_slave_policy(0);

    l2cap_init();
    sdp_init();


    // Initialize A2DP Sink.
    a2dp_sink_init();
    // Register A2DP Sink for HCI events.
    a2dp_sink_register_packet_handler(&a2dp_sink_packet_handler);
    // Register A2DP Sink for receiving media data.
    a2dp_sink_register_media_handler(&handle_l2cap_media_data_packet);

    // Create a stream endpoint to which the streaming channel will be opened.
    avdtp_stream_endpoint_t * local_sink_stream_endpoint = a2dp_sink_create_stream_endpoint(AVDTP_AUDIO, AVDTP_CODEC_SBC, media_sbc_codec_capabilities, sizeof(media_sbc_codec_capabilities),
                                                                                       media_sbc_codec_configuration, sizeof(media_sbc_codec_configuration));
    if (!local_sink_stream_endpoint){
        printf("A2DP Sink: not enough memory to create local stream endpoint\n");
        return 1;
    }
    mitm_context.a2dp_sink_local_seid = avdtp_local_seid(local_sink_stream_endpoint);
    printf("A2DP Sink: created stream endpoint with seid %d\n", mitm_context.a2dp_sink_local_seid);

    memset(sdp_avdtp_sink_service_buffer, 0, sizeof(sdp_avdtp_sink_service_buffer));
    a2dp_sink_create_sdp_record(sdp_avdtp_sink_service_buffer, 0x10001, 1, NULL, NULL);
    sdp_register_service(sdp_avdtp_sink_service_buffer);


    // Initialize  A2DP Source.
    a2dp_source_init();
    // Register A2DP Source for HCI events.
    a2dp_source_register_packet_handler(&a2dp_source_packet_handler);
    // Create stream endpoint.

    avdtp_stream_endpoint_t * local_source_stream_endpoint = a2dp_source_create_stream_endpoint(AVDTP_AUDIO, AVDTP_CODEC_SBC, media_sbc_codec_capabilities, sizeof(media_sbc_codec_capabilities), media_sbc_codec_configuration, sizeof(media_sbc_codec_configuration));
    if (!local_source_stream_endpoint){
        printf("A2DP Source: not enough memory to create local stream endpoint\n");
        return 1;
    }
    mitm_context.a2dp_source_local_seid = avdtp_local_seid(local_source_stream_endpoint);
    printf("A2DP Source: created stream endpoint with seid %d\n", mitm_context.a2dp_source_local_seid);
    
    memset(sdp_avdtp_source_service_buffer, 0, sizeof(sdp_avdtp_source_service_buffer));
    a2dp_source_create_sdp_record(sdp_avdtp_source_service_buffer, 0x10002, 1, NULL, NULL);
    sdp_register_service(sdp_avdtp_source_service_buffer);


    // GAP
    gap_set_local_name("A2DP MITM Demo 00:00:00:00:00:00");
    gap_discoverable_control(1);
    gap_set_class_of_device(0x200408);

    // parse human readable Bluetooth address
    sscanf_bd_addr(smartphone_addr_string, smartphone_addr);
    sscanf_bd_addr(headset_addr_string, headset_addr);

    //
    mitm_context.a2dp_sink_con_handle = HCI_CON_HANDLE_INVALID;
    mitm_context.a2dp_source_con_handle = HCI_CON_HANDLE_INVALID;

    // turn on!
    hci_power_control(HCI_POWER_ON);

#ifdef HAVE_BTSTACK_STDIN
    btstack_stdin_setup(stdin_process);
#endif

    return 0;
}
