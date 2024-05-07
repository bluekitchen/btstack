/*
 * Copyright (C) 2023 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "generic_test.c"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <btstack_lc3.h>

#include "btstack.h"
#include "btstack_config.h"
#include "le-audio/le_audio_base_parser.h"

// Random Broadcast ID, valid for lifetime of BIG
#define BROADCAST_ID (0x112233u)

#define MAX_NUM_BIS 2
#define MAX_SAMPLES_PER_FRAME 480
#define MAX_LC3_FRAME_BYTES   155

static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_packet_callback_registration_t sm_event_callback_registration;
static void show_usage(void);

static const char * pts_address_string = "C007E87E555F";
static const char * tspx_periodic_advertising_data = "0201040503001801180D095054532D4741502D3036423803190000";
static const char * tspx_broadcast_code = "8ED03323D1205E2D58191BF6285C3182";

static bool gap_discoverable;
static bool gap_limited_discoverable;

static bd_addr_type_t   pts_address_type;
static bd_addr_t        pts_address;
static hci_con_handle_t pts_con_handle;

static const uint8_t adv_sid = 0;
static uint8_t adv_handle_regular = 0xff;
static uint8_t adv_handle_periodic = 0;

static le_advertising_set_t le_advertising_set_regular;
static le_advertising_set_t le_advertising_set_periodic;


static le_audio_big_t big_storage;
static le_audio_big_params_t big_params;

static bool have_base;
static bool have_big_info;

// broadcast info
static const uint8_t    big_handle = 1;
static hci_con_handle_t sync_handle;

// BIG Sync
static le_audio_big_sync_t        big_sync_storage;
static le_audio_big_sync_params_t big_sync_params;

static hci_con_handle_t bis_con_handles[MAX_NUM_BIS];
static uint16_t packet_sequence_numbers[MAX_NUM_BIS];
static uint8_t iso_payload[MAX_NUM_BIS * MAX_LC3_FRAME_BYTES];

static const le_extended_advertising_parameters_t extended_params_periodic = {
        .advertising_event_properties = 0,
        .primary_advertising_interval_min = 0x4b0, // 750 ms
        .primary_advertising_interval_max = 0x4b0, // 750 ms
        .primary_advertising_channel_map = 7,
        .own_address_type = 0,
        .peer_address_type = 0,
        .peer_address =  { 0 },
        .advertising_filter_policy = 0,
        .advertising_tx_power = 10, // 10 dBm
        .primary_advertising_phy = 1, // LE 1M PHY
        .secondary_advertising_max_skip = 0,
        .secondary_advertising_phy = 1, // LE 1M PHY
        .advertising_sid = adv_sid,
        .scan_request_notification_enable = 0,
};

static le_extended_advertising_parameters_t extended_params_regular = {
        .advertising_event_properties = 0x13,  // legacy, Connectable and scannable undirected
        .primary_advertising_interval_min = 0x30, // xx ms
        .primary_advertising_interval_max = 0x30, // xx ms
        .primary_advertising_channel_map = 7,
        .own_address_type = 0,
        .peer_address_type = 0,
        .peer_address =  { 0 },
        .advertising_filter_policy = 0,
        .advertising_tx_power = 10, // 10 dBm
        .primary_advertising_phy = 1, // LE 1M PHY
        .secondary_advertising_max_skip = 0,
        .secondary_advertising_phy = 1, // LE 1M PHY
        .advertising_sid = adv_sid,
        .scan_request_notification_enable = 0,
};

static const le_periodic_advertising_parameters_t periodic_params = {
        .periodic_advertising_interval_min = 0x258, // 375 ms
        .periodic_advertising_interval_max = 0x258, // 375 ms
        .periodic_advertising_properties = 0
};

static uint8_t extended_adv_data[32];
static uint8_t extended_adv_data_len = 8;

// general discoverable flags
static uint8_t adv_general_discoverable[] = { 2, 01, 02 };

// limited discoverable flags
static uint8_t adv_limited_discoverable[] = { 2, 01, 01 };

// non discoverable flags
static uint8_t adv_non_discoverable[] = { 2, 01, 00 };


// AD Manufacturer Specific Data - Ericsson, 1, 2, 3, 4
static uint8_t adv_data_1[] = { 7, 0xff, 0x00, 0x00, 1, 2, 3, 4 };
// AD Local Name - 'BTstack'
static uint8_t adv_data_2[] = { 8, 0x09, 'B', 'T', 's', 't', 'a', 'c', 'k' };
// AD Flags - 2 - General Discoverable mode -- flags are always prepended
static uint8_t adv_data_3[] = {  };
// AD Service Data - 0x1812 HID over LE
static uint8_t adv_data_4[] = { 3, 0x16, 0x12, 0x18 };
// AD Service Solicitation -  0x1812 HID over LE
static uint8_t adv_data_5[] = { 3, 0x14, 0x12, 0x18 };
// AD Services
static uint8_t adv_data_6[] = { 3, 0x03, 0x12, 0x18 };
// AD Slave Preferred Connection Interval Range - no min, no max
static uint8_t adv_data_7[] = { 5, 0x12, 0xff, 0xff, 0xff, 0xff };
// AD Tx Power Level - +4 dBm
static uint8_t adv_data_8[] = { 2, 0x0a, 4 };
// AD OOB
static uint8_t adv_data_9[] = { 2, 0x11, 3 };
// AD TK Value
static uint8_t adv_data_0[] = { 17, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
// AD LE Bluetooth Device Address - 66:55:44:33:22:11  public address
static uint8_t adv_data_le_bd_addr[] = { 8, 0x1b, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x00};
// AD Appearance
static uint8_t adv_data_appearance[] = { 3, 0x19, 0x00, 0x00};
// AD LE Role - Peripheral only
static uint8_t adv_data_le_role[] = {2 , 0x1c, 0x00};
// AD Public Target Address
static uint8_t adv_data_public_target_address[] = { 7, 0x17,  0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
// AD Random Target Address
static uint8_t adv_data_random_target_address[] = { 7, 0x18,  0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
// AD Advertising Interval
static uint8_t adv_data_advertising_interval[] =  { 3, BLUETOOTH_DATA_TYPE_ADVERTISING_INTERVAL, 0x00, 0x80};
// AD URI
static uint8_t adv_data_uri[] = {21, BLUETOOTH_DATA_TYPE_URI, 0x016,0x2F,0x2F,0x77,0x77,0x77,0x2E,0x62,0x6C,0x75,0x65,0x74,0x6F,0x6F,0x74,0x68,0x2E,0x63,0x6F,0x6D};

static uint8_t scan_data_len;
static uint8_t scan_data[32];

typedef struct {
    uint16_t len;
    uint8_t * data;
} advertisement_t;

#define ADV(a) { sizeof(a), &a[0]}
static advertisement_t advertisements[] = {
        ADV(adv_data_0),
        ADV(adv_data_1),
        ADV(adv_data_2),
        ADV(adv_data_3),
        ADV(adv_data_4),
        ADV(adv_data_5),
        ADV(adv_data_6),
        ADV(adv_data_7),
        ADV(adv_data_8),
        ADV(adv_data_9),
        ADV(adv_data_le_bd_addr),
        ADV(adv_data_appearance),
        ADV(adv_data_le_role),
        ADV(adv_data_public_target_address),
        ADV(adv_data_random_target_address),
        ADV(adv_data_advertising_interval),
        ADV(adv_data_uri)
};

static int advertisement_index = 2;

static uint8_t period_adv_data[255];
static uint16_t period_adv_data_len;

static int scan_hex_byte(const char * byte_string){
    int upper_nibble = nibble_for_char(*byte_string++);
    if (upper_nibble < 0) return -1;
    int lower_nibble = nibble_for_char(*byte_string);
    if (lower_nibble < 0) return -1;
    return (upper_nibble << 4) | lower_nibble;
}

static int btstack_parse_hex(const char * string, uint16_t len, uint8_t * buffer){
    int i;
    for (i = 0; i < len; i++) {
        int single_byte = scan_hex_byte(string);
        if (single_byte < 0) return 0;
        string += 2;
        buffer[i] = (uint8_t)single_byte;
        // don't check seperator after last byte
        if (i == len - 1) {
            return 1;
        }
        // optional seperator
        char separator = *string;
        if (separator == ':' && separator == '-' && separator == ' ') {
            string++;
        }
    }
    return 1;
}

void update_advertisements(void){

    // update adv data
    memset(extended_adv_data, 0, 32);
    extended_adv_data_len = 0;

    // scan_data_len = 0;

    // add "Flags" to advertisements
    if (gap_discoverable){
        memcpy(extended_adv_data, adv_general_discoverable, sizeof(adv_general_discoverable));
        extended_adv_data_len += sizeof(adv_general_discoverable);
    } else if (gap_limited_discoverable){
        memcpy(extended_adv_data, adv_limited_discoverable, sizeof(adv_limited_discoverable));
        extended_adv_data_len += sizeof(adv_limited_discoverable);
    } else {
        memcpy(extended_adv_data, adv_non_discoverable, sizeof(adv_non_discoverable));
        extended_adv_data_len += sizeof(adv_non_discoverable);
    }

    // append selected adv data
    memcpy(&extended_adv_data[extended_adv_data_len], advertisements[advertisement_index].data, advertisements[advertisement_index].len);
    extended_adv_data_len += advertisements[advertisement_index].len;

    memcpy(&scan_data[scan_data_len], advertisements[advertisement_index].data, advertisements[advertisement_index].len);
    scan_data_len += advertisements[advertisement_index].len;

    // set as adv + scan response data
    if ((extended_params_regular.advertising_event_properties & 0x04) == 0){
        gap_extended_advertising_set_adv_data(adv_handle_regular, extended_adv_data_len, extended_adv_data);
    }
    // gap_scan_response_set_data(scan_data_len, scan_data);

#if 0
    // update advertisement params
    uint8_t adv_type = gap_adv_type();

    bd_addr_t null_addr;
    memset(null_addr, 0, 6);
    uint16_t adv_int_min = 0x800;
    uint16_t adv_int_max = 0x800;
    uint8_t channel_map = 1;
    switch (adv_type){
        case 0:
        case 2:
        case 3:
            gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, channel_map, gap_adv_filter_policy);
            break;
        case 1:
        case 4:
            gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, tester_address_type, tester_address, channel_map, gap_adv_filter_policy);
            break;
    }
    gap_discoverable_control(gap_discoverable);
    gap_connectable_control(gap_connectable);
    if (gap_limited_discoverable){
        gap_set_class_of_device(0x202408);
    } else {
        gap_set_class_of_device(0x200408);
    }
#endif
}

static void start_extended_adv(void){
    gap_extended_advertising_set_params(adv_handle_regular, &extended_params_regular);
    uint16_t timeout = ((extended_params_regular.advertising_event_properties & 0x04) == 0) ? 0 : 100;
    gap_extended_advertising_start(adv_handle_regular, timeout, 0);
}

static void setup_periodic_advertising(void) {
    gap_extended_advertising_setup(&le_advertising_set_periodic, &extended_params_periodic, &adv_handle_periodic);
    gap_extended_advertising_set_adv_data(adv_handle_periodic, extended_adv_data_len, extended_adv_data);
    gap_periodic_advertising_set_params(adv_handle_periodic, &periodic_params);
    gap_periodic_advertising_set_data(adv_handle_periodic, period_adv_data_len, period_adv_data);
    gap_periodic_advertising_start(adv_handle_periodic, 0);
    gap_extended_advertising_start(adv_handle_periodic, 0, 0);
}

static void start_periodic_adv_sync_without_extended_adv(void){
    gap_set_scan_params(1, 0x30, 0x30, 0);
    gap_start_scan();

    uint8_t remote_sid = 0;

    // sync to PA
    gap_periodic_advertiser_list_clear();
    gap_periodic_advertiser_list_add(BD_ADDR_TYPE_LE_PUBLIC, pts_address, remote_sid);
    gap_periodic_advertising_create_sync(0x01, remote_sid, BD_ADDR_TYPE_LE_PUBLIC, pts_address, 0, 1000, 0);
}

static void start_periodic_adv_sync_using_extended_adv(void){
    gap_set_scan_params(1, 0x30, 0x30, 0);
    gap_start_scan();

    uint8_t remote_sid = 0;

    // sync to PA
    gap_periodic_advertiser_list_clear();
    gap_periodic_advertiser_list_add(BD_ADDR_TYPE_LE_PUBLIC, pts_address, remote_sid);
    gap_periodic_advertising_create_sync(0x01, remote_sid, BD_ADDR_TYPE_LE_PUBLIC, pts_address, 0, 1000, 0);
}

static uint8_t num_bis = 1;
static uint16_t octets_per_frame = 26;
static uint16_t sampling_frequency_hz = 8000;
static btstack_lc3_frame_duration_t frame_duration = BTSTACK_LC3_FRAME_DURATION_7500US;

// encryption
static uint8_t encryption = 0;
static uint8_t broadcast_code[16];

static void setup_big(void){
    // Create BIG
    big_params.big_handle = 0;
    big_params.advertising_handle = adv_handle_periodic;
    big_params.num_bis = num_bis;
    big_params.max_sdu = octets_per_frame;
    big_params.max_transport_latency_ms = 31;
    big_params.rtn = 2;
    big_params.phy = 2;
    big_params.packing = 0;
    big_params.encryption = encryption;
    if (encryption) {
        memcpy(big_params.broadcast_code, &broadcast_code[0], 16);
    } else {
        memset(big_params.broadcast_code, 0, 16);
    }
    if (sampling_frequency_hz == 44100){
        // same config as for 48k -> frame is longer by 48/44.1
        big_params.sdu_interval_us = frame_duration == BTSTACK_LC3_FRAME_DURATION_7500US ? 8163 : 10884;
        big_params.framing = 1;
    } else {
        big_params.sdu_interval_us = frame_duration == BTSTACK_LC3_FRAME_DURATION_7500US ? 7500 : 10000;
        big_params.framing = 0;
    }
    gap_big_create(&big_storage, &big_params);
}

static void handle_big_info(const uint8_t * packet, uint16_t size){
    printf("BIG Info advertising report\n");
    sync_handle = hci_subevent_le_biginfo_advertising_report_get_sync_handle(packet);
    uint8_t big_info_encrypted = hci_subevent_le_biginfo_advertising_report_get_encryption(packet);
    if (encryption == big_info_encrypted){
        have_big_info = true;
        if (encryption) {
            printf("Stream is encrypted\n");
        }
    } else {
        printf("Encryption differs (expected %u, actual %u) -> reject BIGInfo\n", encryption, big_info_encrypted);
    }
}

static void enter_create_big_sync(void){
    // stop scanning
    gap_stop_scan();

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
    big_sync_params.num_bis = num_bis;
    uint8_t i;
    printf("BIG Create Sync for BIS: ");
    for (i=0;i<num_bis;i++){
        big_sync_params.bis_indices[i] = i + 1;
        printf("%u ", big_sync_params.bis_indices[i]);
    }
    printf("\n");
    gap_big_sync_create(&big_sync_storage, &big_sync_params);
}

static void send_iso_packet(uint8_t bis_index) {
    hci_reserve_packet_buffer();
    uint8_t * buffer = hci_get_outgoing_packet_buffer();
    // complete SDU, no TimeStamp
    little_endian_store_16(buffer, 0, bis_con_handles[bis_index] | (2 << 12));
    // len
    little_endian_store_16(buffer, 2, 0 + 4 + octets_per_frame);
    // TimeStamp if TS flag is set
    // packet seq nr
    little_endian_store_16(buffer, 4, packet_sequence_numbers[bis_index]);
    // iso sdu len
    little_endian_store_16(buffer, 6, octets_per_frame);
    // copy encoded payload
    memcpy(&buffer[8], &iso_payload[bis_index * MAX_LC3_FRAME_BYTES], octets_per_frame);
    // send
    hci_send_iso_packet_buffer(4 + 0 + 4 + octets_per_frame);
    packet_sequence_numbers[bis_index]++;
}

static void hci_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    uint8_t sync_handle;
    uint8_t bis_index;

    if (packet_type != HCI_EVENT_PACKET) return;
    bd_addr_t addr;
    switch (hci_event_packet_get_type(packet)) {
        case BTSTACK_EVENT_STATE:
            switch(btstack_event_state_get_state(packet)){
                case HCI_STATE_WORKING:
                    show_usage();
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
            printf("Disconnected from %s\n", bd_addr_to_str(pts_address));
            break;
        case SM_EVENT_JUST_WORKS_REQUEST:
            printf("Just Works requested\n");
            sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
            break;
        case SM_EVENT_NUMERIC_COMPARISON_REQUEST:
            printf("Confirming numeric comparison: %"PRIu32"\n", sm_event_numeric_comparison_request_get_passkey(packet));
            sm_numeric_comparison_confirm(sm_event_passkey_display_number_get_handle(packet));
            break;
        case SM_EVENT_PASSKEY_DISPLAY_NUMBER:
            printf("Display Passkey: %"PRIu32"\n", sm_event_passkey_display_number_get_passkey(packet));
            break;
        case SM_EVENT_IDENTITY_CREATED:
            sm_event_identity_created_get_identity_address(packet, addr);
            printf("Identity created: type %u address %s\n", sm_event_identity_created_get_identity_addr_type(packet), bd_addr_to_str(addr));
            break;
        case SM_EVENT_IDENTITY_RESOLVING_SUCCEEDED:
            sm_event_identity_resolving_succeeded_get_identity_address(packet, addr);
            printf("Identity resolved: type %u address %s\n", sm_event_identity_resolving_succeeded_get_identity_addr_type(packet), bd_addr_to_str(addr));
            break;
        case SM_EVENT_IDENTITY_RESOLVING_FAILED:
            sm_event_identity_created_get_address(packet, addr);
            printf("Identity resolving failed\n");
            break;
        case SM_EVENT_PAIRING_COMPLETE:
            switch (sm_event_pairing_complete_get_status(packet)){
                case ERROR_CODE_SUCCESS:
                    printf("Pairing complete, success\n");
                    break;
                case ERROR_CODE_CONNECTION_TIMEOUT:
                    printf("Pairing failed, timeout\n");
                    break;
                case ERROR_CODE_REMOTE_USER_TERMINATED_CONNECTION:
                    printf("Pairing faileed, disconnected\n");
                    break;
                case ERROR_CODE_AUTHENTICATION_FAILURE:
                    printf("Pairing failed, reason = %u\n", sm_event_pairing_complete_get_reason(packet));
                    break;
                default:
                    break;
            }
            break;

        case GAP_EVENT_EXTENDED_ADVERTISING_REPORT:
        {
#if 0
            gap_event_extended_advertising_report_get_address(packet, source_data1.address);
            source_data1.address_type = gap_event_extended_advertising_report_get_address_type(packet);
            source_data1.adv_sid = gap_event_extended_advertising_report_get_advertising_sid(packet);

            uint8_t adv_size = gap_event_extended_advertising_report_get_data_length(packet);
            const uint8_t * adv_data = gap_event_extended_advertising_report_get_data(packet);

            ad_context_t context;
            bool found = false;
            remote_name[0] = '\0';
            uint16_t uuid;
            for (ad_iterator_init(&context, adv_size, adv_data) ; ad_iterator_has_more(&context) ; ad_iterator_next(&context)) {
                uint8_t data_type = ad_iterator_get_data_type(&context);
                const uint8_t *data = ad_iterator_get_data(&context);
                switch (data_type){
                    case BLUETOOTH_DATA_TYPE_SERVICE_DATA_16_BIT_UUID:
                        uuid = little_endian_read_16(data, 0);
                        if (uuid == ORG_BLUETOOTH_SERVICE_BROADCAST_AUDIO_ANNOUNCEMENT_SERVICE){
                            found = true;
                        }
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
            gap_stop_scan();
            break;
#endif
        }

        case HCI_EVENT_LE_META:
            switch (hci_event_le_meta_get_subevent_code(packet)){
                case HCI_SUBEVENT_LE_PERIODIC_ADVERTISING_SYNC_ESTABLISHMENT:
                    sync_handle = hci_subevent_le_periodic_advertising_sync_establishment_get_sync_handle(packet);
                    printf("Periodic advertising sync with handle 0x%04x established\n", sync_handle);
                    break;
                case HCI_SUBEVENT_LE_PERIODIC_ADVERTISING_REPORT:
                    if (have_base) break;
                    have_base = true;
                    printf("Periodic advertising report received\n");
                    if (have_base & have_big_info){
                        enter_create_big_sync();
                    }
                    break;
                case HCI_SUBEVENT_LE_BIGINFO_ADVERTISING_REPORT:
                    if (have_big_info) break;
                    handle_big_info(packet, size);
                    printf("BIGInfo received\n");
                    if (have_base & have_big_info){
                        enter_create_big_sync();
                    }
                    break;
                default:
                    return;
            }
            break;

        case HCI_EVENT_BIS_CAN_SEND_NOW:
            bis_index = hci_event_bis_can_send_now_get_bis_index(packet);
            send_iso_packet(bis_index);
            bis_index++;
            if (bis_index == num_bis){
                hci_request_bis_can_send_now_events(big_params.big_handle);
            }
            break;

        case HCI_EVENT_META_GAP:
            switch (hci_event_gap_meta_get_subevent_code(packet)){
                case GAP_SUBEVENT_LE_CONNECTION_COMPLETE:
                    pts_con_handle = gap_subevent_le_connection_complete_get_connection_handle(packet);
                    pts_address_type = gap_subevent_le_connection_complete_get_peer_address_type(packet);
                    gap_subevent_le_connection_complete_get_peer_address(packet, pts_address);
                    printf("Connection to %s established, con_handle 0x%04x\n", bd_addr_to_str(pts_address), pts_con_handle);
                    break;
                case GAP_SUBEVENT_CIG_CREATED:
#if 0
                    if (bap_app_client_state == BAP_APP_W4_CIG_COMPLETE){
                        bap_app_client_state = BAP_APP_CLIENT_STATE_CONNECTED;

                        printf("CIS Connection Handles: ");
                        for (i=0; i < cig_num_cis; i++){
                            cis_con_handles[i] = gap_subevent_cig_created_get_cis_con_handles(packet, i);
                            printf("0x%04x ", cis_con_handles[i]);
                        }
                        printf("\n");
                        printf("ASCS Client: Configure QoS %u us, ASE ID %d\n", cig_params.sdu_interval_p_to_c, ase_id);
                        printf("       NOTE: Only one CIS supported\n");

                        ascs_qos_configuration.cis_id = cig_params.cis_params[0].cis_id;
                        ascs_qos_configuration.cig_id = gap_subevent_cig_created_get_cig_id(packet);
                        audio_stream_control_service_client_streamendpoint_configure_qos(ascs_cid, ase_id, &ascs_qos_configuration);
                    }
#endif
                    break;
                case GAP_SUBEVENT_CIS_CREATED:
#if 0
                    // only look for cis handle
                    cis_handle = gap_subevent_cis_created_get_cis_con_handle(packet);
                    for (i=0; i < cig_num_cis; i++){
                        if (cis_handle == cis_con_handles[i]){
                            cis_established[i] = true;
                        }
                    }
                    // check for complete
                    bool complete = true;
                    for (i=0; i < cig_num_cis; i++) {
                        complete &= cis_established[i];
                    }
                    if (complete) {
                        printf("All CIS Established\n");
                        cis_setup_next_index = 0;
                        bap_app_client_state = BAP_APP_STREAMING;
                    }
#endif
                    break;
                case GAP_SUBEVENT_BIG_CREATED:
                    printf("BIG Created with BIS Connection handles: \n");
                    for (bis_index=0;bis_index<num_bis;bis_index++){
                        bis_con_handles[bis_index] = gap_subevent_big_created_get_bis_con_handles(packet, bis_index);
                        printf("0x%04x ", bis_con_handles[bis_index]);
                    }
                    printf("\n");
                    printf("Start streaming\n");
                    hci_request_bis_can_send_now_events(big_params.big_handle);
                    break;
                case GAP_SUBEVENT_BIG_SYNC_CREATED: {
                    printf("BIG Sync created with BIS Connection handles: ");
                    uint8_t i;
                    for (i=0;i<num_bis;i++){
                        bis_con_handles[i] = gap_subevent_big_sync_created_get_bis_con_handles(packet, i);
                        printf("0x%04x ", bis_con_handles[i]);
                    }
                    printf("\n");
                    printf("Start receiving\n");
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

static void iso_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){

    uint16_t header = little_endian_read_16(packet, 0);
    // hci_con_handle_t con_handle = header & 0x0fff;
    // uint8_t pb_flag = (header >> 12) & 3;
    uint8_t ts_flag = (header >> 14) & 1;
    // uint16_t iso_load_len = little_endian_read_16(packet, 2);

    uint16_t offset = 4;
    // uint32_t time_stamp = 0;
    if (ts_flag){
        // time_stamp = little_endian_read_32(packet, offset);
        offset += 4;
    }

    uint16_t packet_sequence_number = little_endian_read_16(packet, offset);
    offset += 2;

    uint16_t header_2 = little_endian_read_16(packet, offset);
    uint16_t iso_sdu_length = header_2 & 0x3fff;
    // uint8_t packet_status_flag = (uint8_t) (header_2 >> 14);
    offset += 2;

    if (iso_sdu_length == 0) return;

    // print current packet
    printf("%04x ", packet_sequence_number);
    printf_hexdump(&packet[offset], size - offset);
}

static void show_usage(void){
    bd_addr_t iut_address;
    gap_local_bd_addr(iut_address);
    printf("\n--- Generic H4 Test Console %s ---\n", bd_addr_to_str(iut_address));
    printf("TSPX_bd_addr_iut: ");
    for (uint8_t i=0;i<6;i++) printf("%02x", iut_address[i]);
    printf("\n");
    printf("TSPX_bd_addr_pts: ");
    for (uint8_t i=0;i<6;i++) printf("%02x", pts_address[i]);
    printf("\n");
    printf("TSPX_broadcast_code: ");
    printf_hexdump(broadcast_code, 16);
    printf("---\n");
    printf("a   - start connectable advertising\n");
    printf("c   - connect to %s\n", bd_addr_to_str(pts_address));
    printf("d/D - discoverable off/on\n");
    printf("t/T - limited discoverable off/on\n");
    printf("X   - disconnect\n");
    printf("1   - start periodic advertising\n");
    printf("2   - start periodic advertising sync without extended advertising\n");
    printf("3   - start periodic advertising sync using extended advertising\n");
    printf("x   - enable directed connectable \n");
    printf("4   - send periodic advertisement set info transfer information\n");
    printf("R   - use resolvable private address\n");
    printf("b   - setup BIG\n");
    printf("B   - enable Broadcast Code for BIG\n");
    printf("---\n");
}

static void stdin_process(char cmd){
    uint8_t status = ERROR_CODE_SUCCESS;

    switch (cmd){
        case 'c':
            printf("Connecting...\n");
            status = gap_connect(pts_address, 0);
            break;
        case 'p':
            gap_whitelist_add(0, pts_address);
            gap_whitelist_add(1, pts_address);
            status = gap_connect_with_whitelist();
            printf("Auto Connection Establishment to type %u, addr %s -> %x\n", 0, bd_addr_to_str(pts_address), status);
            break;
        case 'd':
            printf("d - discoverable off\n");
            gap_discoverable = false;
            update_advertisements();
            break;
        case 'D':
            printf("D - discoverable on\n");
            gap_discoverable = true;
            update_advertisements();
            break;
        case 'X':
            printf("Disconnect...\n");
            status = gap_disconnect(pts_con_handle);
            break;
        case '1':
            printf("Start periodic advertising\n");
            setup_periodic_advertising();
            break;
        case 'b':
            printf("Setup BIG using encryption %u\n", encryption);
            setup_big();
            break;
        case 'B':
            printf("Enable encryption for BIG\n");
            encryption = 1;
            break;
        case '2':
            printf("Start periodic advertising sync without extended advertising\n");
            start_periodic_adv_sync_without_extended_adv();
            break;
        case '3':
            printf("Start periodic advertising sync using extended advertising\n");
            start_periodic_adv_sync_using_extended_adv();
            break;
        case '4':
            printf("Send PAST info\n");
            gap_periodic_advertising_set_info_transfer_send(pts_con_handle, 0, adv_handle_periodic);
            break;
        case 'a':
            printf("Start extended advertising\n");
            start_extended_adv();
            break;
        case 't':
            printf("t - limited discoverable off\n");
            hci_send_cmd(&hci_write_current_iac_lap_two_iacs, 2, GAP_IAC_GENERAL_INQUIRY, GAP_IAC_GENERAL_INQUIRY);
            gap_limited_discoverable = 0;
            update_advertisements();
            break;
        case 'T':
            printf("T - limited discoverable on\n");
            gap_limited_discoverable = 1;
            hci_send_cmd(&hci_write_current_iac_lap_two_iacs, 2, GAP_IAC_LIMITED_INQUIRY, GAP_IAC_GENERAL_INQUIRY);
            update_advertisements();
            break;
        case 'x':
            printf("x - directed connectable on\n");
            extended_params_regular.advertising_event_properties = 0x1d;    // legacy, high duty-cycle
            memcpy(extended_params_regular.peer_address, pts_address, 6);
            extended_params_regular.peer_address_type = pts_address_type;
            break;
        case 'R':
            printf("Use Resolvable Private Address\n");
            extended_params_regular.own_address_type = BD_ADDR_TYPE_LE_PUBLIC_IDENTITY;
            memcpy(extended_params_regular.peer_address, pts_address, 6);
            extended_params_regular.peer_address_type = pts_address_type;
            break;
        case '\n':
        case '\r':
            break;

        default:
            show_usage();
            break;
    }

    if (status != ERROR_CODE_SUCCESS){
        printf("Command '%c' failed with status 0x%02x\n", cmd, status);
    }
}

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    UNUSED(argc);
    (void)argv;
    
    l2cap_init();

    gatt_client_init();

    sm_init();
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    sm_set_authentication_requirements(SM_AUTHREQ_SECURE_CONNECTION | SM_AUTHREQ_BONDING);
    sm_event_callback_registration.callback = &hci_event_handler;
    sm_add_event_handler(&sm_event_callback_registration);

    hci_event_callback_registration.callback = &hci_event_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // register for ISO Packet
    hci_register_iso_packet_handler(&iso_packet_handler);

    //
    gap_discoverable = true;
    gap_extended_advertising_setup(&le_advertising_set_regular, &extended_params_regular, &adv_handle_regular);
    update_advertisements();
    gap_extended_advertising_set_resolvable_private_address_update(30);

    // parse human readable Bluetooth address
    sscanf_bd_addr(pts_address_string, pts_address);
    btstack_stdin_setup(stdin_process);

    // parse padv data
    period_adv_data_len = strlen(tspx_periodic_advertising_data)/2;
    btstack_parse_hex(tspx_periodic_advertising_data, period_adv_data_len, period_adv_data);

    // parse broadcast code
    btstack_parse_hex(tspx_broadcast_code, sizeof(broadcast_code), broadcast_code);

    // turn on!
    hci_power_control(HCI_POWER_ON);

    return 0;
}
