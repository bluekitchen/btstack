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

#define BTSTACK_FILE__ "a2dp_sink_demo.c"

/*
 * a2dp_sink_demo.c
 */

// *****************************************************************************
/* EXAMPLE_START(a2dp_sink_demo): Receive audio stream and control its playback.
 *
 * @text This A2DP Sink example demonstrates how to use the A2DP Sink service to 
 * receive an audio data stream from a remote A2DP Source device. In addition,
 * the AVRCP Controller is used to get information on currently played media, 
 * such are title, artist and album, as well as to control the playback, 
 * i.e. to play, stop, repeat, etc.
 *
 * @test To test with a remote device, e.g. a mobile phone,
 * pair from the remote device with the demo, then start playing music on the remote device.
 * Alternatively, set the device_addr_string to the Bluetooth address of your 
 * remote device in the code, and call connect from the UI.
 * 
 * @test To controll the playback, tap SPACE on the console to show the available 
 * AVRCP commands.
 */
// *****************************************************************************

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack.h"
#include "btstack_resample.h"

//#define AVRCP_BROWSING_ENABLED

#ifdef HAVE_BTSTACK_STDIN
#include "btstack_stdin.h"
#endif

#include "btstack_ring_buffer.h"

#ifdef HAVE_POSIX_FILE_IO
#include "wav_util.h"
#define STORE_TO_SBC_FILE 
#define STORE_TO_WAV_FILE 
#endif

#define NUM_CHANNELS 2
#define BYTES_PER_FRAME     (2*NUM_CHANNELS)
#define MAX_SBC_FRAME_SIZE 120

// SBC Decoder for WAV file or live playback
static btstack_sbc_decoder_state_t state;
static btstack_sbc_mode_t mode = SBC_MODE_STANDARD;

// ring buffer for SBC Frames
// below 30: add samples, 30-40: fine, above 40: drop samples
#define OPTIMAL_FRAMES_MIN 30
#define OPTIMAL_FRAMES_MAX 40
#define ADDITIONAL_FRAMES  20
static uint8_t sbc_frame_storage[(OPTIMAL_FRAMES_MAX + ADDITIONAL_FRAMES) * MAX_SBC_FRAME_SIZE];
static btstack_ring_buffer_t sbc_frame_ring_buffer;
static unsigned int sbc_frame_size;

// rest buffer for not fully used sbc frames, with additional frames for resampling
static uint8_t decoded_audio_storage[(128+16) * BYTES_PER_FRAME];
static btstack_ring_buffer_t decoded_audio_ring_buffer;

// 
static int audio_stream_started;

// temp storage of lower-layer request
static int16_t * request_buffer;
static int       request_frames;

#define STORE_FROM_PLAYBACK

// WAV File
#ifdef STORE_TO_WAV_FILE    
static int frame_count = 0;
static char * wav_filename = "avdtp_sink.wav";
#endif

#ifdef STORE_TO_SBC_FILE    
static FILE * sbc_file;
static char * sbc_filename = "avdtp_sink.sbc";
#endif

static int volume_percentage = 0; 

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

static uint8_t events_num = 1;

static uint8_t events[] = {
    AVRCP_NOTIFICATION_EVENT_VOLUME_CHANGED
};

static uint8_t companies_num = 1;
static uint8_t companies[] = {
    0x00, 0x19, 0x58 //BT SIG registered CompanyID
};
#ifdef HAVE_BTSTACK_STDIN
// pts: static bd_addr_t remote = {0x00, 0x1B, 0xDC, 0x08, 0x0A, 0xA5};
// mac 2013: static const char * device_addr_string = "84:38:35:65:d1:15";
// iPhone 5S: 
static const char * device_addr_string = "54:E4:3A:26:A2:39";
#endif

static uint8_t  sdp_avdtp_sink_service_buffer[150];
static uint8_t  sdp_avrcp_target_service_buffer[150];
static avdtp_media_codec_configuration_sbc_t sbc_configuration;
static uint16_t a2dp_cid = 0;
static uint8_t  a2dp_local_seid = 0;
static uint8_t  value[100];

static btstack_packet_callback_registration_t hci_event_callback_registration;

static int media_initialized = 0;

#ifdef HAVE_BTSTACK_STDIN
static bd_addr_t device_addr;
#endif

static uint16_t avrcp_controller_cid = 0;
static uint16_t avrcp_target_cid = 0;
static uint8_t  avrcp_connected = 0;
static uint8_t  sdp_avrcp_controller_service_buffer[200];

static uint8_t media_sbc_codec_capabilities[] = {
    0xFF,//(AVDTP_SBC_44100 << 4) | AVDTP_SBC_STEREO,
    0xFF,//(AVDTP_SBC_BLOCK_LENGTH_16 << 4) | (AVDTP_SBC_SUBBANDS_8 << 2) | AVDTP_SBC_ALLOCATION_METHOD_LOUDNESS,
    2, 53
}; 

static uint8_t media_sbc_codec_configuration[] = {
    (AVDTP_SBC_44100 << 4) | AVDTP_SBC_STEREO,
    (AVDTP_SBC_BLOCK_LENGTH_16 << 4) | (AVDTP_SBC_SUBBANDS_8 << 2) | AVDTP_SBC_ALLOCATION_METHOD_LOUDNESS,
    2, 53
}; 

static btstack_resample_t resample_instance;

