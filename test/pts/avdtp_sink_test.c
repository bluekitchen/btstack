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

#include "btstack.h"
#include "wav_util.h"

#ifdef HAVE_PORTAUDIO
#include "btstack_ring_buffer.h"
#include <portaudio.h>
#endif

#ifdef HAVE_POSIX_FILE_IO
#include "wav_util.h"
#define STORE_SBC_TO_SBC_FILE 
#define STORE_SBC_TO_WAV_FILE 
#endif

#if defined(HAVE_PORTAUDIO) || defined(STORE_SBC_TO_WAV_FILE)
#define DECODE_SBC
#endif

#define NUM_CHANNELS 2
#define BYTES_PER_FRAME     (2*NUM_CHANNELS)
#define MAX_SBC_FRAME_SIZE 120

// SBC Decoder for WAV file or PortAudio
#ifdef DECODE_SBC
static btstack_sbc_decoder_state_t state;
static btstack_sbc_mode_t mode = SBC_MODE_STANDARD;
#endif

#if defined(HAVE_PORTAUDIO) 
#define PREBUFFER_MS        200
static int audio_stream_started = 0;
static int audio_stream_paused = 0;
static btstack_ring_buffer_t ring_buffer;
#endif

// PortAudio - live playback
#ifdef HAVE_PORTAUDIO
#define PA_SAMPLE_TYPE      paInt16
#define SAMPLE_RATE 48000
#define FRAMES_PER_BUFFER   128
#define PREBUFFER_BYTES     (PREBUFFER_MS*SAMPLE_RATE/1000*BYTES_PER_FRAME)
static PaStream * stream;
static uint8_t ring_buffer_storage[2*PREBUFFER_BYTES];
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

// mac 2011:    static const char * device_addr_string = "04:0C:CE:E4:85:D3";
// pts:         
static const char * device_addr_string = "00:1B:DC:07:32:EF";
// mac 2013:    static const char * device_addr_string = "84:38:35:65:d1:15";
// phone 2013:  static const char * device_addr_string = "D8:BB:2C:DF:F0:F2";
// minijambox:  static const char * device_addr_string = "00:21:3C:AC:F7:38";
// head phones: static const char * device_addr_string = "00:18:09:28:50:18";
// bt dongle:   static const char * device_addr_string = "00:15:83:5F:9D:46";
static bd_addr_t device_addr;

static uint8_t is_cmd_triggered_localy = 0;
static uint8_t is_media_header_reported_once = 0;
static uint8_t is_media_initialized = 0;

static uint16_t avdtp_cid = 0;
static uint8_t  local_seid = 0;
static uint8_t  sdp_avdtp_sink_service_buffer[150];

static adtvp_media_codec_information_sbc_t   sbc_capability;
static avdtp_media_codec_configuration_sbc_t sbc_configuration;
static uint8_t local_seid;
static uint8_t remote_seid;
// static avdtp_sep_t sep;

static uint16_t remote_configuration_bitmap;
static avdtp_capabilities_t remote_configuration;
static avdtp_context_t a2dp_sink_context;

static btstack_packet_callback_registration_t hci_event_callback_registration;
#if defined(HAVE_PORTAUDIO) || defined(STORE_SBC_TO_WAV_FILE)
static void handle_pcm_data(int16_t * data, int num_samples, int num_channels, int sample_rate, void * context);
#endif

