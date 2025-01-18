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

#define BTSTACK_FILE__ "hal_audio_f4_discovery.c"

#include <stdio.h>
#include <time.h>

#include "hal_audio.h"
#include "btstack_debug.h"
#include "stm32f4_discovery_audio.h"

// output
#define OUTPUT_BUFFER_NUM_SAMPLES       512
#define NUM_OUTPUT_BUFFERS              2
#define NUM_OUTPUT_CHANNELS             2

// #define MEASURE_SAMPLE_RATE

static void (*audio_played_handler)(uint8_t buffer_index);
static int playback_started;
static uint32_t sink_sample_rate;
static uint8_t  sink_volume;

// our storage
static int16_t output_buffer[NUM_OUTPUT_BUFFERS * OUTPUT_BUFFER_NUM_SAMPLES * NUM_OUTPUT_CHANNELS];   // stereo

// record the time playback started for each buffer
static volatile uint32_t sink_playback_time_us[NUM_OUTPUT_BUFFERS];
static volatile uint8_t  sink_playback_last_buffer;
static uint32_t			 sink_playback_buffer_duration_us;

#ifdef MEASURE_SAMPLE_RATE
static uint32_t stream_start_ms;
static uint32_t stream_samples;
#endif

// input - irq every 16 ms currently
#define INPUT_BUFFER_NUM_SAMPLES       256

static int recording_started;
static int32_t recording_sample_rate;
static uint8_t  source_gain;

static void (*audio_recorded_callback)(const int16_t * buffer, uint16_t num_samples);

static int16_t input_buffer[INPUT_BUFFER_NUM_SAMPLES];   // single mono buffer
static uint16_t  pdm_buffer[INPUT_BUFFER_NUM_SAMPLES*8];

static uint32_t source_sample_rate;
static int source_pcm_samples_per_ms;
static int source_pdm_bytes_per_ms;
static int source_pcm_samples_per_irq;
static int source_pdm_samples_total;

extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;

#include "main.h"
#include <inttypes.h>


void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim) {
	uint32_t time_capture_us = __HAL_TIM_GET_COMPARE(&htim2, TIM_CHANNEL_1);
	// update buffer time
	if (sink_playback_time_us[sink_playback_last_buffer] != 0) {
		sink_playback_buffer_duration_us = time_capture_us - sink_playback_time_us[sink_playback_last_buffer];
	}
	sink_playback_time_us[sink_playback_last_buffer] = time_capture_us;
	sink_playback_last_buffer = 1 - sink_playback_last_buffer;
}

void  BSP_AUDIO_OUT_HalfTransfer_CallBack(void){

#ifdef MEASURE_SAMPLE_RATE
	if (stream_start_ms == 0){
		stream_start_ms = btstack_run_loop_get_time_ms();
	} else {
		stream_samples++;
	}
#endif

	HAL_GPIO_TogglePin(Test1_GPIO_Port, Test1_Pin);
	// uint32_t time_read_us = __HAL_TIM_GET_COUNTER(&htim2);
	// uint32_t time_capture = __HAL_TIM_GET_COMPARE(&htim2, TIM_CHANNEL_1);
	// int32_t delta = (int32_t)(time_capture - time_read_us);
	// printf("0: %" PRIu32 " - %" PRIu32 " => delta %" PRIi32 "\n", time_read_us, time_capture, delta);

	if (sink_playback_buffer_duration_us != 0) {
		(*audio_played_handler)(0);
	}
}

void BSP_AUDIO_OUT_TransferComplete_CallBack(void){

#ifdef MEASURE_SAMPLE_RATE
	if (stream_samples == 500){
		uint32_t now = btstack_run_loop_get_time_ms();
		uint32_t delta = now - stream_start_ms;
		log_info("Samples per second: %u", stream_samples * OUTPUT_BUFFER_NUM_SAMPLES * 1000 / delta);
		stream_start_ms = now;
		stream_samples = 0;
	}
	stream_samples++;
#endif

	HAL_GPIO_TogglePin(Test1_GPIO_Port, Test1_Pin);
	// uint32_t time_read_us = __HAL_TIM_GET_COUNTER(&htim2);
	// uint32_t time_capture = __HAL_TIM_GET_COMPARE(&htim2, TIM_CHANNEL_1);
	// int32_t delta = (int32_t)(time_capture - time_read_us);
	// printf("1: %" PRIu32 " - %" PRIu32 " => delta %" PRIi32 "\n", time_read_us, time_capture, delta);

	if (sink_playback_buffer_duration_us != 0) {
		(*audio_played_handler)(1);
	}
}

