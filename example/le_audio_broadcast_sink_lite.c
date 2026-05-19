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

#define BTSTACK_FILE__ "le_audio_broadcast_sink_lite.c"

/*
 * LE Audio Broadcast Sink
 */


#include "btstack_config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "ad_parser.h"
#include "bluetooth_company_id.h"
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
#include "btstack_lc3_google.h"
#include "btstack_lc3plus_fraunhofer.h"
#include "le-audio/le_audio_util.h"

#include "le_audio_demo_util_sink.h"

#ifdef HAVE_HAL_AUDIO_EXTERNAL_TRIGGER
#include <hal_audio.h>
#endif

// max config
#define MAX_NUM_BIS 5
#define MAX_SAMPLES_PER_FRAME 480

// playback
#define MAX_NUM_LC3_FRAMES   5
#define MAX_BYTES_PER_SAMPLE 4
#define PLAYBACK_BUFFER_SIZE (MAX_NUM_LC3_FRAMES * MAX_SAMPLES_PER_FRAME * MAX_BYTES_PER_SAMPLE)

static void show_usage(void);

static enum {
    APP_W4_WORKING,
    APP_W4_BROADCAST_ADV,
    APP_W4_PA_AND_BIG_INFO,
    APP_W4_BROADCAST_CODE,
    APP_W4_BIG_SYNC_ESTABLISHED,
    APP_STREAMING,
    APP_IDLE
} app_state = APP_W4_WORKING;

//
static btstack_packet_callback_registration_t hci_event_callback_registration;

static bool have_lite_config;
static bool have_big_info;
static bool have_broadcast_code;
static uint32_t bis_sync_mask;
static uint32_t ui_bis_sync_mask = 0xffffffffu;

uint16_t samples_received;
uint16_t samples_dropped;

// remote info
static char remote_name[20];
static bd_addr_t remote;
static bd_addr_type_t remote_type;
static uint8_t remote_sid;
static bool count_mode;

// broadcast info
static const uint8_t    big_handle = 1;
static hci_con_handle_t sync_handle;
static hci_con_handle_t bis_con_handles[MAX_NUM_BIS];
static uint8_t          encryption;
static uint8_t          broadcast_code[16];

// BIG Sync
static le_audio_big_sync_t        big_sync_storage;
static le_audio_big_sync_params_t big_sync_params;

// lc3 codec config
static uint16_t sampling_frequency_hz;
static btstack_lc3_frame_duration_t frame_duration;
static uint16_t octets_per_frame;
static uint8_t  num_bis;

// ui configuration
static bool ui_wait_for_configuration;

// Bluetooth Audio Timesync
#ifdef HAVE_HAL_AUDIO_EXTERNAL_TRIGGER
#define ISO_TIME_SYNC_PERIOD_MS 100
static btstack_timer_source_t iso_time_sync_timer;

#define HCI_OPCODE_HCI_BLUEKITCHEN_ISO_TIME_SYNC HCI_OPCODE (0x3f, 0x200)

static hci_cmd_t hci_iso_time_sync = {
    HCI_OPCODE_HCI_BLUEKITCHEN_ISO_TIME_SYNC, "1"
};

static bool iso_time_sync_trigger_requested;
static void iso_time_sync_send(void) {
    iso_time_sync_trigger_requested = false;
    uint8_t flags = 0;
    hci_send_cmd(&hci_iso_time_sync, flags);
}
static void iso_time_sync_handler(struct btstack_timer_source *ts){
    btstack_run_loop_set_timer(ts, ISO_TIME_SYNC_PERIOD_MS);
    btstack_run_loop_add_timer(ts);
    if (hci_can_send_command_packet_now()) {
        iso_time_sync_send();
    } else {
        iso_time_sync_trigger_requested = true;
    }
}
#endif

static bool handle_lite_config(const uint8_t * data, uint8_t data_size) {
    if (data_size < 8) return false;
    if (little_endian_read_16(data, 0) != BLUETOOTH_COMPANY_ID_BLUEKITCHEN_GMBH) return false;
    if (data[2] != 1) return false;

    uint8_t advertised_num_bis = data[4];
    uint8_t sampling_frequency_index = data[5];
    uint8_t frame_duration_index = data[6];
    uint8_t advertised_octets_per_frame = data[7];
    const uint32_t sampling_frequency_map[] = { 8000, 11025, 16000, 22050, 24000, 32000, 44100, 48000, 88200, 96000, 176400, 192000, 384000 };

    if ((advertised_num_bis == 0) || (advertised_num_bis > MAX_NUM_BIS)) return false;
    if ((sampling_frequency_index == 0) || (sampling_frequency_index > 8)) return false;
    if (frame_duration_index > 1) return false;

    num_bis = advertised_num_bis;
    sampling_frequency_hz = sampling_frequency_map[sampling_frequency_index - 1];
    frame_duration = le_audio_util_get_btstack_lc3_frame_duration(frame_duration_index);
    octets_per_frame = advertised_octets_per_frame;

    uint8_t i;
    bis_sync_mask = 0;
    for (i = 0; i < num_bis; i++) {
        bis_sync_mask |= 1u << i;
    }
    bis_sync_mask &= ui_bis_sync_mask;
    have_lite_config = true;

    printf("Broadcast Lite config: %u BIS, %u Hz, %s ms, %u octets\n",
           num_bis, sampling_frequency_hz, frame_duration_index == 0 ? "7.5" : "10", octets_per_frame);
    return true;
}

