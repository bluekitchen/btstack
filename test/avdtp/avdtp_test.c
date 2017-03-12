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
#include "stdin_support.h"
#include "avdtp_sink.h"

#include "btstack_sbc.h"
#include "wav_util.h"
#include "avdtp_util.h"

#ifdef HAVE_PORTAUDIO
#include <portaudio.h>
#include "btstack_ring_buffer.h"
#endif

#ifdef HAVE_POSIX_FILE_IO
#define STORE_SBC_TO_SBC_FILE 
#define STORE_SBC_TO_WAV_FILE 
#endif

#if defined(HAVE_PORTAUDIO) || defined(STORE_SBC_TO_WAV_FILE)
#define DECODE_SBC
#endif

#define NUM_CHANNELS 2
#define SAMPLE_RATE 44100

// SBC Decoder for WAV file or PortAudio
#ifdef DECODE_SBC
static btstack_sbc_decoder_state_t state;
static btstack_sbc_mode_t mode = SBC_MODE_STANDARD;
static int total_num_samples = 0;
#endif

// PortAdudio - live playback
#ifdef HAVE_PORTAUDIO
#define PA_SAMPLE_TYPE      paInt16
#define FRAMES_PER_BUFFER   128
#define BYTES_PER_FRAME     (2*NUM_CHANNELS)
#define PREBUFFER_MS        300
#define PREBUFFER_BYTES     (PREBUFFER_MS*SAMPLE_RATE/1000*BYTES_PER_FRAME)
static uint8_t ring_buffer_storage[2*PREBUFFER_BYTES];
static btstack_ring_buffer_t ring_buffer;
static PaStream * stream;
static int pa_stream_started = 0;
static int pa_stream_paused = 0;
#endif

// WAV File
#ifdef STORE_SBC_TO_WAV_FILE    
static int frame_count = 0;
static char * wav_filename = "avdtp_sink.wav";
#endif

#ifdef STORE_SBC_TO_SBC_FILE    
static FILE * sbc_file;
static char * sbc_filename = "avdtp_sink.sbc";
#endif

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

// mac 2011: static bd_addr_t remote = {0x04, 0x0C, 0xCE, 0xE4, 0x85, 0xD3};
// pts: static bd_addr_t remote = {0x00, 0x1B, 0xDC, 0x08, 0x0A, 0xA5};
// mac 2013: static bd_addr_t remote = {0x84, 0x38, 0x35, 0x65, 0xd1, 0x15};
static bd_addr_t remote = {0x84, 0x38, 0x35, 0x65, 0xd1, 0x15};
static uint16_t con_handle = 0;
static uint8_t sdp_avdtp_sink_service_buffer[150];
static avdtp_sep_t sep;

static adtvp_media_codec_information_sbc_t sbc_capability;
static avdtp_media_codec_configuration_sbc_t sbc_configuration;
static avdtp_stream_endpoint_t * local_stream_endpoint;

static uint16_t remote_configuration_bitmap;
static avdtp_capabilities_t remote_configuration;

typedef enum {
    AVDTP_APPLICATION_IDLE,
    AVDTP_APPLICATION_W2_DISCOVER_SEPS,
    AVDTP_APPLICATION_W2_GET_CAPABILITIES,
    AVDTP_APPLICATION_W2_GET_ALL_CAPABILITIES,
    AVDTP_APPLICATION_W2_SET_CONFIGURATION,
    AVDTP_APPLICATION_W2_SUSPEND_STREAM_WITH_SEID,
    AVDTP_APPLICATION_W2_RECONFIGURE_WITH_SEID,
    AVDTP_APPLICATION_W2_OPEN_STREAM_WITH_SEID,
    AVDTP_APPLICATION_W2_START_STREAM_WITH_SEID,
    AVDTP_APPLICATION_W2_ABORT_STREAM_WITH_SEID,
    AVDTP_APPLICATION_W2_STOP_STREAM_WITH_SEID,
    AVDTP_APPLICATION_W2_GET_CONFIGURATION
} avdtp_application_state_t;

avdtp_application_state_t app_state = AVDTP_APPLICATION_IDLE;

static btstack_packet_callback_registration_t hci_event_callback_registration;

static int media_initialized = 0;



