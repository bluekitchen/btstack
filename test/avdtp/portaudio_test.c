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

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <portaudio.h>

#include "btstack_ring_buffer.h"
#include "wav_util.h"

#define NUM_CHANNELS        2
#define NUM_SECONDS         1
#define PA_SAMPLE_TYPE      paInt16
#define SAMPLE_RATE         44100
#define FRAMES_PER_BUFFER   400
#define BYTES_PER_FRAME     (2*NUM_CHANNELS)

#ifndef M_PI
#define M_PI  3.14159265
#endif

#define TABLE_SIZE   100

typedef struct {
    int16_t sine[TABLE_SIZE];
    int left_phase;
    int right_phase;
} paTestData;

static uint8_t ring_buffer_storage[3*FRAMES_PER_BUFFER*BYTES_PER_FRAME];
static btstack_ring_buffer_t ring_buffer;

static int total_num_samples = 0;
static const char * wav_filename = "portaudio_sine.wav";

static void write_wav_data(int16_t * data, int num_frames, int num_channels, int sample_rate){
    (void)sample_rate;
    wav_writer_write_int16(num_frames*num_channels, data);
    total_num_samples+=num_frames*num_channels;
    if (total_num_samples>5*SAMPLE_RATE) wav_writer_close();
}

static void fill_ring_buffer(void *userData){
    paTestData *data = (paTestData*)userData;

    while (btstack_ring_buffer_bytes_free(&ring_buffer) > BYTES_PER_FRAME){
        uint8_t write_data[BYTES_PER_FRAME];
        *(int16_t*)&write_data[0] = data->sine[data->left_phase];
        *(int16_t*)&write_data[2] = data->sine[data->right_phase];
        
        btstack_ring_buffer_write(&ring_buffer, write_data, BYTES_PER_FRAME);
        write_wav_data((int16_t*)write_data, 1, NUM_CHANNELS, SAMPLE_RATE);

        data->left_phase += 1;
        if (data->left_phase >= TABLE_SIZE){
            data->left_phase -= TABLE_SIZE;
        }
        data->right_phase += 2; /* higher pitch so we can distinguish left and right. */
        if (data->right_phase >= TABLE_SIZE){
            data->right_phase -= TABLE_SIZE;
        } 
    }
}

static int patestCallback( const void *inputBuffer, void *outputBuffer,
                           unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData ) {
    (void) timeInfo; /* Prevent unused variable warnings. */
    (void) statusFlags;
    (void) inputBuffer;
    (void) userData;

    uint32_t bytes_read = 0;
    int bytes_per_buffer = framesPerBuffer * BYTES_PER_FRAME;

    if (btstack_ring_buffer_bytes_available(&ring_buffer) >= bytes_per_buffer){
        btstack_ring_buffer_read(&ring_buffer, (uint8_t *) outputBuffer, bytes_per_buffer, &bytes_read);
    } else {
        printf("NOT ENOUGH DATA!\n");
        memset(outputBuffer, 0, bytes_per_buffer);
    }
    return paContinue;
}


int main(int argc, const char * argv[]){
    (void) argc;
    (void) argv;
    
    PaError err;
    static paTestData data;
    static PaStream * stream;

    /* initialise sinusoidal wavetable */
    int i;
    for (i=0; i<TABLE_SIZE; i++){
        data.sine[i] = sin(((double)i/(double)TABLE_SIZE) * M_PI * 2.)*32767;
    }
    data.left_phase = data.right_phase = 0;

    err = Pa_Initialize();
    if (err != paNoError){
        printf("Error initializing portaudio: \"%s\"\n",  Pa_GetErrorText(err));
        return paNoError;
    } 

    PaStreamParameters outputParameters;
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
           &data );             /* callback userData */

    memset(ring_buffer_storage, 0, sizeof(ring_buffer_storage));
    btstack_ring_buffer_init(&ring_buffer, ring_buffer_storage, sizeof(ring_buffer_storage));

    wav_writer_open(wav_filename, NUM_CHANNELS, SAMPLE_RATE);

    if (err != paNoError){
        printf("Error opening default stream: \"%s\"\n",  Pa_GetErrorText(err));
        return paNoError;
    } 

    err = Pa_StartStream(stream);
    if (err != paNoError){
        printf("Error starting the stream: \"%s\"\n",  Pa_GetErrorText(err));
        return paNoError;
    } 

    /* Sleep for several seconds. */
    while (1){
        fill_ring_buffer(&data);
        Pa_Sleep(1);
    }
    
    err = Pa_StopStream(stream);
    if (err != paNoError){
        printf("Error stopping the stream: \"%s\"\n",  Pa_GetErrorText(err));
        return paNoError;
    } 

    err = Pa_CloseStream(stream);
    if (err != paNoError){
        printf("Error closing the stream: \"%s\"\n",  Pa_GetErrorText(err));
        return paNoError;
    } 

    err = Pa_Terminate();
    if (err != paNoError){
        printf("Error terminating portaudio: \"%s\"\n",  Pa_GetErrorText(err));
        return paNoError;
    } 
    return 0;
}
