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

#define BTSTACK_FILE__ "le_audio_unicast_gateway_lite.c"

/*
 * LE Audio Unicast Gateway Lite
 * Encodes the LC3 config in advertising and creates CIS without LE Audio profiles.
 */

// *****************************************************************************
/* EXAMPLE_START(le_audio_unicast_gateway_lite): LE Audio - Unicast Gateway Lite
 *
 * @text The LE Audio Unicast Gateway Lite streams audio over a Connected Isochronous Stream (CIS)
 * without using the LE Audio profiles. Instead, the LC3 codec configuration is encoded into
 * manufacturer specific advertising data. A matching Lite Headset can scan for this data, connect,
 * and create a CIS with the same codec parameters.
 *
 * @text Use the console menu to select the codec configuration and start advertising.
 */
// *****************************************************************************

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audio_player_demo_util.h"
#include "bluetooth_data_types.h"
#include "bluetooth_company_id.h"
#include "btstack_audio_player.h"
#include "btstack_debug.h"
#include "btstack_stdin.h"
#include "btstack_event.h"
#include "gap.h"
#include "hci.h"
#include "btstack_lc3.h"
#include "le_audio_demo_util_sink.h"
#include "le_audio_demo_util_source.h"

/*
 * @section Configuration
 *
 * @text The example is intentionally limited to one CIS. The audio payload can be mono or stereo,
 * depending on the selected codec configuration. The selected values are advertised so that
 * the peer can configure its CIG/CIS parameters without using ASCS, PACS, or BAP.
 */
#define MAX_NUM_CIS 1

/* LISTING_START(advertisement): Extended Advertising with Codec Configuration */
static le_extended_advertising_parameters_t extended_params = {
        .advertising_event_properties = 1,  // connectable
        .primary_advertising_interval_min = 160,    // 100 ms
        .primary_advertising_interval_max = 192,    // 120 ms
        .primary_advertising_channel_map = 7,
        .own_address_type = BD_ADDR_TYPE_LE_PUBLIC,
        .peer_address_type = 0,
        .peer_address =  { 0 },
        .advertising_filter_policy = 0,
        .advertising_tx_power = 10, // 10 dBm
        .primary_advertising_phy = 1, // LE 1M PHY
        .secondary_advertising_max_skip = 0,
        .secondary_advertising_phy = 1, // LE 1M PHY
        .advertising_sid = 0,
        .scan_request_notification_enable = 0,
};

static le_advertising_set_t le_advertising_set;
static uint8_t adv_handle = 0;

static uint8_t adv_data[] = {
        // Manufacturer Specific Data to indicate codec
        9,
        BLUETOOTH_DATA_TYPE_MANUFACTURER_SPECIFIC_DATA,
        BLUETOOTH_COMPANY_ID_BLUEKITCHEN_GMBH & 0xff,
        BLUETOOTH_COMPANY_ID_BLUEKITCHEN_GMBH >> 8,
        0, // subtype: LE Audio Connection Source
        0, // flags
        1, // num bis
        8, // sampling frequency in khz
        0, // frame duration
        26, // octets per frame
        // name
        7, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, 'S', 'o', 'u', 'r', 'c', 'e'
};
/* LISTING_END */

static btstack_packet_callback_registration_t hci_event_callback_registration;

static unsigned int     next_cis_index;
static hci_con_handle_t cis_con_handles[MAX_NUM_CIS];
static bool cis_established[MAX_NUM_CIS];
static uint8_t num_cis;

// Optional send interval histogram for throughput/timing experiments.
#ifdef COUNT_MODE
#define MAX_PACKET_INTERVAL_BINS_MS 50
static uint32_t send_time_bins[MAX_PACKET_INTERVAL_BINS_MS];
static uint32_t send_last_ms;
#endif

// LC3 codec configuration used for advertising and ISO frame generation.
static uint32_t sampling_frequency_hz;
static btstack_lc3_frame_duration_t frame_duration;
static uint16_t octets_per_frame;
static uint8_t  num_channels = 2;

// Console menu selection into codec_configurations.
static uint8_t menu_sampling_frequency = 5;
static uint8_t menu_variant = 3;

// Audio producer: reads demo audio and packetizes it into LC3 ISO frames.
static le_audio_demo_source_generator_t iso_gen;
static btstack_audio_player_t audio_player;

static enum {
    APP_W4_WORKING,
    APP_IDLE,
    APP_W4_CIS_COMPLETE,
    APP_STREAMING,
} app_state = APP_W4_WORKING;

