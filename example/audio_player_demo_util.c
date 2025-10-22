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

#define BTSTACK_FILE__ "audio_player_demo_util.c"

#include "audio_player_demo_util.h"

#include "btstack_bool.h"
#include "btstack_config.h"
#include "btstack_debug.h"

#include "btstack_audio_song.h"

#include <stdint.h>

#include "btstack_util.h"

#ifndef AUDIO_PLAYER_MAX_MOD_SONGS
#define AUDIO_PLAYER_MAX_MOD_SONGS 4
#endif
#ifndef AUDIO_PLAYER_MAX_SINE_SONGS
#define AUDIO_PLAYER_MAX_SINE_SONGS 5
#endif

// songs
static btstack_audio_song_silence_t silence_song;
static btstack_audio_song_sine_t sine_songs[AUDIO_PLAYER_MAX_SINE_SONGS];
static btstack_audio_song_mod_t mod_songs[AUDIO_PLAYER_MAX_MOD_SONGS];

// sine songs
static uint16_t sine_song_frequencies_hz[] = {
    261, // 261.63, // C-4
    311, // 311.13, // Es-4
    370, // 369.99, // G-4
    493, // 493.88, // B-4
    583, // 587.33, // D-5
};

static btstack_linked_list_t audio_player_demo_util_playlist;

btstack_linked_list_t audio_player_demo_util_get_playlist_instance(void) {
    if (audio_player_demo_util_playlist == NULL) {
        // generate playlist
        // - silence song
        btstack_audio_song_silence_init(&silence_song);
        btstack_linked_list_add(&audio_player_demo_util_playlist, (btstack_linked_item_t *) &silence_song);
        // - sine tones
        int usable_sines = btstack_min(sizeof(sine_song_frequencies_hz)/sizeof(sine_song_frequencies_hz[0]), AUDIO_PLAYER_MAX_SINE_SONGS);
        for (int i=0; i < usable_sines; i++){
            btstack_audio_song_sine_init(&sine_songs[i], sine_song_frequencies_hz[i]);
            btstack_linked_list_add(&audio_player_demo_util_playlist, (btstack_linked_item_t *) &sine_songs[i]);
        }
        // - mods
        int usable_mods = btstack_min(mod_titles_count, AUDIO_PLAYER_MAX_MOD_SONGS);
        for (int i=0; i < usable_mods; i++) {
            btstack_audio_song_mod_init(&mod_songs[i], &mod_titles[i]);
            btstack_linked_list_add(&audio_player_demo_util_playlist, (btstack_linked_item_t *) &mod_songs[i]);
        }
    }
    return audio_player_demo_util_playlist;
}