static void handle_big_info(const uint8_t * packet, uint16_t size){
    UNUSED(size);
    printf("BIG Info advertising report\n");
    sync_handle = hci_subevent_le_biginfo_advertising_report_get_sync_handle(packet);
    encryption = hci_subevent_le_biginfo_advertising_report_get_encryption(packet);
    if (encryption) {
        printf("Stream is encrypted\n");
    }
    have_big_info = true;
}

static void enter_create_big_sync(void){
    // stop scanning
    gap_stop_scan();

    // setup big sync params
    big_sync_params.big_handle = big_handle;
    big_sync_params.sync_handle = sync_handle;
    big_sync_params.encryption = encryption;
    if (encryption) {
        memcpy(big_sync_params.broadcast_code, &broadcast_code[0], 16);
    } else {
        memset(big_sync_params.broadcast_code, 0, 16);
    }
    big_sync_params.mse = 0;
    big_sync_params.big_sync_timeout_10ms = 100;
    big_sync_params.num_bis = 0;
    uint8_t i;
    printf("BIG Create Sync for BIS: ");
    // bit 0 selects BIS index #1
    for (i=0;i<30;i++){
        if ((bis_sync_mask & (1 << i)) != 0){
            uint8_t bis_index = i + 1;
            big_sync_params.bis_indices[big_sync_params.num_bis++] = i + 1;
            printf("%u ", bis_index);
        }
    }
    printf("\n");

    // init audio sink
    uint16_t iso_interval_1250us = frame_duration == BTSTACK_LC3_FRAME_DURATION_7500US ? 6 : 8;
    uint8_t  pre_transmission_offset = 1;
    le_audio_demo_util_sink_configure_broadcast(big_sync_params.num_bis, 1, sampling_frequency_hz, frame_duration, octets_per_frame,
                                                iso_interval_1250us, pre_transmission_offset);

    // start sync
    app_state = APP_W4_BIG_SYNC_ESTABLISHED;
    uint8_t status = gap_big_sync_create(&big_sync_storage, &big_sync_params);
    if (status != ERROR_CODE_SUCCESS) {
        printf("BIG Create Sync failed, status 0x%02x\n", status);
        app_state = APP_IDLE;
    }
}

static void start_scanning() {
    app_state = APP_W4_BROADCAST_ADV;
    have_lite_config = false;
    have_big_info = false;
    gap_set_scan_params(1, 0x30, 0x30, 0);
    gap_start_scan();
    printf("Start scan..\n");
}

static void got_lite_config_and_big_info() {
    if ((encryption == false) || have_broadcast_code){
        enter_create_big_sync();
    } else {
        printf("Encrypted stream requires broadcast code. Press 'z' to use 0x11111111111111111111111111111111\n");
        app_state = APP_W4_BROADCAST_CODE;
    }
}

static void start_pa_sync(bd_addr_type_t source_type, bd_addr_t source_addr) {
    // ignore other advertisements
    gap_whitelist_add(source_type, source_addr);
    gap_set_scan_params(1, 0x30, 0x30, 1);
    // sync to PA
    gap_periodic_advertiser_list_clear();
    gap_periodic_advertiser_list_add(source_type, source_addr, remote_sid);
    app_state = APP_W4_PA_AND_BIG_INFO;
    printf("Start Periodic Advertising Sync\n");
    gap_periodic_advertising_create_sync(0x01, remote_sid, source_type, source_addr, 0, 1000, 0);
    gap_start_scan();
}

static void pa_sync_established() {
    have_big_info = false;
    app_state = APP_W4_PA_AND_BIG_INFO;
}

