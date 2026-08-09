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
#include <stdio.h>
#include <string.h>
#include "btstack_debug.h"
#include "btstack_audio.h"
#include "btstack_run_loop.h"

#ifdef HAVE_PORTAUDIO

#define PA_SAMPLE_TYPE               paInt16
#define NUM_FRAMES_PER_PA_BUFFER       128
#define NUM_OUTPUT_BUFFERS               4
#define NUM_INPUT_BUFFERS                5
#define DRIVER_POLL_INTERVAL_MS          1

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
static const char * source_device_name = NULL;

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
static void (*playback_callback)(int16_t * buffer, uint16_t num_samples, const btstack_audio_context_t * context);
static void (*recording_callback)(const int16_t * buffer, uint16_t num_samples, const btstack_audio_context_t * context);

// output buffer
static int16_t               output_buffer_storage[NUM_OUTPUT_BUFFERS * NUM_FRAMES_PER_PA_BUFFER * MAX_NR_AUDIO_CHANNELS];
static int16_t             * output_buffers[NUM_OUTPUT_BUFFERS];
static int16_t               output_discard_buffer[NUM_FRAMES_PER_PA_BUFFER * MAX_NR_AUDIO_CHANNELS];
static int                   output_buffer_to_play;
static int                   output_buffer_to_fill;
/* Monotonic SPSC counters avoid the modulo-index full-lap ambiguity. */
static volatile uint32_t      output_buffers_consumed;
static uint32_t               output_buffers_refilled;
static volatile uint64_t     sink_pa_callbacks;
static volatile uint64_t     sink_pa_underflows;
static uint64_t              sink_driver_fills;
static uint64_t              sink_driver_skips;
static uint32_t              sink_driver_ticks;

// input buffer
static int16_t               input_buffer_storage[NUM_INPUT_BUFFERS * NUM_FRAMES_PER_PA_BUFFER * MAX_NR_AUDIO_CHANNELS];
static int16_t             * input_buffers[NUM_INPUT_BUFFERS];
static int                   input_buffer_to_record;
static int                   input_buffer_to_fill;


// timer to fill output ring buffer
static btstack_timer_source_t  driver_timer_sink;
static btstack_timer_source_t  driver_timer_source;

// context array
static btstack_audio_context_t sink_playback_audio_contexts[NUM_OUTPUT_BUFFERS];
static btstack_audio_context_t source_recording_audio_contexts[NUM_INPUT_BUFFERS];

static int portaudio_callback_sink( const void *                     inputBuffer, 
                                    void *                           outputBuffer,
                                    unsigned long                    frames_per_buffer,
                                    const PaStreamCallbackTimeInfo * timeInfo,
                                    PaStreamCallbackFlags            statusFlags,
                                    void *                           userData ) {

    /** portaudio_callback is called from different thread, don't use hci_dump / log_info here without additional checks */

    sink_pa_callbacks++;
    if (statusFlags & paOutputUnderflow) sink_pa_underflows++;
    (void) userData;
    (void) frames_per_buffer;
    (void) inputBuffer;

    uint32_t play_seq = output_buffers_consumed;
    uint32_t play_index = play_seq % NUM_OUTPUT_BUFFERS;

    // get microsecond timestamp
    btstack_time_us_t time_us = (uint32_t) (uint64_t) (timeInfo->outputBufferDacTime * 1000000);
    sink_playback_audio_contexts[play_index].timestamp = time_us;

    // simplified volume control
    uint16_t index;
    int16_t * from_buffer = output_buffers[play_index];
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

    // Publish one consumed staging buffer to the BTstack run-loop thread.
    output_buffers_consumed = play_seq + 1;
    output_buffer_to_play = (int)((play_seq + 1) % NUM_OUTPUT_BUFFERS);

    return 0;
}

