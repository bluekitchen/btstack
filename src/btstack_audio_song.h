/*
 * Copyright (C) 2025 BlueKitchen GmbH
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

/**
 *  @brief TODO
 */

#ifndef BTSTACK_AUDIO_SONG_H
#define BTSTACK_AUDIO_SONG_H

#include "btstack_config.h"
#include "btstack_linked_list.h"

#ifdef ENABLE_MODPLAYER
#include "mods/mod.h"
#endif

#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif

/**
 * Supported Song types
 */
typedef enum {
    BTSTACK_AUDIO_SONG_SILENCE = 0,
    BTSTACK_AUDIO_SONG_SINE,
#ifdef ENABLE_MODPLAYER
    BTSTACK_AUDIO_SONG_MOD,
#endif
#ifdef ENABLE_VORBIS
    BTSTACK_AUDIO_SONG_VORBIS
#endif
} btstack_audio_song_type_t;

/**
 * Generic Song that can be stored in linked list
 */
typedef struct {
    btstack_linked_item_t * next;
    btstack_audio_song_type_t song_type;
    const char * title;
    const char * artist;
    const char * album;
} btstack_audio_song_t;

/**
 * Single Silence tone
 * Title: "Silence"
 */
typedef struct {
        btstack_audio_song_t base;
} btstack_audio_song_silence_t;

/**
 * Single Sine tone
 * Title: "Sine xxxxx hz"
 */
typedef struct {
    btstack_audio_song_t base;
    uint16_t frequency_hz;
    char title_buffer[16];
} btstack_audio_song_sine_t;

/**
 * Single Mod song
 */
#ifdef ENABLE_MODPLAYER
typedef struct {
    btstack_audio_song_t base;
    const mod_title_t * mod_title;
} btstack_audio_song_mod_t;
#endif

/**
 * Single Vorbis song
 */
#ifdef ENABLE_VORBIS
typedef struct {
    btstack_audio_song_t base;
    const char * path;
} btstack_audio_song_vorbis_t;
#endif

/**
 * @brief Init Silence tone
 * @param song
 * @param frequency_hz
 */
void btstack_audio_song_silence_init(btstack_audio_song_silence_t * song);

/**
 * @brief Init Sine tone
 * @param song
 * @param frequency_hz
 */
void btstack_audio_song_sine_init(btstack_audio_song_sine_t * song, uint16_t frequency_hz);

/**
 * @brief Init Mod song
 * @param song
 * @param mod_title
 */
#ifdef ENABLE_MODPLAYER
void btstack_audio_song_mod_init(btstack_audio_song_mod_t * song, const mod_title_t * mod_title);
#endif

/**
 * @brief Init OGG Vorbis song.
 *        Meta information is load from file during playback if available
 * @param song
 * @param path
 */
#ifdef ENABLE_VORBIS
void btstack_audio_song_vorbis_init(btstack_audio_song_vorbis_t * song, const char * path);
#endif

#if defined __cplusplus
}
#endif
#endif // BTSTACK_AUDIO_SONG_H