/* @section Main Application Setup
 *
 * @text The Listing MainConfiguration shows how to setup AD2P Sink and AVRCP controller services. 
 * To announce A2DP Sink and AVRCP Controller services, you need to create corresponding
 * SDP records and register them with the SDP service. 
 * You'll also need to register several packet handlers:
 * - a2dp_sink_packet_handler - handles events on stream connection status (established, released), the media codec configuration, and, the status of the stream itself (opened, paused, stopped).
 * - handle_l2cap_media_data_packet - used to receive streaming data. If STORE_TO_WAV_FILE directive (check btstack_config.h) is used, the SBC decoder will be used to decode the SBC data into PCM frames. The resulting PCM frames are then processed in the SBC Decoder callback.
 * - stdin_process callback - used to trigger AVRCP commands to the A2DP Source device, such are get now playing info, start, stop, volume control. Requires HAVE_BTSTACK_STDIN.
 * - avrcp_controller_packet_handler - used to receive answers for AVRCP commands,
 *
 * @text Note, currently only the SBC codec is supported. 
 * If you want to store the audio data in a file, you'll need to define STORE_TO_WAV_FILE. 
 * If STORE_TO_WAV_FILE directive is defined, the SBC decoder needs to get initialized when a2dp_sink_packet_handler receives event A2DP_SUBEVENT_STREAM_STARTED. 
 * The initialization of the SBC decoder requires a callback that handles PCM data:
 * - handle_pcm_data - handles PCM audio frames. Here, they are stored a in wav file if STORE_TO_WAV_FILE is defined, and/or played using the audio library.
 */

/* LISTING_START(MainConfiguration): Setup Audio Sink and AVRCP Controller services */
static void a2dp_sink_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t * event, uint16_t event_size);
static void avrcp_controller_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void avrcp_target_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void hci_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void handle_l2cap_media_data_packet(uint8_t seid, uint8_t *packet, uint16_t size);

static int a2dp_and_avrcp_setup(void){

    l2cap_init();
    // Initialize AVDTP Sink
    a2dp_sink_init();
    a2dp_sink_register_packet_handler(&a2dp_sink_packet_handler);
    a2dp_sink_register_media_handler(&handle_l2cap_media_data_packet);

    uint8_t status = a2dp_sink_create_stream_endpoint(AVDTP_AUDIO, 
        AVDTP_CODEC_SBC, media_sbc_codec_capabilities, sizeof(media_sbc_codec_capabilities),
        media_sbc_codec_configuration, sizeof(media_sbc_codec_configuration), &a2dp_local_seid);
    if (status != ERROR_CODE_SUCCESS){
        printf("A2DP  Sink      : not enough memory to create local stream endpoint\n");
        return 1;
    }
    // Initialize AVRCP Controller
    avrcp_controller_init();
    avrcp_controller_register_packet_handler(&avrcp_controller_packet_handler);
    
     // Initialize AVRCP Target
    avrcp_target_init();
    avrcp_target_register_packet_handler(&avrcp_target_packet_handler);
    
    // Initialize SDP 
    sdp_init();
    // setup AVDTP sink
    memset(sdp_avdtp_sink_service_buffer, 0, sizeof(sdp_avdtp_sink_service_buffer));
    a2dp_sink_create_sdp_record(sdp_avdtp_sink_service_buffer, 0x10001, 1, NULL, NULL);
    sdp_register_service(sdp_avdtp_sink_service_buffer);
    
    // setup AVRCP Controller
    memset(sdp_avrcp_controller_service_buffer, 0, sizeof(sdp_avrcp_controller_service_buffer));
    uint16_t controller_supported_features = (1 << AVRCP_CONTROLLER_SUPPORTED_FEATURE_CATEGORY_PLAYER_OR_RECORDER);
#ifdef AVRCP_BROWSING_ENABLED
    controller_supported_features |= (1 << AVRCP_CONTROLLER_SUPPORTED_FEATURE_BROWSING);
#endif
    avrcp_controller_create_sdp_record(sdp_avrcp_controller_service_buffer, 0x10002, controller_supported_features, NULL, NULL);
    sdp_register_service(sdp_avrcp_controller_service_buffer);
    
    // setup AVRCP Target
    memset(sdp_avrcp_target_service_buffer, 0, sizeof(sdp_avrcp_target_service_buffer));
    uint16_t target_supported_features = (1 << AVRCP_TARGET_SUPPORTED_FEATURE_CATEGORY_MONITOR_OR_AMPLIFIER);
    avrcp_target_create_sdp_record(sdp_avrcp_target_service_buffer, 0x10003, target_supported_features, NULL, NULL);
    sdp_register_service(sdp_avrcp_target_service_buffer);

    gap_set_local_name("A2DP Sink Demo 00:00:00:00:00:00");
    gap_discoverable_control(1);
    gap_set_class_of_device(0x200408);

    /* Register for HCI events */
    hci_event_callback_registration.callback = &hci_packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

#ifdef HAVE_POSIX_FILE_IO
    if (!btstack_audio_sink_get_instance()){
        printf("No audio playback.\n");
    } else {
        printf("Audio playback supported.\n");
    }
#ifdef STORE_TO_WAV_FILE 
   printf("Audio will be stored to \'%s\' file.\n",  wav_filename);
#endif
#endif
    return 0;
}

static void playback_handler(int16_t * buffer, uint16_t num_frames){

#ifdef STORE_TO_WAV_FILE
    int       wav_samples = num_frames * NUM_CHANNELS;
    int16_t * wav_buffer  = buffer;
#endif
    
    // called from lower-layer but guaranteed to be on main thread
    if (sbc_frame_size == 0){
        memset(buffer, 0, num_frames * BYTES_PER_FRAME);
        return;
    }

    // first fill from resampled audio
    uint32_t bytes_read;
    btstack_ring_buffer_read(&decoded_audio_ring_buffer, (uint8_t *) buffer, num_frames * BYTES_PER_FRAME, &bytes_read);
    buffer          += bytes_read / NUM_CHANNELS;
    num_frames   -= bytes_read / BYTES_PER_FRAME;

    // then start decoding sbc frames using request_* globals
    request_buffer = buffer;
    request_frames = num_frames;
    while (request_frames && btstack_ring_buffer_bytes_available(&sbc_frame_ring_buffer) >= sbc_frame_size){
        // decode frame
        uint8_t sbc_frame[MAX_SBC_FRAME_SIZE];
        btstack_ring_buffer_read(&sbc_frame_ring_buffer, sbc_frame, sbc_frame_size, &bytes_read);
        btstack_sbc_decoder_process_data(&state, 0, sbc_frame, sbc_frame_size);
    }

#ifdef STORE_TO_WAV_FILE
    wav_writer_write_int16(wav_samples, wav_buffer);
#endif
}

