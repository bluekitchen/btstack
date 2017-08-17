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

#include "btstack.h"

#define AVRCP_BROWSING_ENABLED 0

#ifdef HAVE_BTSTACK_STDIN
#include "btstack_stdin.h"
#endif

#ifdef HAVE_AUDIO_DMA
#include "btstack_ring_buffer.h"
#include "hal_audio_dma.h"
#endif

#ifdef HAVE_PORTAUDIO
#include "btstack_ring_buffer.h"
#include <portaudio.h>
#endif

#ifdef HAVE_POSIX_FILE_IO
#include "wav_util.h"
#define STORE_SBC_TO_SBC_FILE 
#define STORE_SBC_TO_WAV_FILE 
#endif

#if defined(HAVE_PORTAUDIO) || defined(STORE_SBC_TO_WAV_FILE) || defined(HAVE_AUDIO_DMA)
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

#if defined(HAVE_PORTAUDIO) || defined (HAVE_AUDIO_DMA)
#define PREBUFFER_MS        200
static int audio_stream_started = 0;
static int audio_stream_paused = 0;
static btstack_ring_buffer_t ring_buffer;
#endif

#ifdef HAVE_AUDIO_DMA
// below 30: add samples, 30-40: fine, above 40: drop samples
#define OPTIMAL_FRAMES_MIN 30
#define OPTIMAL_FRAMES_MAX 40
#define ADDITIONAL_FRAMES  10
#define DMA_AUDIO_FRAMES 128
#define DMA_MAX_FILL_FRAMES 1
#define NUM_AUDIO_BUFFERS 2

static uint16_t audio_samples[(DMA_AUDIO_FRAMES + DMA_MAX_FILL_FRAMES)*2*NUM_AUDIO_BUFFERS];
static uint16_t audio_samples_len[NUM_AUDIO_BUFFERS];
static uint8_t ring_buffer_storage[(OPTIMAL_FRAMES_MAX + ADDITIONAL_FRAMES) * MAX_SBC_FRAME_SIZE];
static const uint16_t silent_buffer[DMA_AUDIO_FRAMES*2];
static volatile int playback_buffer;
static int write_buffer;
static uint8_t sbc_frame_size;
static int sbc_samples_fix;
#endif

// PortAdudio - live playback
#ifdef HAVE_PORTAUDIO
#define PA_SAMPLE_TYPE      paInt16
#define SAMPLE_RATE 48000
#define FRAMES_PER_BUFFER   128
#define PREBUFFER_BYTES     (PREBUFFER_MS*SAMPLE_RATE/1000*BYTES_PER_FRAME)
static PaStream * stream;
static uint8_t ring_buffer_storage[2*PREBUFFER_BYTES];
static int total_num_samples = 0;
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

#ifdef HAVE_BTSTACK_STDIN
// mac 2011: static bd_addr_t remote = {0x04, 0x0C, 0xCE, 0xE4, 0x85, 0xD3};
// pts: static bd_addr_t remote = {0x00, 0x1B, 0xDC, 0x08, 0x0A, 0xA5};
// mac 2013: 
// mac 2013: static const char * device_addr_string = "84:38:35:65:d1:15";
// iPhone 5S: 
static const char * device_addr_string = "54:E4:3A:26:A2:39";
#endif

// bt dongle: -u 02-02 static bd_addr_t remote = {0x00, 0x02, 0x72, 0xDC, 0x31, 0xC1};

static uint8_t  sdp_avdtp_sink_service_buffer[150];
static avdtp_media_codec_configuration_sbc_t sbc_configuration;
static uint16_t a2dp_cid = 0;
static uint8_t  local_seid = 0;

typedef enum {
    AVDTP_APPLICATION_IDLE,
    AVDTP_APPLICATION_CONNECTED,
    AVDTP_APPLICATION_STREAMING
} avdtp_application_state_t;

avdtp_application_state_t app_state = AVDTP_APPLICATION_IDLE;

static btstack_packet_callback_registration_t hci_event_callback_registration;

static int media_initialized = 0;

#ifdef HAVE_BTSTACK_STDIN
static bd_addr_t device_addr;
#endif

