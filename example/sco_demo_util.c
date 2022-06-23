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

#define BTSTACK_FILE__ "sco_demo_util.c"
 
/*
 * sco_demo_util.c - send/receive test data via SCO, used by hfp_*_demo and hsp_*_demo
 */

#include <stdio.h>

#include "sco_demo_util.h"

#include "btstack_audio.h"
#include "btstack_debug.h"
#include "btstack_ring_buffer.h"
#include "classic/btstack_cvsd_plc.h"
#include "classic/btstack_sbc.h"
#include "classic/hfp.h"
#include "classic/hfp_msbc.h"

#ifdef _MSC_VER
// ignore deprecated warning for fopen
#pragma warning(disable : 4996)
#endif

#ifdef HAVE_POSIX_FILE_IO
#include "wav_util.h"
#endif

// test modes
#define SCO_DEMO_MODE_SINE		 0
#define SCO_DEMO_MODE_ASCII		 1
#define SCO_DEMO_MODE_COUNTER	 2
#define SCO_DEMO_MODE_55         3
#define SCO_DEMO_MODE_00         4
#define SCO_DEMO_MODE_MICROPHONE 5

// SCO demo configuration
#define SCO_DEMO_MODE               SCO_DEMO_MODE_MICROPHONE

// number of sco packets until 'report' on console
#define SCO_REPORT_PERIOD           100


#ifdef HAVE_POSIX_FILE_IO
// length and name of wav file on disk
#define SCO_WAV_DURATION_IN_SECONDS 15
#define SCO_WAV_FILENAME            "sco_input.wav"

// name of sbc test files
#define SCO_MSBC_OUT_FILENAME       "sco_output.msbc"
#define SCO_MSBC_IN_FILENAME        "sco_input.msbc"
#endif


// pre-buffer for CVSD and mSBC - also defines latency
#define SCO_CVSD_PA_PREBUFFER_MS    50
#define SCO_MSBC_PA_PREBUFFER_MS    50

// constants
#define NUM_CHANNELS            1
#define CVSD_SAMPLE_RATE        8000
#define MSBC_SAMPLE_RATE        16000
#define BYTES_PER_FRAME         2

#define CVSD_PA_PREBUFFER_BYTES (SCO_CVSD_PA_PREBUFFER_MS * CVSD_SAMPLE_RATE/1000 * BYTES_PER_FRAME)
#define MSBC_PA_PREBUFFER_BYTES (SCO_MSBC_PA_PREBUFFER_MS * MSBC_SAMPLE_RATE/1000 * BYTES_PER_FRAME)

// output

#if (SCO_DEMO_MODE == SCO_DEMO_MODE_SINE) || (SCO_DEMO_MODE == SCO_DEMO_MODE_MICROPHONE)
static int                   audio_output_paused  = 0;
static uint8_t               audio_output_ring_buffer_storage[2*MSBC_PA_PREBUFFER_BYTES];
static btstack_ring_buffer_t audio_output_ring_buffer;
#endif


// input
#if SCO_DEMO_MODE == SCO_DEMO_MODE_MICROPHONE
#define USE_AUDIO_INPUT
static int                   audio_input_paused  = 0;
static uint8_t               audio_input_ring_buffer_storage[2*8000];  // full second input buffer
static btstack_ring_buffer_t audio_input_ring_buffer;
#endif

static int dump_data = 1;
static int count_sent = 0;
static int count_received = 0;
static int negotiated_codec = -1; 

#ifdef ENABLE_HFP_WIDE_BAND_SPEECH
static btstack_sbc_decoder_state_t decoder_state;

#ifdef HAVE_POSIX_FILE_IO
FILE * msbc_file_in;
FILE * msbc_file_out;
#endif

#endif

static btstack_cvsd_plc_state_t cvsd_plc_state;

#define MAX_NUM_MSBC_SAMPLES (16*8)

int num_samples_to_write;
int num_audio_frames;
unsigned int phase;

#if SCO_DEMO_MODE == SCO_DEMO_MODE_SINE