static void handle_pcm_data(int16_t * data, int num_frames, int num_channels, int sample_rate, void * context){
    UNUSED(sample_rate);
    UNUSED(context);
    UNUSED(num_channels);   // must be stereo == 2

    const btstack_audio_sink_t * audio_sink = btstack_audio_sink_get_instance();
    if (!audio_sink){
#ifdef STORE_TO_WAV_FILE
        wav_writer_write_int16(num_frames * NUM_CHANNELS, data);
#endif
        return;
    }

    // resample into request buffer - add some additional space for resampling
    int16_t  output_buffer[(128+16) * NUM_CHANNELS]; // 16 * 8 * 2
    uint32_t resampled_frames = btstack_resample_block(&resample_instance, data, num_frames, output_buffer);

    // store data in btstack_audio buffer first
    int frames_to_copy = btstack_min(resampled_frames, request_frames);
    memcpy(request_buffer, output_buffer, frames_to_copy * BYTES_PER_FRAME);
    request_frames  -= frames_to_copy;
    request_buffer  += frames_to_copy * NUM_CHANNELS;

    // and rest in ring buffer
    int frames_to_store = resampled_frames - frames_to_copy;
    if (frames_to_store){
        int status = btstack_ring_buffer_write(&decoded_audio_ring_buffer, (uint8_t *)&output_buffer[frames_to_copy * NUM_CHANNELS], frames_to_store * BYTES_PER_FRAME);
        if (status){
            printf("Error storing samples in PCM ring buffer!!!\n");
        }
    }
}

static int media_processing_init(avdtp_media_codec_configuration_sbc_t configuration){
    if (media_initialized) return 0;

    btstack_sbc_decoder_init(&state, mode, handle_pcm_data, NULL);

#ifdef STORE_TO_WAV_FILE
    wav_writer_open(wav_filename, configuration.num_channels, configuration.sampling_frequency);
#endif

#ifdef STORE_TO_SBC_FILE    
   sbc_file = fopen(sbc_filename, "wb"); 
#endif

    btstack_ring_buffer_init(&sbc_frame_ring_buffer, sbc_frame_storage, sizeof(sbc_frame_storage));
    btstack_ring_buffer_init(&decoded_audio_ring_buffer, decoded_audio_storage, sizeof(decoded_audio_storage));
    btstack_resample_init(&resample_instance, configuration.num_channels);

    // setup audio playback
    const btstack_audio_sink_t * audio = btstack_audio_sink_get_instance();
    if (audio){
        audio->init(NUM_CHANNELS, configuration.sampling_frequency, &playback_handler);
    }

    audio_stream_started = 0;
    media_initialized = 1;
    return 0;
}

static void media_processing_start(void){
    if (!media_initialized) return;
    // setup audio playback
    const btstack_audio_sink_t * audio = btstack_audio_sink_get_instance();
    if (audio){
        audio->start_stream();
    }
    audio_stream_started = 1;
}

static void media_processing_pause(void){
    if (!media_initialized) return;
    // stop audio playback
    audio_stream_started = 0;
    const btstack_audio_sink_t * audio = btstack_audio_sink_get_instance();
    if (audio){
        audio->stop_stream();
    }
}

static void media_processing_close(void){
    if (!media_initialized) return;
    media_initialized = 0;
    audio_stream_started = 0;
    sbc_frame_size = 0;

#ifdef STORE_TO_WAV_FILE                 
    wav_writer_close();
    int total_frames_nr = state.good_frames_nr + state.bad_frames_nr + state.zero_frames_nr;

    printf("WAV Writer: Decoding done. Processed totaly %d frames:\n - %d good\n - %d bad\n", total_frames_nr, state.good_frames_nr, total_frames_nr - state.good_frames_nr);
    printf("WAV Writer: Written %d frames to wav file: %s\n", frame_count, wav_filename);
#endif

#ifdef STORE_TO_SBC_FILE
    fclose(sbc_file);
#endif     

    // stop audio playback
    const btstack_audio_sink_t * audio = btstack_audio_sink_get_instance();
    if (audio){
        printf("close stream\n");
        audio->close();
    }
}

/* @section Handle Media Data Packet 
 *
 * @text Media data packets, in this case the audio data, are received through the handle_l2cap_media_data_packet callback.
 * Currently, only the SBC media codec is supported. Hence, the media data consists of the media packet header and the SBC packet.
 * The SBC frame will be stored in a ring buffer for later processing (instead of decoding it to PCM right away which would require a much larger buffer)
 * If the audio stream wasn't started already and there are enough SBC frames in the ring buffer, start playback.
 */ 

static int read_media_data_header(uint8_t * packet, int size, int * offset, avdtp_media_packet_header_t * media_header);
static int read_sbc_header(uint8_t * packet, int size, int * offset, avdtp_sbc_codec_header_t * sbc_header);