/*
 * @section Codec Configuration
 *
 * @text The table contains the LC3 configurations from the Basic Audio Profile. The console menu
 * selects one sampling frequency and one variant. The selected tuple determines both the
 * advertisement payload and the LC3 encoder setup.
 */

/* LISTING_START(codecConfiguration): LC3 Codec Configuration Table */
static struct {
    uint16_t samplingrate_hz;
    uint8_t  samplingrate_index;
    uint8_t  num_variants;
    struct {
        const char * name;
        btstack_lc3_frame_duration_t frame_duration;
        uint16_t octets_per_frame;
    } variants[6];
} codec_configurations[] = {
    {
        8000, 0x01, 2,
        {
            {  "8_1",  BTSTACK_LC3_FRAME_DURATION_7500US, 26},
            {  "8_2", BTSTACK_LC3_FRAME_DURATION_10000US, 30}
        }
    },
    {
       16000, 0x03, 2,
       {
            {  "16_1",  BTSTACK_LC3_FRAME_DURATION_7500US, 30},
            {  "16_2", BTSTACK_LC3_FRAME_DURATION_10000US, 40}
       }
    },
    {
        24000, 0x05, 2,
        {
            {  "24_1",  BTSTACK_LC3_FRAME_DURATION_7500US, 45},
            {  "24_2", BTSTACK_LC3_FRAME_DURATION_10000US, 60}
       }
    },
    {
        32000, 0x06, 2,
        {
            {  "32_1",  BTSTACK_LC3_FRAME_DURATION_7500US, 60},
            {  "32_2", BTSTACK_LC3_FRAME_DURATION_10000US, 80}
        }
    },
    {
        44100, 0x07, 2,
        {
            { "441_1",  BTSTACK_LC3_FRAME_DURATION_7500US,  97},
            { "441_2", BTSTACK_LC3_FRAME_DURATION_10000US, 130}
        }
    },
    {
        48000, 0x08, 6,
        {
            {  "48_1", BTSTACK_LC3_FRAME_DURATION_7500US, 75},
            {  "48_2", BTSTACK_LC3_FRAME_DURATION_10000US, 100},
            {  "48_3", BTSTACK_LC3_FRAME_DURATION_7500US, 90},
            {  "48_4", BTSTACK_LC3_FRAME_DURATION_10000US, 120},
            {  "48_5", BTSTACK_LC3_FRAME_DURATION_7500US, 117},
            {  "48_6", BTSTACK_LC3_FRAME_DURATION_10000US, 155}
        }
    },
};
/* LISTING_END */

static void show_usage(void);

static void print_config(void) {
    printf("Config '%s_%u': %u, %s ms, %u octets\n",
           codec_configurations[menu_sampling_frequency].variants[menu_variant].name,
           num_channels,
           codec_configurations[menu_sampling_frequency].samplingrate_hz,
           codec_configurations[menu_sampling_frequency].variants[menu_variant].frame_duration == BTSTACK_LC3_FRAME_DURATION_7500US ? "7.5" : "10",
           codec_configurations[menu_sampling_frequency].variants[menu_variant].octets_per_frame);
}

static void configure_source_generator(void){
    // Open the demo playlist and configure the LC3 source helper.
    btstack_linked_list_t playlist = audio_player_demo_util_get_playlist_instance();
    btstack_audio_player_init(&audio_player, sampling_frequency_hz, num_channels, playlist);
    btstack_audio_player_play(&audio_player);
    printf("Audio Player: %s\n", btstack_audio_player_get_current_song(&audio_player)->title);

    iso_gen.audio_generator = NULL;
    le_audio_demo_util_source_configure(&iso_gen, 1, num_channels, sampling_frequency_hz, frame_duration, octets_per_frame);
    le_audio_demo_util_source_set_audio_generator(&iso_gen, btstack_audio_player_get_generator(&audio_player));
    le_audio_demo_util_source_generate_iso_frame(&iso_gen);
}

/*
 * @section Start Unicast
 *
 * @text The gateway advertises the selected codec configuration and waits for an incoming
 * ACL connection. When the peer requests a CIS, the gateway accepts it and starts streaming
 * after all CIS handles are established.
 */

