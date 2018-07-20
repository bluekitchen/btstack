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

#include "hal_audio.h"
#include "stm32f4_discovery_audio.h"

#define OUTPUT_BUFFER_NUM_SAMPLES       512
#define NUM_OUTPUT_BUFFERS              2

static void (*audio_played_handler)(uint8_t buffer_index);
static int started;

// our storage
static int16_t output_buffer[NUM_OUTPUT_BUFFERS * OUTPUT_BUFFER_NUM_SAMPLES * 2];   // stereo

void  BSP_AUDIO_OUT_HalfTransfer_CallBack(void){
	(*audio_played_handler)(0);
}

void BSP_AUDIO_OUT_TransferComplete_CallBack(void){
	(*audio_played_handler)(1);
}

/**
 * @brief Setup audio codec for specified samplerate and number channels
 * @param Channels
 * @param Sample rate
 * @param Buffer played callback
 * @param Buffer recorded callback (use NULL if no recording)
 */
void hal_audio_init(uint8_t channels, 
                    uint32_t sample_rate,
                    void (*buffer_played_callback)  (uint8_t buffer_index),
                    void (*buffer_recorded_callback)(const int16_t * buffer, uint16_t num_samples)){

	audio_played_handler = buffer_played_callback;
	UNUSED(buffer_recorded_callback);
	BSP_AUDIO_OUT_Init(OUTPUT_DEVICE_BOTH, 80, sample_rate);
}

/**
 * @brief Get number of output buffers in HAL
 * @returns num buffers
 */
uint16_t hal_audio_get_num_output_buffers(void){
	return NUM_OUTPUT_BUFFERS;
}

/**
 * @brief Get size of single output buffer in HAL
 * @returns buffer size
 */
uint16_t hal_audio_get_num_output_buffer_samples(void){
	return OUTPUT_BUFFER_NUM_SAMPLES;
}

/**
 * @brief Reserve output buffer
 * @returns buffer
 */
int16_t * hal_audio_get_output_buffer(uint8_t buffer_index){
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
void hal_audio_start(void){
	started = 1;
	// BSP_AUDIO_OUT_Play gets number bytes -> 1 frame - 16 bit/stereo = 4 bytes
	BSP_AUDIO_OUT_Play( (uint16_t*) output_buffer, NUM_OUTPUT_BUFFERS * OUTPUT_BUFFER_NUM_SAMPLES * 4);
}

/**
 * @brief Close audio codec
 */
void hal_audio_close(void){
	started = 0;
	BSP_AUDIO_OUT_Stop(CODEC_PDWN_HW);
}