static void handle_l2cap_media_data_packet(uint8_t seid, uint8_t *packet, uint16_t size){
    UNUSED(seid);
    int pos = 0;
     
    avdtp_media_packet_header_t media_header;
    if (!read_media_data_header(packet, size, &pos, &media_header)) return;
    
    avdtp_sbc_codec_header_t sbc_header;
    if (!read_sbc_header(packet, size, &pos, &sbc_header)) return;

#ifdef STORE_TO_SBC_FILE
    fwrite(packet+pos, size-pos, 1, sbc_file);
#endif

    const btstack_audio_sink_t * audio = btstack_audio_sink_get_instance();
    // process data right away if there's no audio implementation active, e.g. on posix systems to store as .wav
    if (!audio){
        btstack_sbc_decoder_process_data(&state, 0, packet+pos, size-pos);
        return;
    }

    // store sbc frame size for buffer management
    sbc_frame_size = (size-pos)/ sbc_header.num_frames;
        
    int status = btstack_ring_buffer_write(&sbc_frame_ring_buffer, packet+pos, size-pos);
    if (status){
        printf("Error storing samples in SBC ring buffer!!!\n");
    }

    // decide on audio sync drift based on number of sbc frames in queue
    int sbc_frames_in_buffer = btstack_ring_buffer_bytes_available(&sbc_frame_ring_buffer) / sbc_frame_size;
    uint32_t resampling_factor;

    // nomimal factor (fixed-point 2^16) and compensation offset
    uint32_t nomimal_factor = 0x0FF80;
    uint32_t compensation   = 0x00100;

    if (sbc_frames_in_buffer < OPTIMAL_FRAMES_MIN){
    	resampling_factor = nomimal_factor - compensation;    // stretch samples
    } else if (sbc_frames_in_buffer <= OPTIMAL_FRAMES_MAX){
    	resampling_factor = nomimal_factor;                   // nothing to do
    } else {
    	resampling_factor = nomimal_factor + compensation;    // compress samples
    }

    btstack_resample_set_factor(&resample_instance, resampling_factor);

    // dump
    // printf("%6u %03u %05x\n",  (int) btstack_run_loop_get_time_ms(), sbc_frames_in_buffer, resampling_factor);
    // log_info("%03u %05x", sbc_frames_in_buffer, resampling_factor);

    // start stream if enough frames buffered
    if (!audio_stream_started && sbc_frames_in_buffer >= OPTIMAL_FRAMES_MIN){
        audio_stream_started = 1;
        // setup audio playback
        if (audio){
            audio->start_stream();
        }
    }
}

static int read_sbc_header(uint8_t * packet, int size, int * offset, avdtp_sbc_codec_header_t * sbc_header){
    int sbc_header_len = 12; // without crc
    int pos = *offset;
    
    if (size - pos < sbc_header_len){
        printf("Not enough data to read SBC header, expected %d, received %d\n", sbc_header_len, size-pos);
        return 0;
    }

    sbc_header->fragmentation = get_bit16(packet[pos], 7);
    sbc_header->starting_packet = get_bit16(packet[pos], 6);
    sbc_header->last_packet = get_bit16(packet[pos], 5);
    sbc_header->num_frames = packet[pos] & 0x0f;
    pos++;
    // printf("SBC HEADER: num_frames %u, fragmented %u, start %u, stop %u\n", sbc_header.num_frames, sbc_header.fragmentation, sbc_header.starting_packet, sbc_header.last_packet);
    *offset = pos;
    return 1;
}

static int read_media_data_header(uint8_t *packet, int size, int *offset, avdtp_media_packet_header_t *media_header){
    int media_header_len = 12; // without crc
    int pos = *offset;
    
    if (size - pos < media_header_len){
        printf("Not enough data to read media packet header, expected %d, received %d\n", media_header_len, size-pos);
        return 0;
    }

    media_header->version = packet[pos] & 0x03;
    media_header->padding = get_bit16(packet[pos],2);
    media_header->extension = get_bit16(packet[pos],3);
    media_header->csrc_count = (packet[pos] >> 4) & 0x0F;
    pos++;

    media_header->marker = get_bit16(packet[pos],0);
    media_header->payload_type  = (packet[pos] >> 1) & 0x7F;
    pos++;

    media_header->sequence_number = big_endian_read_16(packet, pos);
    pos+=2;

    media_header->timestamp = big_endian_read_32(packet, pos);
    pos+=4;

    media_header->synchronization_source = big_endian_read_32(packet, pos);
    pos+=4;
    *offset = pos;
    return 1;
}