// input signal: pre-computed sine wave, 266 Hz at 16000 kHz
static const int16_t sine_int16_at_16000hz[] = {
     0,   3135,   6237,   9270,  12202,  14999,  17633,  20073,  22294,  24270,
 25980,  27406,  28531,  29344,  29835,  30000,  29835,  29344,  28531,  27406,
 25980,  24270,  22294,  20073,  17633,  14999,  12202,   9270,   6237,   3135,
     0,  -3135,  -6237,  -9270, -12202, -14999, -17633, -20073, -22294, -24270,
-25980, -27406, -28531, -29344, -29835, -30000, -29835, -29344, -28531, -27406,
-25980, -24270, -22294, -20073, -17633, -14999, -12202,  -9270,  -6237,  -3135,
};

// 8 kHz samples for CVSD/SCO packets in little endian
static void sco_demo_sine_wave_int16_at_8000_hz_little_endian(unsigned int num_samples, uint8_t * data){
    unsigned int i;
    for (i=0; i < num_samples; i++){
        int16_t sample = sine_int16_at_16000hz[phase];
        little_endian_store_16(data, i * 2, sample);
        // ony use every second sample from 16khz table to get 8khz
        phase += 2;
        if (phase >= (sizeof(sine_int16_at_16000hz) / sizeof(int16_t))){
            phase = 0;
        }
    }
}

// 16 kHz samples for mSBC encoder in host endianess
#ifdef ENABLE_HFP_WIDE_BAND_SPEECH
static void sco_demo_sine_wave_int16_at_16000_hz_host_endian(unsigned int num_samples, int16_t * data){
    unsigned int i;
    for (i=0; i < num_samples; i++){
        data[i] = sine_int16_at_16000hz[phase++];
        if (phase >= (sizeof(sine_int16_at_16000hz) / sizeof(int16_t))){
            phase = 0;
        }
    }
}

static void sco_demo_msbc_fill_sine_audio_frame(void){
    if (!hfp_msbc_can_encode_audio_frame_now()) return;
    int num_samples = hfp_msbc_num_audio_samples_per_frame();
    if (num_samples > MAX_NUM_MSBC_SAMPLES) return;
    int16_t sample_buffer[MAX_NUM_MSBC_SAMPLES];
    sco_demo_sine_wave_int16_at_16000_hz_host_endian(num_samples, sample_buffer);
    hfp_msbc_encode_audio_frame(sample_buffer);
    num_audio_frames++;
}
#endif
#endif

#if (SCO_DEMO_MODE == SCO_DEMO_MODE_SINE) || (SCO_DEMO_MODE == SCO_DEMO_MODE_MICROPHONE)

static void playback_callback(int16_t * buffer, uint16_t num_samples){

    uint32_t prebuffer_bytes;
    switch (negotiated_codec){
        case HFP_CODEC_MSBC:
            prebuffer_bytes = MSBC_PA_PREBUFFER_BYTES;
            break;
        case HFP_CODEC_CVSD:
        default:
            prebuffer_bytes = CVSD_PA_PREBUFFER_BYTES;
            break;
    }

    // fill with silence while paused
    if (audio_output_paused){
        if (btstack_ring_buffer_bytes_available(&audio_output_ring_buffer) < prebuffer_bytes){
            memset(buffer, 0, num_samples * BYTES_PER_FRAME);
           return;
        } else {
            // resume playback
            audio_output_paused = 0;
        }
    }

    // get data from ringbuffer
    uint32_t bytes_read = 0;
    btstack_ring_buffer_read(&audio_output_ring_buffer, (uint8_t *) buffer, num_samples * BYTES_PER_FRAME, &bytes_read);
    num_samples -= bytes_read / BYTES_PER_FRAME;
    buffer      += bytes_read / BYTES_PER_FRAME;

    // fill with 0 if not enough
    if (num_samples){
        memset(buffer, 0, num_samples * BYTES_PER_FRAME);
        audio_output_paused = 1;
    }
}

#ifdef USE_AUDIO_INPUT
static void recording_callback(const int16_t * buffer, uint16_t num_samples){
    btstack_ring_buffer_write(&audio_input_ring_buffer, (uint8_t *)buffer, num_samples * 2);
}
#endif

