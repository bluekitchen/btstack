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

#ifndef BTSTACK_AUDIO_PLAYER_H
#define BTSTACK_AUDIO_PLAYER_H

#include "btstack_config.h"
#include "btstack_audio_song.h"
#include "btstack_audio_generator.h"

#if defined __cplusplus
extern "C" {
#endif

typedef struct btstack_audio_player btstack_audio_player_t;

typedef struct {
    btstack_audio_generator_t base;
    btstack_audio_player_t * audio_player;
} btstack_audio_player_generator_t;

struct btstack_audio_player{
    btstack_linked_list_t playlist;
    uint16_t sample_rate;
    uint8_t channels;
    btstack_audio_song_t * current_song;
    btstack_audio_player_generator_t generator;
    bool playing;
    bool active_generator_initialized;
    btstack_audio_generator_silence_t silence_generator;
    union {
        btstack_audio_generator_t        active_generator;
        btstack_audio_generator_sine_t   sine_generator;
#ifdef ENABLE_MODPLAYER
        btstack_audio_generator_mod_t    mod_generator;
#endif
#ifdef ENABLE_VORBIS
        btstack_audio_generator_vorbis_t vorbis_generator;
#endif
    };
};

/**
 * @brief Init audio player for given sample rate and number of channels with a list of songs
 * @param audio_player
 * @param sample_rate
 * @param channels
 * @param playlist must have at least one song
 */
void btstack_audio_player_init(btstack_audio_player_t * audio_player, uint16_t sample_rate, uint8_t channels, btstack_linked_list_t playlist);

/**
 * @brief Get audio generator for this player
 * @param audio_player
 * @return
 */
btstack_audio_generator_t * btstack_audio_player_get_generator(btstack_audio_player_t * audio_player);

/**
 * @brief Start playing if not already playing
 * @param audio_player
 */
void btstack_audio_player_play(btstack_audio_player_t * audio_player);

/**
 * @brief Pause playback, generator returns silence
 * @param audio_player
 */
void btstack_audio_player_stop(btstack_audio_player_t * audio_player);

/**
 * @brief Go to next song incl. wrap around
 * @param audio_player
 */
void btstack_audio_player_next_song(btstack_audio_player_t * audio_player);

/**
 * @breif Go to previous song incl. warp around
 * @param audio_player
 */
void btstack_audio_player_previous_song(btstack_audio_player_t * audio_player);

/**
 * @brief Select individual song
 * @note Ignores request for song not in list
 * @param audio_player
 * @param song
 */
void btstack_audio_player_select_song(btstack_audio_player_t * audio_player, btstack_audio_song_t * song);

/**
 * @brief Get current song, useful to access meta information
 * @param audio_player
 * @return
 */
const btstack_audio_song_t * btstack_audio_player_get_current_song(btstack_audio_player_t * audio_player);

/**
 * @brief Get number of songs in playlist
 * @param audio_player
 * @return
 */
uint16_t btstack_audio_player_get_number_songs(btstack_audio_player_t * audio_player);


/**
 * @breif Free resources used by audio player
 * @param audio_player
 */
void btstack_audio_player_deinit(btstack_audio_player_t * audio_player);

#if defined __cplusplus
}
#endif
#endif // BTSTACK_AUDIO_PLAYER_H