/* LISTING_START(startUnicast): Configure Source and Start Advertising */
static void start_unicast() {
    // use values from table
    sampling_frequency_hz = codec_configurations[menu_sampling_frequency].samplingrate_hz;
    octets_per_frame      = codec_configurations[menu_sampling_frequency].variants[menu_variant].octets_per_frame;
    frame_duration        = codec_configurations[menu_sampling_frequency].variants[menu_variant].frame_duration;

    configure_source_generator();

    // Update manufacturer data with the Lite codec description.
    adv_data[4] = 0; // subtype
    adv_data[5] = 0; // flags
    adv_data[6] = num_channels;
    adv_data[7] = sampling_frequency_hz / 1000;
    adv_data[8] = frame_duration == BTSTACK_LC3_FRAME_DURATION_7500US ? 0 : 1;
    adv_data[9] = octets_per_frame;

    // setup advertisements
    bd_addr_t local_addr;
    gap_local_bd_addr(local_addr);
    bool local_address_invalid = btstack_is_null_bd_addr(local_addr);
    if (local_address_invalid) {
        // Public address is not available; use the random address set by the port.
        extended_params.own_address_type = BD_ADDR_TYPE_LE_RANDOM;
    }
    gap_extended_advertising_setup(&le_advertising_set, &extended_params, &adv_handle);
    if (local_address_invalid) {
        // assume nRF5340/nRF54 where main already set a random address
        uint8_t local_addr_type;
        gap_le_get_own_address(&local_addr_type, local_addr);
        gap_extended_advertising_set_random_address(adv_handle, local_addr);
    }
    gap_extended_advertising_set_adv_data(adv_handle, sizeof(adv_data), adv_data);
    gap_extended_advertising_start(adv_handle, 0, 0);

    num_cis = 1;
    app_state = APP_W4_CIS_COMPLETE;
}
/* LISTING_END */

/*
 * @section Packet Handler
 *
 * @text The packet handler waits until the controller is working, accepts incoming CIS requests,
 * tracks when all CIS handles are established, and configures the ISO sink for audio received
 * from the peer.
 */

/* LISTING_START(packetHandler): HCI and GAP Event Handler */
static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    if (packet_type != HCI_EVENT_PACKET) return;

    hci_con_handle_t cis_con_handle;
    uint8_t i;

    switch (packet[0]) {
        case BTSTACK_EVENT_STATE:
            switch(btstack_event_state_get_state(packet)) {
                case HCI_STATE_WORKING:
                    app_state = APP_IDLE;
                    printf("LE Audio Unicast Gateway Lite uses a private advertising format and creates CIS without LE Audio profiles.\n");
                    printf("It interoperates with the LE Audio Unicast Headset Lite example, but will not work with a smartphone that supports LE Audio.\n");
#ifdef ENABLE_DEMO_MODE
                    // start unicast automatically, 48 kHz, 10 ms, stereo
                    num_channels = 2;
                    menu_sampling_frequency = 5;
                    menu_variant = 1;
                    start_unicast();
#else
                    show_usage();
                    printf("Please select sample frequency and variation, then start advertising\n");
#endif
                    break;
                case HCI_STATE_OFF:
                    printf("Goodbye\n");
                    exit(0);
                    break;
                default:
                    break;
            }
            break;
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            cis_con_handle = hci_event_disconnection_complete_get_connection_handle(packet);
            for (i=0; i < num_cis; i++){
                if (cis_con_handle == cis_con_handles[i]){
                    le_audio_demo_util_sink_close();
                }
            }
            break;
        case HCI_EVENT_LE_META:
            switch(hci_event_le_meta_get_subevent_code(packet)){
                case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
                    // A new ACL connection starts a new sequence of CIS requests.
                    next_cis_index = 0;
                    break;
                case HCI_SUBEVENT_LE_CIS_REQUEST:
                    // The peer created the CIG and now asks us to accept the CIS.
                    cis_con_handles[next_cis_index] = hci_subevent_le_cis_request_get_cis_connection_handle(packet);
                    gap_cis_accept(cis_con_handles[next_cis_index]);
                    next_cis_index++;
                    break;
                default:
                    break;
            }
            break;
        case HCI_EVENT_META_GAP:
            switch (hci_event_gap_meta_get_subevent_code(packet)) {
                case GAP_SUBEVENT_CIS_CREATED: {
                    cis_con_handle = gap_subevent_cis_created_get_cis_con_handle(packet);
                    for (i=0; i < num_cis; i++){
                        if (cis_con_handle == cis_con_handles[i]){
                            cis_established[i] = true;
                        }
                    }
                    // check for complete
                    bool complete = true;
                    for (i=0; i < num_cis; i++) {
                        complete &= cis_established[i];
                    }
                    if (complete) {
                        printf("All CIS Established and ISO Path setup\n");

                        // Configure sink for the optional microphone audio sent by the peer.
                        uint16_t iso_interval_1250us = gap_subevent_cis_created_get_iso_interval_1250us(packet);
                        uint8_t  flush_timeout       = gap_subevent_cis_created_get_flush_timeout_c_to_p(packet);
                        le_audio_demo_util_sink_configure_unicast(1, 1, sampling_frequency_hz, frame_duration,
                                                                  octets_per_frame,
                                                                  iso_interval_1250us, flush_timeout);

                        next_cis_index = 0;
                        app_state = APP_STREAMING;

                        hci_request_cis_can_send_now_events(cis_con_handles[0]);
                    }
                    break;
                }
                default:
                    break;
            }
            break;
        default:
            break;
    }
}
/* LISTING_END */

