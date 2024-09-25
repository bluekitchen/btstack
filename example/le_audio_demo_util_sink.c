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

#include "btstack_sample_rate_compensation.h"
#include "btstack_resample.h"
#include "btstack_fsm.h"

#include "hxcmod.h"
#include "mods/mod.h"

#include "btstack_ring_buffer.h"
#ifdef HAVE_POSIX_FILE_IO
#include "wav_util.h"
#endif

#define MAX_CHANNELS 2
#define MAX_SAMPLES_PER_FRAME 480
#define MAX_LC3_FRAME_BYTES   155

// playback
#define MAX_NUM_LC3_FRAMES   (15*2)
#define MAX_BYTES_PER_SAMPLE 4
#define PLAYBACK_BUFFER_SIZE (MAX_NUM_LC3_FRAMES * MAX_SAMPLES_PER_FRAME * MAX_CHANNELS * MAX_BYTES_PER_SAMPLE)
#define PLAYBACK_START_MS (MAX_NUM_LC3_FRAMES * 20 / 3)

// analysis
#define PACKET_PREFIX_LEN 10

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

// statistics
static uint16_t last_packet_sequence[MAX_CHANNELS];
static uint32_t last_packet_time_ms[MAX_CHANNELS];
static uint8_t  last_packet_prefix[MAX_CHANNELS * PACKET_PREFIX_LEN];

// SINK

static enum {
    LE_AUDIO_SINK_IDLE,
    LE_AUDIO_SINK_INIT,
    LE_AUDIO_SINK_CONFIGURED,
} le_audio_demo_util_sink_state = LE_AUDIO_SINK_IDLE;

static const char * le_audio_demo_sink_filename_wav;
static btstack_sample_rate_compensation_t sample_rate_compensation;
static uint32_t le_audio_demo_sink_received_samples;
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
static uint32_t le_audio_demo_sink_lc3_frames;
static uint32_t samples_received;
static uint32_t samples_played;
static uint32_t samples_dropped;

// Audio FSM
#define TRAN( target ) btstack_fsm_transit( &me->super, (btstack_fsm_state_handler_t)target )

typedef struct {
    btstack_fsm_t super;
    uint32_t receive_time_ms;
    uint32_t last_receive_time_ms;
    uint32_t zero_frames;
    uint32_t have_pcm;
    uint32_t received_samples;
} audio_processing_t;

typedef struct {
    btstack_fsm_event_t super;
    uint16_t sequence_number;
    uint16_t size;
    uint32_t receive_time_ms;
    uint8_t *data;
    uint8_t stream;
} data_event_t;

typedef struct {
    btstack_fsm_event_t super;
    uint32_t time_ms;
} time_event_t;

audio_processing_t audio_processing;

enum EventSignals {
    DATA_SIG = BTSTACK_FSM_USER_SIG,
    TIME_SIG
};

#define AUDIO_FSM_DEBUGx
#ifdef AUDIO_FSM_DEBUG
#define ENUM_TO_TEXT(sig) [sig] = #sig
#define audio_fsm_debug(format, ...) \
  printf( format __VA_OPT__(,) __VA_ARGS__)

const char * const sigToString[] = {
        ENUM_TO_TEXT(BTSTACK_FSM_INIT_SIG),
        ENUM_TO_TEXT(BTSTACK_FSM_ENTRY_SIG),
        ENUM_TO_TEXT(BTSTACK_FSM_EXIT_SIG),
        ENUM_TO_TEXT(DATA_SIG),
        ENUM_TO_TEXT(TIME_SIG),
};
#else
#define audio_fsm_debug(...)
#endif

static btstack_fsm_state_t audio_processing_initial( audio_processing_t * const me, btstack_fsm_event_t const * const e );
static btstack_fsm_state_t audio_processing_waiting( audio_processing_t * const me, btstack_fsm_event_t const * const e );
static btstack_fsm_state_t audio_processing_streaming( audio_processing_t * const me, btstack_fsm_event_t const * const e );
static bool audio_processing_is_streaming( audio_processing_t * const me );

