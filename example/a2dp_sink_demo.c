/*
 * Copyright (C) 2023 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "a2dp_sink_demo.c"

/*
 * a2dp_sink_demo.c
 */

// *****************************************************************************
/* EXAMPLE_START(a2dp_sink_demo): A2DP Sink - Receive Audio Stream and Control Playback
 *
 * @text This A2DP Sink example demonstrates how to use the A2DP Sink service to 
 * receive an audio data stream from a remote A2DP Source device. In addition,
 * the AVRCP Controller is used to get information on currently played media, 
 * such are title, artist and album, as well as to control the playback, 
 * i.e. to play, stop, repeat, etc. If HAVE_BTSTACK_STDIN is set, press SPACE on 
 * the console to show the available AVDTP and AVRCP commands.
 *
 * @text To test with a remote device, e.g. a mobile phone,
 * pair from the remote device with the demo, then start playing music on the remote device.
 * Alternatively, set the device_addr_string to the Bluetooth address of your 
 * remote device in the code, and call connect from the UI.
 * 
 * @text For more info on BTstack audio, see our blog post 
 * [A2DP Sink and Source on STM32 F4 Discovery Board](http://bluekitchen-gmbh.com/a2dp-sink-and-source-on-stm32-f4-discovery-board/).
 * 
 */
// *****************************************************************************

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "btstack.h"

#include "btstack_resample.h"

//#define AVRCP_BROWSING_ENABLED

#ifdef HAVE_BTSTACK_STDIN
#include "btstack_stdin.h"
#endif

#ifdef HAVE_BTSTACK_AUDIO_EFFECTIVE_SAMPLERATE
#include "btstack_sample_rate_compensation.h"
#endif

#include "btstack_ring_buffer.h"

#ifdef HAVE_POSIX_FILE_IO
#include "wav_util.h"
#define STORE_TO_WAV_FILE
#ifdef _MSC_VER
// ignore deprecated warning for fopen
#pragma warning(disable : 4996)
#endif
#endif

#define NUM_CHANNELS 2
#define BYTES_PER_FRAME     (2*NUM_CHANNELS)
#define MAX_SBC_FRAME_SIZE 120

#ifdef HAVE_BTSTACK_STDIN
static const char * device_addr_string = "00:1B:DC:08:E2:72"; // pts v5.0
static bd_addr_t device_addr;
#endif

#ifdef HAVE_BTSTACK_AUDIO_EFFECTIVE_SAMPLERATE
static btstack_sample_rate_compensation_t sample_rate_compensation;
#endif

static btstack_packet_callback_registration_t hci_event_callback_registration;

static uint8_t  sdp_avdtp_sink_service_buffer[150];
static uint8_t  sdp_avrcp_target_service_buffer[150];
static uint8_t  sdp_avrcp_controller_service_buffer[200];
static uint8_t  device_id_sdp_service_buffer[100];

// we support all configurations with bitpool 2-53
static uint8_t media_sbc_codec_capabilities[] = {
    0xFF,//(AVDTP_SBC_44100 << 4) | AVDTP_SBC_STEREO,
    0xFF,//(AVDTP_SBC_BLOCK_LENGTH_16 << 4) | (AVDTP_SBC_SUBBANDS_8 << 2) | AVDTP_SBC_ALLOCATION_METHOD_LOUDNESS,
    2, 53
};

// WAV File
#ifdef STORE_TO_WAV_FILE
static uint32_t audio_frame_count = 0;
static char * wav_filename = "a2dp_sink_demo.wav";
#endif

// SBC Decoder for WAV file or live playback
static const btstack_sbc_decoder_t *   sbc_decoder_instance;
static btstack_sbc_decoder_bluedroid_t sbc_decoder_context;

// ring buffer for SBC Frames
// below 30: add samples, 30-40: fine, above 40: drop samples
#define OPTIMAL_FRAMES_MIN 60
#define OPTIMAL_FRAMES_MAX 80
#define ADDITIONAL_FRAMES  30
static uint8_t sbc_frame_storage[(OPTIMAL_FRAMES_MAX + ADDITIONAL_FRAMES) * MAX_SBC_FRAME_SIZE];
static btstack_ring_buffer_t sbc_frame_ring_buffer;
static unsigned int sbc_frame_size;

// overflow buffer for not fully used sbc frames, with additional frames for resampling
static uint8_t decoded_audio_storage[(128+16) * BYTES_PER_FRAME];
static btstack_ring_buffer_t decoded_audio_ring_buffer;

static int media_initialized = 0;
static int audio_stream_started;
#ifdef HAVE_BTSTACK_AUDIO_EFFECTIVE_SAMPLERATE
static int l2cap_stream_started;
#endif
static btstack_resample_t resample_instance;

// temp storage of lower-layer request for audio samples
static int16_t * request_buffer;
static int       request_frames;

// sink state
static int volume_percentage = 0;
static avrcp_battery_status_t battery_status = AVRCP_BATTERY_STATUS_WARNING;

#ifdef ENABLE_AVRCP_COVER_ART
static char a2dp_sink_demo_image_handle[8];
static avrcp_cover_art_client_t a2dp_sink_demo_cover_art_client;
static bool a2dp_sink_demo_cover_art_client_connected;
static uint16_t a2dp_sink_demo_cover_art_cid;
static uint8_t a2dp_sink_demo_ertm_buffer[2000];
static l2cap_ertm_config_t a2dp_sink_demo_ertm_config = {
        1,  // ertm mandatory
        2,  // max transmit, some tests require > 1
        2000,
        12000,
        512,    // l2cap ertm mtu
        2,
        2,
        1,      // 16-bit FCS
};
static bool a2dp_sink_cover_art_download_active;
static uint32_t a2dp_sink_cover_art_file_size;
#ifdef HAVE_POSIX_FILE_IO
static const char * a2dp_sink_demo_thumbnail_path = "cover.jpg";
static FILE * a2dp_sink_cover_art_file;
#endif
#endif

typedef struct {
    uint8_t  reconfigure;
    uint8_t  num_channels;
    uint16_t sampling_frequency;
    uint8_t  block_length;
    uint8_t  subbands;
    uint8_t  min_bitpool_value;
    uint8_t  max_bitpool_value;
    btstack_sbc_channel_mode_t      channel_mode;
    btstack_sbc_allocation_method_t allocation_method;
} media_codec_configuration_sbc_t;

typedef enum {
    STREAM_STATE_CLOSED,
    STREAM_STATE_OPEN,
    STREAM_STATE_PLAYING,
    STREAM_STATE_PAUSED,
} stream_state_t;

typedef struct {
    uint8_t  a2dp_local_seid;
    uint8_t  media_sbc_codec_configuration[4];
} a2dp_sink_demo_stream_endpoint_t;
static a2dp_sink_demo_stream_endpoint_t a2dp_sink_demo_stream_endpoint;

typedef struct {
    bd_addr_t addr;
    uint16_t  a2dp_cid;
    uint8_t   a2dp_local_seid;
    stream_state_t stream_state;
    media_codec_configuration_sbc_t sbc_configuration;
} a2dp_sink_demo_a2dp_connection_t;
static a2dp_sink_demo_a2dp_connection_t a2dp_sink_demo_a2dp_connection;

