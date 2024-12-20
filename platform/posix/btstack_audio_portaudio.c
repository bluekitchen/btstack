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

#define BTSTACK_FILE__ "btstack_audio_portaudio.c"


#include <stdint.h>
#include <string.h>
#include "btstack_debug.h"
#include "btstack_audio.h"
#include "btstack_run_loop.h"

#ifdef HAVE_PORTAUDIO

#define PA_SAMPLE_TYPE               paInt16
#define NUM_FRAMES_PER_PA_BUFFER       512
#define NUM_OUTPUT_BUFFERS               5
#define NUM_INPUT_BUFFERS                5
#define DRIVER_POLL_INTERVAL_MS          5

#ifndef MAX_NR_AUDIO_CHANNELS
#define MAX_NR_AUDIO_CHANNELS 2
#endif

#include <portaudio.h>

// config
static int                    num_channels_sink;
static int                    num_channels_source;
static int                    num_bytes_per_sample_sink;
static int                    num_bytes_per_sample_source;

static const char * sink_device_name;
static const char * source_device_name = "4-chan";

// portaudio
static int portaudio_initialized;

// state
static int source_initialized;
static int sink_initialized;
static int source_active;
static int sink_active;

static uint8_t sink_volume;

static PaStream * stream_source;
static PaStream * stream_sink;

// client
static void (*playback_callback)(int16_t * buffer, uint16_t num_samples);
static void (*recording_callback)(const int16_t * buffer, uint16_t num_samples);

// output buffer
static int16_t               output_buffer_storage[NUM_OUTPUT_BUFFERS * NUM_FRAMES_PER_PA_BUFFER * MAX_NR_AUDIO_CHANNELS];
static int16_t             * output_buffers[NUM_OUTPUT_BUFFERS];
static int                   output_buffer_to_play;
static int                   output_buffer_to_fill;

// input buffer
static int16_t               input_buffer_storage[NUM_INPUT_BUFFERS * NUM_FRAMES_PER_PA_BUFFER * MAX_NR_AUDIO_CHANNELS];
static int16_t             * input_buffers[NUM_INPUT_BUFFERS];
static int                   input_buffer_to_record;
static int                   input_buffer_to_fill;


// timer to fill output ring buffer
static btstack_timer_source_t  driver_timer_sink;
static btstack_timer_source_t  driver_timer_source;

static int portaudio_callback_sink( const void *                     inputBuffer, 
                                    void *                           outputBuffer,
                                    unsigned long                    frames_per_buffer,
                                    const PaStreamCallbackTimeInfo * timeInfo,
                                    PaStreamCallbackFlags            statusFlags,
                                    void *                           userData ) {

    /** portaudio_callback is called from different thread, don't use hci_dump / log_info here without additional checks */

    (void) timeInfo; /* Prevent unused variable warnings. */
    (void) statusFlags;
    (void) userData;
    (void) frames_per_buffer;
    (void) inputBuffer;

    // simplified volume control
    uint16_t index;
    int16_t * from_buffer = output_buffers[output_buffer_to_play];
    int16_t * to_buffer = (int16_t *) outputBuffer;
    btstack_assert(frames_per_buffer == NUM_FRAMES_PER_PA_BUFFER);

#if 0
    // up to 8 right shifts
    int right_shift = 8 - btstack_min(8, ((sink_volume + 15) / 16));
    for (index = 0; index < (NUM_FRAMES_PER_PA_BUFFER * num_channels_sink); index++){
        *to_buffer++ = (*from_buffer++) >> right_shift;
    }
#else
    // multiply with volume ^ 4
    int16_t x2 = sink_volume * sink_volume;
    int16_t x4 = (x2 * x2) >> 14;
    for (index = 0; index < (NUM_FRAMES_PER_PA_BUFFER * num_channels_sink); index++){
        *to_buffer++ = ((*from_buffer++) * x4) >> 14;
    }
#endif

    // next
    output_buffer_to_play = (output_buffer_to_play + 1 ) % NUM_OUTPUT_BUFFERS;

    return 0;
}

static int portaudio_callback_source( const void *                     inputBuffer, 
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
    (void) outputBuffer;

    // store in one of our buffers
    memcpy(input_buffers[input_buffer_to_fill], inputBuffer, NUM_FRAMES_PER_PA_BUFFER * num_bytes_per_sample_source);

    // next
    input_buffer_to_fill = (input_buffer_to_fill + 1 ) % NUM_INPUT_BUFFERS;

    return 0;
}

