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
// pts:         static const char * device_addr_string = "00:1B:DC:08:0A:A5";
// mac 2013:    static const char * device_addr_string = "84:38:35:65:d1:15";
// phone 2013:  static const char * device_addr_string = "D8:BB:2C:DF:F0:F2";
// minijambox:  
static const char * device_addr_string = "00:21:3C:AC:F7:38";
// head phones: static const char * device_addr_string = "00:18:09:28:50:18";
// bt dongle:   static const char * device_addr_string = "00:15:83:5F:9D:46";
#endif
static bd_addr_t device_addr;

static uint16_t avdtp_cid = 0;
static uint8_t sdp_avdtp_source_service_buffer[150];
static uint8_t remote_seid;

static adtvp_media_codec_information_sbc_t sbc_capability;
static avdtp_media_codec_configuration_sbc_t sbc_configuration;
static avdtp_stream_endpoint_t * local_stream_endpoint;

static uint16_t remote_configuration_bitmap;
static avdtp_capabilities_t remote_configuration;
static avdtp_context_t a2dp_source_context;

static btstack_packet_callback_registration_t hci_event_callback_registration;

static void dump_sbc_capability(adtvp_media_codec_information_sbc_t media_codec_sbc){
    printf(" --- avdtp source --- Received media codec capability:\n");
    printf("  - sampling_frequency: 0x%02x\n", media_codec_sbc.sampling_frequency_bitmap);
    printf("  - channel_mode: 0x%02x\n", media_codec_sbc.channel_mode_bitmap);
    printf("  - block_length: 0x%02x\n", media_codec_sbc.block_length_bitmap);
    printf("  - subbands: 0x%02x\n", media_codec_sbc.subbands_bitmap);
    printf("  - allocation_method: 0x%02x\n", media_codec_sbc.allocation_method_bitmap);
    printf("  - bitpool_value [%d, %d] \n", media_codec_sbc.min_bitpool_value, media_codec_sbc.max_bitpool_value);
    printf("\n");
}

