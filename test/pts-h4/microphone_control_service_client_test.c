/*
 * Copyright (C) 2014 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "microphone_control_service_client_test.c"


#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <btstack_lc3.h>

#include "btstack.h"

#include "microphone_control_service_client_test.h"

#define MICS_CHARACTERISTICS_MAX_NUM  MICROPHONE_CONTROL_SERVICE_CLIENT_NUM_CHARACTERISTICS
#define AICS_CONNECTIONS_MAX_NUM 5
#define AICS_CHARACTERISTICS_MAX_NUM  AICS_CONNECTIONS_MAX_NUM * AUDIO_INPUT_CONTROL_SERVICE_NUM_CHARACTERISTICS

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

static mics_client_connection_t mics_connection;
static gatt_service_client_characteristic_t mics_storage_for_characteristics[MICS_CHARACTERISTICS_MAX_NUM];
static aics_client_connection_t aics_connections[AICS_CONNECTIONS_MAX_NUM];
static gatt_service_client_characteristic_t aics_storage_for_characteristics[AICS_CHARACTERISTICS_MAX_NUM];

static advertising_report_t report;
static btstack_packet_callback_registration_t sm_event_callback_registration;
static hci_con_handle_t connection_handle;
static uint16_t mics_cid;

static bd_addr_t public_pts_address = {0xC0, 0x07, 0xE8, 0x41, 0x45, 0x91};
static int       public_pts_address_type = 0;
static bd_addr_t current_pts_address;
static int       current_pts_address_type;

// AICS
static btstack_packet_callback_registration_t hci_event_callback_registration;
const char * audio_input_description = "AICS Client Test";
static uint8_t aics_connection_index = 0;

static void show_usage(void);
static void hci_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void gatt_client_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static void microphone_control_service_client_setup(void){
    // Init L2CAP
    l2cap_init();

    // Setup ATT server - only needed if LE Peripheral does ATT queries on its own, e.g. Android phones
    att_server_init(profile_data, NULL, NULL);    

    // GATT Client setup
    gatt_client_init();
    
    microphone_control_service_client_init();
    audio_input_control_service_client_init();

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

static void hci_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    bd_addr_t address;

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
            printf("BTstack activated, start scanning\n");
            // app_state = APP_STATE_W4_SCAN_RESULT;
            // gap_start_scan();
            break;

        case GAP_EVENT_ADVERTISING_REPORT:
            if (app_state != APP_STATE_W4_SCAN_RESULT) return;

            gap_event_advertising_report_get_address(packet, address);
            dump_advertising_report(packet);

            if (memcmp(address, public_pts_address, 6) != 0){
                break;
            }
            // stop scanning, and connect to the device
            app_state = APP_STATE_W4_CONNECT;
            gap_stop_scan();
            printf("Stop scan. Connect to device with addr %s.\n", bd_addr_to_str(report.address));
            gap_connect(report.address,report.address_type);
            break;

        case HCI_EVENT_META_GAP:
            // Wait for connection complete
            if (hci_event_gap_meta_get_subevent_code(packet) !=  GAP_SUBEVENT_LE_CONNECTION_COMPLETE) break;
            
            // Get connection handle from event
            connection_handle = gap_subevent_le_connection_complete_get_connection_handle(packet);
            gap_stop_scan();
            app_state = APP_STATE_CONNECTED;
            printf("GAP connected.\n");
            (void) microphone_control_service_client_connect(connection_handle, &gatt_client_event_handler,
                                                             &mics_connection,
                                                             mics_storage_for_characteristics, MICS_CHARACTERISTICS_MAX_NUM,
                                                             aics_connections, AICS_CONNECTIONS_MAX_NUM,
                                                             aics_storage_for_characteristics, AICS_CHARACTERISTICS_MAX_NUM, &mics_cid);
            break;

        case HCI_EVENT_DISCONNECTION_COMPLETE:
            connection_handle = HCI_CON_HANDLE_INVALID;
            // Disconnect battery service

            microphone_control_service_client_disconnect(mics_cid);
            printf("Disconnected\n");

//            printf("Restart scan.\n");
//            app_state = APP_STATE_W4_SCAN_RESULT;
//            gap_start_scan();
            break;
        default:
            break;
    }
}

static void gatt_client_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(size);

    uint8_t status;

    if (hci_event_packet_get_type(packet) != HCI_EVENT_LEAUDIO_META) {
        return;
    }

    uint8_t aics_index;
    switch (hci_event_leaudio_meta_get_subevent_code(packet)) {
        case LEAUDIO_SUBEVENT_MICS_CLIENT_CONNECTED:
            status = leaudio_subevent_mics_client_connected_get_att_status(packet);
            switch (status) {
                case ERROR_CODE_SUCCESS:
                    printf("Microphone Control service client connected, num included AICS services %d\n",
                           leaudio_subevent_mics_client_connected_get_aics_services_num(packet));
                    break;
                default:
                    printf("Microphone Control service client connection failed, err 0x%02x.\n", status);
                    gap_disconnect(connection_handle);
                    break;
            }
            break;

        case LEAUDIO_SUBEVENT_MICS_CLIENT_MUTE:
            printf("Mute: %d\n", leaudio_subevent_mics_client_mute_get_state(packet));
            break;

        case LEAUDIO_SUBEVENT_MICS_CLIENT_AUDIO_INPUT_STATE:
            aics_index = leaudio_subevent_mics_client_audio_input_state_get_aics_index(packet);
            printf("MICS Client Test: received AICS[%d] event AUDIO_INPUT_STATE\n", aics_index);
            status = leaudio_subevent_mics_client_audio_input_state_get_att_status(packet);
            if (status != ATT_ERROR_SUCCESS){
                printf("Read failed, 0x%02x\n", status);
                break;
            }

            printf("Gain Setting:     %d\n", (int8_t)leaudio_subevent_mics_client_audio_input_state_get_gain_setting(packet));
            printf("Mute:             %d\n",  leaudio_subevent_mics_client_audio_input_state_get_mute(packet));
            printf("Gain Mode:        %d\n",  leaudio_subevent_mics_client_audio_input_state_get_gain_mode(packet));
            printf("Chainge Counter:  %d\n", leaudio_subevent_mics_client_audio_input_state_get_change_counter(packet));
            break;

        case LEAUDIO_SUBEVENT_MICS_CLIENT_GAIN_SETTINGS_PROPERTIES:
            aics_index = leaudio_subevent_mics_client_gain_settings_properties_get_aics_index(packet);
            printf("MICS Client Test: received AICS[%d] event GAIN_SETTINGS_PROPERTIES\n", aics_index);
            status = leaudio_subevent_mics_client_gain_settings_properties_get_att_status(packet);
            if (status != ATT_ERROR_SUCCESS){
                printf("Read failed, 0x%02x\n", status);
                break;
            }
            printf("Gain Setting Step Size: %d\n", leaudio_subevent_mics_client_gain_settings_properties_get_units(packet));
            printf("Gain Setting Minimum:   %d\n", (int8_t)leaudio_subevent_mics_client_gain_settings_properties_get_minimum_value(packet));
            printf("Gain Setting Maximum:   %d\n", (int8_t)leaudio_subevent_mics_client_gain_settings_properties_get_maximum_value(packet));
            break;

        case LEAUDIO_SUBEVENT_MICS_CLIENT_AUDIO_INPUT_TYPE:
            aics_index = leaudio_subevent_mics_client_audio_input_type_get_aics_index(packet);
            printf("MICS Client Test: received AICS[%d] event AUDIO_INPUT_TYPE\n", aics_index);
            status = leaudio_subevent_mics_client_audio_input_type_get_att_status(packet);
            if (status != ATT_ERROR_SUCCESS){
                printf("Read failed, 0x%02x\n", status);
                break;
            }
            printf("Input type:  %d\n", leaudio_subevent_mics_client_audio_input_type_get_input_type(packet));
            break;

        case LEAUDIO_SUBEVENT_MICS_CLIENT_AUDIO_INPUT_STATUS:
            aics_index = leaudio_subevent_mics_client_audio_input_status_get_aics_index(packet);
            printf("MICS Client Test: received AICS[%d] event AUDIO_INPUT_STATUS\n", aics_index);
            status = leaudio_subevent_mics_client_audio_input_status_get_att_status(packet);
            if (status != ATT_ERROR_SUCCESS){
                printf("Read failed, 0x%02x\n", status);
                break;
            }
            printf("Input status:  %d\n", leaudio_subevent_mics_client_audio_input_status_get_input_status(packet));
            break;

        case LEAUDIO_SUBEVENT_MICS_CLIENT_AUDIO_DESCRIPTION:
            aics_index = leaudio_subevent_mics_client_audio_description_get_aics_index(packet);
            printf("MICS Client Test: received AICS[%d] event AUDIO_DESCRIPTION\n", aics_index);
            status = leaudio_subevent_mics_client_audio_description_get_att_status(packet);
            if (status != ATT_ERROR_SUCCESS){
                printf("Read failed, 0x%02x\n", status);
                break;
            }
            printf("Audio description:  %s\n", leaudio_subevent_mics_client_audio_description_get_value(packet));
            break;

        case LEAUDIO_SUBEVENT_MICS_CLIENT_WRITE_DONE:
            aics_index = leaudio_subevent_mics_client_write_done_get_aics_index(packet);
            printf("MICS Client Test: received AICS[%d] event WRITE_DONE\n", aics_index);
            status = leaudio_subevent_mics_client_write_done_get_att_status(packet);
            if (status != ATT_ERROR_SUCCESS){
                printf("Write failed, 0x%02x\n", status);
            } else {
                printf("Write successful\n");
            }
            break;

        case LEAUDIO_SUBEVENT_MICS_CLIENT_DISCONNECTED:
            printf("Microphone Control service client disconnected\n");
            break;

        default:
            break;
    }
}

static void show_usage(void){
    uint8_t iut_address_type;
    bd_addr_t      iut_address;
    gap_le_get_own_address(&iut_address_type, iut_address);

    printf("--- MICS Client ---\n");
    printf("c    - Connect to %s\n", bd_addr_to_str(current_pts_address));
    printf("r    - Read Mute state\n");
    printf("m/M  - Mute ON/OFF\n");
    printf("g    - write gain setting, AICS[%d] client\n", aics_connection_index);
    printf("G    - write manual gain mode, AICS[%d] client\n", aics_connection_index);
    printf("h    - write automatic gain mode, AICS[%d] client\n", aics_connection_index);
    printf("H    - read input description, AICS[%d] client\n", aics_connection_index);
    printf("i    - write input description, AICS[%d] client\n", aics_connection_index);
    printf("I    - read input state, AICS[%d] client\n", aics_connection_index);
    printf("j    - read gain setting properties, AICS[%d] client\n", aics_connection_index);
    printf("J    - read input type, AICS[%d] client\n", aics_connection_index);
    printf("n    - read input status, AICS[%d] client\n", aics_connection_index);
    printf("o    - unmute, AICS[%d] client\n", aics_connection_index);
    printf("O    - mute, AICS[%d] client\n", aics_connection_index);
    printf("q    - write incorrect gain setting 110, AICS[%d] client\n", aics_connection_index);
    printf("Q    - qrite incorrect gain setting 110, AICS[%d] client\n", aics_connection_index);
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
            status = microphone_control_service_client_connect(connection_handle, &gatt_client_event_handler,
                                                                 &mics_connection,
                                                                 mics_storage_for_characteristics, MICS_CHARACTERISTICS_MAX_NUM,
                                                                 aics_connections, AICS_CONNECTIONS_MAX_NUM,
                                                                 aics_storage_for_characteristics, AICS_CHARACTERISTICS_MAX_NUM, &mics_cid);

            break;

        case 'r':
            printf("Read Mute state\n");
            status = microphone_control_service_client_read_mute_state(mics_cid);
            break;

        case 'm':
            printf("Mute ON\n");
            status = microphone_control_service_client_mute(mics_cid);
            break;

        case 'M':
            printf("Mute OFF\n");
            status = microphone_control_service_client_unmute(mics_cid);
            break;

        case 'g':
            printf("Write gain setting\n");
            status = microphone_control_service_client_write_gain_setting(mics_cid, aics_connection_index, 100);
            break;

        case 'G':
            printf("Write manual gain mode\n");
            status = microphone_control_service_client_write_manual_gain_mode(mics_cid, aics_connection_index);
            break;

        case 'h':
            printf("Write automatic gain mode\n");
            status = microphone_control_service_client_write_automatic_gain_mode(mics_cid, aics_connection_index);
            break;

        case 'H':
            printf("Read input description\n");
            status = microphone_control_service_client_read_input_description(mics_cid, aics_connection_index);
            break;

        case 'i':
            printf("Write input description\n");
            status = microphone_control_service_client_write_input_description(mics_cid, aics_connection_index, audio_input_description);
            break;

        case 'I':
            printf("Read input state\n");
            status = microphone_control_service_client_read_input_state(mics_cid, aics_connection_index);
            break;

        case 'j':
            printf("Read gain setting properties\n");
            status = microphone_control_service_client_read_gain_setting_properties(mics_cid, aics_connection_index);
            break;

        case 'J':
            printf("Read input type\n");
            status = microphone_control_service_client_read_input_type(mics_cid, aics_connection_index);
            break;

        case 'n':
            printf("Read input status\n");
            status = microphone_control_service_client_read_input_status(mics_cid, aics_connection_index);
            break;

        case 'o':
            printf("Write unmute\n");
            status = microphone_control_service_client_write_unmute(mics_cid, aics_connection_index);
            break;

        case 'O':
            printf("Write mute\n");
            status = microphone_control_service_client_write_mute(mics_cid, aics_connection_index);
            break;

        case 'q':
            printf("Write incorrect gain setting 110\n");
            status = microphone_control_service_client_write_gain_setting(mics_cid, aics_connection_index, 110);
            break;

        case 'Q':
            printf("Write incorrect gain setting -110\n");
            status = microphone_control_service_client_write_gain_setting(mics_cid, aics_connection_index, -110);
            break;
        default:
            show_usage();
            break;
    }

    if (status != ERROR_CODE_SUCCESS){
        printf("MICS Client Test: command %c failed due to 0%02x\n", c, status);
    }
}

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    UNUSED(argc);
    (void)argv;

    btstack_stdin_setup(stdin_process);

    microphone_control_service_client_setup();
    memcpy(current_pts_address, public_pts_address, 6);
    current_pts_address_type = public_pts_address_type;

    app_state = APP_STATE_IDLE;

    // turn on!
    hci_power_control(HCI_POWER_ON);
    return 0;
}