static uint16_t avrcp_cid = 0;
static uint8_t sdp_avrcp_controller_service_buffer[200];

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
    if (audio_stream_paused){

        if (btstack_ring_buffer_bytes_available(&ring_buffer) < PREBUFFER_BYTES){
            // printf("PA: silence\n");
            memset(outputBuffer, 0, bytes_to_copy);
            return 0;
        } else {
            // resume playback
            audio_stream_paused = 0;
        }
    }

    // get data from ringbuffer
    uint32_t bytes_read = 0;
    btstack_ring_buffer_read(&ring_buffer, outputBuffer, bytes_to_copy, &bytes_read);
    bytes_to_copy -= bytes_read;

    // fill with 0 if not enough
    if (bytes_to_copy){
        memset(outputBuffer + bytes_read, 0, bytes_to_copy);
        audio_stream_paused = 1;
    }
    return 0;
}
#endif

#ifdef HAVE_AUDIO_DMA
static int next_buffer(int current){
	if (current == NUM_AUDIO_BUFFERS-1) return 0;
	return current + 1;
}
static uint8_t * start_of_buffer(int num){
	return (uint8_t *) &audio_samples[num * DMA_AUDIO_FRAMES * 2];
}
void hal_audio_dma_done(void){
	if (audio_stream_paused){
		hal_audio_dma_play((const uint8_t *) silent_buffer, DMA_AUDIO_FRAMES*4);
		return;
	}
	// next buffer
	int next_playback_buffer = next_buffer(playback_buffer);
	uint8_t * playback_data;
	if (next_playback_buffer == write_buffer){

		// TODO: stop codec while playing silence when getting 'stream paused'

		// start playing silence
		audio_stream_paused = 1;
		hal_audio_dma_play((const uint8_t *) silent_buffer, DMA_AUDIO_FRAMES*4);
		printf("%6u - paused - bytes in buffer %u\n", (int) btstack_run_loop_get_time_ms(), btstack_ring_buffer_bytes_available(&ring_buffer));
		return;
	}
	playback_buffer = next_playback_buffer;
	playback_data = start_of_buffer(playback_buffer);
	hal_audio_dma_play(playback_data, audio_samples_len[playback_buffer]);
    // btstack_run_loop_embedded_trigger();
}
#endif

#if defined(HAVE_PORTAUDIO) || defined(STORE_SBC_TO_WAV_FILE) 

static void handle_pcm_data(int16_t * data, int num_samples, int num_channels, int sample_rate, void * context){
    UNUSED(sample_rate);
    UNUSED(context);

#ifdef STORE_SBC_TO_WAV_FILE
    wav_writer_write_int16(num_samples*num_channels, data);
    frame_count++;
#endif

#ifdef HAVE_PORTAUDIO
    total_num_samples+=num_samples*num_channels;

    // store pcm samples in ringbuffer
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


#ifdef HAVE_AUDIO_DMA

static void handle_pcm_data(int16_t * data, int num_samples, int num_channels, int sample_rate, void * context){
    UNUSED(sample_rate);
    UNUSED(context);
    total_num_samples+=num_samples*num_channels;

    // store in ring buffer
    uint8_t * write_data = start_of_buffer(write_buffer);
    uint16_t len = num_samples*num_channels*2;
    memcpy(write_data, data, len);
    audio_samples_len[write_buffer] = len;

    // add/drop audio frame to fix drift
    if (sbc_samples_fix > 0){
        memcpy(write_data + len, write_data + len - 4, 4);
        audio_samples_len[write_buffer] += 4;
    }
    if (sbc_samples_fix < 0){
        audio_samples_len[write_buffer] -= 4;
    }

    write_buffer = next_buffer(write_buffer);
}

static void hal_audio_dma_process(btstack_data_source_t * ds, btstack_data_source_callback_type_t callback_type){
	UNUSED(ds);
	UNUSED(callback_type);

	if (!media_initialized) return;

	int trigger_resume = 0;
	if (audio_stream_paused) {
		if (sbc_frame_size && btstack_ring_buffer_bytes_available(&ring_buffer) >= OPTIMAL_FRAMES_MIN * sbc_frame_size){
			trigger_resume = 1;
			// reset buffers
			playback_buffer = NUM_AUDIO_BUFFERS - 1;
			write_buffer = 0;
		} else {
			return;
		}
	}

	while (playback_buffer != write_buffer && btstack_ring_buffer_bytes_available(&ring_buffer) >= sbc_frame_size ){
		uint8_t frame[MAX_SBC_FRAME_SIZE];
	    uint32_t bytes_read = 0;
	    btstack_ring_buffer_read(&ring_buffer, frame, sbc_frame_size, &bytes_read);
	    btstack_sbc_decoder_process_data(&state, 0, frame, sbc_frame_size);
	}

	if (trigger_resume){
		printf("%6u - resume\n", (int) btstack_run_loop_get_time_ms());
		audio_stream_paused = 0;
	}
}

#endif

static int media_processing_init(avdtp_media_codec_configuration_sbc_t configuration){
    if (media_initialized) return 0;
    
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
           patestCallback,      /* use callback */
           NULL );   
    
    if (err != paNoError){
        printf("Error initializing portaudio: \"%s\"\n",  Pa_GetErrorText(err));
        return err;
    }
    log_info("PortAudio: stream opened");
    printf("PortAudio: stream opened\n");
#endif
#ifdef HAVE_AUDIO_DMA
    audio_stream_paused  = 1;
    hal_audio_dma_init(configuration.sampling_frequency);
    hal_audio_dma_set_audio_played(&hal_audio_dma_done);
    // start playing silence
    hal_audio_dma_done();
#endif

 #if defined(HAVE_PORTAUDIO) || defined (HAVE_AUDIO_DMA)
    memset(ring_buffer_storage, 0, sizeof(ring_buffer_storage));
    btstack_ring_buffer_init(&ring_buffer, ring_buffer_storage, sizeof(ring_buffer_storage));
    audio_stream_started = 0;
#endif 
    media_initialized = 1;
    return 0;
}