#ifdef HAVE_PORTAUDIO
static int patestCallback( const void *inputBuffer, void *outputBuffer,
                           unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData ) {

    /** patestCallback is called from different thread, don't use hci_dump / log_info here without additional checks */

    (void) timeInfo; /* Prevent unused variable warnings. */
    (void) statusFlags;
    (void) inputBuffer;
    (void) userData;
    
    int bytes_to_copy = framesPerBuffer * BYTES_PER_FRAME;

    // fill with silence while paused
    if (pa_stream_paused){

        if (btstack_ring_buffer_bytes_available(&ring_buffer) < PREBUFFER_BYTES){
            // printf("PA: silence\n");
            memset(outputBuffer, 0, bytes_to_copy);
            return 0;
        } else {
            // resume playback
            pa_stream_paused = 0;
        }
    }

    // get data from ringbuffer
    uint32_t bytes_read = 0;
    btstack_ring_buffer_read(&ring_buffer, outputBuffer, bytes_to_copy, &bytes_read);
    bytes_to_copy -= bytes_read;

    // fill with 0 if not enough
    if (bytes_to_copy){
        memset(outputBuffer + bytes_read, 0, bytes_to_copy);
        pa_stream_paused = 1;
    }
    return 0;
}
#endif

#ifdef DECODE_SBC
static void handle_pcm_data(int16_t * data, int num_samples, int num_channels, int sample_rate, void * context){
    UNUSED(sample_rate);
    UNUSED(context);

#ifdef STORE_SBC_TO_WAV_FILE
    wav_writer_write_int16(num_samples*num_channels, data);
    frame_count++;
#endif

    total_num_samples+=num_samples*num_channels;

#ifdef HAVE_PORTAUDIO
    if (!pa_stream_started){
        /* -- start stream -- */
        PaError err = Pa_StartStream(stream);
        if (err != paNoError){
            printf("Error starting the stream: \"%s\"\n",  Pa_GetErrorText(err));
            return;
        }
        pa_stream_started = 1; 
        pa_stream_paused  = 1;
    }
    btstack_ring_buffer_write(&ring_buffer, (uint8_t *)data, num_samples*num_channels*2);
#endif
}
#endif

static int init_media_processing(avdtp_media_codec_configuration_sbc_t configuration){
    int num_channels = configuration.num_channels;
    int sample_rate = configuration.sampling_frequency;
    
#ifdef DECODE_SBC
    btstack_sbc_decoder_init(&state, mode, handle_pcm_data, NULL);
#endif

#ifdef STORE_SBC_TO_WAV_FILE
    wav_writer_open(wav_filename, num_channels, sample_rate);  
#endif

#ifdef STORE_SBC_TO_SBC_FILE    
    sbc_file = fopen(sbc_filename, "wb"); 
#endif

#ifdef HAVE_PORTAUDIO
    // int frames_per_buffer = configuration.frames_per_buffer;
    PaError err;
    PaStreamParameters outputParameters;

    /* -- initialize PortAudio -- */
    err = Pa_Initialize();
    if (err != paNoError){
        printf("Error initializing portaudio: \"%s\"\n",  Pa_GetErrorText(err));
        return err;
    } 
    /* -- setup input and output -- */
    outputParameters.device = Pa_GetDefaultOutputDevice(); /* default output device */
    outputParameters.channelCount = num_channels;
    outputParameters.sampleFormat = PA_SAMPLE_TYPE;
    outputParameters.suggestedLatency = Pa_GetDeviceInfo( outputParameters.device )->defaultHighOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;
    /* -- setup stream -- */
    err = Pa_OpenStream(
           &stream,
           NULL,                /* &inputParameters */
           &outputParameters,
           sample_rate,
           0,
           paClipOff,           /* we won't output out of range samples so don't bother clipping them */
           patestCallback,      /* use callback */
           NULL );   
    
    if (err != paNoError){
        printf("Error initializing portaudio: \"%s\"\n",  Pa_GetErrorText(err));
        return err;
    }
    memset(ring_buffer_storage, 0, sizeof(ring_buffer_storage));
    btstack_ring_buffer_init(&ring_buffer, ring_buffer_storage, sizeof(ring_buffer_storage));
    pa_stream_started = 0;
#endif 
    media_initialized = 1;
    return 0;
}

static void close_media_processing(void){
    if (!media_initialized) return;
    media_initialized = 0;
#ifdef STORE_SBC_TO_WAV_FILE 
    printf(" Close wav writer.\n");                   
    wav_writer_close();
    int total_frames_nr = state.good_frames_nr + state.bad_frames_nr + state.zero_frames_nr;

    printf(" Decoding done. Processed totaly %d frames:\n - %d good\n - %d bad\n - %d zero frames\n", total_frames_nr, state.good_frames_nr, state.bad_frames_nr, state.zero_frames_nr);
    printf(" Written %d frames to wav file: %s\n\n", frame_count, wav_filename);
#endif
#ifdef STORE_SBC_TO_SBC_FILE
    fclose(sbc_file);
#endif     

#ifdef HAVE_PORTAUDIO
    PaError err = Pa_StopStream(stream);
    if (err != paNoError){
        printf("Error stopping the stream: \"%s\"\n",  Pa_GetErrorText(err));
        return;
    } 
    pa_stream_started = 0;
    err = Pa_CloseStream(stream);
    if (err != paNoError){
        printf("Error closing the stream: \"%s\"\n",  Pa_GetErrorText(err));
        return;
    } 

    err = Pa_Terminate();
    if (err != paNoError){
        printf("Error terminating portaudio: \"%s\"\n",  Pa_GetErrorText(err));
        return;
    } 
#endif
}