static void dump_sbc_configuration(avdtp_media_codec_configuration_sbc_t configuration){
    printf(" --- avdtp source --- Received media codec configuration:\n");
    printf("  - num_channels: %d\n", configuration.num_channels);
    printf("  - sampling_frequency: %d\n", configuration.sampling_frequency);
    printf("  - channel_mode: %d\n", configuration.channel_mode);
    printf("  - block_length: %d\n", configuration.block_length);
    printf("  - subbands: %d\n", configuration.subbands);
    printf("  - allocation_method: %d\n", configuration.allocation_method);
    printf("  - bitpool_value [%d, %d] \n", configuration.min_bitpool_value, configuration.max_bitpool_value);
    printf("\n");
}

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_AVDTP_META) return;
    UNUSED(channel);
    UNUSED(size);
    uint8_t signal_identifier;
    uint8_t status;
    avdtp_sep_t sep;

    switch (packet[2]){
        case AVDTP_SUBEVENT_SIGNALING_CONNECTION_ESTABLISHED:
            avdtp_cid = avdtp_subevent_signaling_connection_established_get_avdtp_cid(packet);
            status = avdtp_subevent_signaling_connection_established_get_status(packet);
            if (status != 0){
                printf("AVDTP source signaling connection failed: status %d\n", status);
                break;
            }
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
        case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_SBC_CAPABILITY:
            sbc_capability.sampling_frequency_bitmap = avdtp_subevent_signaling_media_codec_sbc_capability_get_sampling_frequency_bitmap(packet);
            sbc_capability.channel_mode_bitmap = avdtp_subevent_signaling_media_codec_sbc_capability_get_channel_mode_bitmap(packet);
            sbc_capability.block_length_bitmap = avdtp_subevent_signaling_media_codec_sbc_capability_get_block_length_bitmap(packet);
            sbc_capability.subbands_bitmap = avdtp_subevent_signaling_media_codec_sbc_capability_get_subbands_bitmap(packet);
            sbc_capability.allocation_method_bitmap = avdtp_subevent_signaling_media_codec_sbc_capability_get_allocation_method_bitmap(packet);
            sbc_capability.min_bitpool_value = avdtp_subevent_signaling_media_codec_sbc_capability_get_min_bitpool_value(packet);
            sbc_capability.max_bitpool_value = avdtp_subevent_signaling_media_codec_sbc_capability_get_max_bitpool_value(packet);
            dump_sbc_capability(sbc_capability);
            break;
        case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_SBC_CONFIGURATION:{
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
            break;
        }  
        case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_OTHER_CAPABILITY:
            printf("Received non SBC codec: not parsed\n");
            break;
        
        case AVDTP_SUBEVENT_SIGNALING_ACCEPT:
            signal_identifier = avdtp_subevent_signaling_accept_get_signal_identifier(packet);
            printf("Accepted %s\n", avdtp_si2str(signal_identifier));

            switch (signal_identifier){
                case AVDTP_SI_START:
                    break;
                case AVDTP_SI_CLOSE:
                    break;
                case AVDTP_SI_SUSPEND:
                    break;
                case AVDTP_SI_ABORT:
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
        default:
            printf("AVDTP Source PTS test: event not parsed\n");
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
    printf("R      - reconfigure stream with %d\n", remote_seid);
    printf("o      - open stream with seid %d\n", remote_seid);
    printf("m      - start stream with %d\n", remote_seid);
    printf("x      - start data stream\n");
    printf("X      - stop  data stream\n");
    printf("A      - abort stream with %d\n", remote_seid);
    printf("S      - stop stream with %d\n", remote_seid);
    printf("P      - suspend stream with %d\n", remote_seid);
    printf("x      - start streaming sine\n");
    printf("X      - stop streaming sine\n");
    printf("Ctrl-c - exit\n");
    printf("---\n");
}

static void stdin_process(char cmd){
    remote_seid = 1;
    switch (cmd){
        case 'c':
            printf("Establish AVDTP Source connection to %s\n", device_addr_string);
            avdtp_source_connect(device_addr, &avdtp_cid);
            break;
        case 'C':
            printf("Disconnect AVDTP Source\n");
            avdtp_source_disconnect(avdtp_cid);
            break;
        case 'd':
            printf("Discover stream endpoints of %s\n", device_addr_string);
            avdtp_source_discover_stream_endpoints(avdtp_cid);
            break;
        case 'g':
            printf("Get capabilities of stream endpoint with seid %d\n", remote_seid);
            avdtp_source_get_capabilities(avdtp_cid, remote_seid);
            break;
        case 'a':
            printf("Get all capabilities of stream endpoint with seid %d\n", remote_seid);
            avdtp_source_get_all_capabilities(avdtp_cid, remote_seid);
            break;
        case 'f':
            printf("Get configuration of stream endpoint with seid %d\n", remote_seid);
            avdtp_source_get_configuration(avdtp_cid, remote_seid);
            break;
        case 's':
            printf("Set configuration of stream endpoint with seid %d\n", remote_seid);
            remote_configuration_bitmap = store_bit16(remote_configuration_bitmap, AVDTP_MEDIA_CODEC, 1);
            remote_configuration.media_codec.media_type = AVDTP_AUDIO;
            remote_configuration.media_codec.media_codec_type = AVDTP_CODEC_SBC;
            remote_configuration.media_codec.media_codec_information_len = sizeof(media_sbc_codec_configuration);
            remote_configuration.media_codec.media_codec_information = (uint8_t *)media_sbc_codec_configuration;
            avdtp_source_set_configuration(avdtp_cid, avdtp_local_seid(local_stream_endpoint), remote_seid, remote_configuration_bitmap, remote_configuration);
            break;
        case 'R':
            printf("Reconfigure stream endpoint with seid %d\n", remote_seid);
            remote_configuration_bitmap = store_bit16(remote_configuration_bitmap, AVDTP_MEDIA_CODEC, 1);
            remote_configuration.media_codec.media_type = AVDTP_AUDIO;
            remote_configuration.media_codec.media_codec_type = AVDTP_CODEC_SBC;
            remote_configuration.media_codec.media_codec_information_len = sizeof(media_sbc_codec_reconfiguration);
            remote_configuration.media_codec.media_codec_information = (uint8_t *)media_sbc_codec_reconfiguration;
            avdtp_source_reconfigure(avdtp_cid, avdtp_local_seid(local_stream_endpoint), remote_seid, remote_configuration_bitmap, remote_configuration);
            break;
        case 'o':
            printf("Establish stream between local %d and remote %d seid\n", avdtp_local_seid(local_stream_endpoint), remote_seid);
            avdtp_source_open_stream(avdtp_cid, avdtp_local_seid(local_stream_endpoint), remote_seid);
            break;
        case 'm': 
            printf("Start stream between local %d and remote %d seid\n", avdtp_local_seid(local_stream_endpoint), avdtp_remote_seid(local_stream_endpoint));
            avdtp_source_start_stream(avdtp_cid, avdtp_local_seid(local_stream_endpoint));
            break;
        case 'A':
            printf("Abort stream between local %d and remote %d seid\n", avdtp_local_seid(local_stream_endpoint), avdtp_remote_seid(local_stream_endpoint));
            avdtp_source_abort_stream(avdtp_cid, avdtp_local_seid(local_stream_endpoint));
            break;
        case 'S':
            printf("Release stream between local %d and remote %d seid\n", avdtp_local_seid(local_stream_endpoint), avdtp_remote_seid(local_stream_endpoint));
            avdtp_source_stop_stream(avdtp_cid, avdtp_local_seid(local_stream_endpoint));
            break;
        case 'P':
            printf("Susspend stream between local %d and remote %d seid\n", avdtp_local_seid(local_stream_endpoint), avdtp_remote_seid(local_stream_endpoint));
            avdtp_source_suspend(avdtp_cid, avdtp_local_seid(local_stream_endpoint));
            break;
        case 'x':
            printf("Start streaming sine\n");
            avdtp_source_start_stream(avdtp_cid, avdtp_local_seid(local_stream_endpoint));
            break;
        case 'X':
            printf("Stop streaming sine\n");
            avdtp_source_stop_stream(avdtp_cid, avdtp_local_seid(local_stream_endpoint));
            break;
            
        case '\n':
        case '\r':
            break;
        default:
            show_usage();
            break;

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
    local_stream_endpoint = avdtp_source_create_stream_endpoint(AVDTP_SOURCE, AVDTP_AUDIO);
    avdtp_source_register_media_transport_category(avdtp_local_seid(local_stream_endpoint));
    avdtp_source_register_media_codec_category(avdtp_local_seid(local_stream_endpoint), AVDTP_AUDIO, AVDTP_CODEC_SBC, (uint8_t *)media_sbc_codec_capabilities, sizeof(media_sbc_codec_capabilities));

    // Initialize SDP 
    sdp_init();
    memset(sdp_avdtp_source_service_buffer, 0, sizeof(sdp_avdtp_source_service_buffer));
    a2dp_source_create_sdp_record(sdp_avdtp_source_service_buffer, 0x10002, 1, NULL, NULL);
    sdp_register_service(sdp_avdtp_source_service_buffer);
    
    gap_set_local_name("BTstack AVDTP Source PTS Test");
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