#ifdef HAVE_PORTAUDIO
static int portaudio_callback( const void *inputBuffer, void *outputBuffer,
                           unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData ) {

    /** portaudio_callback is called from different thread, don't use hci_dump / log_info here without additional checks */

    // Prevent unused variable warnings.
    (void) timeInfo; 
    (void) statusFlags;
    (void) inputBuffer;
    (void) userData;
    
    int bytes_to_copy = framesPerBuffer * BYTES_PER_FRAME;

    // fill ring buffer with silence while stream is paused
    if (audio_stream_paused){
        if (btstack_ring_buffer_bytes_available(&ring_buffer) < PREBUFFER_BYTES){
            memset(outputBuffer, 0, bytes_to_copy);
            return 0;
        } else {
            // resume playback
            audio_stream_paused = 0;
        }
    }

    // get data from ring buffer
    uint32_t bytes_read = 0;
    btstack_ring_buffer_read(&ring_buffer, outputBuffer, bytes_to_copy, &bytes_read);
    bytes_to_copy -= bytes_read;

    // fill ring buffer  with silence if there are not enough bytes to copy
    if (bytes_to_copy){
        memset(outputBuffer + bytes_read, 0, bytes_to_copy);
        audio_stream_paused = 1;
    }
    return 0;
}
#endif

static int media_processing_init(avdtp_media_codec_configuration_sbc_t configuration){
    if (is_media_initialized) return 0;
#ifdef DECODE_SBC
    btstack_sbc_decoder_init(&state, mode, handle_pcm_data, NULL);
#endif

#ifdef STORE_SBC_TO_WAV_FILE
    wav_writer_open(wav_filename, configuration.num_channels, configuration.sampling_frequency);
#endif

#ifdef STORE_SBC_TO_SBC_FILE    
   sbc_file = fopen(sbc_filename, "wb"); 
#endif

#ifdef HAVE_PORTAUDIO
    // int frames_per_buffer = configuration.frames_per_buffer;
    PaError err;
    PaStreamParameters outputParameters;
    const PaDeviceInfo *deviceInfo;

    /* -- initialize PortAudio -- */
    err = Pa_Initialize();
    if (err != paNoError){
        printf("Error initializing portaudio: \"%s\"\n",  Pa_GetErrorText(err));
        return err;
    } 
    /* -- setup input and output -- */
    outputParameters.device = Pa_GetDefaultOutputDevice(); /* default output device */
    outputParameters.channelCount = configuration.num_channels;
    outputParameters.sampleFormat = PA_SAMPLE_TYPE;
    outputParameters.suggestedLatency = Pa_GetDeviceInfo( outputParameters.device )->defaultHighOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;
    deviceInfo = Pa_GetDeviceInfo( outputParameters.device );
    printf("PortAudio: Output device: %s\n", deviceInfo->name);
    log_info("PortAudio: Output device: %s", deviceInfo->name);
    /* -- setup stream -- */
    err = Pa_OpenStream(
           &stream,
           NULL,                /* &inputParameters */
           &outputParameters,
           configuration.sampling_frequency,
           0,
           paClipOff,           /* we won't output out of range samples so don't bother clipping them */
           portaudio_callback,      /* use callback */
           NULL );   
    
    if (err != paNoError){
        printf("Error initializing portaudio: \"%s\"\n",  Pa_GetErrorText(err));
        return err;
    }
    log_info("PortAudio: stream opened");
    printf("PortAudio: stream opened\n");
#endif

 #if defined(HAVE_PORTAUDIO)
    memset(ring_buffer_storage, 0, sizeof(ring_buffer_storage));
    btstack_ring_buffer_init(&ring_buffer, ring_buffer_storage, sizeof(ring_buffer_storage));
    audio_stream_started = 0;
    audio_stream_paused = 0;
#endif 
 is_media_initialized = 1;
    return 0;
}

