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

#include "le_audio_demo_util_sink.h"

#include "btstack_bool.h"
#include "btstack_config.h"
#include <btstack_debug.h>
#include <printf.h>

#include "hci.h"
#include "btstack_audio.h"
#include "btstack_lc3_google.h"
#include "btstack_lc3plus_fraunhofer.h"

#include "btstack_sample_rate_compensation.h"
#include "btstack_resample.h"

#include "hxcmod.h"
#include "mods/mod.h"

#ifdef HAVE_POSIX_FILE_IO
#include "wav_util.h"
#include "btstack_ring_buffer.h"

#endif

//#define DEBUG_PLC
#ifdef DEBUG_PLC
#define printf_plc(...) { \
    printf(__VA_ARGS__);  \
    log_info(__VA_ARGS__);\
}
#else
#define printf_plc(...)  (void)(0);
#endif

#define MAX_CHANNELS 2
#define MAX_SAMPLES_PER_FRAME 480
#define MAX_LC3_FRAME_BYTES   155

// playback
#define MAX_NUM_LC3_FRAMES   15
#define MAX_BYTES_PER_SAMPLE 4
#define PLAYBACK_BUFFER_SIZE (MAX_NUM_LC3_FRAMES * MAX_SAMPLES_PER_FRAME * MAX_CHANNELS * MAX_BYTES_PER_SAMPLE)
#define PLAYBACK_START_MS (MAX_NUM_LC3_FRAMES * 20 / 3)

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

// SINK

static const char * le_audio_demo_sink_filename_wav;
static btstack_sample_rate_compensation_t sample_rate_compensation;
static btstack_resample_t resample_instance;
static bool sink_receive_streaming;

static int16_t pcm_resample[MAX_CHANNELS * MAX_SAMPLES_PER_FRAME * 2];


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

// playback
static uint16_t              playback_start_threshold_bytes;
static bool                  playback_active;
static uint8_t               playback_buffer_storage[PLAYBACK_BUFFER_SIZE];
static btstack_ring_buffer_t playback_buffer;

// PLC
static bool     stream_last_packet_received[MAX_CHANNELS];
static uint16_t stream_last_packet_sequence[MAX_CHANNELS];
static uint16_t group_last_packet_sequence;
static bool     group_last_packet_received;
static uint16_t plc_timeout_initial_ms;
static uint16_t plc_timeout_subsequent_ms;

static uint32_t le_audio_demo_sink_lc3_frames;
static uint32_t samples_received;
static uint32_t samples_played;
static uint32_t samples_dropped;

static btstack_timer_source_t next_packet_timer;

// lc3 decoder
static bool le_audio_demo_lc3plus_decoder_requested = false;
static const btstack_lc3_decoder_t * lc3_decoder;
static int16_t pcm[MAX_CHANNELS * MAX_SAMPLES_PER_FRAME];
static bool have_pcm[MAX_CHANNELS];

static btstack_lc3_decoder_google_t google_decoder_contexts[MAX_CHANNELS];
#ifdef HAVE_LC3PLUS
static btstack_lc3plus_fraunhofer_decoder_t fraunhofer_decoder_contexts[MAX_CHANNELS];
#endif
static void * decoder_contexts[MAX_CHANNELS];

static void le_audio_connection_sink_playback(int16_t * buffer, uint16_t num_samples){
    // called from lower-layer but guaranteed to be on main thread
    log_info("Playback: need %u, have %u", num_samples, btstack_ring_buffer_bytes_available(&playback_buffer) / (le_audio_demo_sink_num_channels * 2));

    samples_played += num_samples;

    uint32_t bytes_needed = num_samples * le_audio_demo_sink_num_channels * 2;
    if (playback_active == false){
        if (btstack_ring_buffer_bytes_available(&playback_buffer) >= playback_start_threshold_bytes) {
            log_info("Playback started");
            playback_active = true;
        }
    } else {
        if (bytes_needed > btstack_ring_buffer_bytes_available(&playback_buffer)) {
            log_info("Playback underrun");
            printf("Playback Underrun\n");
            // empty buffer
            uint32_t bytes_read;
            btstack_ring_buffer_read(&playback_buffer, (uint8_t *) buffer, bytes_needed, &bytes_read);
            playback_active = false;
        }
    }

    if (playback_active){
        uint32_t bytes_read;
        btstack_ring_buffer_read(&playback_buffer, (uint8_t *) buffer, bytes_needed, &bytes_read);
        btstack_assert(bytes_read == bytes_needed);
    } else {
        memset(buffer, 0, bytes_needed);
    }
}

