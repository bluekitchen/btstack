/*
 * Copyright (C) 2022 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "le_audio_unicast_sink.c"

/*
 * LE Audio Unicast Sink
 * Until GATT Services are available, we encode LC3 config in advertising
 */


#include "btstack_config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <fcntl.h>        // open
#include <errno.h>

#include "ad_parser.h"
#include "bluetooth_data_types.h"
#include "bluetooth_company_id.h"
#include "bluetooth_gatt.h"
#include "btstack_debug.h"
#include "btstack_audio.h"
#include "btstack_event.h"
#include "btstack_run_loop.h"
#include "btstack_ring_buffer.h"
#include "btstack_stdin.h"
#include "btstack_util.h"
#include "gap.h"
#include "hci.h"
#include "hci_cmd.h"
#include "btstack_lc3.h"
#include "btstack_lc3_google.h"

#include "le_audio_unicast_sink.h"

#ifdef HAVE_POSIX_FILE_IO
#include "wav_util.h"
#include "le-audio/gatt-service/audio_stream_control_service_server.h"
#include "ble/att_server.h"
#include "ble/sm.h"
#include "l2cap.h"
#include "le-audio/le_audio_util.h"
#include "le-audio/gatt-service/published_audio_capabilities_service_server.h"
#include "le-audio/gatt-service/volume_control_service_server.h"

#endif

//#define DEBUG_PLC
#ifdef DEBUG_PLC
#define printf_plc(...) printf(__VA_ARGS__)
#else
#define printf_plc(...)  (void)(0);
#endif

// max config
#define MAX_CHANNELS 2
#define MAX_SAMPLES_PER_FRAME 480

// playback
#define MAX_NUM_LC3_FRAMES   15
#define MAX_BYTES_PER_SAMPLE 4
#define PLAYBACK_BUFFER_SIZE (MAX_NUM_LC3_FRAMES * MAX_SAMPLES_PER_FRAME * MAX_BYTES_PER_SAMPLE)

// analysis
#define PACKET_PREFIX_LEN 10

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

static void show_usage(void);

static const char * filename_wav = "le_audio_unicast_sink.wav";

static enum {
    APP_W4_WORKING,
    APP_W4_CIG_COMPLETE,
    APP_W4_CIS_CREATED,
    APP_STREAMING,
    APP_IDLE,
} app_state = APP_W4_WORKING;

#define APP_AD_FLAGS 0x06
const uint8_t adv_data[] = {
        // Flags general discoverable
        2, BLUETOOTH_DATA_TYPE_FLAGS, APP_AD_FLAGS,
        // Name
        13, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, 'U', 'n', 'i', 'c', 'a', 's', 't', ' ', 'S', 'i', 'n', 'k',
};
const uint8_t adv_data_len = sizeof(adv_data);

static pacs_record_t sink_pac_records[] = {
        // sink_record_0
        {
                // codec ID
                {HCI_AUDIO_CODING_FORMAT_LC3, 0x0000, 0x0000},
                // capabilities
                {
                        0x3E,
                        LE_AUDIO_CODEC_SAMPLING_FREQUENCY_MASK_8000_HZ |
                        LE_AUDIO_CODEC_SAMPLING_FREQUENCY_MASK_16000_HZ |
                        LE_AUDIO_CODEC_SAMPLING_FREQUENCY_MASK_24000_HZ |
                        LE_AUDIO_CODEC_SAMPLING_FREQUENCY_MASK_32000_HZ |
                        LE_AUDIO_CODEC_SAMPLING_FREQUENCY_MASK_44100_HZ |
                        LE_AUDIO_CODEC_SAMPLING_FREQUENCY_MASK_48000_HZ,
                        LE_AUDIO_CODEC_FRAME_DURATION_MASK_7500US | LE_AUDIO_CODEC_FRAME_DURATION_MASK_10000US,
                        LE_AUDIO_CODEC_AUDIO_CHANNEL_COUNT_MASK_1,
                        26,
                        155,
                        2
                },
                // metadata length
                45,
                // metadata
                {
                        // all metadata set
                        0x0FFE,
                        // (2) preferred_audio_contexts_mask
                        LE_AUDIO_CONTEXT_MASK_UNSPECIFIED,
                        // (2) streaming_audio_contexts_mask
                        LE_AUDIO_CONTEXT_MASK_UNSPECIFIED,
                }
        }
};


//
static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_packet_callback_registration_t sm_event_callback_registration;

// remote info
static hci_con_handle_t remote_handle;

// iso info
static uint8_t num_cis;
static hci_con_handle_t cis_con_handles[MAX_CHANNELS];
static bool cis_established[MAX_CHANNELS];
static uint16_t iso_interval_1250us;
static uint8_t  burst_number;
static uint8_t  flush_timeout;

