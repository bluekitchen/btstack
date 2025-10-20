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
#ifdef HAVE_ALSA
#define BTSTACK_FILE__ "btstack_audio_alsa.c"

#include <btstack_run_loop.h>
#include <alsa/asoundlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <poll.h>

#include "btstack_debug.h"
#include "btstack_audio.h"

static const char* device = "default";
static const char* simple_mixer_name = "Master";
static snd_pcm_t *pcm_handle = NULL;
static snd_mixer_t *mixer_handle = NULL;
static snd_mixer_elem_t* master_volume = NULL;
static long volume_max;
static uint32_t current_sample_rate;
static uint8_t num_channels;
static void (*playback_callback)(int16_t *buffer, uint16_t num_samples);

static unsigned int buffer_time = 500000;   /* ring buffer length in us */
static unsigned int period_time = 100000;   /* period time in us */
static int resample = 1;                    /* enable alsa-lib resampling */
static snd_output_t *output = NULL;
static snd_pcm_sframes_t buffer_size;
static snd_pcm_sframes_t period_size;
static int period_event = 0;                /* produce poll event after each period */

static struct pollfd *ufds = NULL;
static int ufds_count;
// TODO: corner case handling
#if 0
/*
 *   Underrun and suspend recovery
 */
static int xrun_recovery(snd_pcm_t *handle, int err)
{
    printf("stream recovery\n");
    if (err == -EPIPE) {    /* under-run */
        err = snd_pcm_prepare(handle);
        if (err < 0)
            printf("Can't recovery from underrun, prepare failed: %s\n", snd_strerror(err));
        return 0;
    } else if (err == -ESTRPIPE) {
        while ((err = snd_pcm_resume(handle)) == -EAGAIN)
            sleep(1);   /* wait until the suspend flag is released */
        if (err < 0) {
            err = snd_pcm_prepare(handle);
            if (err < 0)
                printf("Can't recovery from suspend, prepare failed: %s\n", snd_strerror(err));
        }
        return 0;
    }
    return err;
}
#endif

static btstack_data_source_t *btstack_audio_alsa_data_sources;

static void btstack_audio_alsa_handler(btstack_data_source_t * ds, btstack_data_source_callback_type_t callback_type) {
    int source_fd = ds->source.fd;
    int err;

    for(int i=0; i<ufds_count; ++i ) {
        int fd = ufds[i].fd;
        short revents = 0;
        if( source_fd == fd ) {
            if( callback_type == DATA_SOURCE_CALLBACK_WRITE ) {
                revents |= POLLOUT;
            } else if( callback_type == DATA_SOURCE_CALLBACK_READ ) {
                revents |= POLLIN;
            }
        }
        ufds[i].revents = revents;
    }
    unsigned short revents;
    err = snd_pcm_poll_descriptors_revents( pcm_handle, ufds, ufds_count, &revents );
    if( err < 0 ) {
        fprintf(stderr, "Can't translate event poll results: %s\n", snd_strerror(err));
        return;
    }

    if( ( revents & POLLOUT ) != POLLOUT ) {
        return;
    }

    int16_t buffer[1024] = { 0 };
    playback_callback( buffer, 512 );
    err = snd_pcm_writei( pcm_handle, buffer, 512 );
    if (err < 512) {
        fprintf(stderr, "Write error: %s\n", snd_strerror(err));
        return;
    }
}

static int alsa_mixer_init() {
    long min;
    int err;
    snd_mixer_selem_id_t *sid;

    err = snd_mixer_open(&mixer_handle, 0);
     if( err < 0 ) {
        fprintf(stderr, "Cannot open mixer device: %s\n", snd_strerror(err));
        return err;
    }
    err = snd_mixer_attach(mixer_handle, device);
    if( err < 0 ) {
        fprintf(stderr, "Cannot attach mixer to device %s: %s\n", device, snd_strerror(err));
        return err;
    }
    err = snd_mixer_selem_register(mixer_handle, NULL, NULL);
      if( err < 0 ) {
        fprintf(stderr, "Can't register simple mixer device: %s\n", snd_strerror(err));
        return err;
    }
    err = snd_mixer_load(mixer_handle);
    if( err < 0 ) {
        fprintf(stderr, "Failed to load mixer: %s\n", snd_strerror(err));
        return err;
    }
    snd_mixer_selem_id_alloca(&sid);
    if( sid == NULL ) {
        fprintf(stderr, "Failed to allocate simple mixer ID: %s\n", snd_strerror(err));
        return err;
    }

    snd_mixer_selem_id_set_index(sid, 0);
    snd_mixer_selem_id_set_name(sid, simple_mixer_name);
    master_volume = snd_mixer_find_selem(mixer_handle, sid);
    if( master_volume == NULL ) {
        fprintf(stderr, "Master volume not found: %s\n", snd_strerror(err));
        return err;
    }
    err = snd_mixer_selem_get_playback_volume_range(master_volume, &min, &volume_max);
    if( err < 0 ) {
        fprintf(stderr, "Failed to query mixer range: %s\n", snd_strerror(err));
        return err;
    }
    return 0;
}

