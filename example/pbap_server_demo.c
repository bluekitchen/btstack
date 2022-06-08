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

#define BTSTACK_FILE__ "pbap_server_demo.c"

// *****************************************************************************
/* EXAMPLE_START(pbap_server_demo): PBAP Server - Demo Phonebook Server
 */
// *****************************************************************************


#define PBAP_SERVER_L2CAP_PSM         0x1001
#define PBAP_SERVER_RFCOMM_CHANNEL_NR 1

#include "btstack_config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack.h"

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static btstack_packet_callback_registration_t hci_event_callback_registration;

static uint16_t pbap_cid;

static uint8_t service_buffer[150];

static uint8_t upload_buffer[1000];

static uint8_t database_identifier[PBAP_DATABASE_IDENTIFIER_LEN];
static uint8_t primary_folder_version[PBAP_FOLDER_VERSION_LEN];
static uint8_t secondary_folder_version[PBAP_FOLDER_VERSION_LEN];

static uint32_t sdp_service_record_handle;

static uint8_t number_missed_calls = 0;

static bool enhanced_missed_calls_supported = true;

// test phonebooks are in the same order as pbap_phonebook_t
static struct {
    bool speed_dial;    // entries support speed dial
    bool date_time;     // entries have a date time field
    bool missed_calls;  // phonebook has missed calls count
    bool enhanced_missed_calls; // phonebook has missed calls if enhanced missed calls is supported
    uint16_t num_cards;
} phonebooks[] = {
    { false, true,  false,  true, 1},
    { false, true,  false, false, 1},
    { false, true,  false, false, 1},
    { false, true,  true,  true, 1},
    { false, true,  false, false, 1},
    { false, false, false, false, 10},
    { true,  false, false, false, 1},
    { false, true,  false,  true, 1},
    { false, true,  false, false, 1},
    { false, true,  true,  true, 1},
    { false, true,  false, false, 1},
    { false, false, false, false, 10}
};

static uint32_t supported_features_default =
        PBAP_SUPPORTED_FEATURES_DOWNLOAD |
        PBAP_SUPPORTED_FEATURES_BROWSING |
        PBAP_SUPPORTED_FEATURES_DATABASE_IDENTIFIER |
        PBAP_SUPPORTED_FEATURES_FOLDER_VERSION_COUNTERS |
        PBAP_SUPPORTED_FEATURES_VCARD_SELECTING |
        PBAP_SUPPORTED_FEATURES_DEFAULT_CONTACT_IMAGE_FORMAT;

static const char * vcard_listing_header = "<?xml version=\"1.0\"?>\n<!DOCTYPE vcard-listing SYSTEM \"vcard-listing.dtd\">\n<vCard-listing version=\"1.0\">";
static const char * vcard_listing_card   = "<card handle=\"%u.vcf\" name=\"last-%02u;first-%02u\"/>";
static const char * vcard_listing_footer = "</vCard-listing>";

static void create_vcard(char * vcard_buffer, uint16_t index, bool speed_dial, bool date_time){
    const char * speed_dial_text = speed_dial ? "X-BT-SPEEDDIALKEY:F1\n" : "";
    const char * date_time_text = date_time ? "X-IRMC-CALL-DATETIME;MISSED:20050320T100000\n" : "";
    sprintf(vcard_buffer, "BEGIN:VCARD\n"
                          "VERSION:2.1\n"
                          "FN:first-%02u last-%02u\n"
                          "N:last-%02u;first-%02u\n"
                          "TEL:+123000%02u\n"
                          "%s%s"
                          "END:VCARD", index, index, index, index, index, speed_dial_text, date_time_text);
}
static void create_item(char * buffer, uint16_t index){
    sprintf(buffer, vcard_listing_card, index, index, index);
}

// send vcards first-last, returns index of next card
static uint16_t send_vcards(uint16_t phonebook_index, uint16_t first, uint16_t last){
    uint16_t max_body_size = pbap_server_get_max_body_size(pbap_cid);
    char vcard_buffer[200];
    uint16_t pos = 0;
    while ((max_body_size > 0) && (first <= last)){
        // create vcard
        create_vcard(vcard_buffer, first, phonebooks[phonebook_index].speed_dial, phonebooks[phonebook_index].date_time);
        // get len
        uint16_t len = strlen(vcard_buffer);
        if (len > max_body_size){
            break;
        }
        memcpy(&upload_buffer[pos], (const uint8_t *) vcard_buffer, len);
        pos += len;
        max_body_size -= len;
        first++;
    }
    uint8_t response_code = (first > last) ? OBEX_RESP_SUCCESS : OBEX_RESP_CONTINUE;
    pbap_server_send_pull_response(pbap_cid, response_code, first, pos, upload_buffer);
    return first;
}