// lc3 codec config
static uint16_t sampling_frequency_hz;
static btstack_lc3_frame_duration_t frame_duration;
static uint16_t number_samples_per_frame;
static uint16_t octets_per_frame;
static uint8_t  num_channels;

// lc3 decoder
static bool request_lc3plus_decoder = false;
static bool use_lc3plus_decoder = false;
static const btstack_lc3_decoder_t * lc3_decoder;
static int16_t pcm[MAX_CHANNELS * MAX_SAMPLES_PER_FRAME];

static btstack_lc3_decoder_google_t google_decoder_contexts[MAX_CHANNELS];
static void * decoder_contexts[MAX_NR_BIS];

// playback - volume in 0..255 to match VCS
static uint8_t  playback_volume = 255;
static uint16_t playback_start_threshold_bytes;
static bool     playback_active;
static uint8_t  playback_buffer_storage[PLAYBACK_BUFFER_SIZE];
static btstack_ring_buffer_t playback_buffer;

// statistics
static bool     last_packet_received[MAX_CHANNELS];
static uint16_t last_packet_sequence[MAX_CHANNELS];
static uint32_t last_packet_time_ms[MAX_CHANNELS];

static uint32_t lc3_frames;
static uint32_t samples_received;
static uint32_t samples_played;
static uint32_t samples_dropped;

// PLC state
static uint16_t               plc_timeout_ms;
static btstack_timer_source_t plc_next_packet_timer;
static uint16_t               plc_cached_iso_sdu_len;

// PACS Server
static pacs_streamendpoint_t sink_node;

// ASCS Server
#define ASCS_NUM_STREAMENDPOINT_CHARACTERISTICS 5
#define ASCS_NUM_CLIENTS 3

static ascs_streamendpoint_characteristic_t ascs_streamendpoint_characteristics[ASCS_NUM_STREAMENDPOINT_CHARACTERISTICS];
static ascs_server_connection_t ascs_clients[ASCS_NUM_CLIENTS];
static btstack_timer_source_t  ascs_server_released_timer;
static le_audio_metadata_t     ascs_server_audio_metadata;

static hci_con_handle_t ascs_server_current_client_con_handle;
static uint8_t ascs_server_current_ase_id;