static btstack_timer_source_t next_packet_timer;

// lc3 decoder
static bool le_audio_demo_lc3plus_decoder_requested = false;
static const btstack_lc3_decoder_t * lc3_decoder;
static int16_t pcm[MAX_CHANNELS * MAX_SAMPLES_PER_FRAME];

static btstack_lc3_decoder_google_t google_decoder_contexts[MAX_CHANNELS];
#ifdef HAVE_LC3PLUS
static btstack_lc3plus_fraunhofer_decoder_t fraunhofer_decoder_contexts[MAX_CHANNELS];
#endif
static void * decoder_contexts[MAX_CHANNELS];

static void le_audio_connection_sink_playback(int16_t * buffer, uint16_t num_samples){
    // called from lower-layer but guaranteed to be on main thread
    log_info("Playback: need %u, have %" PRIu32 "", num_samples, btstack_ring_buffer_bytes_available(&playback_buffer) / (le_audio_demo_sink_num_channels * 2));

    samples_played += num_samples;

    uint32_t bytes_needed = num_samples * le_audio_demo_sink_num_channels * 2;
    if (playback_active == false){
        if (btstack_ring_buffer_bytes_available(&playback_buffer) >= playback_start_threshold_bytes) {
            log_info("Playback started");
            printf("Playback started\n");
            playback_active = true;
        }
    } else {
        if (bytes_needed > btstack_ring_buffer_bytes_available(&playback_buffer)) {
            if( audio_processing_is_streaming( &audio_processing ) ) {
                log_info("Playback underrun");
                printf("Playback Underrun\n");
            } else {
                log_info("Playback stopped");
                printf("Playback stopped\n");
            }
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
    le_audio_demo_util_sink_state = LE_AUDIO_SINK_CONFIGURED;

    le_audio_demo_sink_num_channels = le_audio_demo_sink_num_streams * le_audio_demo_sink_num_channels_per_stream;
    btstack_assert((le_audio_demo_sink_num_channels == 1) || (le_audio_demo_sink_num_channels == 2));

    le_audio_demo_sink_lc3_frames = 0;

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

    // init playback buffer
    btstack_ring_buffer_init(&playback_buffer, playback_buffer_storage, PLAYBACK_BUFFER_SIZE);

    // calc start threshold in bytes for PLAYBACK_START_MS
    playback_start_threshold_bytes = (sampling_frequency_hz / 1000 * PLAYBACK_START_MS) * le_audio_demo_sink_num_channels * 2;

    // sample rate compensation
    le_audio_demo_sink_received_samples = 0;

    // start playback
    const btstack_audio_sink_t * sink = btstack_audio_sink_get_instance();
    if (sink != NULL){
        btstack_sample_rate_compensation_reset( &sample_rate_compensation, btstack_run_loop_get_time_ms() );
        btstack_resample_init(&resample_instance, le_audio_demo_sink_num_channels);
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

    le_audio_demo_util_sink_configure_general(num_streams, num_channels_per_stream, sampling_frequency_hz, frame_duration, octets_per_frame, iso_interval_1250us);
}

void le_audio_demo_util_sink_count(uint8_t stream_index, uint8_t *packet, uint16_t size) {
    UNUSED(size);
    // check for missing packet
    uint16_t header = little_endian_read_16(packet, 0);
    uint8_t ts_flag = (header >> 14) & 1;

    uint16_t offset = 4;
    uint32_t time_stamp = 0;
    if (ts_flag){
        time_stamp = little_endian_read_32(packet, offset);
        offset += 4;
    }

    UNUSED(time_stamp);
    uint32_t receive_time_ms = btstack_run_loop_get_time_ms();

    uint16_t packet_sequence_number = little_endian_read_16(packet, offset);
    offset += 4;

    uint16_t last_seq_no = last_packet_sequence[stream_index];
    bool packet_missed = (last_seq_no != 0) && ((last_seq_no + 1) != packet_sequence_number);
    if (packet_missed){
        // print last packet
        printf("\n");
        printf("%04x %10"PRIu32" %u ", last_seq_no, last_packet_time_ms[stream_index], stream_index);
        printf_hexdump(&last_packet_prefix[le_audio_demo_sink_num_streams*PACKET_PREFIX_LEN], PACKET_PREFIX_LEN);
        last_seq_no++;

        printf(ANSI_COLOR_RED);
        while (last_seq_no < packet_sequence_number){
            printf("%04x            %u MISSING\n", last_seq_no, stream_index);
            last_seq_no++;
        }
        printf(ANSI_COLOR_RESET);

        // print current packet
        printf("%04x %10"PRIu32" %u ", packet_sequence_number, receive_time_ms, stream_index);
        printf_hexdump(&packet[offset], PACKET_PREFIX_LEN);
    }

    // cache current packet
    memcpy(&last_packet_prefix[le_audio_demo_sink_num_streams*PACKET_PREFIX_LEN], &packet[offset], PACKET_PREFIX_LEN);
}

static btstack_fsm_state_t audio_processing_initial( audio_processing_t * const me, btstack_fsm_event_t const * const e ) {
    UNUSED(e);
    audio_fsm_debug("%s\n", __FUNCTION__ );
    return TRAN(audio_processing_waiting);
}

static btstack_fsm_state_t audio_processing_waiting( audio_processing_t * const me, btstack_fsm_event_t const * const e ) {
    audio_fsm_debug("%s - %s\n", __FUNCTION__, sigToString[e->sig]);
    btstack_fsm_state_t status;
    switch(e->sig) {
        case BTSTACK_FSM_ENTRY_SIG: {
            status = BTSTACK_FSM_HANDLED_STATUS;
            break;
        }
        case BTSTACK_FSM_EXIT_SIG: {
            btstack_ring_buffer_init(&playback_buffer, playback_buffer_storage, PLAYBACK_BUFFER_SIZE);

            btstack_sample_rate_compensation_init(&sample_rate_compensation, me->last_receive_time_ms,
                                                  le_audio_demo_sink_sampling_frequency_hz, FLOAT_TO_Q15(1.f));
            me->zero_frames = 0;
            me->received_samples = 0;
            btstack_resample_init( &resample_instance, le_audio_demo_sink_num_channels );
            me->have_pcm = 0;
            status = BTSTACK_FSM_HANDLED_STATUS;
            break;
        }
        case DATA_SIG: {
            data_event_t *data_event = (data_event_t*)e;
            // nothing to do here
            if( data_event->data == NULL ) {
                status = BTSTACK_FSM_IGNORED_STATUS;
                break;
            }

            // ignore empty data at start
            if( data_event->size == 0 ) {
                status = BTSTACK_FSM_IGNORED_STATUS;
                break;
            }

            // always start at first stream
            if( data_event->stream > 0 ) {
                status = BTSTACK_FSM_IGNORED_STATUS;
                break;
            }

            me->last_receive_time_ms = data_event->receive_time_ms;
            status = TRAN(audio_processing_streaming);
            break;
        }
        default: {
            status = BTSTACK_FSM_IGNORED_STATUS;
            break;
        }
    }
    return status;
}

static void audio_processing_resample( audio_processing_t * const me, data_event_t *e ) {
    // mark current packet as handled
    e->data = NULL;
    if( me->have_pcm != (uint32_t)((1<<(le_audio_demo_sink_num_streams*le_audio_demo_sink_num_channels_per_stream))-1) ) {
        return;
    }

    int16_t *data_in = pcm;
    int16_t *data_out = pcm_resample;
#ifdef HAVE_POSIX_FILE_IO
    // write wav samples
    wav_writer_write_int16(le_audio_demo_sink_num_channels * le_audio_demo_sink_num_samples_per_frame, data_in);
#endif

    // count for samplerate compensation
    me->received_samples += le_audio_demo_sink_num_samples_per_frame;

    // store samples in playback buffer
    samples_received += le_audio_demo_sink_num_samples_per_frame;
    uint32_t resampled_frames = btstack_resample_block(&resample_instance, data_in, le_audio_demo_sink_num_samples_per_frame, data_out);
    uint32_t bytes_to_store = resampled_frames * le_audio_demo_sink_num_channels * 2;

    if (btstack_ring_buffer_bytes_free(&playback_buffer) >= bytes_to_store) {
        btstack_ring_buffer_write(&playback_buffer, (uint8_t *)data_out, bytes_to_store);
        log_info("Samples in playback buffer %5" PRIu32 "", btstack_ring_buffer_bytes_available(&playback_buffer) / (le_audio_demo_sink_num_channels * 2));
    } else {
        printf("Samples dropped\n");
        samples_dropped += le_audio_demo_sink_num_samples_per_frame;
    }
    me->have_pcm = 0;
}

static btstack_fsm_state_t audio_processing_decode( audio_processing_t * const me, btstack_fsm_event_t const * const e ) {
    audio_fsm_debug("%s - %s\n", __FUNCTION__, sigToString[e->sig]);
    btstack_fsm_state_t status;
    switch(e->sig) {
        case BTSTACK_FSM_ENTRY_SIG: {
            btstack_assert( (le_audio_demo_sink_num_streams*le_audio_demo_sink_num_channels_per_stream) < (sizeof(me->have_pcm)*8));
            status = BTSTACK_FSM_HANDLED_STATUS;
            break;
        }
        case BTSTACK_FSM_EXIT_SIG: {
            const btstack_audio_sink_t * sink = btstack_audio_sink_get_instance();
            if( sink == NULL ) {

                status = BTSTACK_FSM_HANDLED_STATUS;
                break;
            }
            uint32_t resampling_factor = btstack_sample_rate_compensation_update( &sample_rate_compensation, me->receive_time_ms,
                                                                                  me->received_samples, sink->get_samplerate() );
            btstack_resample_set_factor(&resample_instance, resampling_factor);
            me->received_samples = 0;

            status = BTSTACK_FSM_HANDLED_STATUS;
            break;
        }
        case DATA_SIG: {
            data_event_t *data_event = (data_event_t*)e;
            uint8_t *data_in = data_event->data;
            int16_t *data_out = pcm;
            uint16_t offset = 0;
            uint8_t BFI = 0;
            if (data_event->size != le_audio_demo_sink_num_channels_per_stream * le_audio_demo_sink_octets_per_frame) {
                // incorrect size. we assume that we received this packet on time but cannot decode it, so we use PLC
                BFI = 1;
                printf("predict audio\n");
            }
            uint8_t i;
            for (i = 0 ; i < le_audio_demo_sink_num_channels_per_stream ; i++){
                uint8_t tmp_BEC_detect;
                uint8_t effective_channel = (data_event->stream * le_audio_demo_sink_num_channels_per_stream) + i;
                (void) lc3_decoder->decode_signed_16(decoder_contexts[effective_channel], &data_in[offset], BFI,
                                                     &data_out[effective_channel], le_audio_demo_sink_num_channels,
                                                     &tmp_BEC_detect);
                offset += le_audio_demo_sink_octets_per_frame;
                audio_fsm_debug("effective_channel: %d\n", effective_channel );
                if( (me->have_pcm & (1<<effective_channel)) ) {
                    audio_fsm_debug("de-syncroniced, resync\n");
                    status = TRAN(audio_processing_waiting);
                    break;
                }
                me->have_pcm |= (1<<effective_channel);
            }
            audio_processing_resample( me, data_event );
            status = TRAN(audio_processing_streaming);
            break;
        }
        default: {
            status = BTSTACK_FSM_IGNORED_STATUS;
            break;
        }
    }
    return status;
}

static btstack_fsm_state_t audio_processing_streaming( audio_processing_t * const me, btstack_fsm_event_t const * const e ) {
    audio_fsm_debug("%s - %s\n", __FUNCTION__, sigToString[e->sig]);

    btstack_fsm_state_t status;
    switch(e->sig) {
        case BTSTACK_FSM_ENTRY_SIG: {
            status = BTSTACK_FSM_HANDLED_STATUS;
            break;
        }
        case BTSTACK_FSM_EXIT_SIG: {
            me->last_receive_time_ms = me->receive_time_ms;
            status = BTSTACK_FSM_HANDLED_STATUS;
            break;
        }
        case TIME_SIG: {
            time_event_t *time = (time_event_t*)e;
            printf("time: %" PRId32 " - %" PRId32 " %" PRId32 "\n", time->time_ms, me->last_receive_time_ms, time->time_ms-me->last_receive_time_ms );
            // we were last called ages ago, so just start waiting again
            if( btstack_time_delta( time->time_ms, me->last_receive_time_ms ) > 100) {
                status = TRAN(audio_processing_waiting);
                break;
            }
            status = BTSTACK_FSM_HANDLED_STATUS;
            break;
        }
        case DATA_SIG: {
            data_event_t *data_event = (data_event_t*)e;
            me->receive_time_ms = data_event->receive_time_ms;

            // done processing this data
            if( data_event->data == NULL ) {
                status = BTSTACK_FSM_HANDLED_STATUS;
                break;
            }

            if( btstack_time_delta( data_event->receive_time_ms, me->last_receive_time_ms ) > 100) {
                status = TRAN(audio_processing_waiting);
                break;
            }

            if( me->zero_frames > 10 ) {
                status = TRAN(audio_processing_waiting);
                break;
            }

            // track consecutive audio frames without data
            if( data_event->size == 0 ) {
                me->zero_frames++;
            } else {
                me->zero_frames = 0;
            }

            // will decode and/or predict missing data
            status = TRAN(audio_processing_decode);
            break;
        }
        default: {
            status = BTSTACK_FSM_IGNORED_STATUS;
            break;
        }
    }
    return status;
}

static void audio_processing_constructor( audio_processing_t *me) {
    btstack_fsm_constructor(&me->super, (btstack_fsm_state_handler_t)&audio_processing_initial);
    btstack_fsm_init(&me->super, NULL);
}

static void audio_processing_task( audio_processing_t *me, btstack_fsm_event_t const *e ) {
    btstack_fsm_dispatch_until(&me->super, e);
}

static bool audio_processing_is_streaming( audio_processing_t *me ) {
    btstack_fsm_t *fsm = &me->super;
    time_event_t const time_event = { { TIME_SIG }, btstack_run_loop_get_time_ms() };
    audio_processing_task( me, &time_event.super );
    return fsm->state == (btstack_fsm_state_handler_t)&audio_processing_streaming;
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
    UNUSED(time_stamp);

    data_event_t const data_event = {
            .super.sig = DATA_SIG,
            .sequence_number = packet_sequence_number,
            .stream = stream_index,
            .data = &packet[offset],
            .size = iso_sdu_length,
            .receive_time_ms = receive_time_ms,
    };

    audio_fsm_debug("new data\n stream_index: %d\n", stream_index);
    audio_processing_task( &audio_processing, &data_event.super );

    le_audio_demo_sink_lc3_frames++;

    if (samples_received >= 10 * le_audio_demo_sink_sampling_frequency_hz){
        printf("LC3 Frames: %4" PRIu32 " - samples received %5" PRIu32 ", played %5" PRIu32 ", dropped %5" PRIu32 "\n", le_audio_demo_sink_lc3_frames, samples_received, samples_played, samples_dropped);
        samples_received = 0;
        samples_dropped  =  0;
        samples_played = 0;
    }
}

void le_audio_demo_util_sink_init(const char * filename_wav){
    le_audio_demo_sink_filename_wav = filename_wav;
    le_audio_demo_util_sink_state = LE_AUDIO_SINK_INIT;
    audio_processing_constructor( &audio_processing );
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
    le_audio_demo_util_sink_state = LE_AUDIO_SINK_INIT;
    sink_receive_streaming = false;
    // stop timer
    btstack_run_loop_remove_timer(&next_packet_timer);
}