/**
 * @brief Setup audio codec for specified samplerate and number channels
 * @param Channels
 * @param Sample rate
 * @param Buffer played callback
 * @param Buffer recorded callback (use NULL if no recording)
 */
void hal_audio_sink_init(uint8_t channels, 
                    uint32_t sample_rate,
                    void (*buffer_played_callback)  (uint8_t buffer_index)){

	// F4 Discovery Audio BSP only supports stereo playback
	if (channels == 1){
        log_error("F4 Discovery Audio BSP only supports stereo playback. Please #define HAVE_HAL_AUDIO_SINK_STEREO_ONLY");
		return;
	}

	sink_volume = 100;

	audio_played_handler = buffer_played_callback;
	sink_sample_rate = sample_rate;
}

/**
 * @brief Get number of output buffers in HAL
 * @returns num buffers
 */
uint16_t hal_audio_sink_get_num_output_buffers(void){
	return NUM_OUTPUT_BUFFERS;
}

/**
 * @brief Get size of single output buffer in HAL
 * @returns buffer size
 */
uint16_t hal_audio_sink_get_num_output_buffer_samples(void){
	return OUTPUT_BUFFER_NUM_SAMPLES;
}

/**
 * @brief Reserve output buffer
 * @returns buffer
 */
int16_t * hal_audio_sink_get_output_buffer(uint8_t buffer_index){
	switch (buffer_index){
		case 0:
			return output_buffer;
		case 1:
			return &output_buffer[OUTPUT_BUFFER_NUM_SAMPLES * 2];
		default:
			return NULL;
	}
}

/**
 * @brief Start stream
 */
void hal_audio_sink_start(void){
	playback_started = 1;

	// configure I2S Bit Clock counter to overrun on next tick
	uint16_t num_ticks_per_block = OUTPUT_BUFFER_NUM_SAMPLES * 16;
	__HAL_TIM_SET_COUNTER(&htim3, num_ticks_per_block-1);
	__HAL_TIM_SET_AUTORELOAD(&htim3, num_ticks_per_block-1);
	sink_playback_buffer_duration_us = 0;

	BSP_AUDIO_OUT_Init(OUTPUT_DEVICE_BOTH, 80, sink_sample_rate);

	BSP_AUDIO_OUT_SetVolume(sink_volume);

	// BSP_AUDIO_OUT_Play gets number bytes -> 1 frame - 16 bit/stereo = 4 bytes
	BSP_AUDIO_OUT_Play( (uint16_t*) output_buffer, NUM_OUTPUT_BUFFERS * OUTPUT_BUFFER_NUM_SAMPLES * 4);
}

/**
 * @brief Stop stream
 */
void hal_audio_sink_stop(void){
	playback_started = 0;
	BSP_AUDIO_OUT_Stop(CODEC_PDWN_HW);
}

void hal_audio_sink_set_volume(uint8_t volume){
	// 0..127 -> 0..100
	uint8_t codec_volume = volume * 100 / 127;

	// BSP_AUDIO_OUT_SetVolume can only be called after BSP_AUDIO_OUT_Init in playback started
	if (playback_started) {
		BSP_AUDIO_OUT_SetVolume(codec_volume);
	} else {
		sink_volume = codec_volume;
	}
}

/**
 * @brief Close audio codec
 */
void hal_audio_sink_close(void){
	if (playback_started){
		hal_audio_sink_stop();
	}
}

#ifdef SIMULATE_SINE

// temp sine simulator
// input signal: pre-computed sine wave, 266 Hz at 16000 kHz
static const int16_t sine_int16_at_16000hz[] = {
     0,   3135,   6237,   9270,  12202,  14999,  17633,  20073,  22294,  24270,
 25980,  27406,  28531,  29344,  29835,  30000,  29835,  29344,  28531,  27406,
 25980,  24270,  22294,  20073,  17633,  14999,  12202,   9270,   6237,   3135,
     0,  -3135,  -6237,  -9270, -12202, -14999, -17633, -20073, -22294, -24270,
-25980, -27406, -28531, -29344, -29835, -30000, -29835, -29344, -28531, -27406,
-25980, -24270, -22294, -20073, -17633, -14999, -12202,  -9270,  -6237,  -3135,
};
static unsigned int phase;

// 8 kHz samples in host endianess
static void sco_demo_sine_wave_int16_at_8000_hz_host_endian(unsigned int num_samples, int16_t * data){
    unsigned int i;
    for (i=0; i < num_samples; i++){
        data[i] = sine_int16_at_16000hz[phase++];
        // ony use every second sample from 16khz table to get 8khz
        phase += 2;
        if (phase >= (sizeof(sine_int16_at_16000hz) / sizeof(int16_t))){
            phase = 0;
        }
    }
}


