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

#include "btstack_config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

#define NUM_CHANNELS 2
#define SAMPLE_RATE 44100

#ifdef HAVE_PORTAUDIO
#include <portaudio.h>
#include "btstack_ring_buffer.h"
#endif

#ifdef HAVE_POSIX_FILE_IO
#define STORE_SBC_TO_SBC_FILE 
#define STORE_SBC_TO_WAV_FILE 
#endif

#ifdef HAVE_PORTAUDIO
#define PA_SAMPLE_TYPE      paInt16
#define FRAMES_PER_BUFFER   128
#define BYTES_PER_FRAME     (2*NUM_CHANNELS)
#define PREBUFFER_MS        150
#define PREBUFFER_BYTES     (PREBUFFER_MS*SAMPLE_RATE/1000*BYTES_PER_FRAME)
static uint8_t ring_buffer_storage[2*PREBUFFER_BYTES];
static btstack_ring_buffer_t ring_buffer;

static PaStream * stream;
static uint8_t pa_stream_started = 0;

static int patestCallback( const void *inputBuffer, void *outputBuffer,
                           unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData ) {
    (void) timeInfo; /* Prevent unused variable warnings. */
    (void) statusFlags;
    (void) inputBuffer;
    
    uint32_t bytes_read = 0;
    int bytes_per_buffer = framesPerBuffer * BYTES_PER_FRAME;

    if (btstack_ring_buffer_bytes_available(&ring_buffer) >= bytes_per_buffer){
        btstack_ring_buffer_read(&ring_buffer, outputBuffer, bytes_per_buffer, &bytes_read);
    } else {
        memset(outputBuffer, 0, bytes_per_buffer);
    }
    // printf("bytes avail after read: %d\n", btstack_ring_buffer_bytes_available(&ring_buffer));
    return 0;
}
#endif

#ifdef STORE_SBC_TO_WAV_FILE    
// store sbc as wav:
static btstack_sbc_decoder_state_t state;
static int total_num_samples = 0;
static int frame_count = 0;
static char * wav_filename = "avdtp_sink.wav";
static btstack_sbc_mode_t mode = SBC_MODE_STANDARD;

static void handle_pcm_data(int16_t * data, int num_samples, int num_channels, int sample_rate, void * context){
    wav_writer_write_int16(num_samples*num_channels, data);
    total_num_samples+=num_samples*num_channels;
    frame_count++;

#ifdef HAVE_PORTAUDIO
    if (!pa_stream_started && btstack_ring_buffer_bytes_available(&ring_buffer) >= PREBUFFER_BYTES){
        /* -- start stream -- */
        PaError err = Pa_StartStream(stream);
        if (err != paNoError){
            printf("Error starting the stream: \"%s\"\n",  Pa_GetErrorText(err));
            return;
        }
        pa_stream_started = 1; 
    }
    btstack_ring_buffer_write(&ring_buffer, (uint8_t *)data, num_samples*num_channels*2);
    // printf("bytes avail after write: %d\n", btstack_ring_buffer_bytes_available(&ring_buffer));
#endif
}
#endif

#ifdef STORE_SBC_TO_SBC_FILE    
static FILE * sbc_file;
static char * sbc_filename = "avdtp_sink.sbc";
#endif

static bd_addr_t remote = {0x04, 0x0C, 0xCE, 0xE4, 0x85, 0xD3};
static uint8_t sdp_avdtp_sink_service_buffer[150];

uint16_t local_cid = 0;
static btstack_packet_callback_registration_t hci_event_callback_registration;