static uint16_t send_listing(uint16_t phonebook_index, bool header, uint16_t first, uint16_t last) {
    uint16_t max_body_size = pbap_server_get_max_body_size(pbap_cid);
    char listing_buffer[200];
    uint16_t pos = 0;
    bool done = false;
    // add header
    if (first == 0){
        uint16_t len = strlen(vcard_listing_header);
        memcpy(upload_buffer, vcard_listing_header, len);
        pos += len;
        max_body_size -= len;
    }
    while ((max_body_size > 0) && (first <= last)){
        // add entry
        create_item(listing_buffer, first);
        // get len
        uint16_t len = strlen(listing_buffer);
        if (len > max_body_size){
            break;
        }
        memcpy(&upload_buffer[pos], (const uint8_t *) listing_buffer, len);
        pos += len;
        max_body_size -= len;
        first++;
    }

    if (first > last){
        uint16_t len = (uint16_t) strlen(vcard_listing_footer);
        if (len < max_body_size){
            memcpy(&upload_buffer[pos], (const uint8_t *) vcard_listing_footer, len);
            pos += len;
            max_body_size -= len;
            done = true;
        }
    }

    uint8_t response_code = done ? OBEX_RESP_SUCCESS : OBEX_RESP_CONTINUE;
    uint32_t continuation = done ? 0x10000 : first;
    pbap_server_send_pull_response(pbap_cid, response_code, continuation, pos, upload_buffer);
    return first;

}