static int alsa_init(uint8_t channels, uint32_t samplerate, void (*playback)(int16_t *, uint16_t)) {
    int err, dir;
    snd_pcm_hw_params_t *hw_params;
    snd_pcm_sw_params_t *sw_params;
    unsigned int rrate;
    snd_pcm_uframes_t size;

    err = snd_output_stdio_attach(&output, stdout, 0);
    if (err < 0) {
        fprintf(stderr, "Output failed: %s\n", snd_strerror(err));
        return err;
    }

    err = snd_pcm_open(&pcm_handle, device, SND_PCM_STREAM_PLAYBACK, 0);
    if(err < 0) {
        fprintf(stderr, "Cannot open audio device: %s\n", snd_strerror(err));
        return err;
    }

    snd_pcm_hw_params_alloca(&hw_params);
    if( hw_params == NULL ) {
       fprintf(stderr, "Failed to allocate simple mixer ID: %s\n", snd_strerror(err));
       return err;
    }
    /* choose all parameters */
    err = snd_pcm_hw_params_any(pcm_handle, hw_params);
    if (err < 0) {
        fprintf(stderr, "Broken configuration for playback: no configurations available: %s\n", snd_strerror(err));
        return err;
    }
    /* set hardware resampling */
    err = snd_pcm_hw_params_set_rate_resample(pcm_handle, hw_params, resample);
    if (err < 0) {
        fprintf(stderr, "Resampling setup failed for playback: %s\n", snd_strerror(err));
        return err;
    }
    /* set the interleaved read/write format */
    err = snd_pcm_hw_params_set_access(pcm_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0) {
        fprintf(stderr, "Access type not available for playback: %s\n", snd_strerror(err));
        return err;
    }
    /* set the sample format */
    err = snd_pcm_hw_params_set_format(pcm_handle, hw_params, SND_PCM_FORMAT_S16_LE);
    if (err < 0) {
        fprintf(stderr, "Sample format not available for playback: %s\n", snd_strerror(err));
        return err;
    }
    /* set the count of channels */
    err = snd_pcm_hw_params_set_channels(pcm_handle, hw_params, channels);
    if (err < 0) {
        fprintf(stderr, "Channels count (%u) not available for playbacks: %s\n", channels, snd_strerror(err));
        return err;
    }
    /* set the stream rate */
    rrate = samplerate;
    err = snd_pcm_hw_params_set_rate_near(pcm_handle, hw_params, &rrate, 0);
    if (err < 0) {
        fprintf(stderr, "Rate %uHz not available for playback: %s\n", samplerate, snd_strerror(err));
        return err;
    }
    if (rrate != samplerate) {
        fprintf(stderr, "Rate doesn't match (requested %uHz, get %iHz)\n", samplerate, err);
        return -EINVAL;
    }
    /* set the buffer time */
    err = snd_pcm_hw_params_set_buffer_time_near(pcm_handle, hw_params, &buffer_time, &dir);
    if (err < 0) {
        fprintf(stderr, "Unable to set buffer time %u for playback: %s\n", buffer_time, snd_strerror(err));
        return err;
    }
    err = snd_pcm_hw_params_get_buffer_size(hw_params, &size);
    if (err < 0) {
        fprintf(stderr, "Unable to get buffer size for playback: %s\n", snd_strerror(err));
        return err;
    }
    buffer_size = size;
    /* set the period time */
    err = snd_pcm_hw_params_set_period_time_near(pcm_handle, hw_params, &period_time, &dir);
    if (err < 0) {
        fprintf(stderr, "Unable to set period time %u for playback: %s\n", period_time, snd_strerror(err));
        return err;
    }
    err = snd_pcm_hw_params_get_period_size(hw_params, &size, &dir);
    if (err < 0) {
        fprintf(stderr, "Unable to get period size for playback: %s\n", snd_strerror(err));
        return err;
    }
    period_size = size;

    err = snd_pcm_hw_params(pcm_handle, hw_params);
    if (err < 0) {
        fprintf(stderr, "Cannot set hardware parameters: %s\n", snd_strerror(err));
        return err;
    }

    snd_pcm_sw_params_alloca(&sw_params);

    /* get the current sw params */
    err = snd_pcm_sw_params_current(pcm_handle, sw_params);
    if (err < 0) {
        fprintf(stderr, "Unable to determine current swparams for playback: %s\n", snd_strerror(err));
        return err;
    }
    /* start the transfer when the buffer is almost full: */
    /* (buffer_size / avail_min) * avail_min */
    err = snd_pcm_sw_params_set_start_threshold(pcm_handle, sw_params, (buffer_size / period_size) * period_size);
    if (err < 0) {
        fprintf(stderr, "Unable to set start threshold mode for playback: %s\n", snd_strerror(err));
        return err;
    }
    /* allow the transfer when at least period_size samples can be processed */
    /* or disable this mechanism when period event is enabled (aka interrupt like style processing) */
    err = snd_pcm_sw_params_set_avail_min(pcm_handle, sw_params, period_event ? buffer_size : period_size);
    if (err < 0) {
        fprintf(stderr, "Unable to set avail min for playback: %s\n", snd_strerror(err));
        return err;
    }
    /* enable period events when requested */
    if (period_event) {
        err = snd_pcm_sw_params_set_period_event(pcm_handle, sw_params, 1);
        if (err < 0) {
            fprintf(stderr, "Unable to set period event: %s\n", snd_strerror(err));
            return err;
        }
    }
    /* write the parameters to the playback device */
    err = snd_pcm_sw_params(pcm_handle, sw_params);
    if (err < 0) {
        fprintf(stderr, "Unable to set sw params for playback: %s\n", snd_strerror(err));
        return err;
    }

    current_sample_rate = samplerate;
    num_channels = channels;
    playback_callback = playback;

    ufds_count = snd_pcm_poll_descriptors_count (pcm_handle);
    if (ufds_count <= 0) {
        fprintf(stderr, "Invalid poll descriptors count\n");
        return ufds_count;
    }

    ufds = malloc(sizeof(struct pollfd) * ufds_count);
    if (ufds == NULL) {
        fprintf(stderr, "No enough memory for pollfd\n");
        return -ENOMEM;
    }

    btstack_audio_alsa_data_sources = malloc(sizeof(btstack_data_source_t) * ufds_count);
    if (ufds == NULL) {
        fprintf(stderr, "No enough memory for data sources\n");
        return -ENOMEM;
    }

    err = snd_pcm_poll_descriptors(pcm_handle, ufds, ufds_count);
    if(err < 0) {
        fprintf(stderr, "Unable to obtain poll descriptors for playback: %s\n", snd_strerror(err));
        return err;
    }

    // poll -> data sources
    for(int i=0; i<ufds_count; ++i) {
        int flags = 0;
        int fd = ufds[i].fd;
        short events = ufds[i].events;
        ufds[i].revents = 0;
        btstack_data_source_t *data_source = &btstack_audio_alsa_data_sources[i];
        if( (events & POLLOUT) == POLLOUT ) {
            flags |= DATA_SOURCE_CALLBACK_WRITE;
        }
        if( (events & POLLIN) == POLLIN ) {
            flags |= DATA_SOURCE_CALLBACK_READ;
        }
        btstack_run_loop_set_data_source_fd(data_source, fd);
        btstack_run_loop_set_data_source_handler(data_source, &btstack_audio_alsa_handler);
        btstack_run_loop_add_data_source(data_source);
        btstack_run_loop_enable_data_source_callbacks(data_source, flags);
    }

    // TODO: remove once pipe error handling / priming is implemented
    int16_t samples[48000] = { 0 };
    printf("period size: %ld\n", period_size );
    snd_pcm_writei( pcm_handle, samples, period_size );

    alsa_mixer_init();
    return 0;
}