// return 1 if ok
static int audio_initialize(int sample_rate){

    // -- output -- //

    // init buffers
    memset(audio_output_ring_buffer_storage, 0, sizeof(audio_output_ring_buffer_storage));
    btstack_ring_buffer_init(&audio_output_ring_buffer, audio_output_ring_buffer_storage, sizeof(audio_output_ring_buffer_storage));

    // config and setup audio playback
    const btstack_audio_sink_t * audio_sink = btstack_audio_sink_get_instance();
    if (!audio_sink) return 0;

    audio_sink->init(1, sample_rate, &playback_callback);
    audio_sink->start_stream();

    audio_output_paused  = 1;

    // -- input -- //

#ifdef USE_AUDIO_INPUT
    // init buffers
    memset(audio_input_ring_buffer_storage, 0, sizeof(audio_input_ring_buffer_storage));
    btstack_ring_buffer_init(&audio_input_ring_buffer, audio_input_ring_buffer_storage, sizeof(audio_input_ring_buffer_storage));

    // config and setup audio recording
    const btstack_audio_source_t * audio_source = btstack_audio_source_get_instance();
    if (!audio_source) return 0;

    audio_source->init(1, sample_rate, &recording_callback);
    audio_source->start_stream();

    audio_input_paused  = 1;
#endif

    return 1;
}

static void audio_terminate(void){
    const btstack_audio_sink_t * audio_sink = btstack_audio_sink_get_instance();
    if (!audio_sink) return;
    audio_sink->close();

#ifdef USE_AUDIO_INPUT
    const btstack_audio_source_t * audio_source= btstack_audio_source_get_instance();
    if (!audio_source) return;
    audio_source->close();
#endif
}

#ifdef ENABLE_HFP_WIDE_BAND_SPEECH

static void handle_pcm_data(int16_t * data, int num_samples, int num_channels, int sample_rate, void * context){
    UNUSED(context);
    UNUSED(sample_rate);
    UNUSED(data);
    UNUSED(num_samples);
    UNUSED(num_channels);

#if (SCO_DEMO_MODE == SCO_DEMO_MODE_SINE) || (SCO_DEMO_MODE == SCO_DEMO_MODE_MICROPHONE)

    // printf("handle_pcm_data num samples %u, sample rate %d\n", num_samples, num_channels);

    // samples in callback in host endianess, ready for playback
    btstack_ring_buffer_write(&audio_output_ring_buffer, (uint8_t *)data, num_samples*num_channels*2);

#ifdef SCO_WAV_FILENAME
    if (!num_samples_to_write) return;
    num_samples = btstack_min(num_samples, num_samples_to_write);
    num_samples_to_write -= num_samples;
    wav_writer_write_int16(num_samples, data);
    if (num_samples_to_write == 0){
        wav_writer_close();
    }
#endif /* SCO_WAV_FILENAME */

#endif /* Demo mode sine or microphone */
}
#endif /* ENABLE_HFP_WIDE_BAND_SPEECH */


#ifdef ENABLE_HFP_WIDE_BAND_SPEECH

static void sco_demo_init_mSBC(void){
    printf("SCO Demo: Init mSBC\n");

    btstack_sbc_decoder_init(&decoder_state, SBC_MODE_mSBC, &handle_pcm_data, NULL);    
    hfp_msbc_init();

#ifdef SCO_WAV_FILENAME
    num_samples_to_write = MSBC_SAMPLE_RATE * SCO_WAV_DURATION_IN_SECONDS;
    wav_writer_open(SCO_WAV_FILENAME, 1, MSBC_SAMPLE_RATE);
#endif

#if SCO_DEMO_MODE == SCO_DEMO_MODE_SINE
    sco_demo_msbc_fill_sine_audio_frame();
#endif

#ifdef SCO_MSBC_IN_FILENAME
    msbc_file_in = fopen(SCO_MSBC_IN_FILENAME, "wb");
    printf("SCO Demo: creating mSBC in file %s, %p\n", SCO_MSBC_IN_FILENAME, msbc_file_in);
#endif   

#ifdef SCO_MSBC_OUT_FILENAME
    msbc_file_out = fopen(SCO_MSBC_OUT_FILENAME, "wb");
    printf("SCO Demo: creating mSBC out file %s, %p\n", SCO_MSBC_OUT_FILENAME, msbc_file_out);
#endif   

    audio_initialize(MSBC_SAMPLE_RATE);
}