static void handle_l2cap_media_data_packet(avdtp_stream_endpoint_t * stream_endpoint, uint8_t *packet, uint16_t size){
    
    UNUSED(stream_endpoint);

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
    
#ifdef DECODE_SBC
    btstack_sbc_decoder_process_data(&state, 0, packet+pos, size-pos);
#endif

#ifdef STORE_SBC_TO_SBC_FILE
    fwrite(packet+pos, size-pos, 1, sbc_file);
#endif
#ifdef HAVE_PORTAUDIO
    log_info("PA: bytes avail after recv: %d", btstack_ring_buffer_bytes_available(&ring_buffer));
#endif
}

static void dump_sbc_capability(adtvp_media_codec_information_sbc_t media_codec_sbc){
    printf("Received media codec capability:\n");
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
                    printf("\n --- avdtp_test: HCI_EVENT_DISCONNECTION_COMPLETE ---\n");
                    close_media_processing();
                    break;
                case HCI_EVENT_AVDTP_META:
                    switch (packet[2]){
                        case AVDTP_SUBEVENT_SIGNALING_CONNECTION_ESTABLISHED:
                            con_handle = avdtp_subevent_signaling_connection_established_get_con_handle(packet);
                            printf("\n --- avdtp_test: AVDTP_SUBEVENT_SIGNALING_CONNECTION_ESTABLISHED, con handle 0x%02x ---\n", con_handle);
                            break;
                        case AVDTP_SUBEVENT_SIGNALING_SEP_FOUND:
                            if (app_state != AVDTP_APPLICATION_W2_DISCOVER_SEPS) return;
                            sep.seid = avdtp_subevent_signaling_sep_found_get_seid(packet);
                            sep.in_use = avdtp_subevent_signaling_sep_found_get_in_use(packet);
                            sep.media_type = avdtp_subevent_signaling_sep_found_get_media_type(packet);
                            sep.type = avdtp_subevent_signaling_sep_found_get_sep_type(packet);
                            printf("Found sep: seid %u, in_use %d, media type %d, sep type %d (1-SNK)\n", sep.seid, sep.in_use, sep.media_type, sep.type);
                            break;
                        case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_SBC_CAPABILITY:
                            app_state = AVDTP_APPLICATION_IDLE;
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
                            app_state = AVDTP_APPLICATION_IDLE;
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
                            
                            if (sbc_configuration.reconfigure){
                                close_media_processing();
                                init_media_processing(sbc_configuration);
                            } else {
                                init_media_processing(sbc_configuration);
                            }
                            break;
                        }  
                        case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_OTHER_CAPABILITY:
                            printf(" received non SBC codec. not implemented\n");
                            break;
                        case AVDTP_SUBEVENT_SIGNALING_ACCEPT:
                            app_state = AVDTP_APPLICATION_IDLE;
                            break;
                        default:
                            printf(" not implemented\n");
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
    printf("\n--- Bluetooth AVDTP SINK Test Console %s ---\n", bd_addr_to_str(iut_address));
    printf("c      - create connection to addr %s\n", bd_addr_to_str(remote));
    printf("C      - disconnect\n");
    printf("d      - discover stream endpoints\n");
    printf("g      - get capabilities\n");
    printf("a      - get all capabilities\n");
    printf("s      - set configuration\n");
    printf("f      - get configuration\n");
    printf("R      - reconfigure stream with %d\n", sep.seid);
    printf("o      - open stream with seid %d\n", sep.seid);
    printf("m      - start stream with %d\n", sep.seid);
    printf("A      - abort stream with %d\n", sep.seid);
    printf("S      - stop stream with %d\n", sep.seid);
    printf("P      - suspend stream with %d\n", sep.seid);
    printf("Ctrl-c - exit\n");
    printf("---\n");
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

static void stdin_process(btstack_data_source_t *ds, btstack_data_source_callback_type_t callback_type){
    UNUSED(ds);
    UNUSED(callback_type);

    int cmd = btstack_stdin_read();

    sep.seid = 1;
    switch (cmd){
        case 'c':
            printf("Creating L2CAP Connection to %s, PSM_AVDTP\n", bd_addr_to_str(remote));
            avdtp_sink_connect(remote);
            break;
        case 'C':
            printf("Disconnect not implemented\n");
            avdtp_sink_disconnect(con_handle);
            break;
        case 'd':
            app_state = AVDTP_APPLICATION_W2_DISCOVER_SEPS;
            avdtp_sink_discover_stream_endpoints(con_handle);
            break;
        case 'g':
            app_state = AVDTP_APPLICATION_W2_GET_CAPABILITIES;
            avdtp_sink_get_capabilities(con_handle, sep.seid);
            break;
        case 'a':
            app_state = AVDTP_APPLICATION_W2_GET_ALL_CAPABILITIES;
            avdtp_sink_get_all_capabilities(con_handle, sep.seid);
            break;
        case 'f':
            app_state = AVDTP_APPLICATION_W2_GET_CONFIGURATION;
            avdtp_sink_get_configuration(con_handle, sep.seid);
            break;
        case 's':
            app_state = AVDTP_APPLICATION_W2_SET_CONFIGURATION;
            remote_configuration_bitmap = store_bit16(remote_configuration_bitmap, AVDTP_MEDIA_CODEC, 1);
            remote_configuration.media_codec.media_type = AVDTP_AUDIO;
            remote_configuration.media_codec.media_codec_type = AVDTP_CODEC_SBC;
            remote_configuration.media_codec.media_codec_information_len = sizeof(media_sbc_codec_configuration);
            remote_configuration.media_codec.media_codec_information = media_sbc_codec_configuration;
            avdtp_sink_set_configuration(con_handle, local_stream_endpoint->sep.seid, sep.seid, remote_configuration_bitmap, remote_configuration);
            break;
        case 'R':
            app_state = AVDTP_APPLICATION_W2_RECONFIGURE_WITH_SEID;
            remote_configuration_bitmap = store_bit16(remote_configuration_bitmap, AVDTP_MEDIA_CODEC, 1);
            remote_configuration.media_codec.media_type = AVDTP_AUDIO;
            remote_configuration.media_codec.media_codec_type = AVDTP_CODEC_SBC;
            remote_configuration.media_codec.media_codec_information_len = sizeof(media_sbc_codec_reconfiguration);
            remote_configuration.media_codec.media_codec_information = media_sbc_codec_reconfiguration;
            avdtp_sink_reconfigure(con_handle, sep.seid, remote_configuration_bitmap, remote_configuration);
            break;
        case 'o':
            app_state = AVDTP_APPLICATION_W2_OPEN_STREAM_WITH_SEID;
            avdtp_sink_open_stream(con_handle, sep.seid);
            break;
        case 'm': 
            app_state = AVDTP_APPLICATION_W2_START_STREAM_WITH_SEID;
            avdtp_sink_start_stream(con_handle, sep.seid);
            break;
        case 'A':
            app_state = AVDTP_APPLICATION_W2_ABORT_STREAM_WITH_SEID;
            avdtp_sink_abort_stream(con_handle, sep.seid);
            break;
        case 'S':
            app_state = AVDTP_APPLICATION_W2_STOP_STREAM_WITH_SEID;
            avdtp_sink_stop_stream(con_handle, sep.seid);
            break;
        case 'P':
            app_state = AVDTP_APPLICATION_W2_SUSPEND_STREAM_WITH_SEID;
            avdtp_sink_suspend(con_handle, sep.seid);
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

    UNUSED(argc);
    (void)argv;

    /* Register for HCI events */
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    l2cap_init();
    // Initialize AVDTP Sink
    avdtp_sink_init();
    avdtp_sink_register_packet_handler(&packet_handler);

//#ifndef SMG_BI
    local_stream_endpoint = avdtp_sink_create_stream_endpoint(AVDTP_SINK, AVDTP_AUDIO);
    local_stream_endpoint->sep.seid = 5;
    avdtp_sink_register_media_transport_category(local_stream_endpoint->sep.seid);
    avdtp_sink_register_media_codec_category(local_stream_endpoint->sep.seid, AVDTP_AUDIO, AVDTP_CODEC_SBC, media_sbc_codec_capabilities, sizeof(media_sbc_codec_capabilities));
//#endif
    // uint8_t cp_type_lsb,  uint8_t cp_type_msb, const uint8_t * cp_type_value, uint8_t cp_type_value_len
    // avdtp_sink_register_content_protection_category(seid, 2, 2, NULL, 0);

    avdtp_sink_register_media_handler(&handle_l2cap_media_data_packet);

    // Initialize SDP 
    sdp_init();
    memset(sdp_avdtp_sink_service_buffer, 0, sizeof(sdp_avdtp_sink_service_buffer));
    a2dp_sink_create_sdp_record(sdp_avdtp_sink_service_buffer, 0x10001, 1, NULL, NULL);
    sdp_register_service(sdp_avdtp_sink_service_buffer);
    
    gap_set_local_name("BTstack AVDTP Test");
    gap_discoverable_control(1);
    gap_set_class_of_device(0x200408);
    
    // turn on!
    hci_power_control(HCI_POWER_ON);

    btstack_stdin_setup(stdin_process);
    return 0;
}