static void store_samples_in_ringbuffer(void){
    // check if we have all channels
    uint8_t channel;
    for (channel = 0; channel < le_audio_demo_sink_num_channels; channel++){
        if (have_pcm[channel] == false) return;
    }
#ifdef HAVE_POSIX_FILE_IO
    // write wav samples
    wav_writer_write_int16(le_audio_demo_sink_num_channels * le_audio_demo_sink_num_samples_per_frame, pcm);
#endif
    // store samples in playback buffer
    samples_received += le_audio_demo_sink_num_samples_per_frame;
    uint32_t resampled_frames = btstack_resample_block(&resample_instance, pcm, le_audio_demo_sink_num_samples_per_frame, pcm_resample);
    uint32_t bytes_to_store = resampled_frames * le_audio_demo_sink_num_channels * 2;

    if (btstack_ring_buffer_bytes_free(&playback_buffer) >= bytes_to_store) {
        btstack_ring_buffer_write(&playback_buffer, (uint8_t *) pcm_resample, bytes_to_store);
        log_info("Samples in playback buffer %5u", btstack_ring_buffer_bytes_available(&playback_buffer) / (le_audio_demo_sink_num_channels * 2));
    } else {
        printf("Samples dropped\n");
        samples_dropped += le_audio_demo_sink_num_samples_per_frame;
    }
    memset(have_pcm, 0, sizeof(have_pcm));
}

static void plc_do(uint8_t stream_index) {
    // inject packet
    uint8_t tmp_BEC_detect;
    uint8_t BFI = 1;
    uint8_t i;
    for (i = 0; i < le_audio_demo_sink_num_channels_per_stream; i++){
        uint8_t effective_channel = stream_index + i;
        (void) lc3_decoder->decode_signed_16(decoder_contexts[effective_channel], NULL, BFI,
                                             &pcm[effective_channel], le_audio_demo_sink_num_channels,
                                             &tmp_BEC_detect);
    }
    // and store in ringbuffer when PCM for all channels is available
    store_samples_in_ringbuffer();
}

//
// Perform PLC for packets missing in previous intervals
//
// assumptions:
// - packet sequence number is monotonic increasing
// - if packet with seq nr x is received, all packets with smaller seq number are either received or missed
static void plc_check(uint16_t packet_sequence_number) {
    while (group_last_packet_sequence != packet_sequence_number){
        uint8_t i;
        for (i=0;i<le_audio_demo_sink_num_streams;i++){
            // deal with first packet missing. inject silent samples, pcm buffer is memset to zero at start
            if (stream_last_packet_received[i] == false){
                printf_plc("- ISO #%u, very first packet missing\n", i);
                have_pcm[i] = true;
                store_samples_in_ringbuffer();

                stream_last_packet_received[i] = true;
                stream_last_packet_sequence[i] = group_last_packet_sequence;
                continue;
            }

            // missing packet if big sequence counter is higher than bis sequence counter
            if (btstack_time16_delta(group_last_packet_sequence, stream_last_packet_sequence[i]) > 0) {
                printf_plc("- ISO #%u, PLC for %u\n", i, group_last_packet_sequence);
                plc_do(i);
                btstack_assert((stream_last_packet_sequence[i] + 1) == group_last_packet_sequence);
                stream_last_packet_sequence[i] = group_last_packet_sequence;
            }
        }
        group_last_packet_sequence++;
    }
}