static void dump_sbc_configuration(avdtp_media_codec_configuration_sbc_t configuration){
    printf("    - num_channels: %d\n", configuration.num_channels);
    printf("    - sampling_frequency: %d\n", configuration.sampling_frequency);
    printf("    - channel_mode: %d\n", configuration.channel_mode);
    printf("    - block_length: %d\n", configuration.block_length);
    printf("    - subbands: %d\n", configuration.subbands);
    printf("    - allocation_method: %d\n", configuration.allocation_method);
    printf("    - bitpool_value [%d, %d] \n", configuration.min_bitpool_value, configuration.max_bitpool_value);
    printf("\n");
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
                printf("AVRCP Controller: Connection failed, expected 0x%02X cid, received 0x%02X\n", avrcp_controller_cid, local_cid);
                return;
            }

            status = avrcp_subevent_connection_established_get_status(packet);
            if (status != ERROR_CODE_SUCCESS){
                printf("AVRCP Controller: Connection failed: status 0x%02x\n", status);
                avrcp_controller_cid = 0;
                return;
            }
            
            avrcp_controller_cid = local_cid;
            avrcp_connected = 1;
            avrcp_subevent_connection_established_get_bd_addr(packet, adress);
            printf("AVRCP Controller: Connected to %s, cid 0x%02x\n", bd_addr_to_str(adress), avrcp_controller_cid);

            // automatically enable notifications
            avrcp_controller_enable_notification(avrcp_controller_cid, AVRCP_NOTIFICATION_EVENT_PLAYBACK_STATUS_CHANGED);
            avrcp_controller_enable_notification(avrcp_controller_cid, AVRCP_NOTIFICATION_EVENT_NOW_PLAYING_CONTENT_CHANGED);
            avrcp_controller_enable_notification(avrcp_controller_cid, AVRCP_NOTIFICATION_EVENT_TRACK_CHANGED);
            return;
        }
        case AVRCP_SUBEVENT_CONNECTION_RELEASED:
            printf("AVRCP Controller: Channel released: cid 0x%02x\n", avrcp_subevent_connection_released_get_avrcp_cid(packet));
            avrcp_controller_cid = 0;
            avrcp_connected = 0;
            return;
        default:
            break;
    }

    status = packet[5];
    if (!avrcp_controller_cid) return;

    // ignore INTERIM status
    if (status == AVRCP_CTYPE_RESPONSE_INTERIM){
        switch (packet[2]){
            case AVRCP_SUBEVENT_NOTIFICATION_PLAYBACK_POS_CHANGED:{
                uint32_t playback_position_ms = avrcp_subevent_notification_playback_pos_changed_get_playback_position_ms(packet);
                if (playback_position_ms == AVRCP_NO_TRACK_SELECTED_PLAYBACK_POSITION_CHANGED){
                    printf("AVRCP Controller: playback position changed, no track is selected\n");
                }  
                break;
            }
            default:
                break;
        }
        return;
    } 
            
    memset(value, 0, sizeof(value));
    switch (packet[2]){
        case AVRCP_SUBEVENT_NOTIFICATION_PLAYBACK_POS_CHANGED:
            printf("AVRCP Controller: Playback position changed, position %d ms\n", (unsigned int) avrcp_subevent_notification_playback_pos_changed_get_playback_position_ms(packet));
            break;
        case AVRCP_SUBEVENT_NOTIFICATION_PLAYBACK_STATUS_CHANGED:
            printf("AVRCP Controller: Playback status changed %s\n", avrcp_play_status2str(avrcp_subevent_notification_playback_status_changed_get_play_status(packet)));
            return;
        case AVRCP_SUBEVENT_NOTIFICATION_NOW_PLAYING_CONTENT_CHANGED:
            printf("AVRCP Controller: Playing content changed\n");
            return;
        case AVRCP_SUBEVENT_NOTIFICATION_TRACK_CHANGED:
            printf("AVRCP Controller: Track changed\n");
            return;
        case AVRCP_SUBEVENT_NOTIFICATION_VOLUME_CHANGED:
            printf("AVRCP Controller: Absolute volume changed %d\n", avrcp_subevent_notification_volume_changed_get_absolute_volume(packet));
            return;
        case AVRCP_SUBEVENT_NOTIFICATION_AVAILABLE_PLAYERS_CHANGED:
            printf("AVRCP Controller: Changed\n");
            return; 
        case AVRCP_SUBEVENT_SHUFFLE_AND_REPEAT_MODE:{
            uint8_t shuffle_mode = avrcp_subevent_shuffle_and_repeat_mode_get_shuffle_mode(packet);
            uint8_t repeat_mode  = avrcp_subevent_shuffle_and_repeat_mode_get_repeat_mode(packet);
            printf("AVRCP Controller: %s, %s\n", avrcp_shuffle2str(shuffle_mode), avrcp_repeat2str(repeat_mode));
            break;
        }
        case AVRCP_SUBEVENT_NOW_PLAYING_TRACK_INFO:
            printf("AVRCP Controller:     Track: %d\n", avrcp_subevent_now_playing_track_info_get_track(packet));
            break;

        case AVRCP_SUBEVENT_NOW_PLAYING_TOTAL_TRACKS_INFO:
            printf("AVRCP Controller:     Total Tracks: %d\n", avrcp_subevent_now_playing_total_tracks_info_get_total_tracks(packet));
            break;

        case AVRCP_SUBEVENT_NOW_PLAYING_TITLE_INFO:
            if (avrcp_subevent_now_playing_title_info_get_value_len(packet) > 0){
                memcpy(value, avrcp_subevent_now_playing_title_info_get_value(packet), avrcp_subevent_now_playing_title_info_get_value_len(packet));
                printf("AVRCP Controller:     Title: %s\n", value);
            }  
            break;

        case AVRCP_SUBEVENT_NOW_PLAYING_ARTIST_INFO:
            if (avrcp_subevent_now_playing_artist_info_get_value_len(packet) > 0){
                memcpy(value, avrcp_subevent_now_playing_artist_info_get_value(packet), avrcp_subevent_now_playing_artist_info_get_value_len(packet));
                printf("AVRCP Controller:     Artist: %s\n", value);
            }  
            break;
        
        case AVRCP_SUBEVENT_NOW_PLAYING_ALBUM_INFO:
            if (avrcp_subevent_now_playing_album_info_get_value_len(packet) > 0){
                memcpy(value, avrcp_subevent_now_playing_album_info_get_value(packet), avrcp_subevent_now_playing_album_info_get_value_len(packet));
                printf("AVRCP Controller:     Album: %s\n", value);
            }  
            break;
        
        case AVRCP_SUBEVENT_NOW_PLAYING_GENRE_INFO:
            if (avrcp_subevent_now_playing_genre_info_get_value_len(packet) > 0){
                memcpy(value, avrcp_subevent_now_playing_genre_info_get_value(packet), avrcp_subevent_now_playing_genre_info_get_value_len(packet));
                printf("AVRCP Controller:     Genre: %s\n", value);
            }  
            break;
        
        case AVRCP_SUBEVENT_PLAY_STATUS:
            printf("AVRCP Controller: Song length %"PRIu32" ms, Song position %"PRIu32" ms, Play status %s\n", 
                avrcp_subevent_play_status_get_song_length(packet), 
                avrcp_subevent_play_status_get_song_position(packet),
                avrcp_play_status2str(avrcp_subevent_play_status_get_play_status(packet)));
            break;
        
        case AVRCP_SUBEVENT_OPERATION_COMPLETE:
            printf("AVRCP Controller: %s done\n", avrcp_operation2str(avrcp_subevent_operation_complete_get_operation_id(packet)));
            break;
        
        case AVRCP_SUBEVENT_OPERATION_START:
            printf("AVRCP Controller: Start %s\n", avrcp_operation2str(avrcp_subevent_operation_complete_get_operation_id(packet)));
            break;
        
        default:
            printf("AVRCP Controller: Event 0x%02x is not parsed\n", packet[2]);
            break;
    }  
}