// packet handler
static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    uint8_t status;
    int  phonebook_index;
    const char * handle;
    uint32_t continuation;
    pbap_phonebook_t pbap_phonebook;
    uint16_t total_cards;
    uint16_t start_index;
    uint16_t num_cards_selected;
    uint16_t max_list_count;
    switch (packet_type){
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case HCI_EVENT_PBAP_META:
                    switch (hci_event_pbap_meta_get_subevent_code(packet)){
                        case PBAP_SUBEVENT_CONNECTION_OPENED:
                            status = pbap_subevent_connection_opened_get_status(packet);
                            if (status){
                                printf("[!] Connection failed, status 0x%02x\n", status);
                            } else {
                                pbap_cid = pbap_subevent_connection_opened_get_pbap_cid(packet);
                                printf("[+] Connected pbap_cid 0x%04x\n", pbap_cid);
                            }
                            break;
                        case PBAP_SUBEVENT_CONNECTION_CLOSED:
                            printf("[+] Connection closed\n");
                            break;
                        case PBAP_SUBEVENT_OPERATION_COMPLETED:
                            printf("[+] Operation complete, status 0x%02x\n",
                                   pbap_subevent_operation_completed_get_status(packet));
                            break;
                        case PBAP_SUBEVENT_AUTHENTICATION_REQUEST:
                            printf("[?] Authentication requested\n");
                            break;
                        case PBAP_SUBEVENT_RESET_MISSED_CALLS:
                            pbap_phonebook = pbap_subevent_reset_missed_calls_get_phonebook(packet);
                            printf("[+] Reset new missed call, phonebook %d\n", pbap_phonebook);
                            number_missed_calls = 0;
                            break;
                        case PBAP_SUBEVENT_QUERY_PHONEBOOK_SIZE:
                            pbap_phonebook = pbap_subevent_query_phonebook_size_get_phonebook(packet);
                            phonebook_index = pbap_phonebook - PBAP_PHONEBOOK_TELECOM_CCH;
                            printf("[+] Query phonebook size, phonebook %d\n", pbap_phonebook);
                            pbap_server_set_database_identifier(pbap_cid, database_identifier);
                            pbap_server_set_primary_folder_version(pbap_cid, primary_folder_version);
                            pbap_server_set_secondary_folder_version(pbap_cid, secondary_folder_version);
                            pbap_server_send_phonebook_size(pbap_cid, OBEX_RESP_SUCCESS, phonebooks[phonebook_index].num_cards);
                            break;
                        case PBAP_SUBEVENT_PULL_PHONEBOOK:
                            // get start index
                            pbap_phonebook = pbap_subevent_pull_phonebook_get_phonebook(packet);
                            phonebook_index = pbap_phonebook - PBAP_PHONEBOOK_TELECOM_CCH;
                            continuation = (uint16_t) pbap_subevent_pull_phonebook_get_continuation(packet);
                            if (continuation == 0){
                                // setup headers
                                pbap_server_set_database_identifier(pbap_cid, database_identifier);
                                pbap_server_set_primary_folder_version(pbap_cid, primary_folder_version);
                                pbap_server_set_secondary_folder_version(pbap_cid, secondary_folder_version);
                                // set new missed calls
                                if (phonebooks[phonebook_index].missed_calls || (phonebooks[phonebook_index].enhanced_missed_calls && enhanced_missed_calls_supported)){
                                    printf("[-] new missed calls = %u\n", number_missed_calls);
                                    pbap_server_set_new_missed_calls(pbap_cid, number_missed_calls);
                                }
                            }
                            // send vcard
                            total_cards = phonebooks[phonebook_index].num_cards;
                            start_index = pbap_subevent_pull_phonebook_get_list_start_offset(packet);
                            num_cards_selected = total_cards - start_index;
                            max_list_count = pbap_subevent_pull_phonebook_get_max_list_count(packet);
                            if (max_list_count < 0xffff){
                                num_cards_selected = btstack_min(max_list_count, num_cards_selected);
                            }
                            printf("[+] Pull phonebook %d - list offset %u, num cards %u\n", phonebook_index, start_index, num_cards_selected);
                            // consider already sent cards
                            num_cards_selected -= continuation;
                            start_index += continuation;
                            uint16_t end_index = start_index + num_cards_selected - 1;
                            printf("[-] continuation %u, num cards %u, start index %u, end index %u\n", continuation, num_cards_selected, start_index, end_index);
                            send_vcards(phonebook_index, start_index, end_index);
                            break;
                        case PBAP_SUBEVENT_PULL_VCARD_LISTING:
                            pbap_phonebook = pbap_subevent_pull_vcard_listing_get_phonebook(packet);
                            phonebook_index = pbap_phonebook - PBAP_PHONEBOOK_TELECOM_CCH;
                            continuation = (uint16_t) pbap_subevent_pull_vcard_listing_get_continuation(packet);
                            if (continuation == 0){
                                // setup headers on first response
                                pbap_server_set_database_identifier(pbap_cid, database_identifier);
                                pbap_server_set_primary_folder_version(pbap_cid, primary_folder_version);
                                pbap_server_set_secondary_folder_version(pbap_cid, secondary_folder_version);
                                // set new missed calls
                                if (phonebooks[phonebook_index].missed_calls || (phonebooks[phonebook_index].enhanced_missed_calls && enhanced_missed_calls_supported)){
                                    printf("[-] new missed calls = %u\n", number_missed_calls);
                                    pbap_server_set_new_missed_calls(pbap_cid, number_missed_calls);
                                }
                            }
                            // send vcard listing
                            total_cards = phonebooks[phonebook_index].num_cards;
                            start_index = pbap_subevent_pull_vcard_listing_get_list_start_offset(packet);
                            num_cards_selected = total_cards - start_index;
                            max_list_count = pbap_subevent_pull_vcard_listing_get_max_list_count(packet);
                            if (max_list_count < 0xffff){
                                num_cards_selected = btstack_min(max_list_count, num_cards_selected);
                            }
                            printf("[+] Pull vCard listing %d - list offset %u, num cards %u\n", phonebook_index, start_index, num_cards_selected);
                            // consider already sent cards
                            if (continuation > 0xffff){
                                // just missed the footer
                                start_index = 0xffff;
                                num_cards_selected  = 0;
                            } else {
                                num_cards_selected -= continuation;
                                start_index += continuation;
                            }
                            end_index = start_index + num_cards_selected - 1;
                            printf("[-] continuation %u, num cards %u, start index %u, end index %u\n", continuation, num_cards_selected, start_index, end_index);
                            send_listing(phonebook_index, continuation == 0, start_index, end_index);
                            break;
                        case PBAP_SUBEVENT_PULL_VCARD_ENTRY:
                            pbap_phonebook = pbap_subevent_pull_vcard_entry_get_phonebook(packet);
                            phonebook_index = pbap_phonebook - PBAP_PHONEBOOK_TELECOM_CCH;
                            handle = pbap_subevent_pull_vcard_entry_get_name(packet);
                            start_index = btstack_atoi(handle);
                            printf("[+] Pull vCard entry '%s' (%u) from phonebook %u\n", handle, start_index, phonebook_index);
                            // setup headers
                            pbap_server_set_database_identifier(pbap_cid, database_identifier);
                            send_vcards(phonebook_index, start_index, start_index);
                            break;
                        default:
                            break;
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


#ifdef HAVE_BTSTACK_STDIN

// Testing User Interface
static void show_usage(void){
    bd_addr_t iut_address;
    gap_local_bd_addr(iut_address);

    printf("\n--- Bluetooth PBAP Server Test Console %s ---\n", bd_addr_to_str(iut_address));
    printf("a: set nr missed calls to 5\n");
    printf("b: increment database identifier\n");
    printf("c: increment primary folder version\n");
    printf("d: increment secondary folder version\n");
    printf("e: disable enhanced missed calls\n");
    printf("\n");
}

static void stdin_process(char c) {
    log_info("stdin: %c", c);
    switch (c) {
        case '\n':
        case '\r':
            break;
        case 'a':
            printf("[+] Set number missed calls to 5\n");
            number_missed_calls = 5;
            break;
        case 'b':
            database_identifier[PBAP_DATABASE_IDENTIFIER_LEN-1]++;
            printf("[+] New database identifier: ");
            printf_hexdump(database_identifier, PBAP_DATABASE_IDENTIFIER_LEN);
            break;
        case 'c':
            primary_folder_version[PBAP_FOLDER_VERSION_LEN-1]++;
            printf("[+] New primary folder version: ");
            printf_hexdump(primary_folder_version, PBAP_FOLDER_VERSION_LEN);
            break;
        case 'd':
            secondary_folder_version[PBAP_FOLDER_VERSION_LEN-1]++;
            printf("[+] New secondary folder version: ");
            printf_hexdump(secondary_folder_version, PBAP_FOLDER_VERSION_LEN);
            break;
        case 'e':
            printf("[+] Disable Enhanced Missed Calls: ");
            enhanced_missed_calls_supported = false;
            uint32_t supported_features = supported_features_default;
            sdp_service_record_handle = sdp_create_service_record_handle();
            sdp_unregister_service(sdp_service_record_handle);
            pbap_server_create_sdp_record(service_buffer, sdp_service_record_handle, PBAP_SERVER_RFCOMM_CHANNEL_NR, PBAP_SERVER_L2CAP_PSM, "Phonebook Access PSE", 1, supported_features);
            sdp_register_service(service_buffer);
            break;
        default:
            show_usage();
            break;
    }
}
#endif

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    (void)argc;
    (void)argv;

    gap_discoverable_control(1);
    gap_set_class_of_device(0x200408);
    gap_set_local_name("PBAP Server Demo 00:00:00:00:00:00");

    // init L2CAP
    l2cap_init();

#ifdef ENABLE_BLE
    // Initialize LE Security Manager. Needed for cross-transport key derivation
    sm_init();
#endif

    // init RFCOM
    rfcomm_init();

    // init GOEP Server
    goep_server_init();

    // init PBAP Server
    pbap_server_init(&packet_handler, PBAP_SERVER_RFCOMM_CHANNEL_NR, PBAP_SERVER_L2CAP_PSM, LEVEL_2);

    // setup SDP Record
    sdp_init();
    uint32_t supported_features = supported_features_default | PBAP_SUPPORTED_FEATURES_ENHANCED_MISSED_CALLS;
    pbap_server_create_sdp_record(service_buffer, sdp_create_service_record_handle(), PBAP_SERVER_RFCOMM_CHANNEL_NR,
                                  PBAP_SERVER_L2CAP_PSM, "Phonebook Access PSE", 1, supported_features);
    sdp_register_service(service_buffer);

    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // turn on!
    hci_power_control(HCI_POWER_ON);

#ifdef HAVE_BTSTACK_STDIN
    btstack_stdin_setup(stdin_process);
#endif

    return 0;
}
/* EXAMPLE_END */