static void plc_timeout(btstack_timer_source_t * timer) {
    // Restart timer. This will loose sync with ISO interval, but if we stop caring if we loose that many packets
    btstack_run_loop_set_timer(timer, plc_timeout_subsequent_ms);
    btstack_run_loop_set_timer_handler(timer, plc_timeout);
    btstack_run_loop_add_timer(timer);

    switch (le_audio_demo_sink_type){
        case HCI_ISO_TYPE_CIS:
            // assume no packet received in iso interval => FT packets missed
            printf_plc("PLC: timeout cis, group %u, FT %u", group_last_packet_sequence, le_audio_demo_sink_flush_timeout);
            plc_check(group_last_packet_sequence + le_audio_demo_sink_flush_timeout);
            break;
        case HCI_ISO_TYPE_BIS:
            // assume PTO not used => 1 packet missed
            plc_check(group_last_packet_sequence + 1);
            break;
        default:
            btstack_unreachable();
            break;
    }
}

void le_audio_demo_util_sink_init(const char * filename_wav){
    le_audio_demo_sink_filename_wav = filename_wav;
}

void le_audio_demo_util_sink_enable_lc3plus(bool enable){
    le_audio_demo_lc3plus_decoder_requested = enable;
}

static void setup_lc3_decoder(bool use_lc3plus_decoder){
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

    sink_receive_streaming = false;

    le_audio_demo_sink_num_channels = le_audio_demo_sink_num_streams * le_audio_demo_sink_num_channels_per_stream;
    btstack_assert((le_audio_demo_sink_num_channels == 1) || (le_audio_demo_sink_num_channels == 2));

    le_audio_demo_sink_lc3_frames = 0;

    group_last_packet_received = false;
    uint8_t i;
    for (i=0;i<MAX_CHANNELS;i++){
        stream_last_packet_received[i] = false;
        have_pcm[i] = false;
    }

    le_audio_demo_sink_num_samples_per_frame = btstack_lc3_samples_per_frame(le_audio_demo_sink_sampling_frequency_hz, le_audio_demo_sink_frame_duration);

    // switch to lc3plus if requested and possible
    bool use_lc3plus_decoder = le_audio_demo_lc3plus_decoder_requested && (frame_duration == BTSTACK_LC3_FRAME_DURATION_10000US);

    // init decoder
    setup_lc3_decoder(use_lc3plus_decoder);

    printf("Configure: %u streams, %u channels per stream, sampling rate %u, samples per frame %u, lc3plus %u\n",
           num_streams, num_channels_per_stream, sampling_frequency_hz, le_audio_demo_sink_num_samples_per_frame, use_lc3plus_decoder);

#ifdef HAVE_POSIX_FILE_IO
    // create wav file
    printf("WAV file: %s\n", le_audio_demo_sink_filename_wav);
    wav_writer_open(le_audio_demo_sink_filename_wav, le_audio_demo_sink_num_channels, le_audio_demo_sink_sampling_frequency_hz);
#endif

    // init playback buffer
    btstack_ring_buffer_init(&playback_buffer, playback_buffer_storage, PLAYBACK_BUFFER_SIZE);

    // calc start threshold in bytes for PLAYBACK_START_MS
    playback_start_threshold_bytes = (sampling_frequency_hz / 1000 * PLAYBACK_START_MS) * le_audio_demo_sink_num_channels * 2;

    // start playback
    const btstack_audio_sink_t * sink = btstack_audio_sink_get_instance();
    if (sink != NULL){
        btstack_sample_rate_compensation_reset( &sample_rate_compensation, btstack_run_loop_get_time_ms() );
        btstack_resample_init(&resample_instance, le_audio_demo_sink_num_channels_per_stream);
        sink->init(le_audio_demo_sink_num_channels, le_audio_demo_sink_sampling_frequency_hz, le_audio_connection_sink_playback);
        sink->start_stream();
    }
}