static void le_audio_connection_sink_playback(int16_t * buffer, uint16_t num_samples){
    // called from lower-layer but guaranteed to be on main thread
    log_info("Playback: need %u, have %u samples", num_samples, btstack_ring_buffer_bytes_available(&playback_buffer) / (num_channels * 2));

    samples_played += num_samples;

    uint32_t bytes_needed = num_samples * num_channels * 2;
    if (playback_active == false){
        if (btstack_ring_buffer_bytes_available(&playback_buffer) >= playback_start_threshold_bytes) {
            log_info("Playback started");
            playback_active = true;
        }
    } else {
        if (bytes_needed > btstack_ring_buffer_bytes_available(&playback_buffer)) {
#ifdef DEBUG_AUDIO_BUFFER
            printf("Playback Underrun\n");
#endif
            log_info("Playback underrun");
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

static void setup_lc3_decoder(void){
    uint8_t channel;
    for (channel = 0 ; channel < num_channels ; channel++){
        // pick decoder
        void * decoder_context = NULL;
        {
            decoder_context = &google_decoder_contexts[channel];
            lc3_decoder = btstack_lc3_decoder_google_init_instance(decoder_context);
        }
        decoder_contexts[channel] = decoder_context;
        printf("LC3: channel #%u, samplerate %u, octets per frame %u\n", channel, sampling_frequency_hz, octets_per_frame);
        lc3_decoder->configure(decoder_context, sampling_frequency_hz, frame_duration, octets_per_frame);
    }
    number_samples_per_frame = btstack_lc3_samples_per_frame(sampling_frequency_hz, frame_duration);
    btstack_assert(number_samples_per_frame <= MAX_SAMPLES_PER_FRAME);
}

static void update_playback_volume(void){
    const btstack_audio_sink_t * sink = btstack_audio_sink_get_instance();
    if (sink != NULL){
        sink->set_volume(playback_volume / 2);
    }
}

static void enter_streaming(void){

    // switch to lc3plus if requested and possible
    use_lc3plus_decoder = request_lc3plus_decoder && (frame_duration == BTSTACK_LC3_FRAME_DURATION_10000US);

    // init decoder
    setup_lc3_decoder();

    printf("Configure: %u channels, sampling rate %u, samples per frame %u, lc3plus %u\n", num_channels, sampling_frequency_hz, number_samples_per_frame, use_lc3plus_decoder);

#ifdef HAVE_POSIX_FILE_IO
    // create wav file
    printf("WAV file: %s\n", filename_wav);
    wav_writer_open(filename_wav, num_channels, sampling_frequency_hz);
#endif

    // init playback buffer
    btstack_ring_buffer_init(&playback_buffer, playback_buffer_storage, PLAYBACK_BUFFER_SIZE);

    // set playback start: FT * ISO Interval + 10 ms
    uint16_t playback_start_ms = flush_timeout * (iso_interval_1250us / 4 * 5) + 10;
    uint16_t playback_start_samples = sampling_frequency_hz / 1000 * playback_start_ms;
    playback_start_threshold_bytes = playback_start_samples * num_channels * 2;
    printf("Playback: start %u ms (%u samples, %u bytes)\n", playback_start_ms, playback_start_samples, playback_start_threshold_bytes);

    // set plc timeout: (FT + 0.5) * ISO Interval
    plc_timeout_ms = flush_timeout * iso_interval_1250us * 15 / 8;
    printf("PLC: timeout %u ms\n", plc_timeout_ms);

    // start playback
    const btstack_audio_sink_t * sink = btstack_audio_sink_get_instance();
    if (sink != NULL){
        sink->init(num_channels, sampling_frequency_hz, le_audio_connection_sink_playback);
        sink->start_stream();
        update_playback_volume();
    }

    // reset PLC
    memset(last_packet_received, 0, sizeof(last_packet_received));
    memset(last_packet_sequence, 0, sizeof(last_packet_sequence));
}

static void stop_streaming(void){
    // stop PLC timer
    btstack_run_loop_remove_timer(&plc_next_packet_timer);

    // stop playback
    const btstack_audio_sink_t * sink = btstack_audio_sink_get_instance();
    if (sink != NULL){
        sink->stop_stream();
    }

    // close files
#ifdef HAVE_POSIX_FILE_IO
    printf("Close files\n");
    wav_writer_close();
#endif
}

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    bd_addr_t event_addr;
    hci_con_handle_t cis_handle;
    hci_con_handle_t acl_handle;
    unsigned int i;
    if (packet_type != HCI_EVENT_PACKET) return;
    switch (packet[0]) {
        case BTSTACK_EVENT_STATE:
            switch(btstack_event_state_get_state(packet)) {
                case HCI_STATE_WORKING:
                    printf("Ready, press 'b', 'l' or 'r' to configure as STEREO/LEFT/RIGHT speaker and start advertising\n");
                    break;
                default:
                    break;
            }
            break;
        case SM_EVENT_JUST_WORKS_REQUEST:
            printf("Just Works requested\n");
            sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
            break;
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            if (hci_event_disconnection_complete_get_connection_handle(packet) == remote_handle){
                printf("Disconnected, back to scanning\n");
                stop_streaming();
            }
            break;
        case HCI_EVENT_LE_META:
            switch(hci_event_le_meta_get_subevent_code(packet)) {
                case HCI_SUBEVENT_LE_CIS_REQUEST:
                    cis_con_handles[0] = hci_subevent_le_cis_request_get_cis_connection_handle(packet);
                    gap_cis_accept(cis_con_handles[0]);
                    break;
                default:
                    break;
            }
            break;
        case HCI_EVENT_META_GAP:
            switch (hci_event_gap_meta_get_subevent_code(packet)){
                case GAP_SUBEVENT_LE_CONNECTION_COMPLETE:
                    gap_subevent_le_connection_complete_get_peer_address(packet, event_addr);
                    remote_handle = gap_subevent_le_connection_complete_get_connection_handle(packet);
                    printf("Connected, remote %s, handle %04x\n", bd_addr_to_str(event_addr), remote_handle);
                    break;
                case GAP_SUBEVENT_CIS_CREATED:
                    // only look for cis handle
                    cis_handle = gap_subevent_cis_created_get_cis_con_handle(packet);
                    acl_handle = gap_subevent_cis_created_get_acl_con_handle(packet);
                    for (i=0; i < num_cis; i++){
                        if (cis_handle == cis_con_handles[i]){
                            cis_established[i] = true;
                            // cache cis info
                            iso_interval_1250us = gap_subevent_cis_created_get_iso_interval_1250us(packet);
                            burst_number        = gap_subevent_cis_created_get_burst_number_c_to_p(packet);
                            flush_timeout       = gap_subevent_cis_created_get_flush_timeout_c_to_p(packet);
                        }
                    }
                    // check for complete
                    bool complete = true;
                    for (i=0; i < num_cis; i++) {
                        complete &= cis_established[i];
                    }
                    if (complete) {
                        printf("All CIS Established\n");
                        printf("- ISO Interval:  %u\n", iso_interval_1250us * 1250);
                        printf("- Burst NUmber:  %u\n", burst_number);
                        printf("- Flush Timeout: %u\n", flush_timeout);

                        enter_streaming();
                        audio_stream_control_service_server_streamendpoint_receiver_start_ready(acl_handle,
                                                                                                ascs_server_current_ase_id);
                        app_state = APP_STREAMING;
                    }
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

static void store_samples_in_ringbuffer(void){
#ifdef HAVE_POSIX_FILE_IO
    // write wav samples
    wav_writer_write_int16(num_channels * number_samples_per_frame, pcm);
#endif
    // store samples in playback buffer
    uint32_t bytes_to_store = num_channels * number_samples_per_frame * 2;
    samples_received += number_samples_per_frame;
    uint32_t bytes_free = btstack_ring_buffer_bytes_free(&playback_buffer);
    if (bytes_to_store <= bytes_free) {
        btstack_ring_buffer_write(&playback_buffer, (uint8_t *) pcm, bytes_to_store);
    } else {
#ifdef DEBUG_AUDIO_BUFFER
        printf("Samples dropped\n");
#endif
        log_info("Samples dropped: to store %u, free %u", bytes_to_store, bytes_free);
        samples_dropped += number_samples_per_frame;
    }
}

static void plc_do(void) {
    // inject packet
    uint8_t tmp_BEC_detect;
    uint8_t BFI = 1;
    uint8_t channel;
    for (channel = 0; channel < num_channels; channel++){
        (void) lc3_decoder->decode_signed_16(decoder_contexts[channel], NULL, BFI,
                                             &pcm[channel], num_channels,
                                             &tmp_BEC_detect);
    }

    store_samples_in_ringbuffer();
}

static void plc_timeout(btstack_timer_source_t * timer) {
    // set timer again
    btstack_run_loop_set_timer(&plc_next_packet_timer, plc_timeout_ms);
    btstack_run_loop_set_timer_handler(&plc_next_packet_timer, plc_timeout);
    btstack_run_loop_add_timer(&plc_next_packet_timer);
    printf_plc("- ISO, PLC for timeout\n");
    plc_do();
}

static void iso_packet_handler(uint8_t packet_type, uint16_t a_channel, uint8_t *packet, uint16_t size){

    uint16_t header = little_endian_read_16(packet, 0);
    hci_con_handle_t con_handle = header & 0x0fff;
    // uint8_t pb_flag = (header >> 12) & 3;
    uint8_t ts_flag = (header >> 14) & 1;
    // uint16_t iso_load_len = little_endian_read_16(packet, 2);

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
    // uint8_t packet_status_flag = (uint8_t) (header_2 >> 14);
    offset += 2;

    if (iso_sdu_length == 0) return;

    if (iso_sdu_length != num_channels * octets_per_frame) {
        printf("ISO Length %u != %u * %u\n", iso_sdu_length, num_channels, octets_per_frame);
    }

    // infer channel from con handle - only works for up to 2 channels
    uint8_t cis_channel = (con_handle == cis_con_handles[0]) ? 0 : 1;

    if (last_packet_received[cis_channel]) {
        printf_plc("ISO, receive %u\n", packet_sequence_number);

        int16_t packet_sequence_delta = btstack_time16_delta(packet_sequence_number,
                                                             last_packet_sequence[cis_channel]);
        if (packet_sequence_delta < 1) {
            // drop delayed packet that had already been generated by PLC
            printf_plc("- dropping delayed packet. Current sequence number %u, last received or generated by PLC: %u\n",
                       packet_sequence_number, last_packet_sequence[cis_channel]);
            return;
        }
        // simple check
        if (packet_sequence_number != last_packet_sequence[cis_channel] + 1) {
            printf_plc("- BIS #%u, missing %u\n", cis_channel, last_packet_sequence[cis_channel] + 1);
        }
    } else {
        printf_plc("CIS %u, first packet seq number %u\n", cis_channel, packet_sequence_number);
        last_packet_received[cis_channel] = true;
        last_packet_sequence[cis_channel] = packet_sequence_number - 1;
    }

    // PLC for missing packets
    while (true) {
        uint16_t expected = last_packet_sequence[cis_channel]+1;
        if (expected == packet_sequence_number){
            break;
        }
        printf_plc("- ISO, PLC for %u\n", expected);
        plc_do();
        last_packet_sequence[cis_channel] = expected;
    }

    uint8_t channel;
    for (channel = 0 ; channel < num_channels ; channel++){

        // decode codec frame
        uint8_t tmp_BEC_detect;
        uint8_t BFI = 0;
        (void) lc3_decoder->decode_signed_16(decoder_contexts[channel], &packet[offset], BFI,
                                   &pcm[channel], num_channels,
                                   &tmp_BEC_detect);
        offset += octets_per_frame;
    }

    store_samples_in_ringbuffer();

    log_info("Samples in playback buffer %5u", btstack_ring_buffer_bytes_available(&playback_buffer) / (num_channels * 2));

    lc3_frames++;

    // PLC
    plc_cached_iso_sdu_len = iso_sdu_length;
    btstack_run_loop_remove_timer(&plc_next_packet_timer);
    btstack_run_loop_set_timer(&plc_next_packet_timer, plc_timeout_ms);
    btstack_run_loop_set_timer_handler(&plc_next_packet_timer, plc_timeout);
    btstack_run_loop_add_timer(&plc_next_packet_timer);

    if (samples_received >= sampling_frequency_hz){
        printf("LC3 Frames: %4u - samples received %5u, played %5u, dropped %5u\n", lc3_frames, samples_received, samples_played, samples_dropped);
        samples_received = 0;
        samples_dropped  =  0;
        samples_played = 0;
    }

    last_packet_time_ms[cis_channel]  = receive_time_ms;
    last_packet_sequence[cis_channel] = packet_sequence_number;
}

// PACS Server Handler
static void pacs_server_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_GATTSERVICE_META) return;

    switch (hci_event_gattservice_meta_get_subevent_code(packet)){
        case GATTSERVICE_SUBEVENT_PACS_SERVER_AUDIO_LOCATIONS:
            printf("PACS: audio location received\n");
            break;

        default:
            break;
    }
}

// VCS Server Handler
static void vcs_server_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_GATTSERVICE_META) return;

    switch (hci_event_gattservice_meta_get_subevent_code(packet)){
        case GATTSERVICE_SUBEVENT_VCS_SERVER_VOLUME_STATE:
            playback_volume = gattservice_subevent_vcs_server_volume_state_get_volume_setting(packet);
            update_playback_volume();
            printf("VCS Server: set volume %3u\n", playback_volume);
            break;
        default:
            break;
    }
}

static void ascs_server_released_timer_handler(btstack_timer_source_t * ts){
    UNUSED(ts);
    printf("ASCS Server Released triggered by timer\n");
    audio_stream_control_service_server_streamendpoint_released(ascs_server_current_client_con_handle, ascs_server_current_ase_id, true);
}

static void ascs_server_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_GATTSERVICE_META) return;
    hci_con_handle_t con_handle;
    uint8_t status;

    ascs_codec_configuration_t codec_configuration;
    ascs_qos_configuration_t   qos_configuration;
    uint8_t ase_id;

    switch (hci_event_gattservice_meta_get_subevent_code(packet)){
        case GATTSERVICE_SUBEVENT_ASCS_SERVER_CONNECTED:
            con_handle = gattservice_subevent_ascs_server_connected_get_con_handle(packet);
            status =     gattservice_subevent_ascs_server_connected_get_status(packet);
            printf("ASCS Server: connected, con_handle 0x%04x\n", con_handle);
            break;
        case GATTSERVICE_SUBEVENT_ASCS_SERVER_DISCONNECTED:
            con_handle = gattservice_subevent_ascs_server_disconnected_get_con_handle(packet);
            printf("ASCS Server: disconnected, con_handle 0x%04xn\n", con_handle);
            break;
        case GATTSERVICE_SUBEVENT_ASCS_SERVER_CODEC_CONFIGURATION:
            ase_id = gattservice_subevent_ascs_server_codec_configuration_get_ase_id(packet);
            con_handle = gattservice_subevent_ascs_server_codec_configuration_get_con_handle(packet);

            // use framing for 441
            codec_configuration.framing = gattservice_subevent_ascs_server_codec_configuration_get_sampling_frequency_index(packet) == LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_44100_HZ ? 1 : 0;

            codec_configuration.preferred_phy = LE_AUDIO_SERVER_PHY_MASK_NO_PREFERENCE;
            codec_configuration.preferred_retransmission_number = 0;
            codec_configuration.max_transport_latency_ms = 0x0FA0;
            codec_configuration.presentation_delay_min_us = 0x0000;
            codec_configuration.presentation_delay_max_us = 0xFFAA;
            codec_configuration.preferred_presentation_delay_min_us = 0;
            codec_configuration.preferred_presentation_delay_max_us = 0;

            // codec id:
            codec_configuration.coding_format =  gattservice_subevent_ascs_server_codec_configuration_get_coding_format(packet);;
            codec_configuration.company_id = gattservice_subevent_ascs_server_codec_configuration_get_company_id(packet);
            codec_configuration.vendor_specific_codec_id = gattservice_subevent_ascs_server_codec_configuration_get_vendor_specific_codec_id(packet);

            codec_configuration.specific_codec_configuration.codec_configuration_mask = gattservice_subevent_ascs_server_codec_configuration_get_specific_codec_configuration_mask(packet);
            codec_configuration.specific_codec_configuration.sampling_frequency_index = gattservice_subevent_ascs_server_codec_configuration_get_sampling_frequency_index(packet);
            codec_configuration.specific_codec_configuration.frame_duration_index = gattservice_subevent_ascs_server_codec_configuration_get_frame_duration_index(packet);
            codec_configuration.specific_codec_configuration.audio_channel_allocation_mask = gattservice_subevent_ascs_server_codec_configuration_get_audio_channel_allocation_mask(packet);
            codec_configuration.specific_codec_configuration.octets_per_codec_frame = gattservice_subevent_ascs_server_codec_configuration_get_octets_per_frame(packet);
            codec_configuration.specific_codec_configuration.codec_frame_blocks_per_sdu = gattservice_subevent_ascs_server_codec_configuration_get_frame_blocks_per_sdu(packet);

            // store in existing state
            num_cis = 1;
            num_channels = 1;
            octets_per_frame = codec_configuration.specific_codec_configuration.octets_per_codec_frame;
            sampling_frequency_hz = le_audio_get_sampling_frequency_hz(codec_configuration.specific_codec_configuration.sampling_frequency_index );
            frame_duration = le_audio_util_get_btstack_lc3_frame_duration(codec_configuration.specific_codec_configuration.frame_duration_index);

            printf("ASCS: CODEC_CONFIGURATION_RECEIVED ase_id %d, codec_configuration_mask %02x, con_handle 0x%04x\n", ase_id,
                   codec_configuration.specific_codec_configuration.codec_configuration_mask, con_handle);
            if (codec_configuration.specific_codec_configuration.codec_configuration_mask & (1 <<LE_AUDIO_CODEC_CONFIGURATION_TYPE_AUDIO_CHANNEL_ALLOCATION)){
                printf("- channel allocation: 0x%02x\n", codec_configuration.specific_codec_configuration.audio_channel_allocation_mask);
                if (codec_configuration.specific_codec_configuration.audio_channel_allocation_mask == 3){
                    num_channels = 2;
                }
            }
            if (codec_configuration.specific_codec_configuration.codec_configuration_mask & (1 <<LE_AUDIO_CODEC_CONFIGURATION_TYPE_OCTETS_PER_CODEC_FRAME)){
                printf("- octets per codec frame: %3u\n", codec_configuration.specific_codec_configuration.octets_per_codec_frame);
            }
            audio_stream_control_service_server_streamendpoint_configure_codec(con_handle, ase_id, &codec_configuration);
            break;
        case GATTSERVICE_SUBEVENT_ASCS_SERVER_QOS_CONFIGURATION:
            ase_id = gattservice_subevent_ascs_server_qos_configuration_get_ase_id(packet);
            con_handle = gattservice_subevent_ascs_server_qos_configuration_get_con_handle(packet);

            qos_configuration.cig_id = gattservice_subevent_ascs_server_qos_configuration_get_cig_id(packet);
            qos_configuration.cis_id = gattservice_subevent_ascs_server_qos_configuration_get_cis_id(packet);
            qos_configuration.sdu_interval = gattservice_subevent_ascs_server_qos_configuration_get_sdu_interval(packet);
            qos_configuration.framing = gattservice_subevent_ascs_server_qos_configuration_get_framing(packet);
            qos_configuration.phy = gattservice_subevent_ascs_server_qos_configuration_get_phy(packet);
            qos_configuration.max_sdu = gattservice_subevent_ascs_server_qos_configuration_get_max_sdu(packet);
            qos_configuration.retransmission_number = gattservice_subevent_ascs_server_qos_configuration_get_retransmission_number(packet);
            qos_configuration.max_transport_latency_ms = gattservice_subevent_ascs_server_qos_configuration_get_max_transport_latency(packet);
            qos_configuration.presentation_delay_us = gattservice_subevent_ascs_server_qos_configuration_get_presentation_delay_us(packet);

            printf("ASCS: QOS_CONFIGURATION_RECEIVED ase_id %d\n", ase_id);
            audio_stream_control_service_server_streamendpoint_configure_qos(con_handle, ase_id, &qos_configuration);
            break;
        case GATTSERVICE_SUBEVENT_ASCS_SERVER_ENABLE:
            ascs_server_current_ase_id = gattservice_subevent_ascs_server_disable_get_ase_id(packet);
            con_handle = gattservice_subevent_ascs_server_disable_get_con_handle(packet);
            printf("ASCS: ENABLE ase_id %d\n", ascs_server_current_ase_id);

            audio_stream_control_service_server_streamendpoint_enable(con_handle, ascs_server_current_ase_id);
            break;
        case GATTSERVICE_SUBEVENT_ASCS_SERVER_METADATA:
            con_handle = gattservice_subevent_ascs_server_metadata_get_con_handle(packet);
            ase_id = gattservice_subevent_ascs_server_metadata_get_ase_id(packet);
            printf("ASCS: METADATA_RECEIVED ase_id %d\n", ase_id);
            audio_stream_control_service_server_streamendpoint_metadata_update(con_handle, ase_id, &ascs_server_audio_metadata);
            break;
        case GATTSERVICE_SUBEVENT_ASCS_SERVER_START_READY:
            ase_id = gattservice_subevent_ascs_server_start_ready_get_ase_id(packet);
            con_handle = gattservice_subevent_ascs_server_start_ready_get_con_handle(packet);
            printf("ASCS: START_READY ase_id %d\n", ase_id);
            audio_stream_control_service_server_streamendpoint_receiver_start_ready(con_handle, ase_id);
            break;
        case GATTSERVICE_SUBEVENT_ASCS_SERVER_STOP_READY:
            ase_id = gattservice_subevent_ascs_server_stop_ready_get_ase_id(packet);
            con_handle = gattservice_subevent_ascs_server_stop_ready_get_con_handle(packet);
            printf("ASCS: STOP_READY ase_id %d\n", ase_id);
            audio_stream_control_service_server_streamendpoint_receiver_stop_ready(con_handle, ase_id);
            break;
        case GATTSERVICE_SUBEVENT_ASCS_SERVER_DISABLE:
            ase_id = gattservice_subevent_ascs_server_disable_get_ase_id(packet);
            con_handle = gattservice_subevent_ascs_server_disable_get_con_handle(packet);
            printf("ASCS: DISABLING ase_id %d\n", ase_id);
            audio_stream_control_service_server_streamendpoint_disable(con_handle, ase_id);
            break;
        case GATTSERVICE_SUBEVENT_ASCS_SERVER_RELEASE:
            ascs_server_current_ase_id            = gattservice_subevent_ascs_server_release_get_ase_id(packet);
            ascs_server_current_client_con_handle = gattservice_subevent_ascs_server_release_get_con_handle(packet);
            printf("ASCS: RELEASE ase_id %d\n", ascs_server_current_ase_id);
            audio_stream_control_service_server_streamendpoint_release(ascs_server_current_client_con_handle, ascs_server_current_ase_id);

            stop_streaming();

            // Client request: Release. Accept (enter Releasing State), and trigger Releasing
            // TODO: find better approach
            btstack_run_loop_remove_timer(&ascs_server_released_timer);
            btstack_run_loop_set_timer_handler(&ascs_server_released_timer, &ascs_server_released_timer_handler);
            btstack_run_loop_set_timer(&ascs_server_released_timer, 100);
            btstack_run_loop_add_timer(&ascs_server_released_timer);
            // stop playback
            playback_active = false;
            break;
        case GATTSERVICE_SUBEVENT_ASCS_SERVER_RELEASED:
            ase_id = gattservice_subevent_ascs_server_released_get_ase_id(packet);
            con_handle = gattservice_subevent_ascs_server_released_get_con_handle(packet);
            printf("ASCS: RELEASED ase_id %d\n", ase_id);
            audio_stream_control_service_server_streamendpoint_released(con_handle, ase_id, true);
            break;
        default:
            break;
    }
}