static void driver_timer_handler_sink(btstack_timer_source_t * ts){

    // playback buffer ready to fill
    while (output_buffer_to_play != output_buffer_to_fill){
        (*playback_callback)(output_buffers[output_buffer_to_fill], NUM_FRAMES_PER_PA_BUFFER);

        // next
        output_buffer_to_fill = (output_buffer_to_fill + 1 ) % NUM_OUTPUT_BUFFERS;
    }

    // re-set timer
    btstack_run_loop_set_timer(ts, DRIVER_POLL_INTERVAL_MS);
    btstack_run_loop_add_timer(ts);
}

static void driver_timer_handler_source(btstack_timer_source_t * ts){

    // recording buffer ready to process
    if (input_buffer_to_record != input_buffer_to_fill){

        (*recording_callback)(input_buffers[input_buffer_to_record], NUM_FRAMES_PER_PA_BUFFER);

        // next
        input_buffer_to_record = (input_buffer_to_record + 1 ) % NUM_INPUT_BUFFERS;
    }    

    // re-set timer
    btstack_run_loop_set_timer(ts, DRIVER_POLL_INTERVAL_MS);
    btstack_run_loop_add_timer(ts);
}

static int btstack_audio_portaudio_sink_init(
    uint8_t channels,
    uint32_t samplerate, 
    void (*playback)(int16_t * buffer, uint16_t num_samples)
){
    PaError err;

    btstack_assert(channels <= MAX_NR_AUDIO_CHANNELS);

    num_channels_sink = channels;
    num_bytes_per_sample_sink = 2 * channels;

    if (!playback){
        log_error("No playback callback");
        return 1;
    }

    for (int i=0;i<NUM_OUTPUT_BUFFERS;i++){
        output_buffers[i] = &output_buffer_storage[i * NUM_FRAMES_PER_PA_BUFFER * MAX_NR_AUDIO_CHANNELS];
    }

    /* -- initialize PortAudio -- */
    if (!portaudio_initialized){
        err = Pa_Initialize();
        if (err != paNoError){
            log_error("Portudio: error initializing portaudio: \"%s\"\n",  Pa_GetErrorText(err));
            return err;
        } 
        portaudio_initialized = 1;        
    }

    /* -- find output device by name if requested -- */
    PaDeviceIndex device_index = -1;
    const PaDeviceInfo *output_device_info;
    if (sink_device_name != NULL){
        int num_devices = Pa_GetDeviceCount();
        for (int i = 0; i < num_devices; i++) {
            output_device_info = Pa_GetDeviceInfo(i);
            // Match device by prefix
            if (strncmp(output_device_info->name, sink_device_name, strlen(sink_device_name)) == 0) {
                device_index = i;
                break;
            }
        }
    }

    /* -- use default device otherwise -- */
    if (device_index < 0){
        device_index = Pa_GetDefaultOutputDevice();
        output_device_info = Pa_GetDeviceInfo(device_index );
    }
    /* -- setup output -- */
    PaStreamParameters output_parameters;
    output_parameters.device = device_index;
    output_parameters.channelCount = channels;
    output_parameters.sampleFormat = PA_SAMPLE_TYPE;
    output_parameters.suggestedLatency = output_device_info->defaultHighOutputLatency;
    output_parameters.hostApiSpecificStreamInfo = NULL;

    log_info("PortAudio: sink device: %s", output_device_info->name);
    UNUSED(output_device_info);

    /* -- setup stream -- */
    err = Pa_OpenStream(
           &stream_sink,
           NULL,
           &output_parameters,
           samplerate,
           NUM_FRAMES_PER_PA_BUFFER,
           paClipOff,           /* we won't output out of range samples so don't bother clipping them */
           portaudio_callback_sink,  /* use callback */
           NULL );   
    
    if (err != paNoError){
        log_error("Portudio: error initializing portaudio: \"%s\"\n",  Pa_GetErrorText(err));
        return err;
    }
    log_info("PortAudio: sink stream created");

    const PaStreamInfo * stream_info = Pa_GetStreamInfo(stream_sink);
    log_info("PortAudio: sink latency: %f", stream_info->outputLatency);
    UNUSED(stream_info);

    playback_callback  = playback;

    sink_initialized = 1;
    sink_volume = 127;

    return 0;
}