static void stop_pa_sync(void) {
    app_state = APP_IDLE;
    // close audio
    le_audio_demo_util_sink_close();
    // stop PA sync
    gap_periodic_advertising_terminate_sync(sync_handle);
    // close channels
    for (int i=0;i<num_bis;i++){
        bis_con_handles[i] = HCI_CON_HANDLE_INVALID;
    }
}

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    if (packet_type != HCI_EVENT_PACKET) return;
    switch (packet[0]) {
        case BTSTACK_EVENT_STATE:
            switch(btstack_event_state_get_state(packet)) {
                case HCI_STATE_WORKING:
                    app_state = APP_IDLE;

#ifdef HAVE_HAL_AUDIO_EXTERNAL_TRIGGER
                    // init external timer capture setup
                    hal_audio_external_trigger_init();
                    // set one-shot timer
                    iso_time_sync_timer.process = &iso_time_sync_handler;
                    btstack_run_loop_set_timer(&iso_time_sync_timer, ISO_TIME_SYNC_PERIOD_MS);
                    btstack_run_loop_add_timer(&iso_time_sync_timer);
#endif

                    show_usage();
                    printf("Please select BIS channel if needed and start scanning\n");
                    break;
                case HCI_STATE_OFF:
                    printf("Goodbye\n");
                    exit(0);
                    break;
            default:
                    break;
            }
            break;
#ifdef HAVE_HAL_AUDIO_EXTERNAL_TRIGGER
        case HCI_EVENT_COMMAND_COMPLETE:
            switch (hci_event_command_complete_get_command_opcode(packet)) {
                case HCI_OPCODE_HCI_BLUEKITCHEN_ISO_TIME_SYNC: {
                    const uint8_t * return_params = hci_event_command_complete_get_return_parameters(packet);
                    uint32_t bluetooth_time_us = little_endian_read_32(return_params, 1);
                    uint32_t local_time_us = hal_audio_external_trigger_get_time_us();
                    le_audio_demo_util_sink_process_sync_event(bluetooth_time_us, local_time_us);
                    break;
                }
                default:
                    break;
            }
            break;
#endif
        case GAP_EVENT_EXTENDED_ADVERTISING_REPORT:
        {
            if (app_state != APP_W4_BROADCAST_ADV) break;

            gap_event_extended_advertising_report_get_address(packet, remote);
            uint8_t adv_size = gap_event_extended_advertising_report_get_data_length(packet);
            const uint8_t * adv_data = gap_event_extended_advertising_report_get_data(packet);

            ad_context_t context;
            bool found_lite_config = false;
            remote_name[0] = '\0';
            uint16_t uuid;
            uint32_t broadcast_id = 0;
            for (ad_iterator_init(&context, adv_size, adv_data) ; ad_iterator_has_more(&context) ; ad_iterator_next(&context)) {
                const uint8_t data_type = ad_iterator_get_data_type(&context);
                uint8_t data_size = ad_iterator_get_data_len(&context);
                const uint8_t *data = ad_iterator_get_data(&context);
                switch (data_type){
                    case BLUETOOTH_DATA_TYPE_MANUFACTURER_SPECIFIC_DATA:
                        if (handle_lite_config(data, data_size)) {
                            found_lite_config = true;
                        }
                        break;
                    case BLUETOOTH_DATA_TYPE_SERVICE_DATA_16_BIT_UUID:
                        uuid = little_endian_read_16(data, 0);
                        if (uuid == ORG_BLUETOOTH_SERVICE_BROADCAST_AUDIO_ANNOUNCEMENT_SERVICE){
                            broadcast_id = little_endian_read_24(data, 2);
                        }
                        break;
                    case BLUETOOTH_DATA_TYPE_SHORTENED_LOCAL_NAME:
                    case BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME:
                        data_size = btstack_min(sizeof(remote_name) - 1, data_size);
                        memcpy(remote_name, data, data_size);
                        remote_name[data_size] = 0;
                        break;
                    default:
                        break;
                }
            }
            if (!found_lite_config) break;
            remote_type = gap_event_extended_advertising_report_get_address_type(packet);
            remote_sid = gap_event_extended_advertising_report_get_advertising_sid(packet);
            count_mode = strncmp("COUNT", remote_name, 5) == 0;
            printf("Broadcast sink found, addr %s, name: '%s' (count: %u), Broadcast_ID 0%06" PRIx32 "\n", bd_addr_to_str(remote), remote_name, count_mode, broadcast_id);

            // start pa sync
            start_pa_sync(remote_type, remote);
            break;
        }

        case HCI_EVENT_LE_META:
            switch(hci_event_le_meta_get_subevent_code(packet)) {
                case HCI_SUBEVENT_LE_PERIODIC_ADVERTISING_SYNC_ESTABLISHMENT:
                    sync_handle = hci_subevent_le_periodic_advertising_sync_establishment_get_sync_handle(packet);
                    printf("Periodic advertising sync with handle 0x%04x established\n", sync_handle);
                    pa_sync_established();
                    break;
                case HCI_SUBEVENT_LE_PERIODIC_ADVERTISING_REPORT:
                    break;
                case HCI_SUBEVENT_LE_BIGINFO_ADVERTISING_REPORT:
                    if (app_state != APP_W4_PA_AND_BIG_INFO) break;
                    if (have_big_info) break;
                    handle_big_info(packet, size);
                    if (have_lite_config && have_big_info){
                        got_lite_config_and_big_info();
                    }
                    break;
                case HCI_SUBEVENT_LE_BIG_SYNC_LOST:
                    printf("BIG Sync Lost\n");
                    stop_pa_sync();
                    // start over
                    app_state = APP_IDLE;
                    start_scanning();
                    break;
                default:
                    break;
            }
            break;
        case HCI_EVENT_META_GAP:
            switch (hci_event_gap_meta_get_subevent_code(packet)){
                case GAP_SUBEVENT_BIG_SYNC_CREATED: {
                    printf("BIG Sync created with BIS Connection handles: ");
                    uint8_t i;
                    for (i=0;i<big_sync_params.num_bis;i++){
                        bis_con_handles[i] = gap_subevent_big_sync_created_get_bis_con_handles(packet, i);
                        printf("0x%04x ", bis_con_handles[i]);
                    }
                    printf("\n");
                    app_state = APP_STREAMING;
                    printf("Start receiving\n");
                    break;
                }
                case GAP_SUBEVENT_BIG_SYNC_STOPPED:
                    printf("BIG Sync stopped, big_handle 0x%02x\n", gap_subevent_big_sync_stopped_get_big_handle(packet));
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
#ifdef HAVE_HAL_AUDIO_EXTERNAL_TRIGGER
    if (iso_time_sync_trigger_requested && hci_can_send_command_packet_now()){
        iso_time_sync_send();
    }
#endif
}

static void show_usage(void){
    printf("\n--- LE Audio Broadcast Sink Test Console ---\n");
    printf("l: configure as Left Speaker and start scanning\n");
    printf("r: configure as Right Speaker and start scanning\n");
    printf("s - start scanning\n");
#ifdef HAVE_LC3PLUS
    printf("q - use LC3plus decoder if 10 ms ISO interval is used\n");
#endif
    printf("z - set broadcast code to 0x11111111111111111111111111111111\n");
    printf("t - terminate BIS streams\n");
    printf("---\n");
}

static void stdin_process(char c){
    switch (c){
        case 'l':
            if (app_state != APP_IDLE) break;
            ui_bis_sync_mask = 1;
            printf("Synchronize to BIS #1\n");
            start_scanning();
            break;
        case 'r':
            if (app_state != APP_IDLE) break;
            ui_bis_sync_mask = 2;
            printf("Synchronize to BIS #2\n");
            start_scanning();
            break;
        case 's':
            if (app_state != APP_IDLE) break;
            ui_bis_sync_mask = 0xffffffffu;
            start_scanning();
            break;
        case 'z':
            printf("Set broadcast code to 0x11111111111111111111111111111111\n");
            memset(broadcast_code, 0x11, 16);
            have_broadcast_code = true;
            if ((app_state == APP_W4_BROADCAST_CODE) && have_lite_config && have_big_info){
                enter_create_big_sync();
            }
            break;
#ifdef HAVE_LC3PLUS
        case 'q':
            printf("Use LC3plus decoder for 10 ms ISO interval...\n");
            le_audio_demo_util_sink_enable_lc3plus(true);
            break;
#endif
        case 't':
            switch (app_state){
                case APP_STREAMING:
                case APP_W4_BIG_SYNC_ESTABLISHED:
                    printf("Terminate BIG SYNC\n");
                    gap_big_sync_terminate(big_handle);
                    break;
                default:
                    break;
            }
            break;
        case '\n':
        case '\r':
            break;
        default:
            show_usage();
            break;

    }
}

static void iso_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    UNUSED(channel);
    void (*packet_receive)(uint8_t stream_index, uint8_t *packet, uint16_t size) = le_audio_demo_util_sink_receive;

    if (packet_type != HCI_ISO_DATA_PACKET) return;

    // get stream_index from con_handle
    uint16_t header = little_endian_read_16(packet, 0);
    hci_con_handle_t con_handle = header & 0x0fff;
    uint8_t i;

    if (count_mode){
        packet_receive = le_audio_demo_util_sink_count;
    }

    for (i=0;i<num_bis;i++){
        if (bis_con_handles[i] == con_handle) {
            packet_receive(i, packet, size);
        }
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

    // setup audio processing
    le_audio_demo_util_sink_init("le_audio_broadcast_sink_lite.wav");

    // turn on!
    hci_power_control(HCI_POWER_ON);

    ui_wait_for_configuration = true;
    btstack_stdin_setup(stdin_process);

    return 0;
}