void setup_sink_and_start_advertising(uint8_t audio_location_mask) {
    // PACS Server
    sink_node.records_num = 1;
    sink_node.records = &sink_pac_records[0];
    sink_node.audio_locations_mask = audio_location_mask;
    sink_node.available_audio_contexts_mask = LE_AUDIO_CONTEXT_MASK_UNSPECIFIED;
    sink_node.supported_audio_contexts_mask = LE_AUDIO_CONTEXT_MASK_UNSPECIFIED;
    uint8_t channel_count_mask = LE_AUDIO_CODEC_AUDIO_CHANNEL_COUNT_MASK_1;
    if (audio_location_mask == (LE_AUDIO_LOCATION_MASK_FRONT_LEFT | LE_AUDIO_LOCATION_MASK_FRONT_RIGHT)){
        channel_count_mask = LE_AUDIO_CODEC_AUDIO_CHANNEL_COUNT_MASK_2;
    }
    sink_pac_records[0].codec_capability.supported_audio_channel_counts_mask = channel_count_mask;
    published_audio_capabilities_service_server_init(&sink_node, NULL);
    published_audio_capabilities_service_server_register_packet_handler(&pacs_server_packet_handler);

    // setup advertisements
    uint16_t adv_int_min = 0x0030;
    uint16_t adv_int_max = 0x0030;
    uint8_t adv_type = 0;
    bd_addr_t null_addr;
    memset(null_addr, 0, 6);
    gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);
    gap_advertisements_set_data(adv_data_len, (uint8_t*) adv_data);
    gap_advertisements_enable(1);
    printf("Start Advertising, waiting for connection\n");
}