typedef struct {
    bd_addr_t addr;
    uint16_t  avrcp_cid;
    bool playing;
    uint16_t notifications_supported_by_target;
} a2dp_sink_demo_avrcp_connection_t;
static a2dp_sink_demo_avrcp_connection_t a2dp_sink_demo_avrcp_connection;

/* @section Main Application Setup
 *
 * @text The Listing MainConfiguration shows how to set up AD2P Sink and AVRCP services.
 * Besides calling init() method for each service, you'll also need to register several packet handlers:
 * - hci_packet_handler - handles legacy pairing, here by using fixed '0000' pin code.
 * - a2dp_sink_packet_handler - handles events on stream connection status (established, released), the media codec configuration, and, the status of the stream itself (opened, paused, stopped).
 * - handle_l2cap_media_data_packet - used to receive streaming data. If STORE_TO_WAV_FILE directive (check btstack_config.h) is used, the SBC decoder will be used to decode the SBC data into PCM frames. The resulting PCM frames are then processed in the SBC Decoder callback.
 * - avrcp_packet_handler - receives AVRCP connect/disconnect event.
 * - avrcp_controller_packet_handler - receives answers for sent AVRCP commands.
 * - avrcp_target_packet_handler - receives AVRCP commands, and registered notifications.
 * - stdin_process - used to trigger AVRCP commands to the A2DP Source device, such are get now playing info, start, stop, volume control. Requires HAVE_BTSTACK_STDIN.
 *
 * @text To announce A2DP Sink and AVRCP services, you need to create corresponding
 * SDP records and register them with the SDP service. 
 *
 * @text Note, currently only the SBC codec is supported. 
 * If you want to store the audio data in a file, you'll need to define STORE_TO_WAV_FILE. 
 * If STORE_TO_WAV_FILE directive is defined, the SBC decoder needs to get initialized when a2dp_sink_packet_handler receives event A2DP_SUBEVENT_STREAM_STARTED. 
 * The initialization of the SBC decoder requires a callback that handles PCM data:
 * - handle_pcm_data - handles PCM audio frames. Here, they are stored in a wav file if STORE_TO_WAV_FILE is defined, and/or played using the audio library.
 */

/* LISTING_START(MainConfiguration): Setup Audio Sink and AVRCP services */
static void hci_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void a2dp_sink_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t * packet, uint16_t event_size);
static void handle_l2cap_media_data_packet(uint8_t seid, uint8_t *packet, uint16_t size);
static void avrcp_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void avrcp_controller_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void avrcp_target_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
#ifdef HAVE_BTSTACK_STDIN
static void stdin_process(char cmd);
#endif