static int portaudio_callback_source( const void *                     inputBuffer, 
                                      void *                           outputBuffer,
                                      unsigned long                    samples_per_buffer,
                                      const PaStreamCallbackTimeInfo * timeInfo,
                                      PaStreamCallbackFlags            statusFlags,
                                      void *                           userData ) {

    /** portaudio_callback is called from different thread, don't use hci_dump / log_info here without additional checks */

    (void) statusFlags;
    (void) userData;
    (void) samples_per_buffer;
    (void) outputBuffer;

    // get microsecond timestamp
    btstack_time_us_t time_us = (uint32_t) (uint64_t) (timeInfo->outputBufferDacTime * 1000000);
    source_recording_audio_contexts[input_buffer_to_fill].timestamp = time_us;

    // store in one of our buffers
    memcpy(input_buffers[input_buffer_to_fill], inputBuffer, NUM_FRAMES_PER_PA_BUFFER * num_bytes_per_sample_source);

    // next
    input_buffer_to_fill = (input_buffer_to_fill + 1 ) % NUM_INPUT_BUFFERS;

    return 0;
}

static void driver_timer_handler_sink(btstack_timer_source_t * ts){
    uint32_t consumed = output_buffers_consumed;
    uint32_t lag = consumed - output_buffers_refilled;

    /* Keep latency bounded. If the run loop missed more playback periods than
     * the staging ring can represent, consume/drop the stale PCM now instead
     * of leaving it queued and letting latency grow without bound. */
    if (lag > NUM_OUTPUT_BUFFERS) {
        uint32_t stale = lag - NUM_OUTPUT_BUFFERS;
        while (stale--) {
            (*playback_callback)(output_discard_buffer, NUM_FRAMES_PER_PA_BUFFER, NULL);
            output_buffers_refilled++;
            sink_driver_skips++;
        }
    }

    while (output_buffers_refilled != consumed) {
        uint32_t fill_index = output_buffers_refilled % NUM_OUTPUT_BUFFERS;
        (*playback_callback)(output_buffers[fill_index], NUM_FRAMES_PER_PA_BUFFER,
            &sink_playback_audio_contexts[fill_index]);
        output_buffers_refilled++;
        output_buffer_to_fill = (int)(output_buffers_refilled % NUM_OUTPUT_BUFFERS);
        sink_driver_fills++;
        consumed = output_buffers_consumed;
    }

    sink_driver_ticks++;
    if ((sink_driver_ticks % 200) == 0) {
        printf("[PA] cb=%llu fills=%llu skips=%llu underflows=%llu consumed=%u refilled=%u lag=%u\n",
               (unsigned long long)sink_pa_callbacks,
               (unsigned long long)sink_driver_fills,
               (unsigned long long)sink_driver_skips,
               (unsigned long long)sink_pa_underflows,
               output_buffers_consumed, output_buffers_refilled,
               output_buffers_consumed - output_buffers_refilled);
    }

    btstack_run_loop_set_timer(ts, DRIVER_POLL_INTERVAL_MS);
    btstack_run_loop_add_timer(ts);
}