static uint32_t alsa_get_samplerate(void) {
    return current_sample_rate;
}

static void alsa_set_volume(uint8_t volume) {
    int err;

    btstack_assert( volume <= 127 );
    if( master_volume == NULL ) {
        return;
    }
    err = snd_mixer_selem_set_playback_volume_all(master_volume, volume * volume_max / 127);
    if( err < 0 ) {
        fprintf(stderr, "Failed to set volume: %s\n", snd_strerror(err));
        return;
    }
}

static void alsa_start_stream(void) {
    snd_pcm_start(pcm_handle);
}

static void alsa_stop_stream(void) {
    snd_pcm_drop(pcm_handle);
}

static void alsa_close(void) {
    if( pcm_handle != NULL ) {
        snd_pcm_close(pcm_handle);
    }
    if( mixer_handle != NULL ) {
        snd_mixer_close(mixer_handle);
    }
    if( ufds != NULL ) {
        free( ufds );
    }
    if( btstack_audio_alsa_data_sources != NULL ) {
        free( btstack_audio_alsa_data_sources );
    }
}

btstack_audio_sink_t btstack_audio_alsa_sink = {
    .init = alsa_init,
    .get_samplerate = alsa_get_samplerate,
    .set_volume = alsa_set_volume,
    .start_stream = alsa_start_stream,
    .stop_stream = alsa_stop_stream,
    .close = alsa_close,
};

const btstack_audio_sink_t * btstack_audio_alsa_sink_get_instance(void){
    return &btstack_audio_alsa_sink;
}
#endif // HAVE_ALSA