void le_audio_demo_util_sink_configure_unicast(uint8_t num_streams, uint8_t num_channels_per_stream, uint32_t sampling_frequency_hz,
                                               btstack_lc3_frame_duration_t frame_duration, uint16_t octets_per_frame,
                                               uint32_t iso_interval_1250us, uint8_t flush_timeout){
    le_audio_demo_sink_type = HCI_ISO_TYPE_CIS;
    le_audio_demo_sink_flush_timeout = flush_timeout;

    // set playback start: FT * ISO Interval + max(10 ms, 1/2 ISO Interval)
    uint16_t playback_start_ms = flush_timeout * (iso_interval_1250us * 5 / 4) + btstack_max(10, iso_interval_1250us * 5 / 8);
    uint16_t playback_start_samples = sampling_frequency_hz / 1000 * playback_start_ms;
    playback_start_threshold_bytes = playback_start_samples * num_streams * num_channels_per_stream * 2;
    printf("Playback: start %u ms (%u samples, %u bytes)\n", playback_start_ms, playback_start_samples, playback_start_threshold_bytes);

    // set subsequent plc timeout: FT * ISO Interval
    plc_timeout_subsequent_ms = flush_timeout * iso_interval_1250us * 5 / 4;

    // set initial plc timeout:FT * ISO Interval + 4 ms
    plc_timeout_initial_ms = plc_timeout_subsequent_ms + 4;

    printf("PLC: initial timeout    %u ms\n", plc_timeout_initial_ms);
    printf("PLC: subsequent timeout %u ms\n", plc_timeout_subsequent_ms);

    le_audio_demo_util_sink_configure_general(num_streams, num_channels_per_stream, sampling_frequency_hz,
                                              frame_duration, octets_per_frame, iso_interval_1250us);
}

void le_audio_demo_util_sink_configure_broadcast(uint8_t num_streams, uint8_t num_channels_per_stream, uint32_t sampling_frequency_hz,
                                               btstack_lc3_frame_duration_t frame_duration, uint16_t octets_per_frame,
                                               uint32_t iso_interval_1250us, uint8_t pre_transmission_offset) {
    le_audio_demo_sink_type = HCI_ISO_TYPE_BIS;
    le_audio_demo_sink_pre_transmission_offset = pre_transmission_offset;

    // set playback start: ISO Interval + 10 ms
    uint16_t playback_start_ms = (iso_interval_1250us * 5 / 4) + 10;
    uint16_t playback_start_samples = sampling_frequency_hz / 1000 * playback_start_ms;
    playback_start_threshold_bytes = playback_start_samples * num_streams * num_channels_per_stream * 2;
    printf("Playback: start %u ms (%u samples, %u bytes)\n", playback_start_ms, playback_start_samples, playback_start_threshold_bytes);

    // set subsequent plc timeout: ISO Interval
    plc_timeout_subsequent_ms = iso_interval_1250us * 5 / 4;

    // set initial plc timeout: ISO Interval + 4 ms
    plc_timeout_initial_ms = plc_timeout_subsequent_ms + 4;

    printf("PLC: initial timeout    %u ms\n", plc_timeout_initial_ms);
    printf("PLC: subsequent timeout %u ms\n", plc_timeout_subsequent_ms);

    le_audio_demo_util_sink_configure_unicast(num_streams, num_channels_per_stream, sampling_frequency_hz, frame_duration,
                                              octets_per_frame, iso_interval_1250us, pre_transmission_offset);
}