static void driver_timer_handler_source(btstack_timer_source_t * ts){

    // recording buffer ready to process
    if (input_buffer_to_record != input_buffer_to_fill){

        (*recording_callback)(input_buffers[input_buffer_to_record], NUM_FRAMES_PER_PA_BUFFER,
            &source_recording_audio_contexts[input_buffer_to_record]);

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
    void (*playback)(int16_t * buffer, uint16_t num_samples, const btstack_audio_context_t * context)
){
    PaError err;

    btstack_assert(channels <= MAX_NR_AUDIO_CHANNELS);
    btstack_assert(playback != NULL);

    num_channels_sink = channels;
    num_bytes_per_sample_sink = 2 * channels;

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

    /* -- prefer the Windows WASAPI default endpoint for low latency -- */
    if (device_index < 0){
        PaHostApiIndex wasapi_index = Pa_HostApiTypeIdToHostApiIndex(paWASAPI);
        if (wasapi_index >= 0) {
            const PaHostApiInfo *wasapi_info = Pa_GetHostApiInfo(wasapi_index);
            if (wasapi_info != NULL && wasapi_info->defaultOutputDevice != paNoDevice) {
                device_index = wasapi_info->defaultOutputDevice;
            }
        }
        if (device_index < 0) device_index = Pa_GetDefaultOutputDevice();
        output_device_info = Pa_GetDeviceInfo(device_index);
    }
    const PaHostApiInfo *selected_host = Pa_GetHostApiInfo(output_device_info->hostApi);
    printf("[PADEV] selected index=%d name=%s api=%s low=%.3fms high=%.3fms defaultRate=%.0f\n",
           (int)device_index, output_device_info->name,
           selected_host ? selected_host->name : "?",
           output_device_info->defaultLowOutputLatency * 1000.0,
           output_device_info->defaultHighOutputLatency * 1000.0,
           output_device_info->defaultSampleRate);
    int pa_device_count = Pa_GetDeviceCount();
    for (int di = 0; di < pa_device_count; di++) {
        const PaDeviceInfo *d = Pa_GetDeviceInfo(di);
        if (!d || d->maxOutputChannels <= 0) continue;
        const PaHostApiInfo *h = Pa_GetHostApiInfo(d->hostApi);
        printf("[PADEV] %d api=%s out=%d low=%.3fms high=%.3fms rate=%.0f name=%s\n",
               di, h ? h->name : "?", d->maxOutputChannels,
               d->defaultLowOutputLatency * 1000.0,
               d->defaultHighOutputLatency * 1000.0,
               d->defaultSampleRate, d->name);
    }

    /* -- setup output -- */
    PaStreamParameters output_parameters;
    output_parameters.device = device_index;
    output_parameters.channelCount = channels;
    output_parameters.sampleFormat = PA_SAMPLE_TYPE;
    output_parameters.suggestedLatency = output_device_info->defaultLowOutputLatency;
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
    printf("[PA] device low-latency hint=%.3f ms actual output latency=%.3f ms\n",
           output_device_info->defaultLowOutputLatency * 1000.0,
           stream_info->outputLatency * 1000.0);

    // verify latency
    uint32_t latency_samples = (uint32_t) (stream_info->outputLatency * samplerate);
    uint32_t buffer_samples = NUM_FRAMES_PER_PA_BUFFER * NUM_OUTPUT_BUFFERS;
    int buffers_needed = (latency_samples + NUM_FRAMES_PER_PA_BUFFER - 1) / NUM_FRAMES_PER_PA_BUFFER;
    log_info("PortAudio: output latency of %f requires %u buffers, %u buffers available\n",
    stream_info->outputLatency, buffers_needed, NUM_OUTPUT_BUFFERS);
    UNUSED(buffers_needed);
    if (latency_samples > buffer_samples) {
        log_error("PortAudio: output latency of %f requires %u buffers, but only %u buffers are available\n",
            stream_info->outputLatency, buffers_needed, NUM_OUTPUT_BUFFERS);
    }

    playback_callback  = playback;

    sink_initialized = 1;
    sink_volume = 127;

    return 0;
}

static int btstack_audio_portaudio_source_init(
    uint8_t channels,
    uint32_t samplerate, 
    void (*recording)(const int16_t * buffer, uint16_t num_samples, const btstack_audio_context_t * context)
){
    PaError err;

    btstack_assert(channels <= MAX_NR_AUDIO_CHANNELS);
    btstack_assert(recording != NULL);

    num_channels_source = channels;
    num_bytes_per_sample_source = 2 * channels;


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

    // Pre-fill the entire staging ring before PortAudio starts consuming it.
    uint8_t i;
    for (i=0;i<NUM_OUTPUT_BUFFERS;i++){
        (*playback_callback)(&output_buffer_storage[i * NUM_FRAMES_PER_PA_BUFFER * MAX_NR_AUDIO_CHANNELS], NUM_FRAMES_PER_PA_BUFFER, 0);
    }
    output_buffers_consumed = 0;
    output_buffers_refilled = 0;
    output_buffer_to_play = 0;
    output_buffer_to_fill = 0;
    sink_pa_callbacks = 0;
    sink_pa_underflows = 0;
    sink_driver_fills = 0;
    sink_driver_skips = 0;
    sink_driver_ticks = 0;

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