static void show_usage(void){
    printf("\n--- LE Audio Unicast Sink Test Console ---\n");
    printf(" l - configure as LEFT speaker and start advertising\n");
    printf(" r - configure as RIGHT speaker and start advertising\n");
    printf(" b - configure as STEREO speaker and start advertising\n");
    printf(" [ - volume down\n");
    printf(" ] - volume up\n");
    printf("---\n");
}

static void stdin_process(char c){
    uint8_t volume_step = 10;
    switch (c){
        case 'b':
            printf("Configured as STEREO speaker\n");
            sink_pac_records[0].codec_capability.supported_audio_channel_counts_mask =
                    LE_AUDIO_CODEC_AUDIO_CHANNEL_COUNT_MASK_2;
            setup_sink_and_start_advertising(LE_AUDIO_LOCATION_MASK_FRONT_LEFT | LE_AUDIO_LOCATION_MASK_FRONT_RIGHT);
            break;
        case 'l':
            printf("Configured as LEFT speaker\n");
            sink_pac_records[0].codec_capability.supported_audio_channel_counts_mask =
                    LE_AUDIO_CODEC_AUDIO_CHANNEL_COUNT_MASK_1;
            setup_sink_and_start_advertising(LE_AUDIO_LOCATION_MASK_FRONT_LEFT);
            break;
        case 'r':
            printf("Configured as RIGHT speaker\n");
            sink_pac_records[0].codec_capability.supported_audio_channel_counts_mask =
                    LE_AUDIO_CODEC_AUDIO_CHANNEL_COUNT_MASK_1;
            setup_sink_and_start_advertising(LE_AUDIO_LOCATION_MASK_FRONT_RIGHT);
            break;
        case '[':
            if (playback_volume > volume_step){
                playback_volume -= volume_step;
            } else {
                playback_volume = 0;
            }
            volume_control_service_server_set_volume_state(playback_volume, VCS_MUTE_OFF);
            update_playback_volume();
            break;
        case ']':
            if (playback_volume < (255 - volume_step)){
                playback_volume += volume_step;
            } else {
                playback_volume = 255;
            }
            volume_control_service_server_set_volume_state(playback_volume, VCS_MUTE_OFF);
            if (btstack_audio_sink_get_instance() != NULL){
                btstack_audio_sink_get_instance()->set_volume(playback_volume / 2);
            }
            update_playback_volume();
            break;
        case '\n':
        case '\r':
            break;
        default:
            show_usage();
            break;
    }
}

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    (void) argv;
    (void) argc;

    // setup l2cap
    l2cap_init();

    // LE Secure Connections, Just Works
    sm_init();
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    sm_set_authentication_requirements(SM_AUTHREQ_SECURE_CONNECTION | SM_AUTHREQ_BONDING);

    // setup ATT server
    att_server_init(profile_data, NULL, NULL);

    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // register for ISO Packet
    hci_register_iso_packet_handler(&iso_packet_handler);

    // register for SM events
    sm_event_callback_registration.callback = &packet_handler;
    sm_add_event_handler(&sm_event_callback_registration);

    // ASCS Server
    audio_stream_control_service_server_init(ASCS_NUM_STREAMENDPOINT_CHARACTERISTICS, ascs_streamendpoint_characteristics, ASCS_NUM_CLIENTS, ascs_clients);
    audio_stream_control_service_server_register_packet_handler(&ascs_server_packet_handler);

    // VCS Server
    volume_control_service_server_init(255, VCS_MUTE_OFF, 0, NULL, 0, NULL);
    volume_control_service_server_register_packet_handler(vcs_server_packet_handler);

    // turn on!
    hci_power_control(HCI_POWER_ON);

    btstack_stdin_setup(stdin_process);
    return 0;
}
