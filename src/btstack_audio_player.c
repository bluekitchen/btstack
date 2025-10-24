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

#define BTSTACK_FILE__ "btstack_audio_player.c"

#include "btstack_audio_player.h"

#include "btstack_bool.h"
#include "btstack_config.h"
#include "btstack_debug.h"

#include <stdint.h>
#include <stdlib.h>

static void btstack_audio_player_generator_finalize(btstack_audio_generator_t * base){
    UNUSED(base);
    btstack_unreachable();
}

static void btstack_audio_player_deinit_generator(btstack_audio_player_t * audio_player) {
    if (audio_player->active_generator_initialized) {
        audio_player->active_generator_initialized = false;
        btstack_audio_generator_finalize(&audio_player->active_generator);
    }
}

static void btstack_audio_player_generator_generate(btstack_audio_generator_t * base, int16_t * pcm_buffer, uint16_t num_samples) {
    btstack_audio_player_generator_t * player_generator = (btstack_audio_player_generator_t *) base;
    btstack_audio_player_t * audio_player = player_generator->audio_player;
    if (audio_player->playing && (audio_player->active_generator_initialized)) {
        btstack_audio_generator_generate(&audio_player->active_generator, pcm_buffer, num_samples);
    } else {
        btstack_audio_generator_generate(&audio_player->silence_generator, pcm_buffer, num_samples);
    }
}

void btstack_audio_player_init(btstack_audio_player_t * audio_player, uint16_t sample_rate, uint8_t channels, btstack_linked_list_t playlist) {
    btstack_assert(playlist != NULL);
    audio_player->playlist = playlist;
    audio_player->sample_rate = sample_rate;
    audio_player->channels = channels;
    audio_player->playing = false;
    audio_player->active_generator_initialized = false;
    audio_player->current_song = (btstack_audio_song_t *)playlist;
    audio_player->generator.audio_player = audio_player;
    audio_player->generator.base.channels = channels;
    audio_player->generator.base.samplerate_hz = sample_rate;
    audio_player->generator.base.generate = btstack_audio_player_generator_generate;
    audio_player->generator.base.finalize = btstack_audio_player_generator_finalize;
    btstack_audio_generator_silence_init(&audio_player->silence_generator, sample_rate, channels);
}

btstack_audio_generator_t * btstack_audio_player_get_generator(btstack_audio_player_t * audio_player) {
    return &audio_player->generator.base;
}

void btstack_audio_player_play(btstack_audio_player_t * audio_player) {
    if (audio_player->active_generator_initialized == false) {
        btstack_audio_song_t * song = audio_player->current_song;
        btstack_audio_song_sine_t   * sine_song;
#ifdef ENABLE_MODPLAYER
        btstack_audio_song_mod_t    * mod_song;
#endif
#ifdef ENABLE_VORBIS
        btstack_audio_song_vorbis_t * vorbis_song;
#endif
        switch (song->song_type) {
            case BTSTACK_AUDIO_SONG_SILENCE:
                // just ignore
                break;
            case BTSTACK_AUDIO_SONG_SINE:
                sine_song = (btstack_audio_song_sine_t *)song;
                btstack_audio_generator_sine_init(&audio_player->sine_generator, audio_player->sample_rate,
                    audio_player->channels, sine_song->frequency_hz);
                audio_player->active_generator_initialized = true;
                break;
#ifdef ENABLE_MODPLAYER
            case BTSTACK_AUDIO_SONG_MOD:
                mod_song = (btstack_audio_song_mod_t *)song;
                btstack_audio_generator_modplayer_init(&audio_player->mod_generator,
                    audio_player->sample_rate, audio_player->channels, mod_song->mod_title->data, mod_song->mod_title->len);
                audio_player->active_generator_initialized = true;
                break;
#endif
#ifdef ENABLE_VORBIS
            case BTSTACK_AUDIO_SONG_VORBIS:
                vorbis_song = (btstack_audio_song_vorbis_t *)song;
                btstack_audio_generator_vorbis_init(&audio_player->vorbis_generator, audio_player->sample_rate,
                    audio_player->channels, vorbis_song->path);
                audio_player->active_generator_initialized = true;
                break;
#endif
            default:
                btstack_unreachable();
                break;
        }
    }
    audio_player->playing = true;
}

void btstack_audio_player_stop(btstack_audio_player_t * audio_player) {
    audio_player->playing = false;
};

static void btstack_audio_player_song_changed(btstack_audio_player_t * audio_player) {
    if (audio_player->playing) {
        btstack_audio_player_play(audio_player);
    }
}

void btstack_audio_player_next_song(btstack_audio_player_t * audio_player) {
    btstack_audio_player_deinit_generator(audio_player);
    if (audio_player->current_song->next == NULL) {
        audio_player->current_song = (btstack_audio_song_t *) btstack_linked_list_get_first_item(&audio_player->playlist);
    } else {
        audio_player->current_song = (btstack_audio_song_t *) audio_player->current_song->next;
    }
    btstack_audio_player_song_changed(audio_player);
}

void btstack_audio_player_previous_song(btstack_audio_player_t * audio_player) {
    btstack_audio_player_deinit_generator(audio_player);
    btstack_linked_item_t * previous = btstack_linked_list_get_previous_item(&audio_player->playlist,
        (btstack_linked_item_t *) audio_player->current_song);
    if (previous == NULL) {
        previous =  btstack_linked_list_get_last_item(&audio_player->playlist);
    }
    audio_player->current_song = (btstack_audio_song_t *) previous;
    btstack_audio_player_song_changed(audio_player);
}

void btstack_audio_player_select_song(btstack_audio_player_t * audio_player, btstack_audio_song_t * song) {
    btstack_audio_player_deinit_generator(audio_player);
    btstack_linked_list_iterator_t iterator;
    btstack_linked_list_iterator_init(&iterator, &audio_player->playlist);
    while (btstack_linked_list_iterator_has_next(&iterator)) {
        btstack_audio_song_t * existing_song = (btstack_audio_song_t *) btstack_linked_list_iterator_next(&iterator);
        if (existing_song == song) {
            audio_player->current_song = existing_song;
            btstack_audio_player_song_changed(audio_player);
            return;
        }
    }
}

const btstack_audio_song_t * btstack_audio_player_get_current_song(btstack_audio_player_t * audio_player) {
    return audio_player->current_song;
}

uint16_t btstack_audio_player_get_number_songs(btstack_audio_player_t * audio_player) {
    return btstack_linked_list_count(&audio_player->playlist);
}

void btstack_audio_player_deinit(btstack_audio_player_t * audio_player) {
    btstack_audio_generator_finalize(&audio_player->silence_generator);
    btstack_audio_player_deinit_generator(audio_player);
}