static int setup_demo(void){

    // init protocols
    l2cap_init();
    sdp_init();
#ifdef ENABLE_BLE
    // Initialize LE Security Manager. Needed for cross-transport key derivation
    sm_init();
#endif
#ifdef ENABLE_AVRCP_COVER_ART
    goep_client_init();
    avrcp_cover_art_client_init();
#endif

    // Init profiles
    a2dp_sink_init();
    avrcp_init();
    avrcp_controller_init();
    avrcp_target_init();


    // Configure A2DP Sink
    a2dp_sink_register_packet_handler(&a2dp_sink_packet_handler);
    a2dp_sink_register_media_handler(&handle_l2cap_media_data_packet);
    a2dp_sink_demo_stream_endpoint_t * stream_endpoint = &a2dp_sink_demo_stream_endpoint;
    avdtp_stream_endpoint_t * local_stream_endpoint = a2dp_sink_create_stream_endpoint(AVDTP_AUDIO,
                                                                                       AVDTP_CODEC_SBC, media_sbc_codec_capabilities, sizeof(media_sbc_codec_capabilities),
                                                                                       stream_endpoint->media_sbc_codec_configuration, sizeof(stream_endpoint->media_sbc_codec_configuration));
    btstack_assert(local_stream_endpoint != NULL);
    // - Store stream enpoint's SEP ID, as it is used by A2DP API to identify the stream endpoint
    stream_endpoint->a2dp_local_seid = avdtp_local_seid(local_stream_endpoint);


    // Configure AVRCP Controller + Target
    avrcp_register_packet_handler(&avrcp_packet_handler);
    avrcp_controller_register_packet_handler(&avrcp_controller_packet_handler);
    avrcp_target_register_packet_handler(&avrcp_target_packet_handler);
    

    // Configure SDP

    // - Create and register A2DP Sink service record
    memset(sdp_avdtp_sink_service_buffer, 0, sizeof(sdp_avdtp_sink_service_buffer));
    a2dp_sink_create_sdp_record(sdp_avdtp_sink_service_buffer, sdp_create_service_record_handle(),
                                AVDTP_SINK_FEATURE_MASK_HEADPHONE, NULL, NULL);
    btstack_assert(de_get_len( sdp_avdtp_sink_service_buffer) <= sizeof(sdp_avdtp_sink_service_buffer));
    sdp_register_service(sdp_avdtp_sink_service_buffer);

    // - Create AVRCP Controller service record and register it with SDP. We send Category 1 commands to the media player, e.g. play/pause
    memset(sdp_avrcp_controller_service_buffer, 0, sizeof(sdp_avrcp_controller_service_buffer));
    uint16_t controller_supported_features = 1 << AVRCP_CONTROLLER_SUPPORTED_FEATURE_CATEGORY_PLAYER_OR_RECORDER;
#ifdef AVRCP_BROWSING_ENABLED
    controller_supported_features |= 1 << AVRCP_CONTROLLER_SUPPORTED_FEATURE_BROWSING;
#endif
#ifdef ENABLE_AVRCP_COVER_ART
    controller_supported_features |= 1 << AVRCP_CONTROLLER_SUPPORTED_FEATURE_COVER_ART_GET_LINKED_THUMBNAIL;
#endif
    avrcp_controller_create_sdp_record(sdp_avrcp_controller_service_buffer, sdp_create_service_record_handle(),
                                       controller_supported_features, NULL, NULL);
    btstack_assert(de_get_len( sdp_avrcp_controller_service_buffer) <= sizeof(sdp_avrcp_controller_service_buffer));
    sdp_register_service(sdp_avrcp_controller_service_buffer);

    // - Create and register A2DP Sink service record
    //   -  We receive Category 2 commands from the media player, e.g. volume up/down
    memset(sdp_avrcp_target_service_buffer, 0, sizeof(sdp_avrcp_target_service_buffer));
    uint16_t target_supported_features = 1 << AVRCP_TARGET_SUPPORTED_FEATURE_CATEGORY_MONITOR_OR_AMPLIFIER;
    avrcp_target_create_sdp_record(sdp_avrcp_target_service_buffer,
                                   sdp_create_service_record_handle(), target_supported_features, NULL, NULL);
    btstack_assert(de_get_len( sdp_avrcp_target_service_buffer) <= sizeof(sdp_avrcp_target_service_buffer));
    sdp_register_service(sdp_avrcp_target_service_buffer);

    // - Create and register Device ID (PnP) service record
    memset(device_id_sdp_service_buffer, 0, sizeof(device_id_sdp_service_buffer));
    device_id_create_sdp_record(device_id_sdp_service_buffer,
                                sdp_create_service_record_handle(), DEVICE_ID_VENDOR_ID_SOURCE_BLUETOOTH, BLUETOOTH_COMPANY_ID_BLUEKITCHEN_GMBH, 1, 1);
    btstack_assert(de_get_len( device_id_sdp_service_buffer) <= sizeof(device_id_sdp_service_buffer));
    sdp_register_service(device_id_sdp_service_buffer);


    // Configure GAP - discovery / connection

    // - Set local name with a template Bluetooth address, that will be automatically
    //   replaced with an actual address once it is available, i.e. when BTstack boots
    //   up and starts talking to a Bluetooth module.
    gap_set_local_name("A2DP Sink Demo 00:00:00:00:00:00");

    // - Allow to show up in Bluetooth inquiry
    gap_discoverable_control(1);

    // - Set Class of Device - Service Class: Audio, Major Device Class: Audio, Minor: Headphone
    gap_set_class_of_device(0x200404);

    // - Allow for role switch in general and sniff mode
    gap_set_default_link_policy_settings( LM_LINK_POLICY_ENABLE_ROLE_SWITCH | LM_LINK_POLICY_ENABLE_SNIFF_MODE );

    // - Allow for role switch on outgoing connections
    //   - This allows A2DP Source, e.g. smartphone, to become master when we re-connect to it.
    gap_set_allow_role_switch(true);


    // Register for HCI events
    hci_event_callback_registration.callback = &hci_packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // Inform about audio playback / test options
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
/* LISTING_END */


static void playback_handler(int16_t * buffer, uint16_t num_audio_frames){

#ifdef STORE_TO_WAV_FILE
    int       wav_samples = num_audio_frames * NUM_CHANNELS;
    int16_t * wav_buffer  = buffer;
#endif
    
    // called from lower-layer but guaranteed to be on main thread
    if (sbc_frame_size == 0){
        memset(buffer, 0, num_audio_frames * BYTES_PER_FRAME);
        return;
    }

    // first fill from resampled audio
    uint32_t bytes_read;
    btstack_ring_buffer_read(&decoded_audio_ring_buffer, (uint8_t *) buffer, num_audio_frames * BYTES_PER_FRAME, &bytes_read);
    buffer          += bytes_read / NUM_CHANNELS;
    num_audio_frames   -= bytes_read / BYTES_PER_FRAME;

    // then start decoding sbc frames using request_* globals
    request_buffer = buffer;
    request_frames = num_audio_frames;
    while (request_frames && btstack_ring_buffer_bytes_available(&sbc_frame_ring_buffer) >= sbc_frame_size){
        // decode frame
        uint8_t sbc_frame[MAX_SBC_FRAME_SIZE];
        btstack_ring_buffer_read(&sbc_frame_ring_buffer, sbc_frame, sbc_frame_size, &bytes_read);
        sbc_decoder_instance->decode_signed_16(&sbc_decoder_context, 0, sbc_frame, sbc_frame_size);
    }

#ifdef STORE_TO_WAV_FILE
    audio_frame_count += num_audio_frames;
    wav_writer_write_int16(wav_samples, wav_buffer);
#endif
}

static void handle_pcm_data(int16_t * data, int num_audio_frames, int num_channels, int sample_rate, void * context){
    UNUSED(sample_rate);
    UNUSED(context);
    UNUSED(num_channels);   // must be stereo == 2

    const btstack_audio_sink_t * audio_sink = btstack_audio_sink_get_instance();
    if (!audio_sink){
#ifdef STORE_TO_WAV_FILE
        audio_frame_count += num_audio_frames;
        wav_writer_write_int16(num_audio_frames * NUM_CHANNELS, data);
#endif
        return;
    }

    // resample into request buffer - add some additional space for resampling
    int16_t  output_buffer[(128+16) * NUM_CHANNELS]; // 16 * 8 * 2
    uint32_t resampled_frames = btstack_resample_block(&resample_instance, data, num_audio_frames, output_buffer);

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

static int media_processing_init(media_codec_configuration_sbc_t * configuration){
    if (media_initialized) return 0;
    sbc_decoder_instance = btstack_sbc_decoder_bluedroid_init_instance(&sbc_decoder_context);
    sbc_decoder_instance->configure(&sbc_decoder_context, SBC_MODE_STANDARD, handle_pcm_data, NULL);

#ifdef STORE_TO_WAV_FILE
    wav_writer_open(wav_filename, configuration->num_channels, configuration->sampling_frequency);
#endif

    btstack_ring_buffer_init(&sbc_frame_ring_buffer, sbc_frame_storage, sizeof(sbc_frame_storage));
    btstack_ring_buffer_init(&decoded_audio_ring_buffer, decoded_audio_storage, sizeof(decoded_audio_storage));
    btstack_resample_init(&resample_instance, configuration->num_channels);

    // setup audio playback
    const btstack_audio_sink_t * audio = btstack_audio_sink_get_instance();
    if (audio){
        audio->init(NUM_CHANNELS, configuration->sampling_frequency, &playback_handler);
    }

    audio_stream_started = 0;
    media_initialized = 1;
    return 0;
}

static void media_processing_start(void){
    if (!media_initialized) return;

#ifdef HAVE_BTSTACK_AUDIO_EFFECTIVE_SAMPLERATE
    btstack_sample_rate_compensation_reset( &sample_rate_compensation, btstack_run_loop_get_time_ms() );
#endif
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
#ifdef HAVE_BTSTACK_AUDIO_EFFECTIVE_SAMPLERATE
    l2cap_stream_started = 0;
#endif

    const btstack_audio_sink_t * audio = btstack_audio_sink_get_instance();
    if (audio){
        audio->stop_stream();
    }
    // discard pending data
    btstack_ring_buffer_reset(&decoded_audio_ring_buffer);
    btstack_ring_buffer_reset(&sbc_frame_ring_buffer);
}

static void media_processing_close(void){
    if (!media_initialized) return;

    media_initialized = 0;
    audio_stream_started = 0;
    sbc_frame_size = 0;
#ifdef HAVE_BTSTACK_AUDIO_EFFECTIVE_SAMPLERATE
    l2cap_stream_started = 0;
#endif

#ifdef STORE_TO_WAV_FILE                 
    wav_writer_close();
    uint32_t total_frames_nr = sbc_decoder_context.good_frames_nr + sbc_decoder_context.bad_frames_nr + sbc_decoder_context.zero_frames_nr;

    printf("WAV Writer: Decoding done. Processed %u SBC frames:\n - %d good\n - %d bad\n", total_frames_nr, sbc_decoder_context.good_frames_nr, total_frames_nr - sbc_decoder_context.good_frames_nr);
    printf("WAV Writer: Wrote %u audio frames to wav file: %s\n", audio_frame_count, wav_filename);
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
 * @text Here the audio data, are received through the handle_l2cap_media_data_packet callback.
 * Currently, only the SBC media codec is supported. Hence, the media data consists of the media packet header and the SBC packet.
 * The SBC frame will be stored in a ring buffer for later processing (instead of decoding it to PCM right away which would require a much larger buffer).
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

    int packet_length = size-pos;
    uint8_t *packet_begin = packet+pos;

    const btstack_audio_sink_t * audio = btstack_audio_sink_get_instance();
    // process data right away if there's no audio implementation active, e.g. on posix systems to store as .wav
    if (!audio){
        sbc_decoder_instance->decode_signed_16(&sbc_decoder_context, 0, packet_begin, packet_length);
        return;
    }

    // store sbc frame size for buffer management
    sbc_frame_size = packet_length / sbc_header.num_frames;
    int status = btstack_ring_buffer_write(&sbc_frame_ring_buffer, packet_begin, packet_length);
    if (status != ERROR_CODE_SUCCESS){
        printf("Error storing samples in SBC ring buffer!!!\n");
    }

    // decide on audio sync drift based on number of sbc frames in queue
    int sbc_frames_in_buffer = btstack_ring_buffer_bytes_available(&sbc_frame_ring_buffer) / sbc_frame_size;
#ifdef HAVE_BTSTACK_AUDIO_EFFECTIVE_SAMPLERATE
    if( !l2cap_stream_started && audio_stream_started ) {
        l2cap_stream_started = 1;
        btstack_sample_rate_compensation_init( &sample_rate_compensation, btstack_run_loop_get_time_ms(), a2dp_sink_demo_a2dp_connection.sbc_configuration.sampling_frequency, FLOAT_TO_Q15(1.f) );
    }
    // update sample rate compensation
    if( audio_stream_started && (audio != NULL)) {
        uint32_t resampling_factor = btstack_sample_rate_compensation_update( &sample_rate_compensation, btstack_run_loop_get_time_ms(), sbc_header.num_frames*128, audio->get_samplerate() );
        btstack_resample_set_factor(&resample_instance, resampling_factor);
//        printf("sbc buffer level :            %"PRIu32"\n", btstack_ring_buffer_bytes_available(&sbc_frame_ring_buffer));
    }
#else
    uint32_t resampling_factor;

    // nominal factor (fixed-point 2^16) and compensation offset
    uint32_t nominal_factor = 0x10000;
    uint32_t compensation   = 0x00100;

    if (sbc_frames_in_buffer < OPTIMAL_FRAMES_MIN){
    	resampling_factor = nominal_factor - compensation;    // stretch samples
    } else if (sbc_frames_in_buffer <= OPTIMAL_FRAMES_MAX){
    	resampling_factor = nominal_factor;                   // nothing to do
    } else {
    	resampling_factor = nominal_factor + compensation;    // compress samples
    }

    btstack_resample_set_factor(&resample_instance, resampling_factor);
#endif
    // start stream if enough frames buffered
    if (!audio_stream_started && sbc_frames_in_buffer >= OPTIMAL_FRAMES_MIN){
        media_processing_start();
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

static void dump_sbc_configuration(media_codec_configuration_sbc_t * configuration){
    printf("    - num_channels: %d\n", configuration->num_channels);
    printf("    - sampling_frequency: %d\n", configuration->sampling_frequency);
    printf("    - channel_mode: %d\n", configuration->channel_mode);
    printf("    - block_length: %d\n", configuration->block_length);
    printf("    - subbands: %d\n", configuration->subbands);
    printf("    - allocation_method: %d\n", configuration->allocation_method);
    printf("    - bitpool_value [%d, %d] \n", configuration->min_bitpool_value, configuration->max_bitpool_value);
    printf("\n");
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

#ifdef ENABLE_AVRCP_COVER_ART
static void a2dp_sink_demo_cover_art_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    UNUSED(channel);
    UNUSED(size);
    uint8_t status;
    uint16_t cid;
    switch (packet_type){
        case BIP_DATA_PACKET:
            if (a2dp_sink_cover_art_download_active){
                a2dp_sink_cover_art_file_size += size;
#ifdef HAVE_POSIX_FILE_IO
                fwrite(packet, 1, size, a2dp_sink_cover_art_file);
#else
                printf("Cover art       : TODO - store %u bytes image data\n", size);
#endif
            } else {
                uint16_t i;
                for (i=0;i<size;i++){
                    putchar(packet[i]);
                }
                printf("\n");
            }
            break;
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)){
                case HCI_EVENT_AVRCP_META:
                    switch (hci_event_avrcp_meta_get_subevent_code(packet)){
                        case AVRCP_SUBEVENT_COVER_ART_CONNECTION_ESTABLISHED:
                            status = avrcp_subevent_cover_art_connection_established_get_status(packet);
                            cid = avrcp_subevent_cover_art_connection_established_get_cover_art_cid(packet);
                            if (status == ERROR_CODE_SUCCESS){
                                printf("Cover Art       : connection established, cover art cid 0x%02x\n", cid);
                                a2dp_sink_demo_cover_art_client_connected = true;
                            } else {
                                printf("Cover Art       : connection failed, status 0x%02x\n", status);
                                a2dp_sink_demo_cover_art_cid = 0;
                            }
                            break;
                        case AVRCP_SUBEVENT_COVER_ART_OPERATION_COMPLETE:
                            if (a2dp_sink_cover_art_download_active){
                                a2dp_sink_cover_art_download_active = false;
#ifdef HAVE_POSIX_FILE_IO
                                printf("Cover Art       : download of '%s complete, size %u bytes'\n",
                                       a2dp_sink_demo_thumbnail_path, a2dp_sink_cover_art_file_size);
                                fclose(a2dp_sink_cover_art_file);
                                a2dp_sink_cover_art_file = NULL;
#else
                                printf("Cover Art: download completed\n");
#endif
                            }
                            break;
                        case AVRCP_SUBEVENT_COVER_ART_CONNECTION_RELEASED:
                            a2dp_sink_demo_cover_art_client_connected = false;
                            a2dp_sink_demo_cover_art_cid = 0;
                            printf("Cover Art       : connection released 0x%02x\n",
                                   avrcp_subevent_cover_art_connection_released_get_cover_art_cid(packet));
                            break;
                        default:
                            break;
                    }
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

static uint8_t a2dp_sink_demo_cover_art_connect(void) {
    uint8_t status;
    status = avrcp_cover_art_client_connect(&a2dp_sink_demo_cover_art_client, a2dp_sink_demo_cover_art_packet_handler,
                                            device_addr, a2dp_sink_demo_ertm_buffer,
                                            sizeof(a2dp_sink_demo_ertm_buffer), &a2dp_sink_demo_ertm_config,
                                            &a2dp_sink_demo_cover_art_cid);
    return status;
}
#endif

static void avrcp_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    uint16_t local_cid;
    uint8_t  status;
    bd_addr_t address;

    a2dp_sink_demo_avrcp_connection_t * connection = &a2dp_sink_demo_avrcp_connection;

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_AVRCP_META) return;
    switch (packet[2]){
        case AVRCP_SUBEVENT_CONNECTION_ESTABLISHED: {
            local_cid = avrcp_subevent_connection_established_get_avrcp_cid(packet);
            status = avrcp_subevent_connection_established_get_status(packet);
            if (status != ERROR_CODE_SUCCESS){
                printf("AVRCP: Connection failed, status 0x%02x\n", status);
                connection->avrcp_cid = 0;
                return;
            }

            connection->avrcp_cid = local_cid;
            avrcp_subevent_connection_established_get_bd_addr(packet, address);
            printf("AVRCP: Connected to %s, cid 0x%02x\n", bd_addr_to_str(address), connection->avrcp_cid);

#ifdef HAVE_BTSTACK_STDIN
            // use address for outgoing connections
            avrcp_subevent_connection_established_get_bd_addr(packet, device_addr);
#endif

            avrcp_target_support_event(connection->avrcp_cid, AVRCP_NOTIFICATION_EVENT_VOLUME_CHANGED);
            avrcp_target_support_event(connection->avrcp_cid, AVRCP_NOTIFICATION_EVENT_BATT_STATUS_CHANGED);
            avrcp_target_battery_status_changed(connection->avrcp_cid, battery_status);
        
            // query supported events:
            avrcp_controller_get_supported_events(connection->avrcp_cid);
            return;
        }
        
        case AVRCP_SUBEVENT_CONNECTION_RELEASED:
            printf("AVRCP: Channel released: cid 0x%02x\n", avrcp_subevent_connection_released_get_avrcp_cid(packet));
            connection->avrcp_cid = 0;
            connection->notifications_supported_by_target = 0;
            return;
        default:
            break;
    }
}

static void avrcp_controller_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    // helper to print c strings
    uint8_t avrcp_subevent_value[256];
    uint8_t play_status;
    uint8_t event_id;

    a2dp_sink_demo_avrcp_connection_t * avrcp_connection = &a2dp_sink_demo_avrcp_connection;

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_AVRCP_META) return;
    if (avrcp_connection->avrcp_cid == 0) return;

    memset(avrcp_subevent_value, 0, sizeof(avrcp_subevent_value));
    switch (packet[2]){
        case AVRCP_SUBEVENT_GET_CAPABILITY_EVENT_ID:
            avrcp_connection->notifications_supported_by_target |= (1 << avrcp_subevent_get_capability_event_id_get_event_id(packet));
            break;
        case AVRCP_SUBEVENT_GET_CAPABILITY_EVENT_ID_DONE:
            
            printf("AVRCP Controller: supported notifications by target:\n");
            for (event_id = (uint8_t) AVRCP_NOTIFICATION_EVENT_FIRST_INDEX; event_id < (uint8_t) AVRCP_NOTIFICATION_EVENT_LAST_INDEX; event_id++){
                printf("   - [%s] %s\n", 
                    (avrcp_connection->notifications_supported_by_target & (1 << event_id)) != 0 ? "X" : " ", 
                    avrcp_notification2str((avrcp_notification_event_id_t)event_id));
            }
            printf("\n\n");

            // automatically enable notifications
            avrcp_controller_enable_notification(avrcp_connection->avrcp_cid, AVRCP_NOTIFICATION_EVENT_PLAYBACK_STATUS_CHANGED);
            avrcp_controller_enable_notification(avrcp_connection->avrcp_cid, AVRCP_NOTIFICATION_EVENT_NOW_PLAYING_CONTENT_CHANGED);
            avrcp_controller_enable_notification(avrcp_connection->avrcp_cid, AVRCP_NOTIFICATION_EVENT_TRACK_CHANGED);

#ifdef ENABLE_AVRCP_COVER_ART
            // image handles become invalid on player change, registe for notifications
            avrcp_controller_enable_notification(a2dp_sink_demo_avrcp_connection.avrcp_cid, AVRCP_NOTIFICATION_EVENT_UIDS_CHANGED);
            // trigger cover art client connection
            a2dp_sink_demo_cover_art_connect();
#endif
            break;

        case AVRCP_SUBEVENT_NOTIFICATION_STATE:
            event_id = (avrcp_notification_event_id_t)avrcp_subevent_notification_state_get_event_id(packet);
            printf("AVRCP Controller: %s notification registered\n", avrcp_notification2str(event_id));
            break;

        case AVRCP_SUBEVENT_NOTIFICATION_PLAYBACK_POS_CHANGED:
            printf("AVRCP Controller: Playback position changed, position %d ms\n", (unsigned int) avrcp_subevent_notification_playback_pos_changed_get_playback_position_ms(packet));
            break;
        case AVRCP_SUBEVENT_NOTIFICATION_PLAYBACK_STATUS_CHANGED:
            printf("AVRCP Controller: Playback status changed %s\n", avrcp_play_status2str(avrcp_subevent_notification_playback_status_changed_get_play_status(packet)));
            play_status = avrcp_subevent_notification_playback_status_changed_get_play_status(packet);
            switch (play_status){
                case AVRCP_PLAYBACK_STATUS_PLAYING:
                    avrcp_connection->playing = true;
                    break;
                default:
                    avrcp_connection->playing = false;
                    break;
            }
            break;

        case AVRCP_SUBEVENT_NOTIFICATION_NOW_PLAYING_CONTENT_CHANGED:
            printf("AVRCP Controller: Playing content changed\n");
            break;

        case AVRCP_SUBEVENT_NOTIFICATION_TRACK_CHANGED:
            printf("AVRCP Controller: Track changed\n");
            break;

        case AVRCP_SUBEVENT_NOTIFICATION_AVAILABLE_PLAYERS_CHANGED:
            printf("AVRCP Controller: Available Players Changed\n");
            break;

        case AVRCP_SUBEVENT_SHUFFLE_AND_REPEAT_MODE:{
            uint8_t shuffle_mode = avrcp_subevent_shuffle_and_repeat_mode_get_shuffle_mode(packet);
            uint8_t repeat_mode  = avrcp_subevent_shuffle_and_repeat_mode_get_repeat_mode(packet);
            printf("AVRCP Controller: %s, %s\n", avrcp_shuffle2str(shuffle_mode), avrcp_repeat2str(repeat_mode));
            break;
        }
        case AVRCP_SUBEVENT_NOW_PLAYING_TRACK_INFO:
            printf("AVRCP Controller: Track %d\n", avrcp_subevent_now_playing_track_info_get_track(packet));
            break;

        case AVRCP_SUBEVENT_NOW_PLAYING_TOTAL_TRACKS_INFO:
            printf("AVRCP Controller: Total Tracks %d\n", avrcp_subevent_now_playing_total_tracks_info_get_total_tracks(packet));
            break;

        case AVRCP_SUBEVENT_NOW_PLAYING_TITLE_INFO:
            if (avrcp_subevent_now_playing_title_info_get_value_len(packet) > 0){
                memcpy(avrcp_subevent_value, avrcp_subevent_now_playing_title_info_get_value(packet), avrcp_subevent_now_playing_title_info_get_value_len(packet));
                printf("AVRCP Controller: Title %s\n", avrcp_subevent_value);
            }  
            break;

        case AVRCP_SUBEVENT_NOW_PLAYING_ARTIST_INFO:
            if (avrcp_subevent_now_playing_artist_info_get_value_len(packet) > 0){
                memcpy(avrcp_subevent_value, avrcp_subevent_now_playing_artist_info_get_value(packet), avrcp_subevent_now_playing_artist_info_get_value_len(packet));
                printf("AVRCP Controller: Artist %s\n", avrcp_subevent_value);
            }  
            break;
        
        case AVRCP_SUBEVENT_NOW_PLAYING_ALBUM_INFO:
            if (avrcp_subevent_now_playing_album_info_get_value_len(packet) > 0){
                memcpy(avrcp_subevent_value, avrcp_subevent_now_playing_album_info_get_value(packet), avrcp_subevent_now_playing_album_info_get_value_len(packet));
                printf("AVRCP Controller: Album %s\n", avrcp_subevent_value);
            }  
            break;
        
        case AVRCP_SUBEVENT_NOW_PLAYING_GENRE_INFO:
            if (avrcp_subevent_now_playing_genre_info_get_value_len(packet) > 0){
                memcpy(avrcp_subevent_value, avrcp_subevent_now_playing_genre_info_get_value(packet), avrcp_subevent_now_playing_genre_info_get_value_len(packet));
                printf("AVRCP Controller: Genre %s\n", avrcp_subevent_value);
            }  
            break;

        case AVRCP_SUBEVENT_PLAY_STATUS:
            printf("AVRCP Controller: Song length %"PRIu32" ms, Song position %"PRIu32" ms, Play status %s\n", 
                avrcp_subevent_play_status_get_song_length(packet), 
                avrcp_subevent_play_status_get_song_position(packet),
                avrcp_play_status2str(avrcp_subevent_play_status_get_play_status(packet)));
            break;
        
        case AVRCP_SUBEVENT_OPERATION_COMPLETE:
            printf("AVRCP Controller: %s complete\n", avrcp_operation2str(avrcp_subevent_operation_complete_get_operation_id(packet)));
            break;
        
        case AVRCP_SUBEVENT_OPERATION_START:
            printf("AVRCP Controller: %s start\n", avrcp_operation2str(avrcp_subevent_operation_start_get_operation_id(packet)));
            break;
       
        case AVRCP_SUBEVENT_NOTIFICATION_EVENT_TRACK_REACHED_END:
            printf("AVRCP Controller: Track reached end\n");
            break;

        case AVRCP_SUBEVENT_PLAYER_APPLICATION_VALUE_RESPONSE:
            printf("AVRCP Controller: Set Player App Value %s\n", avrcp_ctype2str(avrcp_subevent_player_application_value_response_get_command_type(packet)));
            break;

#ifdef ENABLE_AVRCP_COVER_ART
        case AVRCP_SUBEVENT_NOTIFICATION_EVENT_UIDS_CHANGED:
            if (a2dp_sink_demo_cover_art_client_connected){
                printf("AVRCP Controller: UIDs changed -> disconnect cover art client\n");
                avrcp_cover_art_client_disconnect(a2dp_sink_demo_cover_art_cid);
            }
            break;

        case AVRCP_SUBEVENT_NOW_PLAYING_COVER_ART_INFO:
            if (avrcp_subevent_now_playing_cover_art_info_get_value_len(packet) == 7){
                memcpy(a2dp_sink_demo_image_handle, avrcp_subevent_now_playing_cover_art_info_get_value(packet), 7);
                printf("AVRCP Controller: Cover Art %s\n", a2dp_sink_demo_image_handle);
            }
            break;
#endif

        default:
            break;
    }  
}

static void avrcp_volume_changed(uint8_t volume){
    const btstack_audio_sink_t * audio = btstack_audio_sink_get_instance();
    if (audio){
        audio->set_volume(volume);
    }
}

static void avrcp_target_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_AVRCP_META) return;
    
    uint8_t volume;
    char const * button_state;
    avrcp_operation_id_t operation_id;

    switch (packet[2]){
        case AVRCP_SUBEVENT_NOTIFICATION_VOLUME_CHANGED:
            volume = avrcp_subevent_notification_volume_changed_get_absolute_volume(packet);
            volume_percentage = volume * 100 / 127;
            printf("AVRCP Target    : Volume set to %d%% (%d)\n", volume_percentage, volume);
            avrcp_volume_changed(volume);
            break;
        
        case AVRCP_SUBEVENT_OPERATION:
            operation_id = (avrcp_operation_id_t) avrcp_subevent_operation_get_operation_id(packet);
            button_state = avrcp_subevent_operation_get_button_pressed(packet) > 0 ? "PRESS" : "RELEASE";
            switch (operation_id){
                case AVRCP_OPERATION_ID_VOLUME_UP:
                    printf("AVRCP Target    : VOLUME UP (%s)\n", button_state);
                    break;
                case AVRCP_OPERATION_ID_VOLUME_DOWN:
                    printf("AVRCP Target    : VOLUME DOWN (%s)\n", button_state);
                    break;
                default:
                    return;
            }
            break;
        default:
            printf("AVRCP Target    : Event 0x%02x is not parsed\n", packet[2]);
            break;
    }
}