static void handle_l2cap_media_data_packet(avdtp_stream_endpoint_t * stream_endpoint, uint8_t *packet, uint16_t size){
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

    // printf("SBC HEADER: num_frames %u, fragmented %u, start %u, stop %u\n", sbc_header.num_frames, sbc_header.fragmentation, sbc_header.starting_packet, sbc_header.last_packet);
    // printf_hexdump( packet+pos, size-pos );
#ifdef STORE_SBC_TO_WAV_FILE
    btstack_sbc_decoder_process_data(&state, 0, packet+pos, size-pos);
#endif

#ifdef STORE_SBC_TO_SBC_FILE
    fwrite(packet+pos, size-pos, 1, sbc_file);
#endif
}

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    bd_addr_t event_addr;
    
    switch (packet_type) {
        case L2CAP_DATA_PACKET:
            // just dump data for now
            printf("source cid %x -- ", channel);
            // printf_hexdump( packet, size );
            break;
            
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {

                case BTSTACK_EVENT_STATE:
                    // bt stack activated, get started 
                    if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING){
                        
                    }
                    break;
                case HCI_EVENT_PIN_CODE_REQUEST:
                    // inform about pin code request
                    printf("Pin code request - using '0000'\n");
                    hci_event_pin_code_request_get_bd_addr(packet, event_addr);
                    hci_send_cmd(&hci_pin_code_request_reply, &event_addr, 4, "0000");
                    break;

                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    // connection closed -> quit tes app
                    printf("\n --- avdtp_test: HCI_EVENT_DISCONNECTION_COMPLETE ---\n");
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
                    // exit(0);
                    break;
                    
                default:
                    // other event
                    break;
            }
            break;
            
        default:
            // other packet type
            break;
    }
}

static void show_usage(void){
    printf("\n--- CLI for L2CAP TEST ---\n");
    printf("c      - create connection to SDP at addr %s\n", bd_addr_to_str(remote));
    printf("d      - disconnect\n");
    printf("Ctrl-c - exit\n");
    printf("---\n");
}

static void stdin_process(btstack_data_source_t *ds, btstack_data_source_callback_type_t callback_type){
    char buffer;
    read(ds->fd, &buffer, 1);
    switch (buffer){
        case 'c':
            printf("Creating L2CAP Connection to %s, PSM_AVDTP\n", bd_addr_to_str(remote));
            avdtp_sink_connect(remote);
            break;
        case 'd':
            printf("L2CAP Channel Closed\n");
            // avdtp_sink_disconnect(local_cid);
            break;
        case '\n':
        case '\r':
            break;
        default:
            show_usage();
            break;

    }
}

static const uint8_t media_sbc_codec_info[] = {
    0xFF, // (AVDTP_SBC_44100 << 4) | AVDTP_SBC_STEREO,
    0xFF, // (AVDTP_SBC_BLOCK_LENGTH_16 << 4) | (AVDTP_SBC_SUBBANDS_8 << 2) | AVDTP_SBC_ALLOCATION_METHOD_LOUDNESS,
    2, 53
}; 


int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){

    /* Register for HCI events */
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    l2cap_init();
    // Initialize AVDTP Sink
    avdtp_sink_init();
    avdtp_sink_register_packet_handler(&packet_handler);

    uint8_t seid = avdtp_sink_register_stream_endpoint(AVDTP_SINK, AVDTP_AUDIO);
    avdtp_sink_register_media_transport_category(seid);
    avdtp_sink_register_media_codec_category(seid, AVDTP_AUDIO, AVDTP_CODEC_SBC, media_sbc_codec_info, sizeof(media_sbc_codec_info));
    
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

#ifdef STORE_SBC_TO_WAV_FILE
    btstack_sbc_decoder_init(&state, mode, handle_pcm_data, NULL);
    wav_writer_open(wav_filename, NUM_CHANNELS, SAMPLE_RATE);  
#endif

#ifdef STORE_SBC_TO_SBC_FILE    
    sbc_file = fopen(sbc_filename, "wb"); 
#endif

#ifdef HAVE_PORTAUDIO
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
    outputParameters.channelCount = NUM_CHANNELS;
    outputParameters.sampleFormat = PA_SAMPLE_TYPE;
    outputParameters.suggestedLatency = Pa_GetDeviceInfo( outputParameters.device )->defaultHighOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;
    /* -- setup stream -- */
    err = Pa_OpenStream(
           &stream,
           NULL,                /* &inputParameters */
           &outputParameters,
           SAMPLE_RATE,
           FRAMES_PER_BUFFER,
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
    
    // turn on!
    hci_power_control(HCI_POWER_ON);

    btstack_stdin_setup(stdin_process);
    return 0;
}