static void media_processing_close(void){
    if (is_media_initialized) return;
    is_media_initialized = 0;

#ifdef STORE_SBC_TO_WAV_FILE                  
    wav_writer_close();
    int total_frames_nr = state.good_frames_nr + state.bad_frames_nr + state.zero_frames_nr;

    printf("WAV Writer: Decoding done. Processed totaly %d frames:\n - %d good\n - %d bad\n", total_frames_nr, state.good_frames_nr, total_frames_nr - state.good_frames_nr);
    printf("WAV Writer: Written %d frames to wav file: %s\n", frame_count, wav_filename);
#endif

#ifdef STORE_SBC_TO_SBC_FILE
    fclose(sbc_file);
#endif     

#if defined(HAVE_PORTAUDIO)
    audio_stream_started = 0;
#endif

#ifdef HAVE_PORTAUDIO
    printf("PortAudio: Stream closed\n");
    log_info("PortAudio: Stream closed");

    PaError err = Pa_StopStream(stream);
    if (err != paNoError){
        printf("Error stopping the stream: \"%s\"\n",  Pa_GetErrorText(err));
        log_error("Error stopping the stream: \"%s\"",  Pa_GetErrorText(err));
        return;
    } 
    err = Pa_CloseStream(stream);
    if (err != paNoError){
        printf("Error closing the stream: \"%s\"\n",  Pa_GetErrorText(err));
        log_error("Error closing the stream: \"%s\"",  Pa_GetErrorText(err));
        return;
    } 
    err = Pa_Terminate();
    if (err != paNoError){
        printf("Error terminating portaudio: \"%s\"\n",  Pa_GetErrorText(err));
        log_error("Error terminating portaudio: \"%s\"",  Pa_GetErrorText(err));
        return;
    } 
#endif
}


/* @section Handle Media Data Packet 
 *
 * @text Media data packets, in this case the audio data, are received through the handle_l2cap_media_data_packet callback.
 * Currently, only the SBC media codec is supported. Hence, the media data consists of the media packet header and the SBC packet.
 * The SBC data will be decoded using an SBC decoder if either HAVE_PORTAUDIO or STORE_SBC_TO_WAV_FILE directive is defined.
 * The resulting PCM frames can be then captured through a PCM data callback registered during SBC decoder setup, i.e. the 
 * handle_pcm_data callback.
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
    
#if defined(HAVE_PORTAUDIO) || defined(STORE_SBC_TO_WAV_FILE)
    btstack_sbc_decoder_process_data(&state, 0, packet+pos, size-pos);
#endif

#ifdef STORE_SBC_TO_SBC_FILE
    fwrite(packet+pos, size-pos, 1, sbc_file);
#endif
}

 /* @section Handle PCM Data 
 *
 * @text In this example, we use the [PortAudio library](http://www.portaudio.com) to play the audio stream. 
 * The PCM data are bufferd in a ring buffer.
 * Aditionally, tha audio data can be stored in the avdtp_sink.wav file. 
 */