static void a2dp_sink_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    uint8_t status;

    uint8_t allocation_method;

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_A2DP_META) return;

    a2dp_sink_demo_a2dp_connection_t * a2dp_conn = &a2dp_sink_demo_a2dp_connection;

    switch (packet[2]){
        case A2DP_SUBEVENT_SIGNALING_MEDIA_CODEC_OTHER_CONFIGURATION:
            printf("A2DP  Sink      : Received non SBC codec - not implemented\n");
            break;
        case A2DP_SUBEVENT_SIGNALING_MEDIA_CODEC_SBC_CONFIGURATION:{
            printf("A2DP  Sink      : Received SBC codec configuration\n");
            a2dp_conn->sbc_configuration.reconfigure = a2dp_subevent_signaling_media_codec_sbc_configuration_get_reconfigure(packet);
            a2dp_conn->sbc_configuration.num_channels = a2dp_subevent_signaling_media_codec_sbc_configuration_get_num_channels(packet);
            a2dp_conn->sbc_configuration.sampling_frequency = a2dp_subevent_signaling_media_codec_sbc_configuration_get_sampling_frequency(packet);
            a2dp_conn->sbc_configuration.block_length = a2dp_subevent_signaling_media_codec_sbc_configuration_get_block_length(packet);
            a2dp_conn->sbc_configuration.subbands = a2dp_subevent_signaling_media_codec_sbc_configuration_get_subbands(packet);
            a2dp_conn->sbc_configuration.min_bitpool_value = a2dp_subevent_signaling_media_codec_sbc_configuration_get_min_bitpool_value(packet);
            a2dp_conn->sbc_configuration.max_bitpool_value = a2dp_subevent_signaling_media_codec_sbc_configuration_get_max_bitpool_value(packet);
            
            allocation_method = a2dp_subevent_signaling_media_codec_sbc_configuration_get_allocation_method(packet);
            
            // Adapt Bluetooth spec definition to SBC Encoder expected input
            a2dp_conn->sbc_configuration.allocation_method = (btstack_sbc_allocation_method_t)(allocation_method - 1);
           
            switch (a2dp_subevent_signaling_media_codec_sbc_configuration_get_channel_mode(packet)){
                case AVDTP_CHANNEL_MODE_JOINT_STEREO:
                    a2dp_conn->sbc_configuration.channel_mode = SBC_CHANNEL_MODE_JOINT_STEREO;
                    break;
                case AVDTP_CHANNEL_MODE_STEREO:
                    a2dp_conn->sbc_configuration.channel_mode = SBC_CHANNEL_MODE_STEREO;
                    break;
                case AVDTP_CHANNEL_MODE_DUAL_CHANNEL:
                    a2dp_conn->sbc_configuration.channel_mode = SBC_CHANNEL_MODE_DUAL_CHANNEL;
                    break;
                case AVDTP_CHANNEL_MODE_MONO:
                    a2dp_conn->sbc_configuration.channel_mode = SBC_CHANNEL_MODE_MONO;
                    break;
                default:
                    btstack_assert(false);
                    break;
            }
            dump_sbc_configuration(&a2dp_conn->sbc_configuration);
            break;
        }

        case A2DP_SUBEVENT_STREAM_ESTABLISHED:
            status = a2dp_subevent_stream_established_get_status(packet);
            if (status != ERROR_CODE_SUCCESS){
                printf("A2DP  Sink      : Streaming connection failed, status 0x%02x\n", status);
                break;
            }

            a2dp_subevent_stream_established_get_bd_addr(packet, a2dp_conn->addr);
            a2dp_conn->a2dp_cid = a2dp_subevent_stream_established_get_a2dp_cid(packet);
            a2dp_conn->a2dp_local_seid = a2dp_subevent_stream_established_get_local_seid(packet);
            a2dp_conn->stream_state = STREAM_STATE_OPEN;

            printf("A2DP  Sink      : Streaming connection is established, address %s, cid 0x%02x, local seid %d\n",
                   bd_addr_to_str(a2dp_conn->addr), a2dp_conn->a2dp_cid, a2dp_conn->a2dp_local_seid);
#ifdef HAVE_BTSTACK_STDIN
            // use address for outgoing connections
            memcpy(device_addr, a2dp_conn->addr, 6);
#endif
            break;
        
#ifdef ENABLE_AVDTP_ACCEPTOR_EXPLICIT_START_STREAM_CONFIRMATION
        case A2DP_SUBEVENT_START_STREAM_REQUESTED:
            printf("A2DP  Sink      : Explicit Accept to start stream, local_seid %d\n", a2dp_subevent_start_stream_requested_get_local_seid(packet));
            a2dp_sink_start_stream_accept(a2dp_cid, a2dp_local_seid);
            break;
#endif
        case A2DP_SUBEVENT_STREAM_STARTED:
            printf("A2DP  Sink      : Stream started\n");
            a2dp_conn->stream_state = STREAM_STATE_PLAYING;
            if (a2dp_conn->sbc_configuration.reconfigure){
                media_processing_close();
            }
            // prepare media processing
            media_processing_init(&a2dp_conn->sbc_configuration);
            // audio stream is started when buffer reaches minimal level
            break;
        
        case A2DP_SUBEVENT_STREAM_SUSPENDED:
            printf("A2DP  Sink      : Stream paused\n");
            a2dp_conn->stream_state = STREAM_STATE_PAUSED;
            media_processing_pause();
            break;
        
        case A2DP_SUBEVENT_STREAM_RELEASED:
            printf("A2DP  Sink      : Stream released\n");
            a2dp_conn->stream_state = STREAM_STATE_CLOSED;
            media_processing_close();
            break;
        
        case A2DP_SUBEVENT_SIGNALING_CONNECTION_RELEASED:
            printf("A2DP  Sink      : Signaling connection released\n");
            a2dp_conn->a2dp_cid = 0;
            media_processing_close();
            break;
        
        default:
            break;
    }
}

