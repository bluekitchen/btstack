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

#define BTSTACK_FILE__ "btstack_audio.c"

#include "btstack_audio.h"

/*
 *  btstack_audio.c
 *
 */

static const btstack_audio_sink_t * btstack_audio_sink_instance;

static const btstack_audio_source_t * btstack_audio_source_instance;

/**
 * @brief Get BTstack Audio Sink Instance
 * @return btstack_audio_sink implementation
 */
const btstack_audio_sink_t * btstack_audio_sink_get_instance(void){
	return btstack_audio_sink_instance;
}

/**
 * @brief Get BTstack Audio Source Instance
 * @return btstack_audio_source implementation
 */
const btstack_audio_source_t * btstack_audio_source_get_instance(void){
	return btstack_audio_source_instance;
}

/**
 * @brief Get BTstack Audio Sink Instance
 * @param btstack_audio_sink implementation
 */
void btstack_audio_sink_set_instance(const btstack_audio_sink_t * audio_sink_impl){
	btstack_audio_sink_instance = audio_sink_impl;
}

/**
 * @brief Get BTstack Audio Source Instance
 * @param btstack_audio_source implementation
 */
void btstack_audio_source_set_instance(const btstack_audio_source_t * audio_source_impl){
	btstack_audio_source_instance = audio_source_impl;
}