static void sco_demo_receive_mSBC(uint8_t * packet, uint16_t size){
#ifdef HAVE_POSIX_FILE_IO
    if (num_samples_to_write){
        if (msbc_file_in){
            // log incoming mSBC data for testing
            fwrite(packet+3, size-3, 1, msbc_file_in);
        }
    }
#endif
    btstack_sbc_decoder_process_data(&decoder_state, (packet[1] >> 4) & 3, packet+3, size-3);  
}
#endif

static void sco_demo_init_CVSD(void){
    printf("SCO Demo: Init CVSD\n");

    btstack_cvsd_plc_init(&cvsd_plc_state);

#ifdef SCO_WAV_FILENAME
    num_samples_to_write = CVSD_SAMPLE_RATE * SCO_WAV_DURATION_IN_SECONDS;
    wav_writer_open(SCO_WAV_FILENAME, 1, CVSD_SAMPLE_RATE);
#endif

    audio_initialize(CVSD_SAMPLE_RATE);
}

static void sco_demo_receive_CVSD(uint8_t * packet, uint16_t size){

    int16_t audio_frame_out[128];    // 

    if (size > sizeof(audio_frame_out)){
        printf("sco_demo_receive_CVSD: SCO packet larger than local output buffer - dropping data.\n");
        return;
    }

    const int audio_bytes_read = size - 3;
    const int num_samples = audio_bytes_read / BYTES_PER_FRAME;

    // convert into host endian
    int16_t audio_frame_in[128];
    int i;
    for (i=0;i<num_samples;i++){
        audio_frame_in[i] = little_endian_read_16(packet, 3 + i * 2);
    }

    // treat packet as bad frame if controller does not report 'all good'
    bool bad_frame = (packet[1] & 0x30) != 0;

    btstack_cvsd_plc_process_data(&cvsd_plc_state, bad_frame, audio_frame_in, num_samples, audio_frame_out);

#ifdef SCO_WAV_FILENAME
    // Samples in CVSD SCO packet are in little endian, ready for wav files (take shortcut)
    const int samples_to_write = btstack_min(num_samples, num_samples_to_write);
    wav_writer_write_le_int16(samples_to_write, audio_frame_out);
    num_samples_to_write -= samples_to_write;
    if (num_samples_to_write == 0){
        wav_writer_close();
    }
#endif

    btstack_ring_buffer_write(&audio_output_ring_buffer, (uint8_t *)audio_frame_out, audio_bytes_read);
}

#endif


void sco_demo_close(void){    
    printf("SCO demo close\n");

    printf("SCO demo statistics: ");
#ifdef ENABLE_HFP_WIDE_BAND_SPEECH
    if (negotiated_codec == HFP_CODEC_MSBC){
        printf("Used mSBC with PLC, number of processed frames: \n - %d good frames, \n - %d zero frames, \n - %d bad frames.\n", decoder_state.good_frames_nr, decoder_state.zero_frames_nr, decoder_state.bad_frames_nr);
    } else 
#endif
    {
        printf("Used CVSD with PLC, number of proccesed frames: \n - %d good frames, \n - %d bad frames.\n", cvsd_plc_state.good_frames_nr, cvsd_plc_state.bad_frames_nr);
    }

    negotiated_codec = -1;

#if (SCO_DEMO_MODE == SCO_DEMO_MODE_SINE) || (SCO_DEMO_MODE == SCO_DEMO_MODE_MICROPHONE)

#if defined(SCO_WAV_FILENAME)
    wav_writer_close();
#endif

    audio_terminate();

#endif
}