#ifdef HAVE_BTSTACK_STDIN
static void show_usage(void){
    bd_addr_t      iut_address;
    gap_local_bd_addr(iut_address);
    printf("\n--- A2DP Sink Demo Console %s ---\n", bd_addr_to_str(iut_address));
    printf("b - A2DP Sink create connection to addr %s\n", bd_addr_to_str(device_addr));
    printf("B - A2DP Sink disconnect\n");

    printf("\n--- AVRCP Controller ---\n");
    printf("c - AVRCP create connection to addr %s\n", bd_addr_to_str(device_addr));
    printf("C - AVRCP disconnect\n");
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
    printf("w - delay report\n");
    printf("x - repeat all tracks\n");
    printf("X - disable repeat mode\n");
    printf("z - shuffle all tracks\n");
    printf("Z - disable shuffle mode\n");

    printf("a/A - register/deregister TRACK_CHANGED\n");
    printf("R/P - register/deregister PLAYBACK_POS_CHANGED\n");

    printf("s/S - send/release long button press REWIND\n");

    printf("\n--- Volume and Battery Control ---\n");
    printf("t - volume up   for 10 percent\n");
    printf("T - volume down for 10 percent\n");
    printf("V - toggle Battery status from AVRCP_BATTERY_STATUS_NORMAL to AVRCP_BATTERY_STATUS_FULL_CHARGE\n");

#ifdef ENABLE_AVRCP_COVER_ART
    printf("\n--- Cover Art Client ---\n");
    printf("d - connect to addr %s\n", bd_addr_to_str(device_addr));
    printf("D - disconnect\n");
    if (a2dp_sink_demo_cover_art_client_connected == false){
        if (a2dp_sink_demo_avrcp_connection.avrcp_cid == 0){
            printf("Not connected, press 'b' or 'c' to first connect AVRCP, then press 'd' to connect cover art client\n");
        } else {
            printf("Not connected, press 'd' to connect cover art client\n");
        }
    } else if (a2dp_sink_demo_image_handle[0] == 0){
        printf("No image handle, use 'j' to get current track info\n");
    }
    printf("---\n");
#endif

}
#endif

