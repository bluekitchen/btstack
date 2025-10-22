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

#define BTSTACK_FILE__ "audio_player.c"

// *****************************************************************************
/* EXAMPLE_START(audio_player): Console app for basic audio player
 *
 * @text Various audio demos require audio input. This example shows how to use
 * our basic audio player for this task and focus on the actual demo.
 */
// *****************************************************************************

#include "btstack.h"
#include "audio_player_demo_util.h"
#include <stdio.h>

#define SAMPLE_RATE 48000
#define NUM_CHANNELS 2

// globals
static btstack_audio_player_t audio_player;
static btstack_audio_generator_t * audio_generator;

/* @section Audio Playback
 *
 * @text Call audio generator provided by audio player to fill playback buffer
 */

static void audio_playback(int16_t * pcm_buffer, uint16_t num_samples_to_write, const btstack_audio_context_t * context){
    UNUSED(context);
    btstack_audio_generator_generate(audio_generator, pcm_buffer, num_samples_to_write);
}

static void show_usage(void) {
    const btstack_audio_song_t * song = btstack_audio_player_get_current_song(&audio_player);
    printf("\n--- Audio Player Demo Console ---\n");
    printf("Current song: '%s'\n", song->title);
#ifdef HAVE_BTSTACK_STDIN
    printf("Commands:\n");
    printf("[ - Go to previous song\n");
    printf("] - Go to next song\n");
    printf("s - Stop playing\n");
    printf("p - Play\n");
#endif
    printf("---\n");
}

#ifdef HAVE_BTSTACK_STDIN
static void show_command_and_song(const char * command) {
    const btstack_audio_song_t * song = btstack_audio_player_get_current_song(&audio_player);
    printf("Operation: %s -> now: '%s'\n", command, song->title);
}

static void stdin_process(char cmd) {
    switch (cmd) {
        case '[':
            btstack_audio_player_previous_song(&audio_player);
            show_command_and_song("Next song");
            break;
        case ']':
            btstack_audio_player_next_song(&audio_player);
            show_command_and_song("Previous song");
            break;
        case 'p':
            btstack_audio_player_play(&audio_player);
            break;
        case 's':
            btstack_audio_player_stop(&audio_player);
            break;
        default:
            show_usage();
            break;
    }
}
#endif

/* @section Main Application Setup
 *
 * @text Listing MainConfiguration shows main application code.
 * It first generates a playlist to be used with audio player and then starts audio playback
 */
/* LISTING_START(MainConfiguration): Setup packet handler  */

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    (void)argc;
    (void)argv;

    // generate player
    btstack_linked_list_t playlist = audio_player_demo_util_get_playlist_instance();
    btstack_audio_player_init(&audio_player, SAMPLE_RATE, NUM_CHANNELS, playlist);
    audio_generator = btstack_audio_player_get_generator(&audio_player);
    btstack_audio_player_play(&audio_player);

    // check audio interface and start playback
    const btstack_audio_sink_t * audio_sink = btstack_audio_sink_get_instance();
    if (!audio_sink){
        printf("BTstack Audio Sink not setup\n");
        return 10;
    }
    audio_sink->init(NUM_CHANNELS, SAMPLE_RATE, &audio_playback);
    audio_sink->start_stream();

    // setup console
#ifdef HAVE_BTSTACK_STDIN
    btstack_stdin_setup(stdin_process);
#endif
    show_usage();

    return 0;
}

/* EXAMPLE_END */
