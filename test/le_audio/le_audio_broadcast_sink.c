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
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
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

#define BTSTACK_FILE__ "le_audio_broadcast_sink.c"

/*
 * LE Audio Broadcast Sink
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
#include "btstack_lc3_ehima.h"
#include "wav_util.h"

// max config
#define MAX_NUM_BIS 2
#define MAX_SAMPLES_PER_FRAME 480

#define DUMP_LEN_LC3_FRAMES 1000

// playback
#define MAX_NUM_LC3_FRAMES   5
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

static const char * filename_lc3 = "le_audio_broadcast_sink.lc3";
static const char * filename_wav = "le_audio_broadcast_sink.wav";

static enum {
    APP_W4_WORKING,
    APP_W4_BROADCAST_ADV,
    APP_W4_PA_AND_BIG_INFO,
    APP_CREATE_BIG_SYNC,
    APP_W4_BIG_SYNC_ESTABLISHED,
    APP_SET_ISO_PATHS,
    APP_STREAMING,
    APP_TERMINATE_BIG,
    APP_IDLE
} app_state = APP_W4_WORKING;

//
static btstack_packet_callback_registration_t hci_event_callback_registration;

static bool have_base;
static bool have_big_info;

uint32_t last_samples_report_ms;
uint32_t samples_received;
uint32_t samples_dropped;
uint16_t frames_per_second[MAX_NUM_BIS];

// remote info
static char remote_name[20];
static bd_addr_t remote;
static bd_addr_type_t remote_type;
static uint8_t remote_sid;
static bool count_mode;
static bool pts_mode;

// broadcast info
static const uint8_t    big_handle = 1;
static hci_con_handle_t sync_handle;
static hci_con_handle_t bis_con_handles[MAX_NUM_BIS];
static unsigned int     next_bis_index;

// analysis
static uint16_t last_packet_sequence[MAX_NUM_BIS];
static uint32_t last_packet_time_ms[MAX_NUM_BIS];
static uint8_t last_packet_prefix[MAX_NUM_BIS * PACKET_PREFIX_LEN];

// lc3 writer
static int dump_file;
static uint32_t lc3_frames;

// lc3 codec config
static uint32_t sampling_frequency_hz;
static btstack_lc3_frame_duration_t frame_duration;
static uint16_t number_samples_per_frame;
static uint16_t octets_per_frame;
static uint8_t  num_bis;

// lc3 decoder
static const btstack_lc3_decoder_t * lc3_decoder;
static lc3_decoder_ehima_t decoder_contexts[MAX_NUM_BIS];
static int16_t pcm[MAX_NUM_BIS * MAX_SAMPLES_PER_FRAME];

// playback
static uint8_t playback_buffer_storage[PLAYBACK_BUFFER_SIZE];
static btstack_ring_buffer_t playback_buffer;

static void le_audio_broadcast_sink_playback(int16_t * buffer, uint16_t num_samples){
    // called from lower-layer but guaranteed to be on main thread
    uint32_t bytes_needed = num_samples * num_bis * 2;

    static bool underrun = true;

    log_info("Playback: need %u, have %u", num_samples, btstack_ring_buffer_bytes_available(&playback_buffer) / ( num_bis * 2));

    if (bytes_needed > btstack_ring_buffer_bytes_available(&playback_buffer)){
        memset(buffer, 0, bytes_needed);
        if (underrun == false){
            log_info("Playback underrun");
            underrun = true;
        }
        return;
    }

    if (underrun){
        underrun = false;
        log_info("Playback started");
    }
    uint32_t bytes_read;
    btstack_ring_buffer_read(&playback_buffer, (uint8_t *) buffer, bytes_needed, &bytes_read);
    btstack_assert(bytes_read == bytes_needed);
}

static void open_lc3_file(void) {
    // open lc3 file
    int oflags = O_WRONLY | O_CREAT | O_TRUNC;
    dump_file = open(filename_lc3, oflags, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (dump_file < 0) {
        printf("failed to open file %s, errno = %d\n", filename_lc3, errno);
        return;
    }

    printf("LC3 binary file: %s\n", filename_lc3);

    // calc bps
    uint16_t frame_duration_100us = (frame_duration == BTSTACK_LC3_FRAME_DURATION_7500US) ? 75 : 100;
    uint32_t bits_per_second = (uint32_t) octets_per_frame * num_bis * 8 * 10000 / frame_duration_100us;

    // write header for floating point implementation
    uint8_t header[18];
    little_endian_store_16(header, 0, 0xcc1c);
    little_endian_store_16(header, 2, sizeof(header));
    little_endian_store_16(header, 4, sampling_frequency_hz / 100);
    little_endian_store_16(header, 6, bits_per_second / 100);
    little_endian_store_16(header, 8, num_bis);
    little_endian_store_16(header, 10, frame_duration_100us * 10);
    little_endian_store_16(header, 12, 0);
    little_endian_store_32(header, 14, DUMP_LEN_LC3_FRAMES * number_samples_per_frame);
    write(dump_file, header, sizeof(header));
}

static void setup_lc3_decoder(void){
    uint8_t channel;
    for (channel = 0 ; channel < num_bis ; channel++){
        lc3_decoder_ehima_t * decoder_context = &decoder_contexts[channel];
        lc3_decoder = lc3_decoder_ehima_init_instance(decoder_context);
        lc3_decoder->configure(decoder_context, sampling_frequency_hz, frame_duration);
    }
    number_samples_per_frame = lc3_decoder->get_number_samples_per_frame(&decoder_contexts[0]);
    btstack_assert(number_samples_per_frame <= MAX_SAMPLES_PER_FRAME);
}

static void close_files(void){
    printf("Close files\n");
    close(dump_file);
    wav_writer_close();
}

static void handle_periodic_advertisement(const uint8_t * packet, uint16_t size){
    // periodic advertisement contains the BASE
    // TODO: BASE might be split across multiple advertisements
    const uint8_t * adv_data = hci_subevent_le_periodic_advertising_report_get_data(packet);
    uint16_t adv_size = hci_subevent_le_periodic_advertising_report_get_data_length(packet);
    uint8_t adv_status = hci_subevent_le_periodic_advertising_report_get_data_status(packet);

    if (adv_status != 0) {
        printf("Periodic Advertisement (status %u): ", adv_status);
        printf_hexdump(adv_data, adv_size);
        return;
    }

    ad_context_t context;
    for (ad_iterator_init(&context, adv_size, adv_data) ; ad_iterator_has_more(&context) ; ad_iterator_next(&context)) {
        uint8_t data_type = ad_iterator_get_data_type(&context);
        uint8_t data_size = ad_iterator_get_data_len(&context);
        const uint8_t * data = ad_iterator_get_data(&context);
        uint16_t uuid;
        switch (data_type){
            case BLUETOOTH_DATA_TYPE_SERVICE_DATA_16_BIT_UUID:
                uuid = little_endian_read_16(data, 0);
                if (uuid == ORG_BLUETOOTH_SERVICE_BASIC_AUDIO_ANNOUNCEMENT_SERVICE){
                    have_base = true;
                    // Level 1: Group Level
                    const uint8_t * base_data = &data[2];
                    uint16_t base_len = data_size - 2;
                    printf("BASE:\n");
                    uint32_t presentation_delay = little_endian_read_24(base_data, 0);
                    printf("- presentation delay: %"PRIu32" us\n", presentation_delay);
                    uint8_t num_subgroups = base_data[3];
                    printf("- num subgroups: %u\n", num_subgroups);
                    uint8_t i;
                    uint16_t offset = 4;
                    for (i=0;i<num_subgroups;i++){
                        // Level 2: Subgroup Level
                        num_bis = base_data[offset++];
                        printf("  - num bis[%u]: %u\n", i, num_bis);
                        // codec_id: coding format = 0x06, vendor and coded id = 0
                        offset += 5;
                        uint8_t codec_specific_configuration_length = base_data[offset++];
                        const uint8_t * codec_specific_configuration = &base_data[offset];
                        printf("  - codec specific config[%u]: ", i);
                        printf_hexdump(codec_specific_configuration, codec_specific_configuration_length);
                        // parse config to get sampling frequency and frame duration
                        uint8_t codec_offset = 0;
                        while ((codec_offset + 1) < codec_specific_configuration_length){
                            uint8_t ltv_len = codec_specific_configuration[codec_offset++];
                            uint8_t ltv_type = codec_specific_configuration[codec_offset];
                            const uint32_t sampling_frequency_map[] = { 8000, 11025, 16000, 22050, 24000, 32000, 44100, 48000, 88200, 96000, 176400, 192000, 384000 };
                            uint8_t sampling_frequency_index;
                            uint8_t frame_duration_index;
                            switch (ltv_type){
                                case 0x01: // sampling frequency
                                    sampling_frequency_index = codec_specific_configuration[codec_offset+1];
                                    // TODO: check range
                                    sampling_frequency_hz = sampling_frequency_map[sampling_frequency_index - 1];
                                    printf("    - sampling frequency[%u]: %"PRIu32"\n", i, sampling_frequency_hz);
                                    break;
                                case 0x02: // 0 = 7.5, 1 = 10 ms
                                    frame_duration_index =  codec_specific_configuration[codec_offset+1];
                                    frame_duration = (frame_duration_index == 0) ? BTSTACK_LC3_FRAME_DURATION_7500US : BTSTACK_LC3_FRAME_DURATION_10000US;
                                    printf("    - frame duration[%u]: %s ms\n", i, (frame_duration == BTSTACK_LC3_FRAME_DURATION_7500US) ? "7.5" : "10");
                                    break;
                                case 0x04:  // octets per coding frame
                                    octets_per_frame = little_endian_read_16(codec_specific_configuration, codec_offset+1);
                                    printf("    - octets per codec frame[%u]: %u\n", i, octets_per_frame);
                                    break;
                                default:
                                    break;
                            }
                            codec_offset += ltv_len;
                        }
                        //
                        offset += codec_specific_configuration_length;
                        uint8_t metadata_length = base_data[offset++];
                        const uint8_t * meta_data = &base_data[offset];
                        offset += metadata_length;
                        printf("  - meta data[%u]: ", i);
                        printf_hexdump(meta_data, metadata_length);
                        uint8_t k;
                        for (k=0;k<num_bis;k++){
                            // Level 3: BIS Level
                            uint8_t bis_index = base_data[offset++];
                            printf("    - bis index[%u][%u]: %u\n", i, k, bis_index);
                            uint8_t codec_specific_configuration_length2 = base_data[offset++];
                            const uint8_t * codec_specific_configuration2 = &base_data[offset];
                            printf("    - codec specific config[%u][%u]: ", i, k);
                            printf_hexdump(codec_specific_configuration2, codec_specific_configuration_length2);
                            offset += codec_specific_configuration_length2;
                        }
                    }
                }
                break;
            default:
                break;
        }
    }
}

static void handle_big_info(const uint8_t * packet, uint16_t size){
    printf("BIG Info advertising report\n");
    sync_handle = hci_subevent_le_biginfo_advertising_report_get_sync_handle(packet);
    have_big_info = true;
}

static void enter_create_big_sync(void){
    // stop scanning
    gap_stop_scan();

    // init decoder
    setup_lc3_decoder();

    printf("Configure: %u channels, sampling rate %u, samples per frame %u\n", num_bis, sampling_frequency_hz, number_samples_per_frame);

    // create lc3 file
    open_lc3_file();

    // create wav file
    printf("WAV file: %s\n", filename_wav);
    wav_writer_open(filename_wav, num_bis, sampling_frequency_hz);

    // init playback buffer
    btstack_ring_buffer_init(&playback_buffer, playback_buffer_storage, PLAYBACK_BUFFER_SIZE);

    // start playback
    // PTS 8.2 sends stereo at half speed for stereo, for now playback at half speed
    const btstack_audio_sink_t * sink = btstack_audio_sink_get_instance();
    if (sink != NULL){
        uint32_t playback_speed;
        if ((num_bis > 1) && pts_mode){
            playback_speed = sampling_frequency_hz / num_bis;
            printf("PTS workaround: playback at %u hz\n", playback_speed);
        } else {
            playback_speed = sampling_frequency_hz;
        };
        sink->init(num_bis, sampling_frequency_hz, le_audio_broadcast_sink_playback);
        sink->start_stream();
    }

    app_state = APP_CREATE_BIG_SYNC;
}

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    if (packet_type != HCI_EVENT_PACKET) return;
    unsigned int i;
    switch (packet[0]) {
        case BTSTACK_EVENT_STATE:
            switch(btstack_event_state_get_state(packet)) {
                case HCI_STATE_WORKING:
                    if (app_state != APP_W4_WORKING) break;
                    app_state = APP_W4_BROADCAST_ADV;
                    gap_set_scan_params(1, 0x30, 0x30, 0);
                    gap_start_scan();
                    printf("Start scan..\n");
                    break;
                case HCI_STATE_OFF:
                    printf("Goodbye\n");
                    exit(0);
                    break;
                default:
                    break;
            }
            break;
        case GAP_EVENT_EXTENDED_ADVERTISING_REPORT:
        {
            if (app_state != APP_W4_BROADCAST_ADV) break;

            gap_event_extended_advertising_report_get_address(packet, remote);
            uint8_t adv_size = gap_event_extended_advertising_report_get_data_length(packet);
            const uint8_t * adv_data = gap_event_extended_advertising_report_get_data(packet);

            ad_context_t context;
            bool found = false;
            remote_name[0] = '\0';
            uint16_t uuid;
            for (ad_iterator_init(&context, adv_size, adv_data) ; ad_iterator_has_more(&context) ; ad_iterator_next(&context)) {
                uint8_t data_type = ad_iterator_get_data_type(&context);
                uint8_t size = ad_iterator_get_data_len(&context);
                const uint8_t *data = ad_iterator_get_data(&context);
                switch (data_type){
                    case BLUETOOTH_DATA_TYPE_SERVICE_DATA_16_BIT_UUID:
                        uuid = little_endian_read_16(data, 0);
                        if (uuid == ORG_BLUETOOTH_SERVICE_BROADCAST_AUDIO_ANNOUNCEMENT_SERVICE){
                            found = true;
                        }
                        break;
                    case BLUETOOTH_DATA_TYPE_SHORTENED_LOCAL_NAME:
                    case BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME:
                        size = btstack_min(sizeof(remote_name) - 1, size);
                        memcpy(remote_name, data, size);
                        remote_name[size] = 0;
                        break;
                    default:
                        break;
                }
            }
            if (!found) break;
            remote_type = gap_event_extended_advertising_report_get_address_type(packet);
            remote_sid = gap_event_extended_advertising_report_get_advertising_sid(packet);
            pts_mode = strncmp("PTS-", remote_name, 4) == 0;
            count_mode = strncmp("COUNT", remote_name, 5) == 0;
            printf("Remote Broadcast sink found, addr %s, name: '%s' (pts-mode: %u, count: %u)\n", bd_addr_to_str(remote), remote_name, pts_mode, count_mode);
            // ignore other advertisements
            gap_whitelist_add(remote_type, remote);
            gap_set_scan_params(1, 0x30, 0x30, 1);
            // sync to PA
            gap_periodic_advertiser_list_clear();
            gap_periodic_advertiser_list_add(remote_type, remote, remote_sid);
            app_state = APP_W4_PA_AND_BIG_INFO;
            printf("Start Periodic Advertising Sync\n");
            gap_periodic_advertising_create_sync(0x01, remote_sid, remote_type, remote, 0, 1000, 0);
            break;
        }

        case HCI_EVENT_LE_META:
            switch(hci_event_le_meta_get_subevent_code(packet)) {
                case HCI_SUBEVENT_LE_PERIODIC_ADVERTISING_SYNC_ESTABLISHMENT:
                    printf("Periodic advertising sync established\n");
                    break;
                case HCI_SUBEVENT_LE_PERIODIC_ADVERTISING_REPORT:
                    if (have_base) break;
                    handle_periodic_advertisement(packet, size);
                    if (have_base & have_big_info){
                        enter_create_big_sync();
                    }
                    break;
                case HCI_SUBEVENT_LE_BIGINFO_ADVERTISING_REPORT:
                    if (have_big_info) break;
                    handle_big_info(packet, size);
                    if (have_base & have_big_info){
                        enter_create_big_sync();
                    }
                    break;
                case HCI_SUBEVENT_LE_BIG_SYNC_ESTABLISHED:
                    printf("BIG Sync Established\n");
                    if (app_state == APP_W4_BIG_SYNC_ESTABLISHED){
                        gap_stop_scan();
                        gap_periodic_advertising_terminate_sync(sync_handle);
                        // update num_bis
                        num_bis = packet[16];
                        for (i=0;i<num_bis;i++){
                            bis_con_handles[i] = little_endian_read_16(packet, 17 + 2*i);
                        }
                        next_bis_index = 0;
                        app_state = APP_SET_ISO_PATHS;
                    }
                    break;
                default:
                    break;
            }
        default:
            break;
    }

    if (!hci_can_send_command_packet_now()) return;

    const uint8_t broadcast_code[16] = { 0 };
    uint8_t bis_array[MAX_NUM_BIS];
    switch(app_state){
        case APP_CREATE_BIG_SYNC:
            app_state = APP_W4_BIG_SYNC_ESTABLISHED;
            printf("BIG Create Sync for BIS: ");
            for (i=0;i<num_bis;i++){
                bis_array[i] = i + 1;
                printf("%u ", bis_array[i]);
            }
            printf("\n");
            hci_send_cmd(&hci_le_big_create_sync, big_handle, sync_handle, 0, broadcast_code, 0, 100, num_bis, bis_array);
            break;
        case APP_SET_ISO_PATHS:
            hci_send_cmd(&hci_le_setup_iso_data_path, bis_con_handles[next_bis_index++], 1, 0, 0, 0, 0, 0, 0, NULL);
            if (next_bis_index == num_bis){
                app_state = APP_STREAMING;
                last_samples_report_ms = btstack_run_loop_get_time_ms();
            }
            break;
        case APP_TERMINATE_BIG:
            hci_send_cmd(&hci_le_big_terminate_sync, big_handle);
            app_state = APP_IDLE;
            printf("Shutdown...\n");
            hci_power_control(HCI_POWER_OFF);
            break;
        default:
            break;
    }
}

static void iso_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){

    uint16_t header = little_endian_read_16(packet, 0);
    hci_con_handle_t con_handle = header & 0x0fff;
    uint8_t pb_flag = (header >> 12) & 3;
    uint8_t ts_flag = (header >> 14) & 1;
    uint16_t iso_load_len = little_endian_read_16(packet, 2);

    uint16_t offset = 4;
    uint32_t time_stamp = 0;
    if (ts_flag){
        uint32_t time_stamp = little_endian_read_32(packet, offset);
        offset += 4;
    }

    uint16_t packet_sequence_number = little_endian_read_16(packet, offset);
    offset += 2;

    uint16_t header_2 = little_endian_read_16(packet, offset);
    uint16_t iso_sdu_length = header_2 & 0x3fff;
    uint8_t packet_status_flag = (uint8_t) (header_2 >> 14);
    offset += 2;

    if (iso_sdu_length == 0) return;

    // infer channel from con handle - only works for up to 2 channels
    uint8_t bis_channel = (con_handle == bis_con_handles[0]) ? 0 : 1;

    if (count_mode){
        // check for missing packet
        uint16_t last_seq_no = last_packet_sequence[bis_channel];
        uint32_t now = btstack_run_loop_get_time_ms();
        bool packet_missed = (last_seq_no != 0) && ((last_seq_no + 1) != packet_sequence_number);
        if (packet_missed){
            // print last packet
            printf("\n");
            printf("%04x %10u %u ", last_seq_no, last_packet_time_ms[bis_channel], bis_channel);
            printf_hexdump(&last_packet_prefix[num_bis*PACKET_PREFIX_LEN], PACKET_PREFIX_LEN);
            last_seq_no++;

            printf(ANSI_COLOR_RED);
            while (last_seq_no < packet_sequence_number){
                printf("%04x            %u MISSING\n", last_seq_no, bis_channel);
                last_seq_no++;
            }
            printf(ANSI_COLOR_RESET);

            // print current packet
            printf("%04x %10u %u ", packet_sequence_number, now, bis_channel);
            printf_hexdump(&packet[offset], PACKET_PREFIX_LEN);
        }

        // cache current packet
        last_packet_time_ms[bis_channel] = now;
        last_packet_sequence[bis_channel] = packet_sequence_number;
        memcpy(&last_packet_prefix[num_bis*PACKET_PREFIX_LEN], &packet[offset], PACKET_PREFIX_LEN);

    } else {

        if ((packet_sequence_number & 0x7c) == 0) {
            printf("%04x %10u %u ", packet_sequence_number, btstack_run_loop_get_time_ms(), bis_channel);
            printf_hexdump(&packet[offset], iso_sdu_length);
        }

        if (lc3_frames < DUMP_LEN_LC3_FRAMES) {
            // store len header only for first bis
            if (bis_channel == 0) {
                uint8_t len_header[2];
                little_endian_store_16(len_header, 0, num_bis * iso_sdu_length);
                write(dump_file, len_header, 2);
            }

            // store single channel codec frame
            write(dump_file, &packet[offset], iso_sdu_length);
        }

        // decode codec frame
        uint8_t tmp_BEC_detect;
        uint8_t BFI = 0;
        (void) lc3_decoder->decode_signed_16(&decoder_contexts[bis_channel], &packet[offset], iso_sdu_length, BFI,
                                   &pcm[bis_channel * MAX_SAMPLES_PER_FRAME], 1,
                                   &tmp_BEC_detect);

        // interleave channel samples
        if ((bis_channel + 1) == num_bis) {
            uint16_t sample;
            int16_t wav_frame[MAX_NUM_BIS];
            uint8_t wav_channel;
            for (sample = 0; sample < number_samples_per_frame; sample++) {
                for (wav_channel = 0; wav_channel < num_bis; wav_channel++) {
                    wav_frame[wav_channel] = pcm[wav_channel * MAX_SAMPLES_PER_FRAME + sample];
                }

                // write wav sample
                if (lc3_frames < DUMP_LEN_LC3_FRAMES) {
                    wav_writer_write_int16(num_bis, wav_frame);
                }

                // store sample in playback buffer
                uint32_t bytes_to_store = num_bis * 2;
                samples_received++;
                if (btstack_ring_buffer_bytes_free(&playback_buffer) >= bytes_to_store) {
                    btstack_ring_buffer_write(&playback_buffer, (uint8_t *) wav_frame, bytes_to_store);
                } else {
                    samples_dropped++;
                }
            }
        }

        log_info("Samples in playback buffer %5u", btstack_ring_buffer_bytes_available(&playback_buffer) / (num_bis * 2));

        lc3_frames++;
        frames_per_second[bis_channel]++;

        uint32_t time_ms = btstack_run_loop_get_time_ms();
        if (btstack_time_delta(time_ms, last_samples_report_ms) > 1000){
            last_samples_report_ms = time_ms;
            printf("LC3 Frames: %4u - ", lc3_frames / num_bis);
            uint8_t i;
            for (i=0;i<num_bis;i++){
                printf("%u ", frames_per_second[i]);
                frames_per_second[i] = 0;
            }
            printf(" frames per second, dropped %u of %u\n", samples_dropped, samples_received);
            samples_received = 0;
            samples_dropped  =  0;
        }

        if (lc3_frames == DUMP_LEN_LC3_FRAMES){
            close_files();
        }
    }
}

static void show_usage(void){
    printf("\n--- LE Audio Broadcast Sink Test Console ---\n");
    printf("x - close files and exit\n");
    printf("---\n");
}

static void stdin_process(char c){
    switch (c){
        case 'x':
            close_files();
            printf("Shutdown...\n");
            hci_power_control(HCI_POWER_OFF);
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
    
    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // register for ISO Packet
    hci_register_iso_packet_handler(&iso_packet_handler);

    // turn on!
    hci_power_control(HCI_POWER_ON);

    btstack_stdin_setup(stdin_process);
    return 0;
}
