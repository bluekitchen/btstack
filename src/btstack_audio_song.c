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

#define BTSTACK_FILE__ "btstack_audio_song.c"

#include "btstack_audio_song.h"
#include "btstack_util.h"

#include <stdint.h>

static void btstack_audio_song_init(btstack_audio_song_t * song, btstack_audio_song_type_t song_type, const char * title, const char * artist, const char * album){
    song->song_type = song_type;
    song->title = title;
    song->artist = artist;
    song->album = album;
}
void btstack_audio_song_silence_init(btstack_audio_song_silence_t * song) {
    btstack_audio_song_init(&song->base, BTSTACK_AUDIO_SONG_SILENCE, "Silence", NULL, NULL);
}

void btstack_audio_song_sine_init(btstack_audio_song_sine_t * song, uint16_t frequency_hz) {
    song->frequency_hz = frequency_hz;
    btstack_snprintf_assert_complete(song->title_buffer, sizeof(song->title_buffer), "Sine %u Hz", frequency_hz);
    btstack_audio_song_init(&song->base, BTSTACK_AUDIO_SONG_SINE, song->title_buffer, NULL, NULL);
}

#ifdef ENABLE_MODPLAYER
void btstack_audio_song_mod_init(btstack_audio_song_mod_t * song, const mod_title_t * mod_title) {
    song->mod_title = mod_title;
    btstack_audio_song_init(&song->base, BTSTACK_AUDIO_SONG_MOD, mod_title->name, NULL, NULL);
}
#endif

#ifdef ENABLE_VORBIS
void btstack_audio_song_vorbis_init(btstack_audio_song_vorbis_t * song, const char * path){
    song->path = path;
    btstack_audio_song_init(&song->base, BTSTACK_AUDIO_SONG_VORBIS, NULL, NULL, NULL);
}
#endif