static void avrcp_target_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    bd_addr_t address;
    uint16_t local_cid;
    uint8_t  status = ERROR_CODE_SUCCESS;

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_AVRCP_META) return;
    
    int volume;

    switch (packet[2]){
        case AVRCP_SUBEVENT_NOTIFICATION_VOLUME_CHANGED:
            volume = avrcp_subevent_notification_volume_changed_get_absolute_volume(packet);
            volume_percentage = volume * 100 / 127;
            printf("AVRCP Target    : Volume set to %d%% (%d)\n", volume_percentage, volume);
            break;
        case AVRCP_SUBEVENT_CONNECTION_ESTABLISHED: 
            local_cid = avrcp_subevent_connection_established_get_avrcp_cid(packet);
            if (avrcp_target_cid != 0 && avrcp_target_cid != local_cid) {
                printf("AVRCP Target    : Connection failed, expected 0x%02X cid, received 0x%02X\n", avrcp_target_cid, local_cid);
                return;
            }

            status = avrcp_subevent_connection_established_get_status(packet);
            if (status != ERROR_CODE_SUCCESS){
                printf("AVRCP Target    : Connection failed: status 0x%02x\n", status);
                avrcp_target_cid = 0;
                return;
            }
            avrcp_target_cid = local_cid;
            avrcp_subevent_connection_established_get_bd_addr(packet, address);
            printf("AVRCP Target    : Connected to %s, cid 0x%02x\n", bd_addr_to_str(address), avrcp_target_cid);
            break;
        case AVRCP_SUBEVENT_EVENT_IDS_QUERY:
            status = avrcp_target_supported_events(avrcp_target_cid, events_num, events, sizeof(events));
            break;
        case AVRCP_SUBEVENT_COMPANY_IDS_QUERY:
            status = avrcp_target_supported_companies(avrcp_target_cid, companies_num, companies, sizeof(companies));
            break;
        case AVRCP_SUBEVENT_OPERATION:{
            avrcp_operation_id_t operation_id = avrcp_subevent_operation_get_operation_id(packet);
            // if (!media_tracker.connected) break;
            switch (operation_id){
                case AVRCP_OPERATION_ID_PLAY:
                    printf("AVRCP Target    : PLAY\n");
                    break;
                case AVRCP_OPERATION_ID_PAUSE:
                    printf("AVRCP Target    : PAUSE\n");
                    break;
                case AVRCP_OPERATION_ID_STOP:
                    printf("AVRCP Target    : STOP\n");
                    break;
                case AVRCP_OPERATION_ID_REWIND:
                    printf("AVRCP Target    : REWIND\n");
                    break;
                case AVRCP_OPERATION_ID_FAST_FORWARD:
                    printf("AVRCP Target    : FAST_FORWARD\n");
                    break;
                case AVRCP_OPERATION_ID_FORWARD:
                    printf("AVRCP Target    : FORWARD\n");
                    break;
                case AVRCP_OPERATION_ID_BACKWARD:
                    printf("AVRCP Target    : BACKWARD\n");
                    break;
                case AVRCP_OPERATION_ID_SKIP:
                    printf("AVRCP Target    : SKIP\n");
                    break;
                case AVRCP_OPERATION_ID_MUTE:
                    printf("AVRCP Target    : MUTE\n");
                    break;
                case AVRCP_OPERATION_ID_CHANNEL_UP:
                    printf("AVRCP Target    : CHANNEL_UP\n");
                    break;
                case AVRCP_OPERATION_ID_CHANNEL_DOWN:
                    printf("AVRCP Target    : CHANNEL_DOWN\n");
                    break;
                case AVRCP_OPERATION_ID_SELECT:
                    printf("AVRCP Target    : SELECT\n");
                    break;
                case AVRCP_OPERATION_ID_UP:
                    printf("AVRCP Target    : UP\n");
                    break;
                case AVRCP_OPERATION_ID_DOWN:
                    printf("AVRCP Target    : DOWN\n");
                    break;
                case AVRCP_OPERATION_ID_LEFT:
                    printf("AVRCP Target    : LEFT\n");
                    break;
                case AVRCP_OPERATION_ID_RIGHT:
                    printf("AVRCP Target    : RIGTH\n");
                    break;
                case AVRCP_OPERATION_ID_ROOT_MENU:
                    printf("AVRCP Target    : ROOT_MENU\n");
                    break;
                default:
                    return;
            }
            break;
        }
        case AVRCP_SUBEVENT_CONNECTION_RELEASED:
            printf("AVRCP Target    : Disconnected, cid 0x%02x\n", avrcp_subevent_connection_released_get_avrcp_cid(packet));
            return;
        default:
            printf("AVRCP Target    : Event 0x%02x is not parsed\n", packet[2]);
            break;
    }
}

static void hci_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) == HCI_EVENT_PIN_CODE_REQUEST) {
        bd_addr_t address;
        printf("Pin code request - using '0000'\n");
        hci_event_pin_code_request_get_bd_addr(packet, address);
        gap_pin_code_response(address, "0000");
    }
}

