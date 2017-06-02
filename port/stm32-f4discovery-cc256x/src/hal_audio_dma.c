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

#include "hal_audio_dma.h"
#include "stm32f4_discovery_audio.h"

static void (*audio_played_handler)(void);
static int started;

void BSP_AUDIO_OUT_TransferComplete_CallBack(void){
	if (audio_played_handler){
		audio_played_handler();
	}
}

void hal_audio_dma_init(uint32_t sample_rate){
	BSP_AUDIO_OUT_Init(OUTPUT_DEVICE_BOTH, 100, sample_rate);
}

void hal_audio_dma_set_audio_played(void (*handler)(void)){
	audio_played_handler = handler;
}

void hal_audio_dma_play(const uint8_t * audio_data, uint16_t audio_len){
	if (!started){
		started = 1;
		BSP_AUDIO_OUT_Play(audio_data, audio_len);
	} else {
		BSP_AUDIO_OUT_ChangeBuffer(audio_data, audio_len >> 1);
	}
}

void hal_audio_dma_close(void){
	started = 0;
	BSP_AUDIO_OUT_Stop(CODEC_PDWN_HW);
}