/*
 * @section ISO Packet Handler
 *
 * @text ISO transmission is driven by HCI_EVENT_CIS_CAN_SEND_NOW. The source helper sends one
 * frame per CIS and generates the next LC3 frame after the current group has been sent.
 * Incoming ISO packets are passed to the sink helper for optional microphone monitoring.
 */

/* LISTING_START(isoPacketHandler): Send and Receive ISO Packets */
static void handle_iso_can_send_now_event(const uint8_t *packet, uint16_t size) {
    UNUSED(size);
    if (app_state != APP_STREAMING) return;

    hci_con_handle_t cis_con_handle = hci_event_cis_can_send_now_get_cis_con_handle(packet);
    uint8_t i;
    for (i=0; i < num_cis; i++){
        if (cis_con_handle == cis_con_handles[i]){
            le_audio_demo_util_source_send(&iso_gen, i, cis_con_handle);
            if (hci_event_cis_can_send_now_get_group_complete(packet)) {
                le_audio_demo_util_source_generate_iso_frame(&iso_gen);
                hci_request_cis_can_send_now_events(cis_con_handle);
            }
        }
    }
}

static void iso_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    UNUSED(channel);

    switch (packet_type) {
        case HCI_EVENT_PACKET:
            btstack_assert(hci_event_packet_get_type(packet) == HCI_EVENT_CIS_CAN_SEND_NOW);
            handle_iso_can_send_now_event(packet, size);
            break;
        case HCI_ISO_DATA_PACKET:
            le_audio_demo_util_sink_receive(0, packet, size);
            break;
        default:
            break;
    }
}
/* LISTING_END */

static void show_usage(void){
    printf("\n--- LE Audio Unicast Source Test Console ---\n");
    print_config();
    printf("---\n");
    printf("c - toggle channels\n");
    printf("f - next sampling frequency\n");
    printf("v - next codec variant\n");
    printf("n - next song while streaming\n");
    printf("s - start advertising\n");
    printf("x - shutdown\n");
    printf("---\n");
}

static void stdin_process(char c){
    switch (c){
        case 'c':
            if (app_state != APP_IDLE){
                printf("Codec configuration can only be changed in idle state\n");
                break;
            }
            num_channels = 3 - num_channels;
            print_config();
            break;
        case 'f':
            if (app_state != APP_IDLE){
                printf("Codec configuration can only be changed in idle state\n");
                break;
            }
            menu_sampling_frequency++;
            if (menu_sampling_frequency >= 6){
                menu_sampling_frequency = 0;
            }
            if (menu_variant >= codec_configurations[menu_sampling_frequency].num_variants){
                menu_variant = 0;
            }
            print_config();
            break;
        case 'v':
            if (app_state != APP_IDLE){
                printf("Codec configuration can only be changed in idle state\n");
                break;
            }
            menu_variant++;
            if (menu_variant >= codec_configurations[menu_sampling_frequency].num_variants){
                menu_variant = 0;
            }
            print_config();
            break;
        case 's':
            if (app_state != APP_IDLE){
                printf("Cannot start advertising - not in idle state\n");
                break;
            }
            start_unicast();

            break;
        case 'n':
            if (app_state != APP_STREAMING){
                printf("Audio player can only change songs while streaming\n");
                break;
            }
            btstack_audio_player_next_song(&audio_player);
            printf("Audio Player: %s\n", btstack_audio_player_get_current_song(&audio_player)->title);
            break;
        case '\n':
        case '\r':
            break;
        default:
            show_usage();
            break;
    }
}

/*
 * @section Main Application Setup
 *
 * @text The main application registers HCI and ISO packet handlers, initializes the demo audio
 * source and sink helpers, powers on the controller, and enables the console menu.
 */

/* LISTING_START(MainConfiguration): Register Handlers and Start Bluetooth Stack */
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
    le_audio_demo_util_sink_init("le_audio_unicast_source.wav");
    le_audio_demo_util_source_init(&iso_gen);

    // turn on!
    hci_power_control(HCI_POWER_ON);

    btstack_stdin_setup(stdin_process);
    return 0;
}
/* LISTING_END */
/* EXAMPLE_END */