void le_audio_demo_util_sink_receive(uint8_t stream_index, uint8_t *packet, uint16_t size) {
    uint16_t header = little_endian_read_16(packet, 0);
    hci_con_handle_t con_handle = header & 0x0fff;
    uint8_t pb_flag = (header >> 12) & 3;
    uint8_t ts_flag = (header >> 14) & 1;
    uint16_t iso_load_len = little_endian_read_16(packet, 2);

    uint16_t offset = 4;
    uint32_t time_stamp = 0;
    if (ts_flag){
        time_stamp = little_endian_read_32(packet, offset);
        offset += 4;
    }

    uint32_t receive_time_ms = btstack_run_loop_get_time_ms();

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

    // start with first packet on first stream
    if (group_last_packet_received == false){
        if (stream_index != 0){
            printf("Ignore first packet for second stream\n");
            return;
        }
        group_last_packet_received = true;
        group_last_packet_sequence = packet_sequence_number;
    }

    if (stream_last_packet_received[stream_index]) {
        printf_plc("ISO #%u, receive %u\n", stream_index, packet_sequence_number);

        int16_t packet_sequence_delta = btstack_time16_delta(packet_sequence_number,
                                                             stream_last_packet_sequence[stream_index]);
        if (packet_sequence_delta < 1) {
            // drop delayed packet that had already been generated by PLC
            printf_plc("- dropping delayed packet. Current sequence number %u, last received or generated by PLC: %u\n",
                       packet_sequence_number, stream_last_packet_sequence[stream_index]);
            return;
        }
        // simple check
        if (packet_sequence_number != stream_last_packet_sequence[stream_index] + 1) {
            printf_plc("- ISO #%u, missing %u\n", stream_index, stream_last_packet_sequence[stream_index] + 1);
        }
    } else {
        printf_plc("ISO %u, first packet seq number %u\n", stream_index, packet_sequence_number);
        stream_last_packet_received[stream_index] = true;
    }

    plc_check(packet_sequence_number);

    // either empty packets or num channels * num octets
    if ((iso_sdu_length != 0) && (iso_sdu_length != le_audio_demo_sink_num_channels_per_stream * le_audio_demo_sink_octets_per_frame)) {
        printf("ISO Length %u != %u * %u\n", iso_sdu_length, le_audio_demo_sink_num_channels_per_stream, le_audio_demo_sink_octets_per_frame);
        log_info("ISO Length %u != %u * %u", iso_sdu_length, le_audio_demo_sink_num_channels_per_stream, le_audio_demo_sink_octets_per_frame);
        return;
    }

    const btstack_audio_sink_t * sink = btstack_audio_sink_get_instance();
    if( (sink != NULL) && (iso_sdu_length>0)) {
        if( !sink_receive_streaming && playback_active ) {
            btstack_sample_rate_compensation_init( &sample_rate_compensation, receive_time_ms,
                    le_audio_demo_sink_sampling_frequency_hz, FLOAT_TO_Q15(1.f) );
            sink_receive_streaming = true;
        }
        if( sink_receive_streaming ) {
            uint32_t resampling_factor = btstack_sample_rate_compensation_update( &sample_rate_compensation, receive_time_ms,
                    le_audio_demo_sink_num_samples_per_frame, sink->get_samplerate() );
            btstack_resample_set_factor(&resample_instance, resampling_factor);
        }
    }


    if (iso_sdu_length == 0) {
        // empty packet -> generate silence
        memset(pcm, 0, sizeof(pcm));
        uint8_t i;
        for (i = 0 ; i < le_audio_demo_sink_num_channels_per_stream ; i++) {
            have_pcm[stream_index + i] = true;
        }
    } else {
        // regular packet -> decode codec frame
        uint8_t i;
        for (i = 0 ; i < le_audio_demo_sink_num_channels_per_stream ; i++){
            uint8_t tmp_BEC_detect;
            uint8_t BFI = 0;
            uint8_t effective_channel = stream_index + i;
            (void) lc3_decoder->decode_signed_16(decoder_contexts[effective_channel], &packet[offset], BFI,
                                                 &pcm[effective_channel], le_audio_demo_sink_num_channels,
                                                 &tmp_BEC_detect);
            offset += le_audio_demo_sink_octets_per_frame;
            have_pcm[stream_index + i] = true;
        }
    }

    store_samples_in_ringbuffer();

    le_audio_demo_sink_lc3_frames++;

    // PLC
    btstack_run_loop_remove_timer(&next_packet_timer);
    btstack_run_loop_set_timer(&next_packet_timer, plc_timeout_initial_ms);
    btstack_run_loop_set_timer_handler(&next_packet_timer, plc_timeout);
    btstack_run_loop_add_timer(&next_packet_timer);

    if (samples_received >= le_audio_demo_sink_sampling_frequency_hz){
        printf("LC3 Frames: %4u - samples received %5u, played %5u, dropped %5u\n", le_audio_demo_sink_lc3_frames, samples_received, samples_played, samples_dropped);
        samples_received = 0;
        samples_dropped  =  0;
        samples_played = 0;
    }

    stream_last_packet_sequence[stream_index] = packet_sequence_number;
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
    }
    sink_receive_streaming = false;
    // stop timer
    btstack_run_loop_remove_timer(&next_packet_timer);
}