// 16 kHz samples in host endianess
static void sco_demo_sine_wave_int16_at_16000_hz_host_endian(unsigned int num_samples, int16_t * data){
    unsigned int i;
    for (i=0; i < num_samples; i++){
        data[i] = sine_int16_at_16000hz[phase++];
        if (phase >= (sizeof(sine_int16_at_16000hz) / sizeof(int16_t))){
            phase = 0;
        }
    }
}

static void generate_sine(void){
     if (recording_sample_rate == 8000){
     	sco_demo_sine_wave_int16_at_8000_hz_host_endian(INPUT_BUFFER_NUM_SAMPLES, input_buffer);
     } else {
     	sco_demo_sine_wave_int16_at_16000_hz_host_endian(INPUT_BUFFER_NUM_SAMPLES, input_buffer);
     }
     // notify
     (*audio_recorded_callback)(input_buffer, INPUT_BUFFER_NUM_SAMPLES);
}
#else

static void process_pdm(uint16_t * pdm_half_buffer){
	
	int samples_needed = source_pcm_samples_per_irq;
	int16_t * pcm_buffer = input_buffer;

	while (samples_needed){
		// TODO: use int16_t for pcm samples
		BSP_AUDIO_IN_PDMToPCM(pdm_half_buffer, (uint16_t *) pcm_buffer);
		pdm_half_buffer += source_pdm_bytes_per_ms / 2;
		pcm_buffer      += source_pcm_samples_per_ms;
		samples_needed  -= source_pcm_samples_per_ms; 
	}

     // notify
     (*audio_recorded_callback)(input_buffer, source_pcm_samples_per_irq);
}

#endif

void BSP_AUDIO_IN_HalfTransfer_CallBack(void){
#ifdef SIMULATE_SINE
	generate_sine();
#else
	process_pdm(&pdm_buffer[0]);
#endif
}

void BSP_AUDIO_IN_TransferComplete_CallBack(void){
#ifdef SIMULATE_SINE
	generate_sine();
#else
	process_pdm(&pdm_buffer[source_pdm_samples_total/2]);
#endif
}

/**
 * @brief Setup audio codec for recording using specified samplerate and number of channels
 * @param Channels
 * @param Sample rate
 * @param Buffer recorded callback
 */
void hal_audio_source_init(uint8_t channels, 
                           uint32_t sample_rate,
                           void (*buffer_recorded_callback)(const int16_t * buffer, uint16_t num_samples)){

	source_sample_rate = sample_rate;

	// Driver only supports mono recording
	if (channels != 1){
		log_error("F4 Discovery only has single microphone, stereo recording not supported");
		return;
	}

	int decimation = 64;

	source_gain = 100;

	// size of input & output of PDM filter depend on output frequency and decimation
	source_pcm_samples_per_irq = sample_rate / 1000 * 16;	// 256@16 kHz, 128@8 kHz

	source_pcm_samples_per_ms = sample_rate / 1000;
	source_pdm_bytes_per_ms   = source_pcm_samples_per_ms * decimation / 8;

	source_pdm_samples_total  = INPUT_BUFFER_NUM_SAMPLES * 8 * sample_rate / 16000;

	log_info("Source: PDM bytes per ms %u, PDM samples total %u - PCM samples per ms %u", source_pdm_bytes_per_ms, source_pdm_samples_total, source_pcm_samples_per_ms);

    audio_recorded_callback = buffer_recorded_callback;
    recording_sample_rate = sample_rate;
}

/**
 * @brief Start stream
 */
void hal_audio_source_start(void){
    BSP_AUDIO_IN_Init(source_sample_rate, 16, 1);
	BSP_AUDIO_IN_SetVolume(source_gain);
    BSP_AUDIO_IN_Record(pdm_buffer, source_pdm_samples_total);
    recording_started = 1;
}

/**
 * @brief Stop stream
 */
void hal_audio_source_stop(void){
	if (!recording_started) return;
	BSP_AUDIO_IN_Stop();
    recording_started = 0;
}

void hal_audio_source_set_gain(uint8_t gain){
	// 0..127 -> 0..100
	uint8_t codec_gain = gain * 100 / 127;
	// BSP_AUDIO_IN_SetVolume can only be called after BSP_AUDIO_IN_Init in recording started
	if (recording_started) {
		BSP_AUDIO_IN_SetVolume(codec_gain);
	} else {
		source_gain = codec_gain;
	}
}

/**
 * @brief Close audio codec
 */
void hal_audio_source_close(void){
	if (recording_started) {
		hal_audio_source_stop();
	}
}
