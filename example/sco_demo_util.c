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

#define __BTSTACK_FILE__ "sco_demo_util.c"
 
/*
 * sco_demo_util.c - send/receive test data via SCO, used by hfp_*_demo and hsp_*_demo
 */

#include <stdio.h>

#include "sco_demo_util.h"
#include "btstack_debug.h"
#include "classic/btstack_sbc.h"
#include "classic/btstack_cvsd_plc.h"
#include "classic/hfp_msbc.h"
#include "classic/hfp.h"

#ifdef HAVE_POSIX_FILE_IO
#include "wav_util.h"
#endif

#ifdef HAVE_PORTAUDIO
#include <portaudio.h>
#include "btstack_ring_buffer.h"
#endif


// test modes
#define SCO_DEMO_MODE_SINE		 0
#define SCO_DEMO_MODE_ASCII		 1
#define SCO_DEMO_MODE_COUNTER	 2
#define SCO_DEMO_MODE_55         3
#define SCO_DEMO_MODE_00         4
#define SCO_DEMO_MODE_MICROPHONE 5

// SCO demo configuration
#define SCO_DEMO_MODE               SCO_DEMO_MODE_SINE

// number of sco packets until 'report' on console
#define SCO_REPORT_PERIOD           100

#ifdef HAVE_POSIX_FILE_IO
// length and name of wav file on disk
#define SCO_WAV_DURATION_IN_SECONDS 15
#define SCO_WAV_FILENAME            "sco_input.wav"
#endif

// name of sbc test files
#define SCO_MSBC_OUT_FILENAME       "sco_output.msbc"
#define SCO_MSBC_IN_FILENAME        "sco_input.msbc"

// pre-buffer for CVSD and mSBC - also defines latency
#define SCO_CVSD_PA_PREBUFFER_MS    50
#define SCO_MSBC_PA_PREBUFFER_MS    50

// constants
#define NUM_CHANNELS            1
#define CVSD_BYTES_PER_FRAME    (2*NUM_CHANNELS)
#define CVSD_SAMPLE_RATE        8000
#define MSBC_SAMPLE_RATE        16000
#define MSBC_BYTES_PER_FRAME    (2*NUM_CHANNELS)

#if defined(HAVE_PORTAUDIO) && (SCO_DEMO_MODE == SCO_DEMO_MODE_SINE || SCO_DEMO_MODE == SCO_DEMO_MODE_MICROPHONE)
#define USE_PORTAUDIO
#define CVSD_PA_PREBUFFER_BYTES (SCO_CVSD_PA_PREBUFFER_MS * CVSD_SAMPLE_RATE/1000 * CVSD_BYTES_PER_FRAME)
#define MSBC_PA_PREBUFFER_BYTES (SCO_MSBC_PA_PREBUFFER_MS * MSBC_SAMPLE_RATE/1000 * MSBC_BYTES_PER_FRAME)
#endif

#ifdef USE_PORTAUDIO

// bidirectional audio stream
static PaStream *            pa_stream;

// output
static int                   pa_output_started = 0;
static int                   pa_output_paused = 0;
static uint8_t               pa_output_ring_buffer_storage[2*MSBC_PA_PREBUFFER_BYTES];
static btstack_ring_buffer_t pa_output_ring_buffer;

// input
#if SCO_DEMO_MODE == SCO_DEMO_MODE_MICROPHONE
#define USE_PORTAUDIO_INPUT
static int                   pa_input_started = 0;
static int                   pa_input_paused = 0;
static uint8_t               pa_input_ring_buffer_storage[2*8000];  // full second input buffer
static btstack_ring_buffer_t pa_input_ring_buffer;
static int                   pa_input_counter;
#endif

#endif

static int dump_data = 1;
static int count_sent = 0;
static int count_received = 0;
static int negotiated_codec = -1; 

#ifdef ENABLE_HFP_WIDE_BAND_SPEECH
btstack_sbc_decoder_state_t decoder_state;
#endif

btstack_cvsd_plc_state_t cvsd_plc_state;

#ifdef ENABLE_HFP_WIDE_BAND_SPEECH
FILE * msbc_file_in;
FILE * msbc_file_out;
#endif

int num_samples_to_write;
int num_audio_frames;
int phase;

#if SCO_DEMO_MODE == SCO_DEMO_MODE_SINE