#if defined(HAVE_PORTAUDIO) || defined(STORE_SBC_TO_WAV_FILE) 
static void handle_pcm_data(int16_t * data, int num_samples, int num_channels, int sample_rate, void * context){
    UNUSED(sample_rate);
    UNUSED(context);

#ifdef STORE_SBC_TO_WAV_FILE
    wav_writer_write_int16(num_samples*num_channels, data);
    frame_count++;
#endif

#ifdef HAVE_PORTAUDIO
    // store pcm samples in ring buffer
    btstack_ring_buffer_write(&ring_buffer, (uint8_t *)data, num_samples*num_channels*2);

    if (!audio_stream_started){
        audio_stream_paused  = 1;
        /* -- start stream -- */
        PaError err = Pa_StartStream(stream);
        if (err != paNoError){
            printf("Error starting the stream: \"%s\"\n",  Pa_GetErrorText(err));
            return;
        }
        audio_stream_started = 1; 
    }
#endif
}
#endif

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
    // TODO: read csrc list
    
    // printf_hexdump( packet, pos );
    if (!is_media_header_reported_once){
        is_media_header_reported_once = 1;
        printf("MEDIA HEADER: %u timestamp, version %u, padding %u, extension %u, csrc_count %u\n", 
            media_header->timestamp, media_header->version, media_header->padding, media_header->extension, media_header->csrc_count);
        printf("MEDIA HEADER: marker %02x, payload_type %02x, sequence_number %u, synchronization_source %u\n", 
            media_header->marker, media_header->payload_type, media_header->sequence_number, media_header->synchronization_source);
    }
    return 1;
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

    if (hci_event_packet_get_type(packet) == HCI_EVENT_PIN_CODE_REQUEST){
        // inform about pin code request
        bd_addr_t event_addr;
        printf("Pin code request - using '0000'\n");
        hci_event_pin_code_request_get_bd_addr(packet, event_addr);
        gap_pin_code_response(event_addr, "0000");
    }

    if (hci_event_packet_get_type(packet) != HCI_EVENT_AVDTP_META) return; 
    UNUSED(channel);
    UNUSED(size);
    uint8_t status;

    switch (packet[2]){
        case AVDTP_SUBEVENT_SIGNALING_CONNECTION_ESTABLISHED:
            avdtp_cid = avdtp_subevent_signaling_connection_established_get_avdtp_cid(packet);
            status    = avdtp_subevent_signaling_connection_established_get_status(packet);
            if (status != ERROR_CODE_SUCCESS){
                printf("AVDTP connection failed with status 0x%02x.\n", status);
                break;    
            }
            printf("AVDTP Sink connected: avdtp_cid 0x%02x.\n", avdtp_cid);
            break;
        case AVDTP_SUBEVENT_SIGNALING_CONNECTION_RELEASED:
            avdtp_cid = avdtp_subevent_signaling_connection_released_get_avdtp_cid(packet);
            printf("AVDTP connection released: avdtp_cid 0x%02x.\n", avdtp_cid);
            break;
        case AVDTP_SUBEVENT_SIGNALING_SEP_FOUND:
            remote_seid = avdtp_subevent_signaling_sep_found_get_remote_seid(packet);
            printf("Found sep: seid %u, in_use %d, media type %d, sep type %d (1-SNK)\n", 
                remote_seid, avdtp_subevent_signaling_sep_found_get_in_use(packet), 
                avdtp_subevent_signaling_sep_found_get_media_type(packet), avdtp_subevent_signaling_sep_found_get_sep_type(packet));
            break;
        case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_SBC_CAPABILITY:
            printf("Received SBC codec capabilities\n");

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
            printf("Received SBC codec configuration\n");
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
            
            avdtp_sink_delay_report(avdtp_cid, remote_seid, 100);
            media_processing_close();
            media_processing_init(sbc_configuration);
            break;
        }  
        case AVDTP_SUBEVENT_STREAMING_CONNECTION_ESTABLISHED:
            printf("Streaming connection opened.\n");
            break;

        case AVDTP_SUBEVENT_STREAMING_CONNECTION_RELEASED:
            printf("Streaming connection released.\n");
            is_cmd_triggered_localy = 0;
            is_media_header_reported_once = 0;
            media_processing_close();
            break;
        case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_OTHER_CAPABILITY:
            printf("Received non SBC codec, event not parsed.\n");
            break;

        case AVDTP_SUBEVENT_SIGNALING_ACCEPT:{
            switch (avdtp_subevent_signaling_accept_get_signal_identifier(packet)){
                case  AVDTP_SI_START:
                    printf("Stream started\n");
                    media_processing_init(sbc_configuration);
                    break;
                case AVDTP_SI_SUSPEND:
                    printf("Stream paused\n");
                    media_processing_close();
                    break;
                case AVDTP_SI_ABORT:
                case AVDTP_SI_CLOSE:
                    printf("Stream stoped\n");
                    media_processing_close();
                    break;
                default:
                    break;
            }
            if (is_cmd_triggered_localy){
                is_cmd_triggered_localy = 0;
                printf("AVDTP Sink command accepted\n");
            }
            break;
        }
        case AVDTP_SUBEVENT_SIGNALING_REJECT:
        case AVDTP_SUBEVENT_SIGNALING_GENERAL_REJECT:
            if (is_cmd_triggered_localy){
                is_cmd_triggered_localy = 0;
                printf("AVDTP Sink command rejected\n");
            }
            break;
        default:
            if (is_cmd_triggered_localy){
                is_cmd_triggered_localy = 0;
            }
            printf("AVDTP Sink event 0x%02x not parsed\n", packet[2]);
            break; 
    }
}


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

