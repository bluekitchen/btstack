/*
 * Copyright (C) 2017 BlueKitchen GmbH
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

#define __BTSTACK_FILE__ "btstack_audio_portaudio.c"


#include <stdint.h>
#include <string.h>
#include "btstack_debug.h"
#include "btstack_audio.h"
#include "btstack_run_loop.h"

#ifdef HAVE_PORTAUDIO

#define PA_SAMPLE_TYPE               paInt16
#define NUM_FRAMES_PER_PA_BUFFER       512
#define NUM_OUTPUT_BUFFERS               3
#define NUM_INPUT_BUFFERS                2
#define DRIVER_POLL_INTERVAL_MS          5

#include <portaudio.h>

// config
static int                    num_channels;
static int                    num_bytes_per_sample;

// portaudio
static PaStream * stream;

// client
static void (*playback_callback)(int16_t * buffer, uint16_t num_samples);
static void (*recording_callback)(const int16_t * buffer, uint16_t num_samples);

// output buffer
static int16_t               output_buffer_a[NUM_FRAMES_PER_PA_BUFFER * 2];   // stereo
static int16_t               output_buffer_b[NUM_FRAMES_PER_PA_BUFFER * 2];   // stereo
static int16_t               output_buffer_c[NUM_FRAMES_PER_PA_BUFFER * 2];   // stereo
static int16_t             * output_buffers[NUM_OUTPUT_BUFFERS] = { output_buffer_a, output_buffer_b, output_buffer_c};
static int                   output_buffer_to_play;
static int                   output_buffer_to_fill;

// input buffer
static int16_t               input_buffer_a[NUM_FRAMES_PER_PA_BUFFER * 2];   // stereo
static int16_t               input_buffer_b[NUM_FRAMES_PER_PA_BUFFER * 2];   // stereo
static int16_t             * input_buffers[NUM_INPUT_BUFFERS] = { input_buffer_a, input_buffer_b};
static int                   input_buffer_to_record;
static int                   input_buffer_to_fill;


// timer to fill output ring buffer
static btstack_timer_source_t  driver_timer;

static int portaudio_callback( const void *                     inputBuffer, 
                               void *                           outputBuffer,
                               unsigned long                    samples_per_buffer,
                               const PaStreamCallbackTimeInfo * timeInfo,
                               PaStreamCallbackFlags            statusFlags,
                               void *                           userData ) {

    /** portaudio_callback is called from different thread, don't use hci_dump / log_info here without additional checks */

    (void) timeInfo; /* Prevent unused variable warnings. */
    (void) statusFlags;
    (void) userData;
    (void) samples_per_buffer;

    // -- playback / output
    if (playback_callback){

        // fill from one of our buffers
        memcpy(outputBuffer, output_buffers[output_buffer_to_play], NUM_FRAMES_PER_PA_BUFFER * num_bytes_per_sample);

        // next
        output_buffer_to_play = (output_buffer_to_play + 1 ) % NUM_OUTPUT_BUFFERS;
    }

    // -- recording / input
    if (recording_callback){

        // store in one of our buffers
        memcpy(input_buffers[input_buffer_to_fill], inputBuffer, NUM_FRAMES_PER_PA_BUFFER * num_bytes_per_sample);

        // next
        input_buffer_to_fill = (input_buffer_to_fill + 1 ) % NUM_INPUT_BUFFERS;
    }

    return 0;
}

static void driver_timer_handler(btstack_timer_source_t * ts){

    // playback buffer ready to fill
    if (playback_callback && output_buffer_to_play != output_buffer_to_fill){
        (*playback_callback)(output_buffers[output_buffer_to_fill], NUM_FRAMES_PER_PA_BUFFER);

        // next
        output_buffer_to_fill = (output_buffer_to_fill + 1 ) % NUM_OUTPUT_BUFFERS;
    }

    // recording buffer ready to process
    if (recording_callback && input_buffer_to_record != input_buffer_to_fill){

        (*recording_callback)(input_buffers[input_buffer_to_record], NUM_FRAMES_PER_PA_BUFFER);

        // next
        input_buffer_to_record = (input_buffer_to_record + 1 ) % NUM_INPUT_BUFFERS;
    }    

    // re-set timer
    btstack_run_loop_set_timer(ts, DRIVER_POLL_INTERVAL_MS);
    btstack_run_loop_add_timer(ts);
}