static int btstack_audio_portaudio_source_init(
    uint8_t channels,
    uint32_t samplerate, 
    void (*recording)(const int16_t * buffer, uint16_t num_samples)
){
    PaError err;

    btstack_assert(channels <= MAX_NR_AUDIO_CHANNELS);

    num_channels_source = channels;
    num_bytes_per_sample_source = 2 * channels;

    if (!recording){
        log_error("No recording callback");
        return 1;
    }

    for (int i=0;i<NUM_INPUT_BUFFERS;i++){
        input_buffers[i] = &input_buffer_storage[i * NUM_FRAMES_PER_PA_BUFFER * MAX_NR_AUDIO_CHANNELS];
    }

    /* -- initialize PortAudio -- */
    if (!portaudio_initialized){
        err = Pa_Initialize();
        if (err != paNoError){
            log_error("Portudio: Error initializing portaudio: \"%s\"\n",  Pa_GetErrorText(err));
            return err;
        } 
        portaudio_initialized = 1;        
    }

    /* -- find input device by name if requested -- */
    PaDeviceIndex device_index = -1;
    const PaDeviceInfo *input_device_info;
    if (source_device_name != NULL){
        int num_devices = Pa_GetDeviceCount();
        for (int i = 0; i < num_devices; i++) {
            input_device_info = Pa_GetDeviceInfo(i);
            // Match device by prefix
            if (strncmp(input_device_info->name, source_device_name, strlen(source_device_name)) == 0) {
                device_index = i;
                break;
            }
        }
    }

    /* -- use default device otherwise -- */
    if (device_index < 0){
        device_index = Pa_GetDefaultInputDevice();
        input_device_info = Pa_GetDeviceInfo(device_index );
    }

    /* -- setup input -- */
    PaStreamParameters theInputParameters;
    theInputParameters.device = device_index;
    theInputParameters.channelCount = channels;
    theInputParameters.sampleFormat = PA_SAMPLE_TYPE;
    theInputParameters.suggestedLatency = input_device_info->defaultHighInputLatency;
    theInputParameters.hostApiSpecificStreamInfo = NULL;

    log_info("PortAudio: source device: %s", input_device_info->name);
    UNUSED(input_device_info);

    /* -- setup stream -- */
    err = Pa_OpenStream(
           &stream_source,
           &theInputParameters,
           NULL,
           samplerate,
           NUM_FRAMES_PER_PA_BUFFER,
           paClipOff,           /* we won't output out of range samples so don't bother clipping them */
           portaudio_callback_source,  /* use callback */
           NULL );   
    
    if (err != paNoError){
        log_error("Error initializing portaudio: \"%s\"\n",  Pa_GetErrorText(err));
        return err;
    }
    log_info("PortAudio: source stream created");

    const PaStreamInfo * stream_info = Pa_GetStreamInfo(stream_source);
    log_info("PortAudio: source latency: %f", stream_info->inputLatency);
    UNUSED(stream_info);

    recording_callback = recording;

    source_initialized = 1;

    return 0;
}

static uint32_t btstack_audio_portaudio_sink_get_samplerate(void) {
    const PaStreamInfo *stream_info = Pa_GetStreamInfo(stream_sink);
    return stream_info->sampleRate;
}

static uint32_t btstack_audio_portaudio_source_get_samplerate(void) {
    const PaStreamInfo *stream_info = Pa_GetStreamInfo(stream_source);
    return stream_info->sampleRate;
}

static void btstack_audio_portaudio_sink_set_volume(uint8_t volume){
    sink_volume = volume;
}

static void btstack_audio_portaudio_source_set_gain(uint8_t gain){
    UNUSED(gain);
}

static void btstack_audio_portaudio_sink_start_stream(void){

    if (!playback_callback) return;

    // fill buffers once
    uint8_t i;
    for (i=0;i<NUM_OUTPUT_BUFFERS-1;i++){
        (*playback_callback)(&output_buffer_storage[i * NUM_FRAMES_PER_PA_BUFFER * MAX_NR_AUDIO_CHANNELS], NUM_FRAMES_PER_PA_BUFFER);
    }
    output_buffer_to_play = 0;
    output_buffer_to_fill = NUM_OUTPUT_BUFFERS-1;

    /* -- start stream -- */
    PaError err = Pa_StartStream(stream_sink);
    if (err != paNoError){
        log_error("PortAudio: error starting sink stream: \"%s\"\n",  Pa_GetErrorText(err));
        return;
    }

    // start timer
    btstack_run_loop_set_timer_handler(&driver_timer_sink, &driver_timer_handler_sink);
    btstack_run_loop_set_timer(&driver_timer_sink, DRIVER_POLL_INTERVAL_MS);
    btstack_run_loop_add_timer(&driver_timer_sink);

    sink_active = 1;
}