static uint8_t media_sbc_codec_reconfiguration[] = {
    // (AVDTP_SBC_44100 << 4) | AVDTP_SBC_STEREO,
    // (AVDTP_SBC_BLOCK_LENGTH_16 << 4) | (AVDTP_SBC_SUBBANDS_8 << 2) | AVDTP_SBC_ALLOCATION_METHOD_SNR,
    // 32, 32
    (AVDTP_SBC_44100 << 4) | AVDTP_SBC_MONO,
    (AVDTP_SBC_BLOCK_LENGTH_8 << 4) | (AVDTP_SBC_SUBBANDS_8 << 2) | AVDTP_SBC_ALLOCATION_METHOD_LOUDNESS,
    32, 32 
}; 

#ifdef HAVE_BTSTACK_STDIN
static void show_usage(void){
    bd_addr_t      iut_address;
    gap_local_bd_addr(iut_address);
    printf("\n--- Bluetooth AVDTP SINK Test Console %s ---\n", bd_addr_to_str(iut_address));
    printf("c      - create connection to addr %s\n", device_addr_string);
    printf("d      - discover stream endpoints\n");
    printf("g      - get capabilities\n");
    printf("a      - get all capabilities\n");
    printf("s      - set configuration\n");
    printf("f      - get configuration\n");
    printf("R      - reconfigure stream with %d\n", remote_seid);
    printf("o      - establish stream with seid %d\n", remote_seid);
    printf("m      - start stream with %d\n", remote_seid);
    printf("A      - abort stream with %d\n", remote_seid);
    printf("P      - suspend (pause) stream with %d\n", remote_seid);
    printf("S      - stop (release) stream with %d\n", remote_seid);
    printf("D      - send delay report");
    printf("C      - disconnect\n");
    printf("Ctrl-c - exit\n");
    printf("---\n");
}

