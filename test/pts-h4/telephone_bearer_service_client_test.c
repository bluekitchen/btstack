/*
 * Copyright (C) 2024 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "telephone_bearer_service_client_test.c"


#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <btstack_lc3.h>

#include "btstack.h"
#include "le-audio/gatt-service/telephone_bearer_service_client.h"

typedef struct advertising_report {
    uint8_t   type;
    uint8_t   event_type;
    uint8_t   address_type;
    bd_addr_t address;
    uint8_t   rssi;
    uint8_t   length;
    const uint8_t * data;
} advertising_report_t;

static enum {
    APP_STATE_IDLE,
    APP_STATE_W4_SCAN_RESULT,
    APP_STATE_W4_CONNECT,
    APP_STATE_CONNECTED
} app_state;

static tbs_client_connection_t tbs_connection;

static btstack_packet_callback_registration_t sm_event_callback_registration;
static hci_con_handle_t connection_handle;
static uint16_t tbs_cid;

static bd_addr_t public_pts_address = {0xC0, 0x07, 0xE8, 0x56, 0x28, 0xB3};
static int       public_pts_address_type = 0;
static bd_addr_t current_pts_address;
static int       current_pts_address_type;

enum bearer_type_e {
    GENERIC_BEARER,
    INDIVIDUAL_BEARER,
};
int bearer_type = INDIVIDUAL_BEARER;

static btstack_packet_callback_registration_t hci_event_callback_registration;

static void show_usage(void);
static void hci_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void tbs_gatt_client_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static void telephone_bearer_service_client_setup(void){
    // Init L2CAP
    l2cap_init();

    // GATT Client setup
    gatt_client_init();
    
    telephone_bearer_service_client_init();

    sm_init();
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    sm_set_authentication_requirements(SM_AUTHREQ_SECURE_CONNECTION | SM_AUTHREQ_BONDING);
    sm_event_callback_registration.callback = &hci_event_handler;
    sm_add_event_handler(&sm_event_callback_registration);

    hci_event_callback_registration.callback = &hci_event_handler;
    hci_add_event_handler(&hci_event_callback_registration);
} 

static void dump_advertising_report(uint8_t *packet){
    bd_addr_t address;
    gap_event_advertising_report_get_address(packet, address);

    printf("    * adv. event: evt-type %u, addr-type %u, addr %s, rssi %u, length adv %u, data: ", 
        gap_event_advertising_report_get_advertising_event_type(packet),
        gap_event_advertising_report_get_address_type(packet), 
        bd_addr_to_str(address), 
        gap_event_advertising_report_get_rssi(packet), 
        gap_event_advertising_report_get_data_length(packet));
    printf_hexdump(gap_event_advertising_report_get_data(packet), gap_event_advertising_report_get_data_length(packet));
    
}

static void dump_extended_advertising_report(uint8_t *packet){
    bd_addr_t address;
    gap_event_extended_advertising_report_get_address(packet, address);

    printf("    * adv. event: evt-type %u, addr-type %u, addr %s, rssi %u, length adv %u, data: ",
        gap_event_extended_advertising_report_get_advertising_event_type(packet),
        gap_event_extended_advertising_report_get_address_type(packet),
        bd_addr_to_str(address),
        gap_event_extended_advertising_report_get_rssi(packet),
        gap_event_extended_advertising_report_get_data_length(packet));
    printf_hexdump(gap_event_extended_advertising_report_get_data(packet), gap_event_extended_advertising_report_get_data_length(packet));

}

static void hci_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET){
        return;  
    }

    bd_addr_t addr;
    switch (hci_event_packet_get_type(packet)) {

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

        case BTSTACK_EVENT_STATE:
            // BTstack activated, get started
            if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING) break;
            printf("BTstack activated, start scanning.\n");
            app_state = APP_STATE_W4_SCAN_RESULT;
            gap_start_scan();
            break;

        case GAP_EVENT_ADVERTISING_REPORT: {
            bd_addr_t address;

            if (app_state != APP_STATE_W4_SCAN_RESULT) return;

            gap_event_advertising_report_get_address(packet, address);

            if (memcmp(address, public_pts_address, 6) != 0){
                break;
            }

            current_pts_address_type = gap_event_advertising_report_get_address_type(packet);
            dump_advertising_report(packet);

            // stop scanning, and connect to the device
            app_state = APP_STATE_W4_CONNECT;
            gap_stop_scan();
            printf("Stop scan. Connect to device with addr %s.\n", bd_addr_to_str(current_pts_address));
            gap_connect(current_pts_address, current_pts_address_type);
            break;
        }
        case GAP_EVENT_EXTENDED_ADVERTISING_REPORT: {
            bd_addr_t address;

            if (app_state != APP_STATE_W4_SCAN_RESULT) return;

            gap_event_extended_advertising_report_get_address(packet, address);
            if (memcmp(address, public_pts_address, 6) != 0){
                break;
            }

            current_pts_address_type = gap_event_extended_advertising_report_get_address_type(packet);
            dump_extended_advertising_report(packet);

            // stop scanning, and connect to the device
            app_state = APP_STATE_W4_CONNECT;
            gap_stop_scan();
            printf("Stop scan. Connect to device with addr %s.\n", bd_addr_to_str(current_pts_address));
            gap_connect(current_pts_address, current_pts_address_type);
            break;
        }
        case HCI_EVENT_META_GAP:
            // Wait for connection complete
            if (hci_event_gap_meta_get_subevent_code(packet) !=  GAP_SUBEVENT_LE_CONNECTION_COMPLETE) break;
            
            // Get connection handle from event
            connection_handle = gap_subevent_le_connection_complete_get_connection_handle(packet);
            gap_stop_scan();
            app_state = APP_STATE_CONNECTED;
            printf("GAP connected.\n");
            uint8_t service_index = 0;
            if( bearer_type == GENERIC_BEARER ) {
                (void) telephone_generic_bearer_service_client_connect(connection_handle, &tbs_gatt_client_event_handler,
                                                                 &tbs_connection, service_index,
                                                                 &tbs_cid);
            } else {
                (void) telephone_bearer_service_client_connect(connection_handle, &tbs_gatt_client_event_handler,
                                                                 &tbs_connection, service_index,
                                                                 &tbs_cid);
            }
            break;

        case HCI_EVENT_DISCONNECTION_COMPLETE:
            connection_handle = HCI_CON_HANDLE_INVALID;

            telephone_bearer_service_client_disconnect(tbs_cid);
            printf("Disconnected\n");

            // BTstack activated, get started
            printf("Restart scanning.\n");
            app_state = APP_STATE_W4_SCAN_RESULT;
            gap_start_scan();

            break;
        default:
            break;
    }
}

static void tbs_gatt_client_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(size);

    uint8_t status;

    if (hci_event_packet_get_type(packet) != HCI_EVENT_LEAUDIO_META) {
        return;
    }

    switch (hci_event_gattservice_meta_get_subevent_code(packet)) {
        case LEAUDIO_SUBEVENT_TBS_CLIENT_CONNECTED:
            status = leaudio_subevent_tbs_client_connected_get_att_status(packet);
            switch (status) {
                case ERROR_CODE_SUCCESS:
                    printf("TBS Client Test: connected\n");
                    break;

                default:
                    printf("TBS Client Test: connection failed, err 0x%02x.\n", status);
                    gap_disconnect(connection_handle);
                    break;
            }
            break;

        case LEAUDIO_SUBEVENT_TBS_CLIENT_WRITE_DONE:
            status = leaudio_subevent_tbs_client_write_done_get_att_status(packet);
            if (status != ATT_ERROR_SUCCESS){
                printf("TBS Client Test: Write failed, 0x%02x\n", status);
            } else {
                printf("TBS Client Test: Write successful\n");
            }
            break;

        case LEAUDIO_SUBEVENT_TBS_CLIENT_BEARER_PROVIDER_NAME:
            printf("Provider name: \"%s\"\n", leaudio_subevent_tbs_client_bearer_provider_name_get_name(packet));
            break;
        case LEAUDIO_SUBEVENT_TBS_CLIENT_BEARER_UCI:
            printf("Bearer UCI: \"%s\"\n", leaudio_subevent_tbs_client_bearer_uci_get_uci(packet));
            break;
        case LEAUDIO_SUBEVENT_TBS_CLIENT_BEARER_TECHNOLOGY:
            printf("Bearer Technology: \"%#04x\"\n", leaudio_subevent_tbs_client_bearer_technology_get_technology(packet));
            break;
        case LEAUDIO_SUBEVENT_TBS_CLIENT_BEARER_URI_SCHEMES_SUPPORTED_LIST:
            printf("Bearer URI Schemes Supported List: \"%s\"\n", leaudio_subevent_tbs_client_bearer_uri_schemes_supported_list_get_list(packet));
            break;
        case LEAUDIO_SUBEVENT_TBS_CLIENT_BEARER_SIGNAL_STRENGTH:
            printf("Signal Strength: \"%d\"\n", leaudio_subevent_tbs_client_bearer_signal_strength_get_strength(packet));
            break;
        case LEAUDIO_SUBEVENT_TBS_CLIENT_BEARER_SIGNAL_STRENGTH_REPORTING_INTERVAL:
            printf("Signal Strength Reporting Interval: \"%d\"\n", leaudio_subevent_tbs_client_bearer_signal_strength_reporting_interval_get_interval(packet));
            break;
        case LEAUDIO_SUBEVENT_TBS_CLIENT_BEARER_LIST_CURRENT_CALLS: {
            uint8_t length = leaudio_subevent_tbs_client_bearer_list_current_calls_get_list_length(packet);
            if( length == 0 ) {
                break; // Empty list
            }
            printf("call_index | call_state | flags | uri\n");
            btstack_event_iterator_t iter;
            for( leaudio_subevent_tbs_client_bearer_list_current_calls_list_init( &iter, packet );
                 leaudio_subevent_tbs_client_bearer_list_current_calls_list_has_next(&iter, packet);
                 leaudio_subevent_tbs_client_bearer_list_current_calls_list_next(&iter)) {
                uint8_t call_index  = leaudio_subevent_tbs_client_bearer_list_current_calls_get_list_item_call_index(&iter);
                uint8_t call_state  = leaudio_subevent_tbs_client_bearer_list_current_calls_get_list_item_state(&iter);
                uint8_t flags       = leaudio_subevent_tbs_client_bearer_list_current_calls_get_list_item_flags(&iter);
                uint8_t const *uri  = leaudio_subevent_tbs_client_bearer_list_current_calls_get_list_item_uri(&iter);
                printf( "%10d | %10d | %5x | \"%s\"\n", call_index, call_state, flags, uri );
            }
            break;
        }
        case LEAUDIO_SUBEVENT_TBS_CLIENT_CONTENT_CONTROL_ID:
            printf("Content Control ID: \"%#04x\"\n", leaudio_subevent_tbs_client_content_control_id_get_id(packet));
            break;
        case LEAUDIO_SUBEVENT_TBS_CLIENT_STATUS_FLAGS:
            printf("Client Status Flags: \"%#06x\"\n", leaudio_subevent_tbs_client_status_flags_get_flags(packet));
            break;
        case LEAUDIO_SUBEVENT_TBS_CLIENT_INCOMING_CALL_TARGET_BEARER_URI: {
            uint8_t call_index = leaudio_subevent_tbs_client_incoming_call_get_call_index(packet);
            uint8_t const *uri = leaudio_subevent_tbs_client_incoming_call_get_uri(packet);
            printf("Incoming Call Target Bearer URI: \"%d\": \"%s\"\n", call_index, uri);
            break;
        }
        case LEAUDIO_SUBEVENT_TBS_CLIENT_CALL_STATE: {
            uint8_t length = leaudio_subevent_tbs_client_call_state_get_list_length(packet);
            if( length == 0 ) {
                // Empty list
                break;
            }
            printf("call_index | call_state | flags\n");
            btstack_event_iterator_t iter;
            for( leaudio_subevent_tbs_client_call_state_list_init(&iter, packet);
                 leaudio_subevent_tbs_client_call_state_list_has_next(&iter, packet);
                 leaudio_subevent_tbs_client_call_state_list_next(&iter)) {
                uint8_t call_index = leaudio_subevent_tbs_client_call_state_get_list_item_call_index(&iter);
                uint8_t call_state = leaudio_subevent_tbs_client_call_state_get_list_item_call_state(&iter);
                uint8_t flags      = leaudio_subevent_tbs_client_call_state_get_list_item_flags(&iter);
                printf("%10d | %10d | %5x\n", call_index, call_state, flags );
            }
            break;
        }
        case LEAUDIO_SUBEVENT_TBS_CLIENT_CALL_CONTROL_POINT: {
            uint8_t opcode = leaudio_subevent_tbs_client_call_control_point_get_requested_opcode(packet);
            uint8_t call_index = leaudio_subevent_tbs_client_call_control_point_get_call_index(packet);
            uint8_t result_code = leaudio_subevent_tbs_client_call_control_point_get_result_code(packet);
            printf("opcode: %d, call_index: %d, result_code: %d\n", opcode, call_index, result_code);
            break;
        }
        case LEAUDIO_SUBEVENT_TBS_CLIENT_CALL_CONTROL_POINT_OPTIONAL_OPCODES:
            printf("Call Control Point Optional Opcodes: \"%#06x\"\n", leaudio_subevent_tbs_client_call_control_point_optional_opcodes_get_mask(packet));
            break;
        case LEAUDIO_SUBEVENT_TBS_CLIENT_TERMINATION_REASON: {
            uint8_t call_index = leaudio_subevent_tbs_client_termination_reason_get_call_index(packet);
            uint8_t reason = leaudio_subevent_tbs_client_termination_reason_get_reason(packet);
            printf("call_index: %d, reason_code: %d\n", call_index, reason);
            break;
        }
        case LEAUDIO_SUBEVENT_TBS_CLIENT_INCOMING_CALL: {
            uint8_t call_index = leaudio_subevent_tbs_client_incoming_call_get_call_index(packet);
            uint8_t const *uri = leaudio_subevent_tbs_client_incoming_call_get_uri(packet);
            printf("Incoming Call: \"%d\": \"%s\"\n", call_index, uri);
            break;
        }
        case LEAUDIO_SUBEVENT_TBS_CLIENT_CALL_FRIENDLY_NAME: {
            uint8_t call_index = leaudio_subevent_tbs_client_call_control_point_get_call_index(packet);
            uint8_t const *name = leaudio_subevent_tbs_client_call_friendly_name_get_name(packet);
            printf("Call Friendly Name: \"%d\": \"%s\"\n", call_index, name);
            break;
        }
        case LEAUDIO_SUBEVENT_TBS_CLIENT_DISCONNECTED:
            printf("TBS Client Test: disconnected\n");
            break;

        default:
            break;
    }
}

static void show_usage(void){
    uint8_t iut_address_type;
    bd_addr_t      iut_address;
    gap_le_get_own_address(&iut_address_type, iut_address);

    printf("--- TBS Client Test---\n");
    printf("c    - Connect to %s\n", bd_addr_to_str(current_pts_address));
    printf("b    - switch between generic bearer and individual bearer\n");
    printf("g    - accept call with index 1\n");
    printf("h    - terminate call with index 1\n");
    printf("j    - hold call with index 1\n");
    printf("k    - retrieve call with index 1\n");
    printf("p    - do a call with call index 1\n");
    printf("q    - join calls with index 1 and 2\n");
    printf("n    - query provider name\n");
    printf("u    - query uci\n");
    printf("t    - query technology\n");
    printf("s    - query scheme supported list\n");
    printf("i    - query signal strength\n");
    printf("r    - query signal strength reporting interval\n");
    printf("v    - set signal strength reporting interval\n");
    printf("l    - query list current calls\n");
    printf("d    - query content control id\n");
    printf("a    - query incoming call target bearer uri\n");
    printf("f    - query status flags\n");
    printf("e    - query call state\n");
    printf("o    - query call control point optional opcodes\n");
    printf("m    - query incoming call\n");
    printf("y    - query call friendly name\n");
}

static void stdin_process(char c){
    uint8_t status = ERROR_CODE_SUCCESS;

    switch (c){
        case 'c':
            printf("Connect to %s\n", bd_addr_to_str(current_pts_address));
            app_state = APP_STATE_W4_CONNECT;
            status = gap_connect(current_pts_address, current_pts_address_type);
            break;

        case 'b':
            if( bearer_type == INDIVIDUAL_BEARER ) {
                bearer_type = GENERIC_BEARER;
            } else {
                bearer_type = INDIVIDUAL_BEARER;
            }
            printf("Bearer type to connect to: %s\n", (bearer_type==INDIVIDUAL_BEARER)?"individual bearer":"generic bearer");
            break;
        case 'g':
            telephone_bearer_service_client_call_accept( tbs_cid, 1 );
            break;
        case 'h':
            telephone_bearer_service_client_call_terminate( tbs_cid, 1 );
            break;
        case 'j':
            telephone_bearer_service_client_call_hold( tbs_cid, 1 );
            break;
        case 'k':
            telephone_bearer_service_client_call_retrieve( tbs_cid, 1 );
            break;
        case 'p':
            telephone_bearer_service_client_call_originate(tbs_cid, 1, "tel:5551234");
            break;
        case 'q':
            {
                const uint8_t call_index_list[] = { 1, 2 };
                telephone_bearer_service_client_call_join(tbs_cid, call_index_list, 2);
            }
            break;
        case 'n':
            telephone_bearer_service_client_get_provider_name(tbs_cid);
            break;
        case 'u':
            telephone_bearer_service_client_get_uci(tbs_cid);
            break;
        case 't':
            telephone_bearer_service_client_get_technology(tbs_cid);
            break;
        case 's':
            telephone_bearer_service_client_get_schemes_supported_list(tbs_cid);
            break;
        case 'i':
            telephone_bearer_service_client_get_signal_strength(tbs_cid);
            break;
        case 'r':
            telephone_bearer_service_client_get_signal_strength_reporting_interval(tbs_cid);
            break;
        case 'v':
            telephone_bearer_service_client_set_strength_reporting_interval( tbs_cid, 6 );
            break;
        case 'l':
            telephone_bearer_service_client_get_list_current_calls(tbs_cid);
            break;
        case 'd':
            telephone_bearer_service_client_get_content_control_id(tbs_cid);
            break;
        case 'a':
            telephone_bearer_service_client_get_incoming_call_target_bearer_uri(tbs_cid);
            break;
        case 'f':
            telephone_bearer_service_client_get_status_flags(tbs_cid);
            break;
        case 'e':
            telephone_bearer_service_client_get_call_state(tbs_cid);
            break;
        case 'o':
            telephone_bearer_service_client_get_call_control_point_optional_opcodes(tbs_cid);
            break;
        case 'm':
            telephone_bearer_service_client_get_incoming_call(tbs_cid);
            break;
        case 'y':
            telephone_bearer_service_client_get_call_friendly_name(tbs_cid);
            break;

        case '\r':
            break;
        case '\n':
        default:
            show_usage();
            break;
    }

    if (status != ERROR_CODE_SUCCESS){
        printf("TBS Client Test: command %c failed due to 0%02x\n", c, status);
    }
}

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    UNUSED(argc);
    (void)argv;

    btstack_stdin_setup(stdin_process);

    telephone_bearer_service_client_setup();
    memcpy(current_pts_address, public_pts_address, 6);
    current_pts_address_type = public_pts_address_type;

    app_state = APP_STATE_IDLE;

    // turn on!
    hci_power_control(HCI_POWER_ON);
    return 0;
}