static void a2dp_sink_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    uint16_t cid;
    bd_addr_t address;
    uint8_t status;

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_A2DP_META) return;

    switch (packet[2]){
        case A2DP_SUBEVENT_SIGNALING_MEDIA_CODEC_OTHER_CONFIGURATION:
            printf("A2DP  Sink      : Received non SBC codec - not implemented\n");
            break;
        case A2DP_SUBEVENT_SIGNALING_MEDIA_CODEC_SBC_CONFIGURATION:{
            printf("A2DP  Sink      : Received SBC codec configuration\n");
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
            dump_sbc_configuration(sbc_configuration);

            if (sbc_configuration.reconfigure){
                media_processing_close();
            }
            // prepare media processing
            media_processing_init(sbc_configuration);
            break;
        }  
        case A2DP_SUBEVENT_STREAM_ESTABLISHED:
            a2dp_subevent_stream_established_get_bd_addr(packet, address);
            status = a2dp_subevent_stream_established_get_status(packet);
            cid = a2dp_subevent_stream_established_get_a2dp_cid(packet);
            if (!a2dp_cid){
                // incoming connection
                a2dp_cid = cid;
            } else if (cid != a2dp_cid) {
                break;
            }
            if (status){
                printf("A2DP  Sink      : Streaming connection failed, status 0x%02x\n", status);
                break;
            }
            
            memcpy(device_addr, address, 6);
            a2dp_local_seid = a2dp_subevent_stream_established_get_local_seid(packet);
            printf("A2DP  Sink      : Streaming connection is established, address %s, cid 0x%02X, local seid %d\n", bd_addr_to_str(address), a2dp_cid, a2dp_local_seid);
            break;
        
        case A2DP_SUBEVENT_STREAM_STARTED:
            cid = a2dp_subevent_stream_started_get_a2dp_cid(packet);
            if (cid != a2dp_cid) break;
            a2dp_local_seid = a2dp_subevent_stream_started_get_local_seid(packet);
            printf("A2DP  Sink      : Stream started\n");
            media_processing_start();
            break;
        
        case A2DP_SUBEVENT_STREAM_SUSPENDED:
            cid = a2dp_subevent_stream_suspended_get_a2dp_cid(packet);
            if (cid != a2dp_cid) break;
            a2dp_local_seid = a2dp_subevent_stream_suspended_get_local_seid(packet);
            printf("A2DP  Sink      : Stream paused\n");
            media_processing_pause();
            break;
        
        case A2DP_SUBEVENT_STREAM_RELEASED:
            a2dp_local_seid = a2dp_subevent_stream_released_get_local_seid(packet);
            printf("A2DP  Sink      : Stream released\n");
            media_processing_close();
            break;
        case A2DP_SUBEVENT_SIGNALING_CONNECTION_RELEASED:
            cid = a2dp_subevent_signaling_connection_released_get_a2dp_cid(packet);
            printf("A2DP  Sink      : Signaling connection released\n");
            media_processing_close();
            break;
        default:
            printf("A2DP  Sink      : Not parsed 0x%02x\n", packet[2]);
            break; 
    }
}

#ifdef HAVE_BTSTACK_STDIN
static void show_usage(void){
    bd_addr_t      iut_address;
    gap_local_bd_addr(iut_address);
    printf("\n--- Bluetooth AVDTP Sink/AVRCP Connection Test Console %s ---\n", bd_addr_to_str(iut_address));
    printf("b      - AVDTP Sink create  connection to addr %s\n", bd_addr_to_str(device_addr));
    printf("B      - AVDTP Sink disconnect\n");
    printf("c      - AVRCP Controller create connection to addr %s\n", bd_addr_to_str(device_addr));
    printf("C      - AVRCP Controller disconnect\n");
    printf("d      - AVRCP Target create connection to addr %s\n", bd_addr_to_str(device_addr));
    printf("D      - AVRCP Target disconnect\n");

    printf("\n--- Bluetooth AVRCP Commands %s ---\n", bd_addr_to_str(iut_address));
    printf("O - get play status\n");
    printf("j - get now playing info\n");
    printf("k - play\n");
    printf("K - stop\n");
    printf("L - pause\n");
    printf("u - start fast forward\n");
    printf("U - stop  fast forward\n");
    printf("n - start rewind\n");
    printf("N - stop rewind\n");
    printf("i - forward\n");
    printf("I - backward\n");
    printf("M - mute\n");
    printf("r - skip\n");
    printf("q - query repeat and shuffle mode\n");
    printf("v - repeat single track\n");
    printf("x - repeat all tracks\n");
    printf("X - disable repeat mode\n");
    printf("z - shuffle all tracks\n");
    printf("Z - disable shuffle mode\n");

    printf("a/A - register/deregister TRACK_CHANGED\n");
    printf("R/P - register/deregister PLAYBACK_POS_CHANGED\n");

    printf("\n--- Volume Control ---\n");
    printf("t - volume up   for 10 percent\n");
    printf("T - volume down for 10 percent\n");

    printf("---\n");
}
#endif