// input signal: pre-computed sine wave, 160 Hz at 16000 kHz
static const int16_t sine_int16_at_16000hz[] = {
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

// 8 kHz samples for CVSD/SCO packets in little endian
static void sco_demo_sine_wave_int16_at_8000_hz_little_endian(int num_samples, int16_t * data){
    int i;
    for (i=0; i < num_samples; i++){
        int16_t sample = sine_int16_at_16000hz[phase];
        little_endian_store_16((uint8_t *) data, i * 2, sample);
        // ony use every second sample from 16khz table to get 8khz
        phase += 2;
        if (phase >= (sizeof(sine_int16_at_16000hz) / sizeof(int16_t))){
            phase = 0;
        }
    }
}

// 16 kHz samples for mSBC encoder in host endianess
#ifdef ENABLE_HFP_WIDE_BAND_SPEECH
static void sco_demo_sine_wave_int16_at_16000_hz_host_endian(int num_samples, int16_t * data){
    int i;
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
    int16_t sample_buffer[num_samples];
    sco_demo_sine_wave_int16_at_16000_hz_host_endian(num_samples, sample_buffer);
    hfp_msbc_encode_audio_frame(sample_buffer);
    num_audio_frames++;
}
#endif
#endif

#ifdef USE_PORTAUDIO
static int portaudio_callback( const void *inputBuffer, void *outputBuffer,
                           unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData ) {
    (void) timeInfo; /* Prevent unused variable warnings. */
    (void) statusFlags;
    (void) inputBuffer;
    (void) userData;
    
// output part

    // config based on codec
    int bytes_to_copy;
    int prebuffer_bytes;
    switch (negotiated_codec){
        case HFP_CODEC_MSBC:
            bytes_to_copy   = framesPerBuffer * MSBC_BYTES_PER_FRAME;
            prebuffer_bytes = MSBC_PA_PREBUFFER_BYTES;
            break;
        case HFP_CODEC_CVSD:
            bytes_to_copy   = framesPerBuffer * CVSD_BYTES_PER_FRAME;
            prebuffer_bytes = CVSD_PA_PREBUFFER_BYTES;
            break;
        default:
            bytes_to_copy   = framesPerBuffer * 2;  // assume 1 channel / 16 bit audio samples
            prebuffer_bytes = 0xfffffff;
            break;            
    }

    // fill with silence while paused
    if (pa_output_paused){
        if (btstack_ring_buffer_bytes_available(&pa_output_ring_buffer) < prebuffer_bytes){
            memset(outputBuffer, 0, bytes_to_copy);
            return 0;
        } else {
            // resume playback
            pa_output_paused = 0;
        }
    }

    // get data from ringbuffer
    uint32_t bytes_read = 0;
    btstack_ring_buffer_read(&pa_output_ring_buffer, outputBuffer, bytes_to_copy, &bytes_read);
    bytes_to_copy -= bytes_read;

    // fill with 0 if not enough
    if (bytes_to_copy){
        memset(outputBuffer + bytes_read, 0, bytes_to_copy);
        pa_output_paused = 1;
    }
// end of output part

// input part -- just store in ring buffer
#ifdef USE_PORTAUDIO_INPUT
    btstack_ring_buffer_write(&pa_input_ring_buffer, (uint8_t *)inputBuffer, framesPerBuffer * 2);
    pa_input_counter += framesPerBuffer * 2;
#endif

    return 0;
}

// return 1 if ok
static int portaudio_initialize(int sample_rate){
    PaError err;

    /* -- initialize PortAudio -- */
    printf("PortAudio: Initialize\n");
    err = Pa_Initialize();
    if( err != paNoError ) return 0;

    /* -- setup input and output -- */
    const PaDeviceInfo *deviceInfo;
    PaStreamParameters * inputParameters = NULL;
    PaStreamParameters outputParameters;
    outputParameters.device = Pa_GetDefaultOutputDevice(); /* default output device */
    outputParameters.channelCount = NUM_CHANNELS;
    outputParameters.sampleFormat = paInt16;
    outputParameters.suggestedLatency = Pa_GetDeviceInfo( outputParameters.device )->defaultHighOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;
    deviceInfo = Pa_GetDeviceInfo( outputParameters.device );
    log_info("PortAudio: Output device: %s", deviceInfo->name);
#ifdef USE_PORTAUDIO_INPUT
    PaStreamParameters theInputParameters;
    theInputParameters.device = Pa_GetDefaultInputDevice(); /* default input device */
    theInputParameters.channelCount = NUM_CHANNELS;
    theInputParameters.sampleFormat = paInt16;
    theInputParameters.suggestedLatency = Pa_GetDeviceInfo( theInputParameters.device )->defaultHighOutputLatency;
    theInputParameters.hostApiSpecificStreamInfo = NULL;
    inputParameters = &theInputParameters;
    deviceInfo = Pa_GetDeviceInfo( inputParameters->device );
    log_info("PortAudio: Input device: %s", deviceInfo->name);
#endif

    /* -- setup output stream -- */
    printf("PortAudio: Open stream\n");
    err = Pa_OpenStream(
           &pa_stream,
           inputParameters,
           &outputParameters,
           sample_rate,
           0,
           paClipOff, /* we won't output out of range samples so don't bother clipping them */
           portaudio_callback,  
           NULL );  
    if (err != paNoError){
        printf("Error opening portaudio stream: \"%s\"\n",  Pa_GetErrorText(err));
        return 0;
    }
    memset(pa_output_ring_buffer_storage, 0, sizeof(pa_output_ring_buffer_storage));
    btstack_ring_buffer_init(&pa_output_ring_buffer, pa_output_ring_buffer_storage, sizeof(pa_output_ring_buffer_storage));
#ifdef USE_PORTAUDIO_INPUT
    memset(pa_input_ring_buffer_storage, 0, sizeof(pa_input_ring_buffer_storage));
    btstack_ring_buffer_init(&pa_input_ring_buffer, pa_input_ring_buffer_storage, sizeof(pa_input_ring_buffer_storage));
    printf("PortAudio: Input buffer size %u\n", btstack_ring_buffer_bytes_free(&pa_input_ring_buffer));
#endif

    /* -- start stream -- */
    err = Pa_StartStream(pa_stream);
    if (err != paNoError){
        printf("Error starting the stream: \"%s\"\n",  Pa_GetErrorText(err));
        return 0;
    }
    pa_output_started = 1; 
    pa_output_paused  = 1;
#ifdef USE_PORTAUDIO_INPUT
    pa_input_started = 1; 
    pa_input_paused  = 1;
#endif

    return 1;
}

static void portaudio_terminate(void){
    if (!pa_stream) return;

    PaError err;
    printf("PortAudio: Stop Stream\n");
    err = Pa_StopStream(pa_stream);
    if (err != paNoError){
        printf("Error stopping the stream: \"%s\"\n",  Pa_GetErrorText(err));
        return;
    } 
    printf("PortAudio: Close Stream\n");
    err = Pa_CloseStream(pa_stream);
    if (err != paNoError){
        printf("Error closing the stream: \"%s\"\n",  Pa_GetErrorText(err));
        return;
    } 
    pa_stream = NULL;
    printf("PortAudio: Terminate\n");
    err = Pa_Terminate();
    if (err != paNoError){
        printf("Error terminating portaudio: \"%s\"\n",  Pa_GetErrorText(err));
        return;
    }
}
#endif

#if (SCO_DEMO_MODE == SCO_DEMO_MODE_SINE) || (SCO_DEMO_MODE == SCO_DEMO_MODE_MICROPHONE)

#ifdef ENABLE_HFP_WIDE_BAND_SPEECH
static void handle_pcm_data(int16_t * data, int num_samples, int num_channels, int sample_rate, void * context){
    UNUSED(context);
    UNUSED(sample_rate);
    UNUSED(data);
    UNUSED(num_samples);
    UNUSED(num_channels);

#if (SCO_DEMO_MODE == SCO_DEMO_MODE_SINE) || (SCO_DEMO_MODE == SCO_DEMO_MODE_MICROPHONE)

    // printf("handle_pcm_data num samples %u, sample rate %d\n", num_samples, num_channels);
#ifdef HAVE_PORTAUDIO
    // samples in callback in host endianess, ready for PortAudio playback
    btstack_ring_buffer_write(&pa_output_ring_buffer, (uint8_t *)data, num_samples*num_channels*2);
#endif /* HAVE_PORTAUDIO */

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

#ifdef USE_PORTAUDIO
    portaudio_initialize(MSBC_SAMPLE_RATE);
#endif  
}

static void sco_demo_receive_mSBC(uint8_t * packet, uint16_t size){
    if (num_samples_to_write){
        if (msbc_file_in){
            // log incoming mSBC data for testing
            fwrite(packet+3, size-3, 1, msbc_file_in);
        }
    }
    btstack_sbc_decoder_process_data(&decoder_state, (packet[1] >> 4) & 3, packet+3, size-3);  
}
#endif

static void sco_demo_init_CVSD(void){
    printf("SCO Demo: Init CVSD\n");

#if defined(SCO_WAV_FILENAME) || defined(USE_PORTAUDIO)
    btstack_cvsd_plc_init(&cvsd_plc_state);
#endif

#ifdef SCO_WAV_FILENAME
    num_samples_to_write = CVSD_SAMPLE_RATE * SCO_WAV_DURATION_IN_SECONDS;
    wav_writer_open(SCO_WAV_FILENAME, 1, CVSD_SAMPLE_RATE);
#endif

#ifdef USE_PORTAUDIO
    portaudio_initialize(CVSD_SAMPLE_RATE);
#endif  
}

static void sco_demo_receive_CVSD(uint8_t * packet, uint16_t size){
    if (!num_samples_to_write) return;

    int16_t audio_frame_out[128];    // 

    if (size > sizeof(audio_frame_out)){
        printf("sco_demo_receive_CVSD: SCO packet larger than local output buffer - dropping data.\n");
        return;
    }

#if defined(SCO_WAV_FILENAME) || defined(USE_PORTAUDIO)
    const int audio_bytes_read = size - 3;
    const int num_samples = audio_bytes_read / CVSD_BYTES_PER_FRAME;

    // convert into host endian
    int16_t audio_frame_in[128];
    int i;
    for (i=0;i<num_samples;i++){
        audio_frame_in[i] = little_endian_read_16(packet, 3 + i * 2);
    }

    btstack_cvsd_plc_process_data(&cvsd_plc_state, audio_frame_in, num_samples, audio_frame_out);
#endif

#ifdef SCO_WAV_FILENAME
    // Samples in CVSD SCO packet are in little endian, ready for wav files (take shortcut)
    const int samples_to_write = btstack_min(num_samples, num_samples_to_write);
    wav_writer_write_le_int16(samples_to_write, audio_frame_out);
    num_samples_to_write -= samples_to_write;
    if (num_samples_to_write == 0){
        wav_writer_close();
    }
#endif

#ifdef USE_PORTAUDIO
    btstack_ring_buffer_write(&pa_output_ring_buffer, (uint8_t *)audio_frame_out, audio_bytes_read);
#endif
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

#ifdef HAVE_PORTAUDIO
    portaudio_terminate();
#endif

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
	// status
#if SCO_DEMO_MODE == SCO_DEMO_MODE_MICROPHONE
    printf("SCO Demo: Sending and receiving audio via portaudio.\n");
#endif
#if SCO_DEMO_MODE == SCO_DEMO_MODE_SINE
#ifdef HAVE_PORTAUDIO
	printf("SCO Demo: Sending sine wave, audio output via portaudio.\n");
#else
	printf("SCO Demo: Sending sine wave, hexdump received data.\n");
#endif
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

#if SCO_DEMO_MODE == SCO_DEMO_MODE_ASCII
    phase = 'a';
#endif
}

void sco_report(void);
void sco_report(void){
    printf("SCO: sent %u, received %u\n", count_sent, count_received);
}

void sco_demo_send(hci_con_handle_t sco_handle){

    if (!sco_handle) return;
    
    int sco_packet_length = hci_get_sco_packet_length();
    int sco_payload_length = sco_packet_length - 3;

    hci_reserve_packet_buffer();
    uint8_t * sco_packet = hci_get_outgoing_packet_buffer();
#if SCO_DEMO_MODE == SCO_DEMO_MODE_SINE
#ifdef ENABLE_HFP_WIDE_BAND_SPEECH
    if (negotiated_codec == HFP_CODEC_MSBC){
        // overwrite
        sco_payload_length = 24;
        sco_packet_length = sco_payload_length + 3;

        if (hfp_msbc_num_bytes_in_stream() < sco_payload_length){
            log_error("mSBC stream is empty.");
        }
        hfp_msbc_read_from_stream(sco_packet + 3, sco_payload_length);
        if (msbc_file_out){
            // log outgoing mSBC data for testing
            fwrite(sco_packet + 3, sco_payload_length, 1, msbc_file_out);
        }

        sco_demo_msbc_fill_sine_audio_frame();
    } else
#endif
    {
        const int audio_samples_per_packet = sco_payload_length / CVSD_BYTES_PER_FRAME;  
        sco_demo_sine_wave_int16_at_8000_hz_little_endian(audio_samples_per_packet, (int16_t *) (sco_packet+3));
    }
#endif

#if SCO_DEMO_MODE == SCO_DEMO_MODE_MICROPHONE

#ifdef HAVE_PORTAUDIO
    if (negotiated_codec == HFP_CODEC_MSBC){
        // MSBC

        // overwrite
        sco_payload_length = 24;
        sco_packet_length = sco_payload_length + 3;

        if (pa_input_paused){
            if (btstack_ring_buffer_bytes_available(&pa_input_ring_buffer) >= MSBC_PA_PREBUFFER_BYTES){
                // resume sending
                pa_input_paused = 0;
            }
        }

        if (!pa_input_paused){
            int num_samples = hfp_msbc_num_audio_samples_per_frame();
            if (hfp_msbc_can_encode_audio_frame_now() && btstack_ring_buffer_bytes_available(&pa_input_ring_buffer) >= (num_samples * MSBC_BYTES_PER_FRAME)){
                int16_t sample_buffer[num_samples];
                uint32_t bytes_read;
                btstack_ring_buffer_read(&pa_input_ring_buffer, (uint8_t*) sample_buffer, num_samples * MSBC_BYTES_PER_FRAME, &bytes_read);
                hfp_msbc_encode_audio_frame(sample_buffer);
                num_audio_frames++;
            }
        }

        if (hfp_msbc_num_bytes_in_stream() < sco_payload_length){
            log_error("mSBC stream should not be empty.");
            memset(sco_packet + 3, 0, sco_payload_length);
            pa_input_paused = 1;
        } else {
            hfp_msbc_read_from_stream(sco_packet + 3, sco_payload_length);
            if (msbc_file_out){
                // log outgoing mSBC data for testing
                fwrite(sco_packet + 3, sco_payload_length, 1, msbc_file_out);
            }
        }

    } else {
        // CVSD

        log_info("send: bytes avail %u, free %u, counter %u", btstack_ring_buffer_bytes_available(&pa_input_ring_buffer), btstack_ring_buffer_bytes_free(&pa_input_ring_buffer), pa_input_counter);
        // fill with silence while paused
        int bytes_to_copy = sco_payload_length;
        if (pa_input_paused){
            if (btstack_ring_buffer_bytes_available(&pa_input_ring_buffer) >= CVSD_PA_PREBUFFER_BYTES){
                // resume sending
                pa_input_paused = 0;
            }
        }

        // get data from ringbuffer
        uint16_t pos = 0;
        uint8_t * sample_data = &sco_packet[3];
        if (!pa_input_paused){
            uint32_t bytes_read = 0;
            btstack_ring_buffer_read(&pa_input_ring_buffer, sample_data, bytes_to_copy, &bytes_read);
            // flip 16 on big endian systems
            // @note We don't use (uint16_t *) casts since all sample addresses are odd which causes crahses on some systems
            if (btstack_is_big_endian()){
                int i;
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
            pa_input_paused = 1;
        }
    }
#else
    // just send '0's
    if (negotiated_codec == HFP_CODEC_MSBC){
        sco_payload_length = 24;
        sco_packet_length = sco_payload_length + 3;
    }
    memset(sco_packet + 3, 0, sco_payload_length);
#endif
#endif

#if SCO_DEMO_MODE == SCO_DEMO_MODE_ASCII
    memset(&sco_packet[3], phase++, sco_payload_length);
    if (phase > 'z') phase = 'a';
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

    if (packet[1] & 0x30){
        crc_errors++;
        // printf("SCO CRC Error: %x - data: ", (packet[1] & 0x30) >> 4);
        // printf_hexdump(&packet[3], size-3);
        return;
    }
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
