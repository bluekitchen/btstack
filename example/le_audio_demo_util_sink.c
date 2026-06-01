/*
 * Copyright (C) {copyright_year} BlueKitchen GmbH
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

#define BTSTACK_FILE__ "le_audio_demo_util_sink.c"

#include <stdio.h>
#include <inttypes.h>

#include "le_audio_demo_util_sink.h"

#include "btstack_bool.h"
#include "btstack_config.h"
#include <btstack_debug.h>
#include <stdio.h>

#include "hci.h"
#include "btstack_audio.h"
#include "btstack_lc3_google.h"
#include "btstack_lc3plus_fraunhofer.h"
#include "btstack_resample.h"
#include "btstack_sample_rate_compensation.h"

#include "btstack_ring_buffer.h"

#ifdef ENABLE_BOX_DEMO
#include "demo_ui.h"
#endif

#ifdef HAVE_POSIX_FILE_IO
#include "wav_util.h"
#endif

// Featuresa
#define ENABLE_SAMPLERATE_COMPENSATION

// ANSI Colors
#define ANSI_COLOR_RED     "\033[31m"
#define ANSI_COLOR_YELLOW  "\033[33m"
#define ANSI_COLOR_RESET   "\033[0m"

#define MAX_CHANNELS 2
#define MAX_SAMPLES_PER_FRAME 480
#define MAX_LC3_FRAME_BYTES   155

// playback
#define MAX_NUM_LC3_FRAMES   (20)
#define BYTES_PER_SAMPLE 2

// sample rate compensation & resampler
#ifdef ENABLE_SAMPLERATE_COMPENSATION
// limit resampler to 10% more samples
#define SAMPLERATE_COMPENSATION_MAX_FACTOR 0x11999
#define MAX_PCM_BUFFER_SAMPLES ((MAX_SAMPLES_PER_FRAME * SAMPLERATE_COMPENSATION_MAX_FACTOR / 0x10000) + 1)
#else
#define MAX_PCM_BUFFER_SAMPLES MAX_SAMPLES_PER_FRAME
#endif


// per channel data
typedef struct {
    uint32_t start_time_us;
    uint32_t duration_us;
    uint8_t  lc3_frame[MAX_LC3_FRAME_BYTES];
    uint8_t  BFI;
} lc3_packet_t;

// ringbuffer full if (write_index + 1) == read_index
typedef struct {
    // index of lc3_packet that will be written next
    uint8_t      write_index;
    // index of lc3_packet that will be read next
    uint8_t      read_index;
    // received lc3 packets
    lc3_packet_t lc3_packets[MAX_NUM_LC3_FRAMES];
    // index of buffer that has been decoded in pcm_samples, or, 0xff
    uint8_t      pcm_buffer_index;
    // decoded pcm samples
    int16_t      pcm_buffer_samples[MAX_PCM_BUFFER_SAMPLES];
    uint16_t     pcm_buffer_samples_total;
    // index of next sample to play (generic playback only
    uint16_t     pcm_buffer_next_sample;
#ifdef ENABLE_SAMPLERATE_COMPENSATION
    // resampler
    btstack_resample_t resampler;
#endif
} lc3_data_t;
static lc3_data_t lc3_data[MAX_CHANNELS];

// playback engine

// SINK

static enum {
    LE_AUDIO_SINK_IDLE,
    LE_AUDIO_SINK_INIT,
    LE_AUDIO_SINK_CONFIGURED,
} le_audio_demo_util_sink_state = LE_AUDIO_SINK_IDLE;

static const char * le_audio_demo_sink_filename_wav;


static btstack_lc3_frame_duration_t le_audio_demo_sink_frame_duration;
static hci_iso_type_t               le_audio_demo_sink_type;

static uint32_t le_audio_demo_sink_sampling_frequency_hz;
static uint16_t le_audio_demo_sink_num_samples_per_frame;
static uint8_t  le_audio_demo_sink_num_streams;
static uint8_t  le_audio_demo_sink_num_channels_per_stream;
static uint8_t  le_audio_demo_sink_num_channels;
static uint16_t le_audio_demo_sink_octets_per_frame;
static uint16_t le_audio_demo_sink_iso_interval_1250us;
static uint8_t  le_audio_demo_sink_flush_timeout;
static uint8_t  le_audio_demo_sink_pre_transmission_offset;

// PLC
static uint32_t samples_received;
static uint32_t samples_played;
static uint16_t frames_lost;
static uint16_t frames_total;

// lc3 decoder
static bool le_audio_demo_lc3plus_decoder_requested = false;
static const btstack_lc3_decoder_t * lc3_decoder;

static btstack_lc3_decoder_google_t google_decoder_contexts[MAX_CHANNELS];
#ifdef HAVE_LC3PLUS
static btstack_lc3plus_fraunhofer_decoder_t fraunhofer_decoder_contexts[MAX_CHANNELS];
#endif
static void * decoder_contexts[MAX_CHANNELS];

#ifdef HAVE_HAL_AUDIO_EXTERNAL_TRIGGER
#define ENABLE_ACCURATE_PLAYBACK
#endif

// sample rate compensation & resample buffer
#ifdef ENABLE_SAMPLERATE_COMPENSATION
// TODO: not used with accurate playback yet
#ifndef ENABLE_ACCURATE_PLAYBACK
static int16_t  resample_buffer[MAX_SAMPLES_PER_FRAME];
#endif
static uint32_t resample_factor;
static btstack_sample_rate_compensation_t sample_rate_compensation;
static uint32_t playback_sample_rate;
static uint32_t iso_receive_start_ms;
static bool sample_rate_compensation_started;
#endif

// accurate playback
static uint32_t le_audio_demo_util_sink_presentation_delay_us;
static int32_t  le_audio_demo_util_sink_time_offset_us;

// generic playback
#ifndef ENABLE_ACCURATE_PLAYBACK
#define PLAYBACK_START_MS (30)
static uint16_t              playback_start_threshold_frames;
static bool                  playback_active;
#endif

// visualizer
static void (*le_audio_demo_util_sink_visualizer)(uint8_t num_channels, uint16_t num_samples, const int16_t * pcm);

static void le_audio_demo_util_sink_ringbuffer_init(uint8_t channel) {
    lc3_data[channel].read_index = 0;
    lc3_data[channel].write_index = 0;
    lc3_data[channel].pcm_buffer_index = 0xff;
}

static inline uint8_t le_audio_demo_util_ringbuffer_next(uint8_t index) {
    return (index + 1) % MAX_NUM_LC3_FRAMES;
}

static inline bool le_audio_demo_util_sink_ringbuffer_empty(uint8_t channel) {
    return lc3_data[channel].write_index == lc3_data[channel].read_index;
}

static bool le_audio_demo_util_sink_ringbuffer_full(uint8_t channel) {
    return le_audio_demo_util_ringbuffer_next(lc3_data[channel].write_index) == lc3_data[channel].read_index;
}

static uint8_t le_audio_demo_util_sink_ringbuffer_available(uint8_t channel) {
    if (lc3_data[channel].write_index >= lc3_data[channel].read_index) {
        return lc3_data[channel].write_index - lc3_data[channel].read_index;
    } else {
        return MAX_NUM_LC3_FRAMES + lc3_data[channel].write_index - lc3_data[channel].read_index;
    }
}

#ifdef ENABLE_ACCURATE_PLAYBACK

// With external trigger, we can convert LE ISO timestamps to local time and provide sample accurate playback
static void le_audio_connection_sink_playback(int16_t * buffer, uint16_t num_samples, const btstack_audio_context_t * context){
    btstack_assert(context != NULL);

    uint16_t original_num_samples = num_samples;
    int16_t * original_buffer = buffer;
    UNUSED(original_num_samples);
    UNUSED(original_buffer);

    // cache buffer playback end time to avoid division
    static uint32_t buffer_playback_time_us = 0;
    static uint32_t cached_num_samples = 0;
    if (num_samples != cached_num_samples) {
        buffer_playback_time_us = num_samples * 1000000 / le_audio_demo_sink_sampling_frequency_hz;
        cached_num_samples = num_samples;
        printf("Playback buffer: %u us\n", (unsigned) buffer_playback_time_us);
    }
    uint32_t playback_end_time_us = context->timestamp + buffer_playback_time_us;

    samples_played += num_samples;

    for (uint8_t channel = 0; channel < le_audio_demo_sink_num_channels; channel++) {
        // reset values for channel
        uint32_t playback_start_time_us = context->timestamp;
        uint16_t num_samples_for_channel = num_samples;

        // use pointer to avoid index calculation for each sample
        int16_t * buffer_start = &buffer[channel];

        // find earliest packet to play
        while ((num_samples_for_channel > 0) && (le_audio_demo_util_sink_ringbuffer_empty(channel) == false)){

            uint8_t index = lc3_data[channel].read_index;
            const lc3_packet_t * packet = &lc3_data[channel].lc3_packets[index];
            uint32_t packet_end_time_us = packet->start_time_us + packet->duration_us;

            // drop stale packets
            if (btstack_time_delta(playback_start_time_us, packet_end_time_us) >= 0){
                // printf("Drop buffer %2u for %" PRIu32 " us - %" PRIu32 " us\n", index, packet->start_time_us, packet_end_time_us);
                lc3_data[channel].read_index = le_audio_demo_util_ringbuffer_next(index);
                continue;
            }

            // decode packet if not already decoded with stride = 1 (one channel)
            if (lc3_data[channel].pcm_buffer_index != index) {
                uint8_t tmp_BEC_detect;
                (void) lc3_decoder->decode_signed_16(decoder_contexts[channel], lc3_data[channel].lc3_packets[index].lc3_frame,
                    lc3_data[channel].lc3_packets[index].BFI,
                    lc3_data[channel].pcm_buffer_samples,
                    1,
                    &tmp_BEC_detect);
                lc3_data[channel].pcm_buffer_index = index;
            }

            if (playback_end_time_us < packet->start_time_us){
                // not ready to play - abort
                // printf("Playback %u - %" PRIu32 " us: buffer %2u, start %" PRIu32 " us - in the future\n", channel, playback_start_time_us, index, packet->start_time_us);
                break;
            }

            // silence until start of next packet
            int32_t start_offset_us = btstack_time_delta(playback_start_time_us, packet->start_time_us);
            if (start_offset_us < 0) {
                // only later part of playback buffer can be filled
                uint16_t silent_samples = -start_offset_us * le_audio_demo_sink_sampling_frequency_hz / 1000000;
                if (silent_samples > 0) {
                    // printf("Playback %u - %" PRIu32 " us: buffer %2u, start %" PRIu32 " us - silent samples %u\n", channel, playback_start_time_us, index, packet->start_time_us, silent_samples);
                    for (uint16_t i = 0; i < silent_samples; i++){
                        *buffer_start = 0;
                        buffer_start += le_audio_demo_sink_num_channels;
                    }
                    num_samples_for_channel -= silent_samples;
                }
                playback_start_time_us = packet->start_time_us;
            } else {
                // copy samples
                uint16_t sample_index = start_offset_us * le_audio_demo_sink_sampling_frequency_hz / 1000000;
                btstack_assert(sample_index <= le_audio_demo_sink_num_samples_per_frame);
                uint16_t samples_to_copy = btstack_min(le_audio_demo_sink_num_samples_per_frame - sample_index, num_samples_for_channel);
                // printf("Playback %u - %" PRIu32 " us: buffer %2u, start %" PRIu32 " us - offset %" PRIu32 " us, sample %u -> copy %u\n", channel,
                //    playback_start_time_us, index, packet->start_time_us, start_offset_us, sample_index, samples_to_copy);
                for (uint16_t i = 0 ; i < samples_to_copy; i++) {
                    *buffer_start = lc3_data[channel].pcm_buffer_samples[sample_index++];
                    buffer_start += le_audio_demo_sink_num_channels;
                }
                num_samples_for_channel -= samples_to_copy;
                if (num_samples_for_channel > 0) {
                    // if there's still samples left, update playback_start_time_us to end of current packet
                    // and continue with next packet
                    playback_start_time_us = packet_end_time_us;
                    lc3_data[channel].read_index = le_audio_demo_util_ringbuffer_next(index);
                }
                // otherwise, we are done
            }
        }

        // silence
        if (num_samples_for_channel > 0) {
            for (uint16_t i = 0; i < num_samples_for_channel; i++){
                *buffer_start = 0;
                buffer_start += le_audio_demo_sink_num_channels;
            }
        }
    }
#ifdef HAVE_POSIX_FILE_IO
    // write wav samples
    wav_writer_write_int16(le_audio_demo_sink_num_channels * orignal_num_samples, original_buffer);
#endif

    if (le_audio_demo_util_sink_visualizer != NULL) {
        (*le_audio_demo_util_sink_visualizer)(le_audio_demo_sink_num_channels, original_num_samples, original_buffer);
    }
}


#else

// Without external trigger, we start playback after a minimum number of frames have been received and
// use sample rate compensation to control playback speed
static void le_audio_connection_sink_playback(int16_t * buffer, uint16_t num_samples, const btstack_audio_context_t * context) {
    UNUSED(context);

    uint16_t original_num_samples = num_samples;
    int16_t * original_buffer = buffer;
    UNUSED(original_num_samples);
    UNUSED(original_buffer);

    samples_played += num_samples;

    log_info("Playback: %u samples, active %u, lc3 frames available %u (write %2u, read %2u)",
        num_samples, playback_active, le_audio_demo_util_sink_ringbuffer_available(0),
        lc3_data[0].write_index, lc3_data[0].read_index);

    if( playback_active == false ) {
        if( le_audio_demo_util_sink_ringbuffer_available(0) >= playback_start_threshold_frames ) {
            log_info("Playback started");
            printf("Playback started\n");
            playback_active = true;
        }
    }

    if (playback_active) {
        while (num_samples > 0) {
            // if no samples available but lc3 frames available, decode next one
            if (lc3_data[0].pcm_buffer_index == 0xff) {
                if (le_audio_demo_util_sink_ringbuffer_empty(0)) {
                    break;
                }
                // decode lc3 frame
                for (uint8_t channel = 0; channel < le_audio_demo_sink_num_channels; channel++) {
                    uint8_t index = lc3_data[channel].read_index;
                    lc3_data[channel].read_index = le_audio_demo_util_ringbuffer_next(index);
                    uint8_t tmp_BEC_detect;
#ifdef ENABLE_SAMPLERATE_COMPENSATION
                    int16_t * pcm_output = resample_buffer;
#else
                    int16_t * pcm_output = lc3_data[channel].pcm_buffer_samples;
#endif
                    (void) lc3_decoder->decode_signed_16(decoder_contexts[channel], lc3_data[channel].lc3_packets[index].lc3_frame,
                        lc3_data[channel].lc3_packets[index].BFI,
                        pcm_output,
                    1,
                        &tmp_BEC_detect);
                    lc3_data[channel].pcm_buffer_index = index;
                    lc3_data[channel].pcm_buffer_next_sample = 0;
#ifdef ENABLE_SAMPLERATE_COMPENSATION
                    uint16_t pcm_samples_total = btstack_resample_block(&lc3_data[channel].resampler, pcm_output, le_audio_demo_sink_num_samples_per_frame, lc3_data[channel].pcm_buffer_samples);
#else
                    uint16_t pcm_samples_total = le_audio_demo_sink_num_samples_per_frame;
#endif
                    lc3_data[channel].pcm_buffer_samples_total = pcm_samples_total;
                    if (tmp_BEC_detect || (lc3_data[channel].lc3_packets[index].BFI == 1)) {
                        frames_lost++;
                        // printf( ANSI_COLOR_YELLOW "predict audio channel %u\n" ANSI_COLOR_RESET, channel);
                    }
                    frames_total++;
                }
            }

            // copy samples
            uint16_t samples_ready = lc3_data[0].pcm_buffer_samples_total - lc3_data[0].pcm_buffer_next_sample;
            uint16_t samples_to_copy = btstack_min(samples_ready, num_samples);
            for (uint16_t i = 0; i < samples_to_copy; i++){
                for (uint8_t channel = 0; channel < le_audio_demo_sink_num_channels; channel++) {
                    *buffer++ = lc3_data[channel].pcm_buffer_samples[lc3_data[channel].pcm_buffer_next_sample++];
                }
            }
            num_samples -= samples_to_copy;

            // discard buffer if all samples have been copied
            if (lc3_data[0].pcm_buffer_next_sample == lc3_data[0].pcm_buffer_samples_total) {
                lc3_data[0].pcm_buffer_index = 0xff;
            }
        }
    }

    // silence buffer
    if (num_samples > 0) {
        uint32_t bytes_needed = num_samples * le_audio_demo_sink_num_channels * BYTES_PER_SAMPLE;
        memset(buffer, 0, bytes_needed);
        if (playback_active == true) {
            printf(ANSI_COLOR_RED "Playback underrun\n" ANSI_COLOR_RESET);
            playback_active = false;
        }
    }

#ifdef HAVE_POSIX_FILE_IO
    // write wav samples
    wav_writer_write_int16(le_audio_demo_sink_num_channels * original_num_samples, original_buffer);
#endif

    if (le_audio_demo_util_sink_visualizer != NULL) {
        (*le_audio_demo_util_sink_visualizer)(le_audio_demo_sink_num_channels, original_num_samples, original_buffer);
    }
}

#endif

static void le_audio_demo_util_sink_store_packet(uint8_t stream_index, uint32_t sdu_sync_ref_us, const uint8_t * data, uint16_t len) {

#ifdef ENABLE_ACCURATE_PLAYBACK
    uint32_t start_time_us = sdu_sync_ref_us + le_audio_demo_util_sink_presentation_delay_us - le_audio_demo_util_sink_time_offset_us;
    uint32_t duration_us = le_audio_demo_sink_frame_duration == BTSTACK_LC3_FRAME_DURATION_10000US ? 10000 : 7500;
    uint32_t end_time_us = start_time_us + duration_us;
    UNUSED(end_time_us);
#else
    UNUSED(sdu_sync_ref_us);
#endif

    uint8_t BFI = len != le_audio_demo_sink_octets_per_frame * le_audio_demo_sink_num_channels_per_stream;
    for (uint8_t i = 0 ; i < le_audio_demo_sink_num_channels_per_stream ; i++){
        // unify one stream with two channels and two streams with one channel
        uint8_t effective_channel = stream_index + i;
        btstack_assert(effective_channel <= MAX_CHANNELS);
        if (le_audio_demo_util_sink_ringbuffer_full(effective_channel)) {
            log_info("Drop LC3 frame, ringbuffer full\n");
            lc3_data[effective_channel].read_index = le_audio_demo_util_ringbuffer_next(lc3_data[effective_channel].read_index);
        }
        btstack_assert(le_audio_demo_util_sink_ringbuffer_full(effective_channel) == false);

        uint8_t index = lc3_data[effective_channel].write_index;
        lc3_packet_t * packet = &lc3_data[effective_channel].lc3_packets[index];
#ifdef ENABLE_ACCURATE_PLAYBACK
        packet->start_time_us  = start_time_us;
        packet->duration_us    = duration_us;
#endif
        packet->BFI = BFI;
        if (BFI == 0) {
            // valid data
            // printf("Store %u audio buffer %u for %" PRIu32 " us - %" PRIu32 " us\n", effective_channel, index, packet->start_time_us, end_time_us);
            memcpy(packet->lc3_frame, data, le_audio_demo_sink_octets_per_frame);
            data += le_audio_demo_sink_octets_per_frame;
        } else {
            // use PLC
            // printf("Store %u empty buffer %u for %" PRIu32 " us - %" PRIu32 " us\n", effective_channel, index, packet->start_time_us, end_time_us);
        }
        lc3_data[effective_channel].write_index = le_audio_demo_util_ringbuffer_next(index);
    }
    log_info("Store: lc3 frames available %u (write %2u, read %2u)", le_audio_demo_util_sink_ringbuffer_available(0),
        lc3_data[0].write_index, lc3_data[0].read_index);
}

void le_audio_demo_util_sink_enable_lc3plus(bool enable){
    le_audio_demo_lc3plus_decoder_requested = enable;
}

static void setup_lc3_decoder(bool use_lc3plus_decoder){
    UNUSED(use_lc3plus_decoder);

    uint8_t channel;
    for (channel = 0 ; channel < le_audio_demo_sink_num_channels ; channel++){
        // pick decoder
        void * decoder_context = NULL;
#ifdef HAVE_LC3PLUS
        if (use_lc3plus_decoder){
            decoder_context = &fraunhofer_decoder_contexts[channel];
            lc3_decoder = btstack_lc3plus_fraunhofer_decoder_init_instance(decoder_context);
        }
        else
#endif
        {
            decoder_context = &google_decoder_contexts[channel];
            lc3_decoder = btstack_lc3_decoder_google_init_instance(decoder_context);
        }
        decoder_contexts[channel] = decoder_context;
        lc3_decoder->configure(decoder_context, le_audio_demo_sink_sampling_frequency_hz, le_audio_demo_sink_frame_duration, le_audio_demo_sink_octets_per_frame);
    }
    btstack_assert(le_audio_demo_sink_num_samples_per_frame <= MAX_SAMPLES_PER_FRAME);
}

static void reset_counters(void) {
    samples_received = 0;
    samples_played = 0;
    frames_lost = 0;
    frames_total = 0;
}

void le_audio_demo_util_sink_configure_general(uint8_t num_streams, uint8_t num_channels_per_stream,
                                               uint32_t sampling_frequency_hz,
                                               btstack_lc3_frame_duration_t frame_duration, uint16_t octets_per_frame,
                                               uint32_t iso_interval_1250us) {
    le_audio_demo_sink_sampling_frequency_hz = sampling_frequency_hz;
    le_audio_demo_sink_frame_duration = frame_duration;
    le_audio_demo_sink_octets_per_frame = octets_per_frame;
    le_audio_demo_sink_iso_interval_1250us = iso_interval_1250us;
    le_audio_demo_sink_num_streams = num_streams;
    le_audio_demo_sink_num_channels_per_stream = num_channels_per_stream;

    le_audio_demo_util_sink_state = LE_AUDIO_SINK_CONFIGURED;

    le_audio_demo_sink_num_channels = le_audio_demo_sink_num_streams * le_audio_demo_sink_num_channels_per_stream;
    btstack_assert((le_audio_demo_sink_num_channels == 1) || (le_audio_demo_sink_num_channels == 2));

    le_audio_demo_sink_num_samples_per_frame = btstack_lc3_samples_per_frame(le_audio_demo_sink_sampling_frequency_hz, le_audio_demo_sink_frame_duration);

    // switch to lc3plus if requested and possible
    bool use_lc3plus_decoder = le_audio_demo_lc3plus_decoder_requested && (frame_duration == BTSTACK_LC3_FRAME_DURATION_10000US);

    // init decoder
    setup_lc3_decoder(use_lc3plus_decoder);

    printf("Configure: %u streams, %u channels per stream, sampling rate %" PRIu32 ", samples per frame %u, lc3plus %u\n",
           num_streams, num_channels_per_stream, sampling_frequency_hz, le_audio_demo_sink_num_samples_per_frame, use_lc3plus_decoder);

#ifdef HAVE_POSIX_FILE_IO
    // create wav file
    printf("WAV file: %s\n", le_audio_demo_sink_filename_wav);
    wav_writer_open(le_audio_demo_sink_filename_wav, le_audio_demo_sink_num_channels, le_audio_demo_sink_sampling_frequency_hz);
#endif

    for (uint8_t channel = 0; channel < le_audio_demo_sink_num_channels; channel++) {
        le_audio_demo_util_sink_ringbuffer_init(channel);
    }

#ifndef ENABLE_ACCURATE_PLAYBACK
    // calc start threshold in frames for PLAYBACK_START_MS
    if (frame_duration == BTSTACK_LC3_FRAME_DURATION_10000US) {
        playback_start_threshold_frames = PLAYBACK_START_MS * 1000 / 10000;
    } else {
        playback_start_threshold_frames = PLAYBACK_START_MS * 1000 /  7500;
    }
    printf("Playback start threshold: %u frames\n", playback_start_threshold_frames);
    playback_active = false;
#endif

    reset_counters();

    // start playback
    const btstack_audio_sink_t * sink = btstack_audio_sink_get_instance();
    if (sink != NULL){
        // setup audio sink
        sink->init(le_audio_demo_sink_num_channels, le_audio_demo_sink_sampling_frequency_hz, le_audio_connection_sink_playback);
        sink->set_volume(70);

#ifdef ENABLE_SAMPLERATE_COMPENSATION
        // setup resampler
        for (uint8_t channel = 0; channel < le_audio_demo_sink_num_channels; channel++){
            btstack_resample_init(&lc3_data[channel].resampler, 1);
        }
        // cache playback sample rate
        playback_sample_rate = sink->get_samplerate();
        // ignore first second
        iso_receive_start_ms = 0;
        sample_rate_compensation_started = false;
#endif

        // start playback
        sink->start_stream();
    }
#ifdef ENABLE_SAMPLERATE_COMPENSATION
    else {
        // cache playback sample rate
        playback_sample_rate = le_audio_demo_sink_sampling_frequency_hz;
    }
#endif

}

void le_audio_demo_util_sink_configure_unicast(uint8_t num_streams, uint8_t num_channels_per_stream, uint32_t sampling_frequency_hz,
                                               btstack_lc3_frame_duration_t frame_duration, uint16_t octets_per_frame,
                                               uint32_t iso_interval_1250us, uint8_t flush_timeout){
    le_audio_demo_sink_type = HCI_ISO_TYPE_CIS;
    le_audio_demo_sink_flush_timeout = flush_timeout;

    le_audio_demo_util_sink_configure_general(num_streams, num_channels_per_stream, sampling_frequency_hz,
                                              frame_duration, octets_per_frame, iso_interval_1250us);
}

void le_audio_demo_util_sink_configure_broadcast(uint8_t num_streams, uint8_t num_channels_per_stream, uint32_t sampling_frequency_hz,
                                               btstack_lc3_frame_duration_t frame_duration, uint16_t octets_per_frame,
                                               uint32_t iso_interval_1250us, uint8_t pre_transmission_offset) {
    le_audio_demo_sink_type = HCI_ISO_TYPE_BIS;
    le_audio_demo_sink_pre_transmission_offset = pre_transmission_offset;

    le_audio_demo_util_sink_configure_general(num_streams, num_channels_per_stream, sampling_frequency_hz, frame_duration, octets_per_frame, iso_interval_1250us);
}

void le_audio_demo_util_sink_count(uint8_t stream_index, uint8_t *packet, uint16_t size) {
    UNUSED(stream_index);
    UNUSED(size);
    UNUSED(packet);
}

void le_audio_demo_util_sink_receive(uint8_t stream_index, uint8_t *packet, uint16_t size) {
    UNUSED(size);

    if (le_audio_demo_util_sink_state != LE_AUDIO_SINK_CONFIGURED) return;

    uint16_t header = little_endian_read_16(packet, 0);
    hci_con_handle_t con_handle = header & 0x0fff;
    uint8_t pb_flag = (header >> 12) & 3;
    uint8_t ts_flag = (header >> 14) & 1;
    uint16_t iso_load_len = little_endian_read_16(packet, 2);

    uint16_t offset = 4;
    uint32_t sdu_sync_ref_us = 0;
    if (ts_flag){
        sdu_sync_ref_us = little_endian_read_32(packet, offset);
        offset += 4;
    }

    uint16_t packet_sequence_number = little_endian_read_16(packet, offset);
    offset += 2;

    uint16_t header_2 = little_endian_read_16(packet, offset);
    uint16_t iso_sdu_length = header_2 & 0x3fff;
    uint8_t packet_status_flag = (uint8_t) (header_2 >> 14);
    offset += 2;

    // avoid warning for (yet) unused fields
    UNUSED(con_handle);
    UNUSED(pb_flag);
    UNUSED(iso_load_len);
    UNUSED(packet_status_flag);
    UNUSED(packet_sequence_number);

    // store in ring buffer
    le_audio_demo_util_sink_store_packet(stream_index, sdu_sync_ref_us, &packet[offset], iso_sdu_length);

    // correct for the fact that stereo will have two packets
    samples_received += le_audio_demo_sink_num_samples_per_frame / le_audio_demo_sink_num_streams;

    int report_interval_s = 3;
    if (samples_received >= report_interval_s * le_audio_demo_sink_sampling_frequency_hz){
        int packet_loss_percentage = 0;
        if (frames_total > 0) {
            packet_loss_percentage = frames_lost * 100 / frames_total;
        }
#ifdef ENABLE_SAMPLERATE_COMPENSATION
        printf("Frames lost %2u %%, LC3 frames queued %u, resample factor %f. Samples: received %5"PRIu32 " - played %5" PRIu32 "\n",
            packet_loss_percentage, le_audio_demo_util_sink_ringbuffer_available(0), Q16_TO_FLOAT(resample_factor),
            samples_received / report_interval_s, samples_played / report_interval_s);
#else
        printf("Frames lost %2u %%, LC3 frames queued %u. Samples received %5 "PRIu32 ", samples played %5" PRIu32 "\n",
            packet_loss_percentage,  le_audio_demo_util_sink_ringbuffer_available(0),
            samples_received / report_interval_s samples_played / report_interval_s);
#endif
        reset_counters();
    }

#ifdef ENABLE_SAMPLERATE_COMPENSATION
    uint32_t time_ms = btstack_run_loop_get_time_ms();
    // only run sample rate compensation for first stream
    if (stream_index == 0) {
        if (sample_rate_compensation_started == false) {
            if (iso_receive_start_ms == 0) {
                // 1 - set start time
                iso_receive_start_ms = time_ms;
            } else {
                // 2 - wait for one second
                if (btstack_time_delta(time_ms, iso_receive_start_ms) > 1000) {
                    // 3 - start sample rate compensation
                    sample_rate_compensation_started = true;
                    btstack_sample_rate_compensation_init(&sample_rate_compensation, time_ms, playback_sample_rate, FLOAT_TO_Q15(1.f));
                }
            }
        } else {
            resample_factor = btstack_sample_rate_compensation_update(&sample_rate_compensation, time_ms, le_audio_demo_sink_num_samples_per_frame, playback_sample_rate);
        }
    }
#endif
}

void le_audio_demo_util_sink_init(const char * filename_wav){
    le_audio_demo_sink_filename_wav = filename_wav;
    le_audio_demo_util_sink_state = LE_AUDIO_SINK_INIT;
}

/**
 * @brief Close sink: close wav file, stop playback
 */