static int btstack_audio_portaudio_init(
    uint8_t channels,
    uint32_t samplerate, 
    void (*playback)(int16_t * buffer, uint16_t num_samples),
    void (*recording)(const int16_t * buffer, uint16_t num_samples)
){

    num_channels = channels;
    num_bytes_per_sample = 2 * channels;

    /* -- initialize PortAudio -- */
    PaError err = Pa_Initialize();
    if (err != paNoError){
        log_error("Error initializing portaudio: \"%s\"\n",  Pa_GetErrorText(err));
        return err;
    } 

    PaStreamParameters theInputParameters;
    PaStreamParameters theOutputParameters;

    PaStreamParameters * inputParameters = NULL;
    PaStreamParameters * outputParameters = NULL;

    /* -- setup output -- */
    if (playback){
        theOutputParameters.device = Pa_GetDefaultOutputDevice(); /* default output device */
        theOutputParameters.channelCount = channels;
        theOutputParameters.sampleFormat = PA_SAMPLE_TYPE;
        theOutputParameters.suggestedLatency = Pa_GetDeviceInfo( theOutputParameters.device )->defaultHighOutputLatency;
        theOutputParameters.hostApiSpecificStreamInfo = NULL;

        const PaDeviceInfo *outputDeviceInfo;
        outputDeviceInfo = Pa_GetDeviceInfo( theOutputParameters.device );
        log_info("PortAudio: Output device: %s", outputDeviceInfo->name);

        outputParameters = &theOutputParameters;
    }

    /* -- setup input -- */
    if (recording){
        theInputParameters.device = Pa_GetDefaultInputDevice(); /* default input device */
        theInputParameters.channelCount = channels;
        theInputParameters.sampleFormat = PA_SAMPLE_TYPE;
        theInputParameters.suggestedLatency = Pa_GetDeviceInfo( theInputParameters.device )->defaultHighInputLatency;
        theInputParameters.hostApiSpecificStreamInfo = NULL;

        const PaDeviceInfo *inputDeviceInfo;
        inputDeviceInfo = Pa_GetDeviceInfo( theInputParameters.device );
        log_info("PortAudio: Input device: %s", inputDeviceInfo->name);

        inputParameters = &theInputParameters;
    }

    /* -- setup stream -- */
    err = Pa_OpenStream(
           &stream,
           inputParameters,
           outputParameters,
           samplerate,
           NUM_FRAMES_PER_PA_BUFFER,
           paClipOff,           /* we won't output out of range samples so don't bother clipping them */
           portaudio_callback,  /* use callback */
           NULL );   
    
    if (err != paNoError){
        log_error("Error initializing portaudio: \"%s\"\n",  Pa_GetErrorText(err));
        return err;
    }
    log_info("PortAudio: stream opened");

    const PaStreamInfo * stream_info = Pa_GetStreamInfo(stream);
    log_info("PortAudio: Input  latency: %f", stream_info->inputLatency);
    log_info("PortAudio: Output latency: %f", stream_info->outputLatency);

    playback_callback  = playback;
    recording_callback = recording;

    return 0;
}

static void btstack_audio_portaudio_start_stream(void){

    // fill buffer once
    (*playback_callback)(output_buffer_a, NUM_FRAMES_PER_PA_BUFFER);
    (*playback_callback)(output_buffer_b, NUM_FRAMES_PER_PA_BUFFER);
    output_buffer_to_play = 0;
    output_buffer_to_fill = 2;

    /* -- start stream -- */
    PaError err = Pa_StartStream(stream);
    if (err != paNoError){
        log_error("Error starting the stream: \"%s\"\n",  Pa_GetErrorText(err));
        return;
    }

    // start timer
    btstack_run_loop_set_timer_handler(&driver_timer, &driver_timer_handler);
    btstack_run_loop_set_timer(&driver_timer, DRIVER_POLL_INTERVAL_MS);
    btstack_run_loop_add_timer(&driver_timer);
}

static void btstack_audio_portaudio_close(void){

    // stop timer
    btstack_run_loop_remove_timer(&driver_timer);

    log_info("PortAudio: Stream closed");
    PaError err = Pa_StopStream(stream);
    if (err != paNoError){
        log_error("Error stopping the stream: \"%s\"",  Pa_GetErrorText(err));
        return;
    } 
    err = Pa_CloseStream(stream);
    if (err != paNoError){
        log_error("Error closing the stream: \"%s\"",  Pa_GetErrorText(err));
        return;
    } 
    err = Pa_Terminate();
    if (err != paNoError){
        log_error("Error terminating portaudio: \"%s\"",  Pa_GetErrorText(err));
        return;
    } 
}

static const btstack_audio_t btstack_audio_portaudio = {
    /* int (*init)(..);*/                                       &btstack_audio_portaudio_init,
    /* void (*start_stream(void));*/                            &btstack_audio_portaudio_start_stream,
    /* void (*close)(void); */                                  &btstack_audio_portaudio_close
};

const btstack_audio_t * btstack_audio_portaudio_get_instance(void){
    return &btstack_audio_portaudio;
}

#endif