void sco_demo_set_codec(uint8_t codec){
    if (negotiated_codec == codec) return;
    negotiated_codec = codec;

#if (SCO_DEMO_MODE == SCO_DEMO_MODE_SINE) || (SCO_DEMO_MODE == SCO_DEMO_MODE_MICROPHONE)
    if (negotiated_codec == HFP_CODEC_MSBC){
#ifdef ENABLE_HFP_WIDE_BAND_SPEECH
        sco_demo_init_mSBC();
#endif
    } else {
        sco_demo_init_CVSD();
    }
#endif
}

void sco_demo_init(void){

#ifdef ENABLE_CLASSIC_LEGACY_CONNECTIONS_FOR_SCO_DEMOS
    printf("Disable BR/EDR Secure Connctions due to incompatibilities with SCO connections\n");
    gap_secure_connections_enable(false);
#endif

	// status
#if SCO_DEMO_MODE == SCO_DEMO_MODE_MICROPHONE
    printf("SCO Demo: Sending and receiving audio via btstack_audio.\n");
#endif
#if SCO_DEMO_MODE == SCO_DEMO_MODE_SINE
    if (btstack_audio_sink_get_instance()){
        printf("SCO Demo: Sending sine wave, audio output via btstack_audio.\n");
    } else {
        printf("SCO Demo: Sending sine wave, hexdump received data.\n");
    }
#endif
#if SCO_DEMO_MODE == SCO_DEMO_MODE_ASCII
	printf("SCO Demo: Sending ASCII blocks, print received data.\n");
#endif
#if SCO_DEMO_MODE == SCO_DEMO_MODE_COUNTER
	printf("SCO Demo: Sending counter value, hexdump received data.\n");
#endif

#if (SCO_DEMO_MODE == SCO_DEMO_MODE_SINE) || (SCO_DEMO_MODE == SCO_DEMO_MODE_MICROPHONE)
    hci_set_sco_voice_setting(0x60);    // linear, unsigned, 16-bit, CVSD
#else
    hci_set_sco_voice_setting(0x03);    // linear, unsigned, 8-bit, transparent
#endif
}

void sco_report(void);
void sco_report(void){
    printf("SCO: sent %u, received %u\n", count_sent, count_received);
}