void le_audio_demo_util_sink_close(void){
#ifdef HAVE_POSIX_FILE_IO
    printf("Close WAV file\n");
    wav_writer_close();
#endif
    // stop playback
    const btstack_audio_sink_t * sink = btstack_audio_sink_get_instance();
    if (sink != NULL){
        sink->stop_stream();
        sink->close();
    }
    le_audio_demo_util_sink_state = LE_AUDIO_SINK_INIT;
}

void le_audio_demo_util_sink_set_presentation_delay(uint32_t presentation_delay_us) {
    le_audio_demo_util_sink_presentation_delay_us = presentation_delay_us;
}

void le_audio_demo_util_sink_process_sync_event(uint32_t bluetooth_time_us, uint32_t local_time_us) {
    le_audio_demo_util_sink_time_offset_us = btstack_time_delta(bluetooth_time_us, local_time_us);
    log_info("ISO timestamp %010"PRIu32" us, local timestamp %010" PRIu32 " => offset %10" PRIi32" us\n",
        bluetooth_time_us, local_time_us, le_audio_demo_util_sink_time_offset_us);
}

void le_audio_demo_util_sink_set_visualizer( void (*callback)(uint8_t num_channels, uint16_t num_samples, const int16_t * pcm)) {
    le_audio_demo_util_sink_visualizer = callback;
}