static void btstack_audio_portaudio_source_start_stream(void){

    if (!recording_callback) return;

    /* -- start stream -- */
    PaError err = Pa_StartStream(stream_source);
    if (err != paNoError){
        log_error("PortAudio: error starting source stream: \"%s\"\n",  Pa_GetErrorText(err));
        return;
    }

    // start timer
    btstack_run_loop_set_timer_handler(&driver_timer_source, &driver_timer_handler_source);
    btstack_run_loop_set_timer(&driver_timer_source, DRIVER_POLL_INTERVAL_MS);
    btstack_run_loop_add_timer(&driver_timer_source);

    source_active = 1;
}

static void btstack_audio_portaudio_sink_stop_stream(void){

    if (!playback_callback) return;
    if (!sink_active)       return;

    // stop timer
    btstack_run_loop_remove_timer(&driver_timer_sink);

    PaError err = Pa_StopStream(stream_sink);
    if (err != paNoError){
        log_error("PortAudio: error stopping sink stream: \"%s\"",  Pa_GetErrorText(err));
        return;
    } 

    sink_active = 0;
}

static void btstack_audio_portaudio_source_stop_stream(void){

    if (!recording_callback) return;
    if (!source_active)      return;

    // stop timer
    btstack_run_loop_remove_timer(&driver_timer_source);

    PaError err = Pa_StopStream(stream_source);
    if (err != paNoError){
        log_error("PortAudio: error stopping source stream: \"%s\"",  Pa_GetErrorText(err));
        return;
    } 

    source_active = 0;
}

static void btstack_audio_portaudio_close_pa_if_not_needed(void){
    if (source_initialized) return;
    if (sink_initialized) return;
    PaError err = Pa_Terminate();
    if (err != paNoError){
        log_error("Portudio: Error terminating portaudio: \"%s\"",  Pa_GetErrorText(err));
        return;
    } 
    portaudio_initialized = 0;
}

static void btstack_audio_portaudio_sink_close(void){

    if (!playback_callback) return;

    if (sink_active){
        btstack_audio_portaudio_sink_stop_stream();
    }

    PaError err = Pa_CloseStream(stream_sink);
    if (err != paNoError){
        log_error("PortAudio: error closing sink stream: \"%s\"",  Pa_GetErrorText(err));
        return;
    } 

    sink_initialized = 0;
    btstack_audio_portaudio_close_pa_if_not_needed();
}

static void btstack_audio_portaudio_source_close(void){

    if (!recording_callback) return;

    if (source_active){
        btstack_audio_portaudio_source_stop_stream();
    }

    PaError err = Pa_CloseStream(stream_source);
    if (err != paNoError){
        log_error("PortAudio: error closing source stream: \"%s\"",  Pa_GetErrorText(err));
        return;
    } 

    source_initialized = 0;
    btstack_audio_portaudio_close_pa_if_not_needed();
}

static const btstack_audio_sink_t btstack_audio_portaudio_sink = {
    /* int (*init)(..);*/                                       &btstack_audio_portaudio_sink_init,
    /* uint32_t (*get_samplerate)() */                          &btstack_audio_portaudio_sink_get_samplerate,
    /* void (*set_volume)(uint8_t volume); */                   &btstack_audio_portaudio_sink_set_volume,
    /* void (*start_stream(void));*/                            &btstack_audio_portaudio_sink_start_stream,
    /* void (*stop_stream)(void)  */                            &btstack_audio_portaudio_sink_stop_stream,
    /* void (*close)(void); */                                  &btstack_audio_portaudio_sink_close
};

static const btstack_audio_source_t btstack_audio_portaudio_source = {
    /* int (*init)(..);*/                                       &btstack_audio_portaudio_source_init,
    /* uint32_t (*get_samplerate)() */                          &btstack_audio_portaudio_source_get_samplerate,
    /* void (*set_gain)(uint8_t gain); */                       &btstack_audio_portaudio_source_set_gain,
    /* void (*start_stream(void));*/                            &btstack_audio_portaudio_source_start_stream,
    /* void (*stop_stream)(void)  */                            &btstack_audio_portaudio_source_stop_stream,
    /* void (*close)(void); */                                  &btstack_audio_portaudio_source_close
};

const btstack_audio_sink_t * btstack_audio_portaudio_sink_get_instance(void){
    return &btstack_audio_portaudio_sink;
}

const btstack_audio_source_t * btstack_audio_portaudio_source_get_instance(void){
    return &btstack_audio_portaudio_source;
}

void btstack_audio_portaudio_sink_set_device(const char * device_name){
    sink_device_name = device_name;
}

void btstack_audio_portaudio_source_set_device(const char * device_name){
    source_device_name = device_name;
}

#endif