static void media_processing_close(void){
    if (!media_initialized) return;
    media_initialized = 0;

#ifdef STORE_SBC_TO_WAV_FILE 
    printf("WAV Writer: close file.\n");                   
    wav_writer_close();
    int total_frames_nr = state.good_frames_nr + state.bad_frames_nr + state.zero_frames_nr;

    printf("WAV Writer: Decoding done. Processed totaly %d frames:\n - %d good\n - %d bad\n - %d zero frames\n", total_frames_nr, state.good_frames_nr, state.bad_frames_nr, state.zero_frames_nr);
    printf("WAV Writer: Written %d frames to wav file: %s\n", frame_count, wav_filename);
#endif

#ifdef STORE_SBC_TO_SBC_FILE
    fclose(sbc_file);
#endif     

#if defined(HAVE_PORTAUDIO) || defined (HAVE_AUDIO_DMA)
    audio_stream_started = 0;
#endif

#ifdef HAVE_PORTAUDIO
    printf("PortAudio: Steram closed\n");
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

#ifdef HAVE_AUDIO_DMA
    hal_audio_dma_close();
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

#ifdef HAVE_AUDIO_DMA
    // store sbc frame size for buffer management
    sbc_frame_size = (size-pos)/ sbc_header.num_frames;
#endif
    
    UNUSED(sbc_header);
    // printf("SBC HEADER: num_frames %u, fragmented %u, start %u, stop %u\n", sbc_header.num_frames, sbc_header.fragmentation, sbc_header.starting_packet, sbc_header.last_packet);
    // printf_hexdump( packet+pos, size-pos );
    
#if defined(HAVE_PORTAUDIO) || defined(STORE_SBC_TO_WAV_FILE)
    btstack_sbc_decoder_process_data(&state, 0, packet+pos, size-pos);
#endif

#ifdef HAVE_AUDIO_DMA
    btstack_ring_buffer_write(&ring_buffer,  packet+pos, size-pos);

    // decide on audio sync drift based on number of sbc frames in queue
    int sbc_frames_in_buffer = btstack_ring_buffer_bytes_available(&ring_buffer) / sbc_frame_size;
    if (sbc_frames_in_buffer < OPTIMAL_FRAMES_MIN){
    	sbc_samples_fix = 1;	// duplicate last sample
    } else if (sbc_frames_in_buffer <= OPTIMAL_FRAMES_MAX){
    	sbc_samples_fix = 0;	// nothing to do
    } else {
    	sbc_samples_fix = -1;	// drop last sample
    }

    // dump
    printf("%6u %03u %d\n",  (int) btstack_run_loop_get_time_ms(), sbc_frames_in_buffer, sbc_samples_fix);

#endif

#ifdef STORE_SBC_TO_SBC_FILE
    fwrite(packet+pos, size-pos, 1, sbc_file);
#endif
}