void sco_demo_send(hci_con_handle_t sco_handle){

    if (sco_handle == HCI_CON_HANDLE_INVALID) return;
    
    int sco_packet_length = hci_get_sco_packet_length();
    int sco_payload_length = sco_packet_length - 3;

    hci_reserve_packet_buffer();
    uint8_t * sco_packet = hci_get_outgoing_packet_buffer();
#if SCO_DEMO_MODE == SCO_DEMO_MODE_SINE
#ifdef ENABLE_HFP_WIDE_BAND_SPEECH
    if (negotiated_codec == HFP_CODEC_MSBC){

        if (hfp_msbc_num_bytes_in_stream() < sco_payload_length){
            log_error("mSBC stream is empty.");
        }
        hfp_msbc_read_from_stream(sco_packet + 3, sco_payload_length);
#ifdef HAVE_POSIX_FILE_IO
        if (msbc_file_out){
            // log outgoing mSBC data for testing
            fwrite(sco_packet + 3, sco_payload_length, 1, msbc_file_out);
        }
#endif
        sco_demo_msbc_fill_sine_audio_frame();
    } else
#endif
    {
        const int audio_samples_per_packet = sco_payload_length / BYTES_PER_FRAME;  
        sco_demo_sine_wave_int16_at_8000_hz_little_endian(audio_samples_per_packet, &sco_packet[3]);
    }
#endif

#if SCO_DEMO_MODE == SCO_DEMO_MODE_MICROPHONE

    if (btstack_audio_source_get_instance()){

        if (negotiated_codec == HFP_CODEC_MSBC){
            // MSBC

            if (audio_input_paused){
                if (btstack_ring_buffer_bytes_available(&audio_input_ring_buffer) >= MSBC_PA_PREBUFFER_BYTES){
                    // resume sending
                    audio_input_paused = 0;
                }
            }

            if (!audio_input_paused){
                int num_samples = hfp_msbc_num_audio_samples_per_frame();
                if (num_samples > MAX_NUM_MSBC_SAMPLES) return; // assert
                if (hfp_msbc_can_encode_audio_frame_now() && btstack_ring_buffer_bytes_available(&audio_input_ring_buffer) >= (unsigned int)(num_samples * BYTES_PER_FRAME)){
                    int16_t sample_buffer[MAX_NUM_MSBC_SAMPLES];
                    uint32_t bytes_read;
                    btstack_ring_buffer_read(&audio_input_ring_buffer, (uint8_t*) sample_buffer, num_samples * BYTES_PER_FRAME, &bytes_read);
                    hfp_msbc_encode_audio_frame(sample_buffer);
                    num_audio_frames++;
                }
                if (hfp_msbc_num_bytes_in_stream() < sco_payload_length){
                    log_error("mSBC stream should not be empty.");
                }
            }

            if (audio_input_paused || hfp_msbc_num_bytes_in_stream() < sco_payload_length){
                memset(sco_packet + 3, 0, sco_payload_length);
                audio_input_paused = 1;
            } else {
                hfp_msbc_read_from_stream(sco_packet + 3, sco_payload_length);
#ifdef ENABLE_HFP_WIDE_BAND_SPEECH
#ifdef HAVE_POSIX_FILE_IO
                if (msbc_file_out){
                    // log outgoing mSBC data for testing
                    fwrite(sco_packet + 3, sco_payload_length, 1, msbc_file_out);
                }
#endif
#endif
            }

        } else {
            // CVSD

            log_debug("send: bytes avail %u, free %u", btstack_ring_buffer_bytes_available(&audio_input_ring_buffer), btstack_ring_buffer_bytes_free(&audio_input_ring_buffer));
            // fill with silence while paused
            int bytes_to_copy = sco_payload_length;
            if (audio_input_paused){
                if (btstack_ring_buffer_bytes_available(&audio_input_ring_buffer) >= CVSD_PA_PREBUFFER_BYTES){
                    // resume sending
                    audio_input_paused = 0;
                }
            }

            // get data from ringbuffer
            uint16_t pos = 0;
            uint8_t * sample_data = &sco_packet[3];
            if (!audio_input_paused){
                uint32_t bytes_read = 0;
                btstack_ring_buffer_read(&audio_input_ring_buffer, sample_data, bytes_to_copy, &bytes_read);
                // flip 16 on big endian systems
                // @note We don't use (uint16_t *) casts since all sample addresses are odd which causes crahses on some systems
                if (btstack_is_big_endian()){
                    unsigned int i;
                    for (i=0;i<bytes_read;i+=2){
                        uint8_t tmp        = sample_data[i*2];
                        sample_data[i*2]   = sample_data[i*2+1];
                        sample_data[i*2+1] = tmp;
                    }
                }
                bytes_to_copy -= bytes_read;
                pos           += bytes_read;
            }

            // fill with 0 if not enough
            if (bytes_to_copy){
                memset(sample_data + pos, 0, bytes_to_copy);
                audio_input_paused = 1;
            }
        }
    }
    else {
        // just send '0's
        memset(sco_packet + 3, 0, sco_payload_length);
    }
#endif

#if SCO_DEMO_MODE == SCO_DEMO_MODE_ASCII
    // store packet counter-xxxx
    snprintf((char *)&sco_packet[3], 5, "%04u", phase++);
    uint8_t ascii = (phase & 0x0f) + 'a';
    sco_packet[3+4] = '-';
    memset(&sco_packet[3+5], ascii, sco_payload_length-5);
#endif
#if SCO_DEMO_MODE == SCO_DEMO_MODE_COUNTER
    int j;
    for (j=0;j<sco_payload_length;j++){
        sco_packet[3+j] = phase++;
    }
#endif
#if SCO_DEMO_MODE == SCO_DEMO_MODE_55
    int j;
    for (j=0;j<sco_payload_length;j++){
        // sco_packet[3+j] = j & 1 ? 0x35 : 0x53;
        sco_packet[3+j] = 0x55;
    }
#endif
#if SCO_DEMO_MODE == SCO_DEMO_MODE_00
    int j;
    for (j=0;j<sco_payload_length;j++){
        sco_packet[3+j] = 0x00;
    }
    // additional hack
    // big_endian_store_16(sco_packet, 5, phase++);
    (void) phase;
#endif

    // test silence
    // memset(sco_packet+3, 0, sco_payload_length);

    // set handle + flags
    little_endian_store_16(sco_packet, 0, sco_handle);
    // set len
    sco_packet[2] = sco_payload_length;
    // finally send packet 
    hci_send_sco_packet_buffer(sco_packet_length);

    // request another send event
    hci_request_sco_can_send_now_event();

    count_sent++;
#if SCO_DEMO_MODE != SCO_DEMO_MODE_55
    if ((count_sent % SCO_REPORT_PERIOD) == 0) sco_report();
#endif
}