#ifdef HAVE_BTSTACK_STDIN

static void stdin_process(char cmd){
    uint8_t status = ERROR_CODE_SUCCESS;
    uint8_t volume;
    avrcp_battery_status_t old_battery_status;

    a2dp_sink_demo_a2dp_connection_t *  a2dp_connection  = &a2dp_sink_demo_a2dp_connection;
    a2dp_sink_demo_avrcp_connection_t * avrcp_connection = &a2dp_sink_demo_avrcp_connection;

    switch (cmd){
        case 'b':
            status = a2dp_sink_establish_stream(device_addr, &a2dp_connection->a2dp_cid);
            printf(" - Create AVDTP connection to addr %s, and local seid %d, cid 0x%02x.\n",
                   bd_addr_to_str(device_addr), a2dp_connection->a2dp_local_seid, a2dp_connection->a2dp_cid);
            break;
        case 'B':
            printf(" - AVDTP disconnect from addr %s.\n", bd_addr_to_str(device_addr));
            a2dp_sink_disconnect(a2dp_connection->a2dp_cid);
            break;
        case 'c':
            printf(" - Create AVRCP connection to addr %s.\n", bd_addr_to_str(device_addr));
            status = avrcp_connect(device_addr, &avrcp_connection->avrcp_cid);
            break;
        case 'C':
            printf(" - AVRCP disconnect from addr %s.\n", bd_addr_to_str(device_addr));
            status = avrcp_disconnect(avrcp_connection->avrcp_cid);
            break;

        case '\n':
        case '\r':
            break;
        case 'w':
            printf("Send delay report\n");
            avdtp_sink_delay_report(a2dp_connection->a2dp_cid, a2dp_connection->a2dp_local_seid, 100);
            break;
        // Volume Control
        case 't':
            volume_percentage = volume_percentage <= 90 ? volume_percentage + 10 : 100;
            volume = volume_percentage * 127 / 100;
            printf(" - volume up   for 10 percent, %d%% (%d) \n", volume_percentage, volume);
            status = avrcp_target_volume_changed(avrcp_connection->avrcp_cid, volume);
            avrcp_volume_changed(volume);
            break;
        case 'T':
            volume_percentage = volume_percentage >= 10 ? volume_percentage - 10 : 0;
            volume = volume_percentage * 127 / 100;
            printf(" - volume down for 10 percent, %d%% (%d) \n", volume_percentage, volume);
            status = avrcp_target_volume_changed(avrcp_connection->avrcp_cid, volume);
            avrcp_volume_changed(volume);
            break;
        case 'V':
            old_battery_status = battery_status;

            if (battery_status < AVRCP_BATTERY_STATUS_FULL_CHARGE){
                battery_status = (avrcp_battery_status_t)((uint8_t) battery_status + 1);
            } else {
                battery_status = AVRCP_BATTERY_STATUS_NORMAL;
            }
            printf(" - toggle battery value, old %d, new %d\n", old_battery_status, battery_status);
            status = avrcp_target_battery_status_changed(avrcp_connection->avrcp_cid, battery_status);
            break;
        case 'O':
            printf(" - get play status\n");
            status = avrcp_controller_get_play_status(avrcp_connection->avrcp_cid);
            break;
        case 'j':
            printf(" - get now playing info\n");
            status = avrcp_controller_get_now_playing_info(avrcp_connection->avrcp_cid);
            break;
        case 'k':
            printf(" - play\n");
            status = avrcp_controller_play(avrcp_connection->avrcp_cid);
            break;
        case 'K':
            printf(" - stop\n");
            status = avrcp_controller_stop(avrcp_connection->avrcp_cid);
            break;
        case 'L':
            printf(" - pause\n");
            status = avrcp_controller_pause(avrcp_connection->avrcp_cid);
            break;
        case 'u':
            printf(" - start fast forward\n");
            status = avrcp_controller_press_and_hold_fast_forward(avrcp_connection->avrcp_cid);
            break;
        case 'U':
            printf(" - stop fast forward\n");
            status = avrcp_controller_release_press_and_hold_cmd(avrcp_connection->avrcp_cid);
            break;
        case 'n':
            printf(" - start rewind\n");
            status = avrcp_controller_press_and_hold_rewind(avrcp_connection->avrcp_cid);
            break;
        case 'N':
            printf(" - stop rewind\n");
            status = avrcp_controller_release_press_and_hold_cmd(avrcp_connection->avrcp_cid);
            break;
        case 'i':
            printf(" - forward\n");
            status = avrcp_controller_forward(avrcp_connection->avrcp_cid);
            break;
        case 'I':
            printf(" - backward\n");
            status = avrcp_controller_backward(avrcp_connection->avrcp_cid);
            break;
        case 'M':
            printf(" - mute\n");
            status = avrcp_controller_mute(avrcp_connection->avrcp_cid);
            break;
        case 'r':
            printf(" - skip\n");
            status = avrcp_controller_skip(avrcp_connection->avrcp_cid);
            break;
        case 'q':
            printf(" - query repeat and shuffle mode\n");
            status = avrcp_controller_query_shuffle_and_repeat_modes(avrcp_connection->avrcp_cid);
            break;
        case 'v':
            printf(" - repeat single track\n");
            status = avrcp_controller_set_repeat_mode(avrcp_connection->avrcp_cid, AVRCP_REPEAT_MODE_SINGLE_TRACK);
            break;
        case 'x':
            printf(" - repeat all tracks\n");
            status = avrcp_controller_set_repeat_mode(avrcp_connection->avrcp_cid, AVRCP_REPEAT_MODE_ALL_TRACKS);
            break;
        case 'X':
            printf(" - disable repeat mode\n");
            status = avrcp_controller_set_repeat_mode(avrcp_connection->avrcp_cid, AVRCP_REPEAT_MODE_OFF);
            break;
        case 'z':
            printf(" - shuffle all tracks\n");
            status = avrcp_controller_set_shuffle_mode(avrcp_connection->avrcp_cid, AVRCP_SHUFFLE_MODE_ALL_TRACKS);
            break;
        case 'Z':
            printf(" - disable shuffle mode\n");
            status = avrcp_controller_set_shuffle_mode(avrcp_connection->avrcp_cid, AVRCP_SHUFFLE_MODE_OFF);
            break;
        case 'a':
            printf("AVRCP: enable notification TRACK_CHANGED\n");
            status = avrcp_controller_enable_notification(avrcp_connection->avrcp_cid, AVRCP_NOTIFICATION_EVENT_TRACK_CHANGED);
            break;
        case 'A':
            printf("AVRCP: disable notification TRACK_CHANGED\n");
            status = avrcp_controller_disable_notification(avrcp_connection->avrcp_cid, AVRCP_NOTIFICATION_EVENT_TRACK_CHANGED);
            break;
        case 'R':
            printf("AVRCP: enable notification PLAYBACK_POS_CHANGED\n");
            status = avrcp_controller_enable_notification(avrcp_connection->avrcp_cid, AVRCP_NOTIFICATION_EVENT_PLAYBACK_POS_CHANGED);
            break;
        case 'P':
            printf("AVRCP: disable notification PLAYBACK_POS_CHANGED\n");
            status = avrcp_controller_disable_notification(avrcp_connection->avrcp_cid, AVRCP_NOTIFICATION_EVENT_PLAYBACK_POS_CHANGED);
            break;
         case 's':
            printf("AVRCP: send long button press REWIND\n");
            status = avrcp_controller_start_press_and_hold_cmd(avrcp_connection->avrcp_cid, AVRCP_OPERATION_ID_REWIND);
            break;
        case 'S':
            printf("AVRCP: release long button press REWIND\n");
            status = avrcp_controller_release_press_and_hold_cmd(avrcp_connection->avrcp_cid);
            break;
#ifdef ENABLE_AVRCP_COVER_ART
        case 'd':
            printf(" - Create AVRCP Cover Art connection to addr %s.\n", bd_addr_to_str(device_addr));
            status = a2dp_sink_demo_cover_art_connect();
            break;
        case 'D':
            printf(" - AVRCP Cover Art disconnect from addr %s.\n", bd_addr_to_str(device_addr));
            status = avrcp_cover_art_client_disconnect(a2dp_sink_demo_cover_art_cid);
            break;
        case '@':
            printf("Get linked thumbnail for '%s'\n", a2dp_sink_demo_image_handle);
#ifdef HAVE_POSIX_FILE_IO
            a2dp_sink_cover_art_file = fopen(a2dp_sink_demo_thumbnail_path, "w");
#endif
            a2dp_sink_cover_art_download_active = true;
            a2dp_sink_cover_art_file_size = 0;
            status = avrcp_cover_art_client_get_linked_thumbnail(a2dp_sink_demo_cover_art_cid, a2dp_sink_demo_image_handle);
            break;
#endif
        default:
            show_usage();
            return;
    }
    if (status != ERROR_CODE_SUCCESS){
        printf("Could not perform command, status 0x%02x\n", status);
    }
}
#endif

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    UNUSED(argc);
    (void)argv;

    setup_demo();

#ifdef HAVE_BTSTACK_STDIN
    // parse human-readable Bluetooth address
    sscanf_bd_addr(device_addr_string, device_addr);
    btstack_stdin_setup(stdin_process);
#endif

    // turn on!
    printf("Starting BTstack ...\n");
    hci_power_control(HCI_POWER_ON);
    return 0;
}
/* EXAMPLE_END */