static void dump_sbc_configuration(avdtp_media_codec_configuration_sbc_t configuration){
    printf(" -- a2dp sink demo: Received media codec configuration:\n");
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
            if (avrcp_cid != 0 && avrcp_cid != local_cid) {
                printf("AVRCP Connection failed, expected 0x%02X l2cap cid, received 0x%02X\n", avrcp_cid, local_cid);
                return;
            }

            status = avrcp_subevent_connection_established_get_status(packet);
            if (status != ERROR_CODE_SUCCESS){
                printf("AVRCP Connection failed: status 0x%02x\n", status);
                avrcp_cid = 0;
                return;
            }
            
            avrcp_cid = local_cid;
            avrcp_subevent_connection_established_get_bd_addr(packet, adress);
            printf("Channel successfully opened: %s, avrcp_cid 0x%02x\n", bd_addr_to_str(adress), avrcp_cid);

            // automatically enable notifications
            avrcp_controller_enable_notification(avrcp_cid, AVRCP_NOTIFICATION_EVENT_PLAYBACK_STATUS_CHANGED);
            avrcp_controller_enable_notification(avrcp_cid, AVRCP_NOTIFICATION_EVENT_NOW_PLAYING_CONTENT_CHANGED);
            return;
        }
        case AVRCP_SUBEVENT_CONNECTION_RELEASED:
            printf("Channel released: avrcp_cid 0x%02x\n", avrcp_subevent_connection_released_get_avrcp_cid(packet));
            avrcp_cid = 0;
            return;
        default:
            break;
    }

    status = packet[5];
    if (!avrcp_cid) return;

    // avoid printing INTERIM status
    if (status == AVRCP_CTYPE_RESPONSE_INTERIM) return;
            
    printf("AVRCP: command status: %s, ", avrcp_ctype2str(status));
    switch (packet[2]){
        case AVRCP_SUBEVENT_NOTIFICATION_PLAYBACK_STATUS_CHANGED:
            printf("notification, playback status changed %s\n", avrcp_play_status2str(avrcp_subevent_notification_playback_status_changed_get_play_status(packet)));
            return;
        case AVRCP_SUBEVENT_NOTIFICATION_NOW_PLAYING_CONTENT_CHANGED:
            printf("notification, playing content changed\n");
            return;
        case AVRCP_SUBEVENT_NOTIFICATION_TRACK_CHANGED:
            printf("notification track changed\n");
            return;
        case AVRCP_SUBEVENT_NOTIFICATION_VOLUME_CHANGED:
            printf("notification absolute volume changed %d\n", avrcp_subevent_notification_volume_changed_get_absolute_volume(packet));
            return;
        case AVRCP_SUBEVENT_NOTIFICATION_AVAILABLE_PLAYERS_CHANGED:
            printf("notification changed\n");
            return; 
        case AVRCP_SUBEVENT_SHUFFLE_AND_REPEAT_MODE:{
            uint8_t shuffle_mode = avrcp_subevent_shuffle_and_repeat_mode_get_shuffle_mode(packet);
            uint8_t repeat_mode  = avrcp_subevent_shuffle_and_repeat_mode_get_repeat_mode(packet);
            printf("%s, %s\n", avrcp_shuffle2str(shuffle_mode), avrcp_repeat2str(repeat_mode));
            break;
        }
        case AVRCP_SUBEVENT_NOW_PLAYING_INFO:{
            uint8_t value[100];
            printf("now playing: \n");
            if (avrcp_subevent_now_playing_info_get_title_len(packet) > 0){
                memcpy(value, avrcp_subevent_now_playing_info_get_title(packet), avrcp_subevent_now_playing_info_get_title_len(packet));
                printf("    Title: %s\n", value);
            }    
            if (avrcp_subevent_now_playing_info_get_album_len(packet) > 0){
                memcpy(value, avrcp_subevent_now_playing_info_get_album(packet), avrcp_subevent_now_playing_info_get_album_len(packet));
                printf("    Album: %s\n", value);
            }
            if (avrcp_subevent_now_playing_info_get_artist_len(packet) > 0){
                memcpy(value, avrcp_subevent_now_playing_info_get_artist(packet), avrcp_subevent_now_playing_info_get_artist_len(packet));
                printf("    Artist: %s\n", value);
            }
            if (avrcp_subevent_now_playing_info_get_genre_len(packet) > 0){
                memcpy(value, avrcp_subevent_now_playing_info_get_genre(packet), avrcp_subevent_now_playing_info_get_genre_len(packet));
                printf("    Genre: %s\n", value);
            }
            printf("    Track: %d\n", avrcp_subevent_now_playing_info_get_track(packet));
            printf("    Total nr. tracks: %d\n", avrcp_subevent_now_playing_info_get_total_tracks(packet));
            printf("    Song length: %d ms\n", avrcp_subevent_now_playing_info_get_song_length(packet));
            break;
        }
        case AVRCP_SUBEVENT_PLAY_STATUS:
            printf("song length: %d ms, song position: %d ms, play status: %s\n", 
                avrcp_subevent_play_status_get_song_length(packet), 
                avrcp_subevent_play_status_get_song_position(packet),
                avrcp_play_status2str(avrcp_subevent_play_status_get_play_status(packet)));
            break;
        case AVRCP_SUBEVENT_OPERATION_COMPLETE:
            printf("operation done %s\n", avrcp_operation2str(avrcp_subevent_operation_complete_get_operation_id(packet)));
            break;
        case AVRCP_SUBEVENT_OPERATION_START:
            printf("operation start %s\n", avrcp_operation2str(avrcp_subevent_operation_complete_get_operation_id(packet)));
            break;
        case AVRCP_SUBEVENT_PLAYER_APPLICATION_VALUE_RESPONSE:
            // response to set shuffle and repeat mode
            printf("\n");
            break;
        default:
            printf("Not implemented\n");
            break;
    }  
}

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    uint16_t cid;
    bd_addr_t address;

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_A2DP_META) return;

    switch (packet[2]){
        case A2DP_SUBEVENT_SIGNALING_MEDIA_CODEC_OTHER_CONFIGURATION:
            printf(" -- a2dp sink demo: received non SBC codec. not implemented.\n");
            break;
        case A2DP_SUBEVENT_SIGNALING_MEDIA_CODEC_SBC_CONFIGURATION:{
            printf(" -- a2dp sink demo: received SBC codec configuration.\n");
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
            cid = a2dp_subevent_stream_established_get_a2dp_cid(packet);
            if (cid != a2dp_cid) break;
            local_seid = a2dp_subevent_stream_established_get_local_seid(packet);
            printf(" -- a2dp sink demo: streaming connection is established, address %s, a2dp cid 0x%02X, local_seid %d\n", bd_addr_to_str(address), a2dp_cid, local_seid);
            app_state = AVDTP_APPLICATION_STREAMING;
            break;
        
        case A2DP_SUBEVENT_STREAM_STARTED:
            cid = a2dp_subevent_stream_started_get_a2dp_cid(packet);
            if (cid != a2dp_cid) break;
            local_seid = a2dp_subevent_stream_started_get_local_seid(packet);
            printf(" -- a2dp sink demo: stream started, a2dp cid 0x%02X, local_seid %d\n", a2dp_cid, local_seid);
            // started
            media_processing_init(sbc_configuration);
            break;
        
        case A2DP_SUBEVENT_STREAM_SUSPENDED:
            cid = a2dp_subevent_stream_suspended_get_a2dp_cid(packet);
            if (cid != a2dp_cid) break;
            local_seid = a2dp_subevent_stream_suspended_get_local_seid(packet);
            printf(" -- a2dp sink demo: stream paused, a2dp cid 0x%02X, local_seid %d\n", a2dp_cid, local_seid);
            // paused/stopped
            media_processing_close();
            break;
        
        case A2DP_SUBEVENT_STREAM_RELEASED:
            cid = a2dp_subevent_stream_released_get_a2dp_cid(packet);
            if (cid != a2dp_cid) break;
            local_seid = a2dp_subevent_stream_released_get_local_seid(packet);
            app_state = AVDTP_APPLICATION_IDLE;
            printf(" -- a2dp sink demo: stream released, a2dp cid 0x%02X, local_seid %d\n", a2dp_cid, local_seid);

            // paused/stopped
            media_processing_close();
            break;
        
        default:
            printf(" not implemented\n");
            break; 
    }
}