/**
 * @brief Process received data
 */
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

void sco_demo_receive(uint8_t * packet, uint16_t size){

    dump_data = 1;

    count_received++;
    static uint32_t packets = 0;
    static uint32_t crc_errors = 0;
    static uint32_t data_received = 0;
    static uint32_t byte_errors = 0;

    data_received += size - 3;
    packets++;
    if (data_received > 100000){
        printf("Summary: data %07u, packets %04u, packet with crc errors %0u, byte errors %04u\n",  (unsigned int) data_received,  (unsigned int) packets, (unsigned int) crc_errors, (unsigned int) byte_errors);
        crc_errors = 0;
        byte_errors = 0;
        data_received = 0;
        packets = 0;
    }

#if (SCO_DEMO_MODE == SCO_DEMO_MODE_SINE) || (SCO_DEMO_MODE == SCO_DEMO_MODE_MICROPHONE)
    switch (negotiated_codec){
#ifdef ENABLE_HFP_WIDE_BAND_SPEECH
        case HFP_CODEC_MSBC:
            sco_demo_receive_mSBC(packet, size);
            break;
#endif
        case HFP_CODEC_CVSD:
            sco_demo_receive_CVSD(packet, size);
            break;
        default:
            break;
    }
    dump_data = 0;
#endif

#if 0
    if (packet[1] & 0x30){
        crc_errors++;
        printf("SCO CRC Error: %x - data: ", (packet[1] & 0x30) >> 4);
        printf_hexdump(&packet[3], size-3);
        return;
    }
#endif

    if (dump_data){
#if SCO_DEMO_MODE == SCO_DEMO_MODE_ASCII
        printf("data: ");
        int i;
        for (i=3;i<size;i++){
            printf("%c", packet[i]);
        }
        printf("\n");
        dump_data = 0;
#endif
#if SCO_DEMO_MODE == SCO_DEMO_MODE_COUNTER
        // colored hexdump with expected
        static uint8_t expected_byte = 0;
        int i;
        printf("data: ");
        for (i=3;i<size;i++){
            if (packet[i] != expected_byte){
                printf(ANSI_COLOR_RED "%02x " ANSI_COLOR_RESET, packet[i]);
            } else {
                printf("%02x ", packet[i]);
            }
            expected_byte = packet[i]+1;
        }
        printf("\n");
#endif
#if SCO_DEMO_MODE == SCO_DEMO_MODE_55 || SCO_DEMO_MODE == SCO_DEMO_MODE_00
        int i;
        int contains_error = 0;
        for (i=3;i<size;i++){
            if (packet[i] != 0x00 && packet[i] != 0x35 && packet[i] != 0x53 && packet[i] != 0x55){
                contains_error = 1;
                byte_errors++;
            }
        }
        if (contains_error){
            printf("data: ");
            for (i=0;i<3;i++){
                printf("%02x ", packet[i]);
            }
            for (i=3;i<size;i++){
                if (packet[i] != 0x00 && packet[i] != 0x35 && packet[i] != 0x53 && packet[i] != 0x55){
                    printf(ANSI_COLOR_RED "%02x " ANSI_COLOR_RESET, packet[i]);
                } else {
                    printf("%02x ", packet[i]);
                }
            }
            printf("\n");
        }
#endif
    }
}