#ifdef HAVE_BTSTACK_STDIN
static void stdin_process(char cmd){
    uint8_t status = ERROR_CODE_SUCCESS;
    
    switch (cmd){
        case 'b':
            status = a2dp_sink_establish_stream(device_addr, a2dp_local_seid, &a2dp_cid);
            printf(" - Create AVDTP connection to addr %s, and local seid %d, expected cid 0x%02x.\n", bd_addr_to_str(device_addr), a2dp_local_seid, a2dp_cid);
            break;
        case 'B':
            printf(" - AVDTP disconnect from addr %s.\n", bd_addr_to_str(device_addr));
            status = avdtp_sink_disconnect(a2dp_cid);
            break;
        case 'c':
            printf(" - Create AVRCP Controller connection to addr %s.\n", bd_addr_to_str(device_addr));
            status = avrcp_controller_connect(device_addr, &avrcp_controller_cid);
            break;
        case 'C':
            printf(" - AVRCP Controller disconnect from addr %s.\n", bd_addr_to_str(device_addr));
            status = avrcp_controller_disconnect(avrcp_controller_cid);
            break;
        case 'd':
            printf(" - Create AVRCP Target connection to addr %s.\n", bd_addr_to_str(device_addr));
            status = avrcp_target_connect(device_addr, &avrcp_target_cid);
            break;
        case 'D':
            printf(" - AVRCP Target disconnect from addr %s.\n", bd_addr_to_str(device_addr));
            status = avrcp_target_disconnect(avrcp_target_cid);
            break;

        case '\n':
        case '\r':
            break;
        // Volume Control
        case 't':
            volume_percentage = volume_percentage <= 90 ? volume_percentage + 10 : 100;
            printf(" - volume up   for 10 percent, %d%% (%d) \n", volume_percentage, volume_percentage * 127 / 100);
            status = avrcp_target_volume_changed(avrcp_target_cid, volume_percentage * 127 / 100);
            break;
        case 'T':
            volume_percentage = volume_percentage >= 10 ? volume_percentage - 10 : 0;
            printf(" - volume down for 10 percent, %d%% (%d) \n", volume_percentage, volume_percentage * 127 / 100);
            status = avrcp_target_volume_changed(avrcp_target_cid, volume_percentage * 127 / 100);
            break;

        case 'O':
            printf(" - get play status\n");
            status = avrcp_controller_get_play_status(avrcp_controller_cid);
            break;
        case 'j':
            printf(" - get now playing info\n");
            status = avrcp_controller_get_now_playing_info(avrcp_controller_cid);
            break;
        case 'k':
            printf(" - play\n");
            status = avrcp_controller_play(avrcp_controller_cid);
            break;
        case 'K':
            printf(" - stop\n");
            status = avrcp_controller_stop(avrcp_controller_cid);
            break;
        case 'L':
            printf(" - pause\n");
            status = avrcp_controller_pause(avrcp_controller_cid);
            break;
        case 'u':
            printf(" - start fast forward\n");
            status = avrcp_controller_press_and_hold_fast_forward(avrcp_controller_cid);
            break;
        case 'U':
            printf(" - stop fast forward\n");
            status = avrcp_controller_release_press_and_hold_cmd(avrcp_controller_cid);
            break;
        case 'n':
            printf(" - start rewind\n");
            status = avrcp_controller_press_and_hold_rewind(avrcp_controller_cid);
            break;
        case 'N':
            printf(" - stop rewind\n");
            status = avrcp_controller_release_press_and_hold_cmd(avrcp_controller_cid);
            break;
        case 'i':
            printf(" - forward\n");
            status = avrcp_controller_forward(avrcp_controller_cid); 
            break;
        case 'I':
            printf(" - backward\n");
            status = avrcp_controller_backward(avrcp_controller_cid);
            break;
        case 'M':
            printf(" - mute\n");
            status = avrcp_controller_mute(avrcp_controller_cid);
            break;
        case 'r':
            printf(" - skip\n");
            status = avrcp_controller_skip(avrcp_controller_cid);
            break;
        case 'q':
            printf(" - query repeat and shuffle mode\n");
            status = avrcp_controller_query_shuffle_and_repeat_modes(avrcp_controller_cid);
            break;
        case 'v':
            printf(" - repeat single track\n");
            status = avrcp_controller_set_repeat_mode(avrcp_controller_cid, AVRCP_REPEAT_MODE_SINGLE_TRACK);
            break;
        case 'x':
            printf(" - repeat all tracks\n");
            status = avrcp_controller_set_repeat_mode(avrcp_controller_cid, AVRCP_REPEAT_MODE_ALL_TRACKS);
            break;
        case 'X':
            printf(" - disable repeat mode\n");
            status = avrcp_controller_set_repeat_mode(avrcp_controller_cid, AVRCP_REPEAT_MODE_OFF);
            break;
        case 'z':
            printf(" - shuffle all tracks\n");
            status = avrcp_controller_set_shuffle_mode(avrcp_controller_cid, AVRCP_SHUFFLE_MODE_ALL_TRACKS);
            break;
        case 'Z':
            printf(" - disable shuffle mode\n");
            status = avrcp_controller_set_shuffle_mode(avrcp_controller_cid, AVRCP_SHUFFLE_MODE_OFF);
            break;
        case 'a':
            printf("AVRCP: enable notification TRACK_CHANGED\n");
            avrcp_controller_enable_notification(avrcp_controller_cid, AVRCP_NOTIFICATION_EVENT_TRACK_CHANGED);
            break;
        case 'A':
            printf("AVRCP: disable notification TRACK_CHANGED\n");
            avrcp_controller_disable_notification(avrcp_controller_cid, AVRCP_NOTIFICATION_EVENT_TRACK_CHANGED);
            break;
        case 'R':
            printf("AVRCP: enable notification PLAYBACK_POS_CHANGED\n");
            avrcp_controller_enable_notification(avrcp_controller_cid, AVRCP_NOTIFICATION_EVENT_PLAYBACK_POS_CHANGED);
            break;
        case 'P':
            printf("AVRCP: disable notification PLAYBACK_POS_CHANGED\n");
            avrcp_controller_disable_notification(avrcp_controller_cid, AVRCP_NOTIFICATION_EVENT_PLAYBACK_POS_CHANGED);
            break;

        default:
            show_usage();
            return;
    }
    if (status != ERROR_CODE_SUCCESS){
        printf("Could not perform command, status 0x%2x\n", status);
    }
}
#endif

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    UNUSED(argc);
    (void)argv;

    a2dp_and_avrcp_setup();

#ifdef HAVE_BTSTACK_STDIN
    // parse human readable Bluetooth address
    sscanf_bd_addr(device_addr_string, device_addr);
    btstack_stdin_setup(stdin_process);
#endif

    // turn on!
    printf("Starting BTstack ...\n");
    hci_power_control(HCI_POWER_ON);
    return 0;
}