static void stdin_process(char cmd){
    uint8_t status = ERROR_CODE_SUCCESS;
    remote_seid = 1;
    is_cmd_triggered_localy = 1;
    switch (cmd){
        case 'c':
            printf("Establish AVDTP Sink connection to %s\n", device_addr_string);
            status = avdtp_sink_connect(device_addr, &avdtp_cid);
            break;
        case 'C':
            printf("Disconnect AVDTP Sink\n");
            status = avdtp_sink_disconnect(avdtp_cid);
            break;
        case 'd':
            printf("Discover stream endpoints of %s\n", device_addr_string);
            status = avdtp_sink_discover_stream_endpoints(avdtp_cid);
            break;
        case 'g':
            printf("Get capabilities of stream endpoint with seid %d\n", remote_seid);
            status = avdtp_sink_get_capabilities(avdtp_cid, remote_seid);
            break;
        case 'a':
            printf("Get all capabilities of stream endpoint with seid %d\n", remote_seid);
            status = avdtp_sink_get_all_capabilities(avdtp_cid, remote_seid);
            break;
        case 'f':
            printf("Get configuration of stream endpoint with seid %d\n", remote_seid);
            status = avdtp_sink_get_configuration(avdtp_cid, remote_seid);
            break;
        case 's':
            printf("Set configuration of stream endpoint with seid %d\n", remote_seid);
            remote_configuration_bitmap = store_bit16(remote_configuration_bitmap, AVDTP_MEDIA_CODEC, 1);
            remote_configuration.media_codec.media_type = AVDTP_AUDIO;
            remote_configuration.media_codec.media_codec_type = AVDTP_CODEC_SBC;
            remote_configuration.media_codec.media_codec_information_len = sizeof(media_sbc_codec_configuration);
            remote_configuration.media_codec.media_codec_information = media_sbc_codec_configuration;
            status = avdtp_sink_set_configuration(avdtp_cid, local_seid, remote_seid, remote_configuration_bitmap, remote_configuration);
            break;
        case 'R':
            printf("Reconfigure stream endpoint with seid %d\n", remote_seid);
            remote_configuration_bitmap = 0;
            remote_configuration_bitmap = store_bit16(remote_configuration_bitmap, AVDTP_MEDIA_CODEC, 1);
            remote_configuration.media_codec.media_type = AVDTP_AUDIO;
            remote_configuration.media_codec.media_codec_type = AVDTP_CODEC_SBC;
            remote_configuration.media_codec.media_codec_information_len = sizeof(media_sbc_codec_reconfiguration);
            remote_configuration.media_codec.media_codec_information = media_sbc_codec_reconfiguration;
            status = avdtp_sink_reconfigure(avdtp_cid, local_seid, remote_seid, remote_configuration_bitmap, remote_configuration);
            break;
        case 'o':
            printf("Establish stream between local %d and remote %d seid\n", local_seid, remote_seid);
            status = avdtp_sink_open_stream(avdtp_cid, local_seid, remote_seid);
            break;
        case 'm': 
            printf("Start stream between local %d and remote %d seid\n", local_seid, remote_seid);
            status = avdtp_sink_start_stream(avdtp_cid, local_seid);
            break;
        case 'A':
            printf("Abort stream between local %d and remote %d seid\n", local_seid, remote_seid);
            status = avdtp_sink_abort_stream(avdtp_cid, local_seid);
            break;
        case 'S':
            printf("Release stream between local %d and remote %d seid\n", local_seid, remote_seid);
            status = avdtp_sink_stop_stream(avdtp_cid, local_seid);
            break;
        case 'P':
            printf("Suspend stream between local %d and remote %d seid\n", local_seid, remote_seid);
            status = avdtp_sink_suspend(avdtp_cid, local_seid);
            break;
        case 'D':
            printf("Send delay report between local %d and remote %d seid\n", local_seid, remote_seid);
            status = avdtp_sink_delay_report(avdtp_cid, remote_seid, 100);
            break;
        case '\n':
        case '\r':
            is_cmd_triggered_localy = 0;
            break;
        default:
            is_cmd_triggered_localy = 0;
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
    avdtp_sink_init(&a2dp_sink_context);
    avdtp_sink_register_packet_handler(&packet_handler);

    avdtp_stream_endpoint_t * local_stream_endpoint = avdtp_sink_create_stream_endpoint(AVDTP_SINK, AVDTP_AUDIO);
    if (!local_stream_endpoint) {
        printf("AVDTP Sink: not enough memory to create local_stream_endpoint\n");
        return 1;
    }
    local_seid = avdtp_local_seid(local_stream_endpoint);
    avdtp_sink_register_media_transport_category(local_seid);
    avdtp_sink_register_media_codec_category(local_seid, AVDTP_AUDIO, AVDTP_CODEC_SBC, media_sbc_codec_capabilities, sizeof(media_sbc_codec_capabilities));

    avdtp_sink_register_media_handler(&handle_l2cap_media_data_packet);
    // Initialize SDP 
    sdp_init();
    memset(sdp_avdtp_sink_service_buffer, 0, sizeof(sdp_avdtp_sink_service_buffer));
    a2dp_sink_create_sdp_record(sdp_avdtp_sink_service_buffer, 0x10001, 1, NULL, NULL);
    sdp_register_service(sdp_avdtp_sink_service_buffer);
    // printf("BTstack AVDTP Sink, supported features 0x%04x\n", );
    gap_set_local_name("BTstack AVDTP Sink PTS 00:00:00:00:00:00");
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