#ifdef HAVE_BTSTACK_STDIN
static void show_usage(void){
    bd_addr_t      iut_address;
    gap_local_bd_addr(iut_address);
    printf("\n--- Bluetooth AVDTP Sink/AVRCP Connection Test Console %s ---\n", bd_addr_to_str(iut_address));
    printf("b      - AVDTP Sink create  connection to addr %s\n", device_addr_string);
    printf("B      - AVDTP Sink disconnect\n");
    printf("c      - AVRCP create connection to addr %s\n", device_addr_string);
    printf("C      - AVRCP disconnect\n");

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
    printf("t - volume up\n");
    printf("T - volume down\n");
    printf("p - absolute volume of 50 percent\n");
    printf("M - mute\n");
    printf("r - skip\n");
    printf("q - query repeat and shuffle mode\n");
    printf("v - repeat single track\n");
    printf("x - repeat all tracks\n");
    printf("X - disable repeat mode\n");
    printf("z - shuffle all tracks\n");
    printf("Z - disable shuffle mode\n");
    printf("---\n");
}
#endif

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

#ifdef HAVE_BTSTACK_STDIN
static void stdin_process(char cmd){
    uint8_t status = ERROR_CODE_SUCCESS;
    switch (cmd){
        case 'b':
            printf(" - Create AVDTP connection to addr %s.\n", bd_addr_to_str(device_addr));
            status = a2dp_sink_establish_stream(device_addr, local_seid, &a2dp_cid);
            break;
        case 'B':
            printf(" - Disconnect\n");
            status = avdtp_sink_disconnect(a2dp_cid);
            break;
        case 'c':
            printf(" - Create AVRCP connection to addr %s.\n", bd_addr_to_str(device_addr));
            status = avrcp_controller_connect(device_addr, &avrcp_cid);
            break;
        case 'C':
            printf(" - Disconnect\n");
            status = avrcp_controller_disconnect(avrcp_cid);
            break;

        case '\n':
        case '\r':
            break;
        case 'O':
            printf(" - get play status\n");
            status = avrcp_controller_get_play_status(avrcp_cid);
            break;
        case 'j':
            printf(" - get now playing info\n");
            status = avrcp_controller_get_now_playing_info(avrcp_cid);
            break;
        case 'k':
            printf(" - play\n");
            status = avrcp_controller_play(avrcp_cid);
            break;
        case 'K':
            printf(" - stop\n");
            status = avrcp_controller_stop(avrcp_cid);
            break;
        case 'L':
            printf(" - pause\n");
            status = avrcp_controller_pause(avrcp_cid);
            break;
        case 'u':
            printf(" - start fast forward\n");
            status = avrcp_controller_start_fast_forward(avrcp_cid);
            break;
        case 'U':
            printf(" - stop fast forward\n");
            status = avrcp_controller_stop_fast_forward(avrcp_cid);
            break;
        case 'n':
            printf(" - start rewind\n");
            status = avrcp_controller_start_rewind(avrcp_cid);
            break;
        case 'N':
            printf(" - stop rewind\n");
            status = avrcp_controller_stop_rewind(avrcp_cid);
            break;
        case 'i':
            printf(" - forward\n");
            status = avrcp_controller_forward(avrcp_cid); 
            break;
        case 'I':
            printf(" - backward\n");
            status = avrcp_controller_backward(avrcp_cid);
            break;
        case 't':
            printf(" - volume up\n");
            status = avrcp_controller_volume_up(avrcp_cid);
            break;
        case 'T':
            printf(" - volume down\n");
            status = avrcp_controller_volume_down(avrcp_cid);
            break;
        case 'p':
            printf(" - absolute volume of 50 percent\n");
            status = avrcp_controller_set_absolute_volume(avrcp_cid, 50);
            break;
        case 'M':
            printf(" - mute\n");
            status = avrcp_controller_mute(avrcp_cid);
            break;
        case 'r':
            printf(" - skip\n");
            status = avrcp_controller_skip(avrcp_cid);
            break;
        case 'q':
            printf(" - query repeat and shuffle mode\n");
            status = avrcp_controller_query_shuffle_and_repeat_modes(avrcp_cid);
            break;
        case 'v':
            printf(" - repeat single track\n");
            status = avrcp_controller_set_repeat_mode(avrcp_cid, AVRCP_REPEAT_MODE_SINGLE_TRACK);
            break;
        case 'x':
            printf(" - repeat all tracks\n");
            status = avrcp_controller_set_repeat_mode(avrcp_cid, AVRCP_REPEAT_MODE_ALL_TRACKS);
            break;
        case 'X':
            printf(" - disable repeat mode\n");
            status = avrcp_controller_set_repeat_mode(avrcp_cid, AVRCP_REPEAT_MODE_OFF);
            break;
        case 'z':
            printf(" - shuffle all tracks\n");
            status = avrcp_controller_set_shuffle_mode(avrcp_cid, AVRCP_SHUFFLE_MODE_ALL_TRACKS);
            break;
        case 'Z':
            printf(" - disable shuffle mode\n");
            status = avrcp_controller_set_shuffle_mode(avrcp_cid, AVRCP_SHUFFLE_MODE_OFF);
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

    /* Register for HCI events */
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    l2cap_init();
    // Initialize AVDTP Sink
    a2dp_sink_init();
    a2dp_sink_register_packet_handler(&packet_handler);
    a2dp_sink_register_media_handler(&handle_l2cap_media_data_packet);

    avdtp_stream_endpoint_t * local_stream_endpoint = a2dp_sink_create_stream_endpoint(AVDTP_AUDIO, AVDTP_CODEC_SBC, media_sbc_codec_capabilities, sizeof(media_sbc_codec_capabilities), media_sbc_codec_configuration, sizeof(media_sbc_codec_configuration));
    if (!local_stream_endpoint){
        printf("A2DP Sink: not enough memory to create local stream endpoint\n");
        return 1;
    }
    local_seid = avdtp_local_seid(local_stream_endpoint);
    // Initialize AVRCP COntroller
    avrcp_controller_init();
    avrcp_controller_register_packet_handler(&avrcp_controller_packet_handler);
    
    // Initialize SDP 
    sdp_init();
    // setup AVDTP sink
    memset(sdp_avdtp_sink_service_buffer, 0, sizeof(sdp_avdtp_sink_service_buffer));
    a2dp_sink_create_sdp_record(sdp_avdtp_sink_service_buffer, 0x10001, 1, NULL, NULL);
    sdp_register_service(sdp_avdtp_sink_service_buffer);
    
    // setup AVRCP
    memset(sdp_avrcp_controller_service_buffer, 0, sizeof(sdp_avrcp_controller_service_buffer));
    avrcp_controller_create_sdp_record(sdp_avrcp_controller_service_buffer, 0x10001, AVRCP_BROWSING_ENABLED, 1, NULL, NULL);
    sdp_register_service(sdp_avrcp_controller_service_buffer);
    

    gap_set_local_name("BTstack A2DP Sink Test");
    gap_discoverable_control(1);
    gap_set_class_of_device(0x200408);
    printf("sdp, gap done\n");

    // turn on!
    hci_power_control(HCI_POWER_ON);

#ifdef HAVE_AUDIO_DMA
    static btstack_data_source_t hal_audio_dma_data_source;
    // set up polling data_source
    btstack_run_loop_set_data_source_handler(&hal_audio_dma_data_source, &hal_audio_dma_process);
    btstack_run_loop_enable_data_source_callbacks(&hal_audio_dma_data_source, DATA_SOURCE_CALLBACK_POLL);
    btstack_run_loop_add_data_source(&hal_audio_dma_data_source);
#endif

#ifdef HAVE_BTSTACK_STDIN
    // parse human readable Bluetooth address
    sscanf_bd_addr(device_addr_string, device_addr);
    btstack_stdin_setup(stdin_process);
#endif

    return 0;
}
