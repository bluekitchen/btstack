/*
 * Copyright (C) 2018 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "bap_service_client_test.c"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <btstack_lc3.h>

#include "btstack.h"
#include "btstack_config.h"

#define BASS_CLIENT_NUM_SOURCES 3
#define ASCS_CLIENT_NUM_STREAMENDPOINTS 4
#define MAX_CHANNELS 2

static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_packet_callback_registration_t sm_event_callback_registration;
static void show_usage(void);

static const char * bap_app_server_addr_string = "C007E8414591";

static bd_addr_t        bap_app_server_addr;
static hci_con_handle_t bap_app_client_con_handle;
static char operation_cmd = 0;

// MCS
#define MCS_CLIENT_CHARACTERISTICS_MAX_NUM 25
static uint16_t mcs_cid;
static mcs_client_connection_t mcs_connection;
static gatt_service_client_characteristic_t mcs_connection_characteristics[MCS_CLIENT_CHARACTERISTICS_MAX_NUM];

typedef enum {
    BAP_APP_CLIENT_STATE_IDLE = 0,
    BAP_APP_CLIENT_STATE_W4_GAP_CONNECTED,
    BAP_APP_CLIENT_STATE_W4_BASS_CONNECTED,
    BAP_APP_CLIENT_STATE_W4_PACS_CONNECTED,
    BAP_APP_CLIENT_STATE_W4_MCS_CONNECTED,
    BAP_APP_CLIENT_STATE_CONNECTED,
    BAP_APP_W4_CIG_COMPLETE,
    BAP_APP_W4_CIS_CREATED,
    BAP_APP_STREAMING,
    BAP_APP_CLIENT_STATE_W4_GAP_DISCONNECTED,
} bap_app_client_state_t; 

static bap_app_client_state_t bap_app_client_state = BAP_APP_CLIENT_STATE_IDLE;

static void bap_client_reset(void){
    bap_app_client_state      = BAP_APP_CLIENT_STATE_IDLE;
    bap_app_client_con_handle = HCI_CON_HANDLE_INVALID;
}

static void hci_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    bd_addr_t addr;
    uint8_t event = hci_event_packet_get_type(packet);
    
    switch (event) {
        case BTSTACK_EVENT_STATE:
            switch(btstack_event_state_get_state(packet)){
                case HCI_STATE_WORKING:
                    bap_client_reset();
                    show_usage();
                    printf("Please select sample frequency and variation\n");
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
            bap_client_reset();
            printf("BAP  Client: disconnected from %s\n", bd_addr_to_str(bap_app_server_addr));
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

        // case GAP_EVENT_EXTENDED_ADVERTISING_REPORT:
        // {
        //     gap_event_extended_advertising_report_get_address(packet, source_data1.address);
        //     source_data1.address_type = gap_event_extended_advertising_report_get_address_type(packet);
        //     source_data1.adv_sid = gap_event_extended_advertising_report_get_advertising_sid(packet);

        //     uint8_t adv_size = gap_event_extended_advertising_report_get_data_length(packet);
        //     const uint8_t * adv_data = gap_event_extended_advertising_report_get_data(packet);

        //     ad_context_t context;
        //     bool found = false;
        //     remote_name[0] = '\0';
        //     uint16_t uuid;
        //     for (ad_iterator_init(&context, adv_size, adv_data) ; ad_iterator_has_more(&context) ; ad_iterator_next(&context)) {
        //         uint8_t data_type = ad_iterator_get_data_type(&context);
        //         const uint8_t *data = ad_iterator_get_data(&context);
        //         switch (data_type){
        //             case BLUETOOTH_DATA_TYPE_SERVICE_DATA_16_BIT_UUID:
        //                 uuid = little_endian_read_16(data, 0);
        //                 if (uuid == ORG_BLUETOOTH_SERVICE_BROADCAST_AUDIO_ANNOUNCEMENT_SERVICE){
        //                     found = true;
        //                 }
        //                 break;
        //             default:
        //                 break;
        //         }
        //     }
        //     if (!found) break;
        //     remote_type = gap_event_extended_advertising_report_get_address_type(packet);
        //     remote_sid = gap_event_extended_advertising_report_get_advertising_sid(packet);
        //     pts_mode = strncmp("PTS-", remote_name, 4) == 0;
        //     count_mode = strncmp("COUNT", remote_name, 5) == 0;
        //     printf("Remote Broadcast sink found, addr %s, name: '%s' (pts-mode: %u, count: %u)\n", bd_addr_to_str(remote), remote_name, pts_mode, count_mode);
        //     gap_stop_scan();
        //     break;
        // }

        case HCI_EVENT_LE_META:
            switch (hci_event_le_meta_get_subevent_code(packet)){
                case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
                    bap_app_client_con_handle = hci_subevent_le_connection_complete_get_connection_handle(packet);
                    bap_app_client_state      = BAP_APP_CLIENT_STATE_CONNECTED;
                    hci_subevent_le_connection_complete_get_peer_address(packet, bap_app_server_addr);
                    printf("BAP  Client: connection to %s established, con_handle 0x%04x\n", bd_addr_to_str(bap_app_server_addr), bap_app_client_con_handle);
                    break;
                default:
                    return;
            }
            break;

        default:
            break;
    }
}

static void mcs_client_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_GATTSERVICE_META) return;

    switch (hci_event_gattservice_meta_get_subevent_code(packet)){
        case GATTSERVICE_SUBEVENT_MCS_CLIENT_CONNECTED:
            if (bap_app_client_con_handle != gattservice_subevent_mcs_client_connected_get_con_handle(packet)){
                printf("MCS Client: expected con handle 0x%04x, received 0x%04x\n", bap_app_client_con_handle, gattservice_subevent_mcs_client_connected_get_con_handle(packet));
                return;
            }

            bap_app_client_state = BAP_APP_CLIENT_STATE_CONNECTED;

            if (gattservice_subevent_mcs_client_connected_get_status(packet) != ERROR_CODE_SUCCESS){
                printf("MCS client: connection failed, cid 0x%04x, con_handle 0x%04x, status 0x%02x\n", mcs_cid, bap_app_client_con_handle, 
                    gattservice_subevent_mcs_client_connected_get_status(packet));
                return;
            }
            printf("MCS client: connected, cid 0x%04x\n", mcs_cid);
            break;

        case GATTSERVICE_SUBEVENT_MCS_CLIENT_MEDIA_PLAYER_NAME:
            printf("MCS Client: Media Player Name \"%s\"\n", (char *) gattservice_subevent_mcs_client_media_player_name_get_value(packet));
            break;
        case GATTSERVICE_SUBEVENT_MCS_CLIENT_MEDIA_PLAYER_ICON_OBJECT_ID:
            break;
        case GATTSERVICE_SUBEVENT_MCS_CLIENT_MEDIA_PLAYER_ICON_OBJECT_URL:
            break;
        case GATTSERVICE_SUBEVENT_MCS_CLIENT_TRACK_CHANGED:
            break;
        case GATTSERVICE_SUBEVENT_MCS_CLIENT_TRACK_TITLE:
            break;
        case GATTSERVICE_SUBEVENT_MCS_CLIENT_TRACK_DURATION:
            break;
        case GATTSERVICE_SUBEVENT_MCS_CLIENT_TRACK_POSITION:
            break;
        // case GATTSERVICE_SUBEVENT_MSC_CLIENT_TRACK_SPEED:
            // break;
        case GATTSERVICE_SUBEVENT_MSC_CLIENT_SEEKING_SPEED:
            break;
        case GATTSERVICE_SUBEVENT_MCS_CLIENT_CURRENT_TRACK_SEGMENTS_OBJECT_ID:
            break;
        case GATTSERVICE_SUBEVENT_MCS_CLIENT_CURRENT_TRACK_OBJECT_ID:
            break;
        case GATTSERVICE_SUBEVENT_MCS_CLIENT_NEXT_TRACK_OBJECT_ID:
            break;
        case GATTSERVICE_SUBEVENT_MCS_CLIENT_PARENT_GROUP_OBJECT_ID:
            break;
        case GATTSERVICE_SUBEVENT_MCS_CLIENT_PLAYING_ORDER:
            break;
        case GATTSERVICE_SUBEVENT_MCS_CLIENT_PLAYING_ORDER_SUPPORTED:
            break;         
        case GATTSERVICE_SUBEVENT_MCS_CLIENT_MEDIA_STATE:
            break;
        case GATTSERVICE_SUBEVENT_MCS_CLIENT_CONTROL_POINT_OPCODES_SUPPORTED:
            break;
        case GATTSERVICE_SUBEVENT_MCS_CLIENT_CONTROL_POINT_NOTIFICATION:
            break;
        case GATTSERVICE_SUBEVENT_MCS_CLIENT_SEARCH_CONTROL_POINT_NOTIFICATION:
            break;
        case GATTSERVICE_SUBEVENT_MCS_CLIENT_SEARCH_RESULT_OBJECT_ID:
            break;
        case GATTSERVICE_SUBEVENT_MCS_CLIENT_CONTENT_CONTROL_ID:
            break;
        case GATTSERVICE_SUBEVENT_MCS_CLIENT_DISCONNECTED:
            mcs_cid = 0;
            printf("MCS Client: disconnected\n");
            break;
        default:
            break;
    }
}

static void show_usage(void){
    bd_addr_t      iut_address;
    gap_local_bd_addr(iut_address);
    printf("\n--- BAP Client Test Console %s ---\n", bd_addr_to_str(iut_address));
    printf("c   - connect to %s\n", bap_app_server_addr_string);    
    printf("X   - disconnect from %s\n", bap_app_server_addr_string);       
    
    printf("\n--- MCS Client Test Console %s ---\n", bd_addr_to_str(iut_address));
    printf("a   - connect to %s\n", bap_app_server_addr_string);
    printf("b   - get media player name\n");
    printf("\n");
    printf(" \n");
    printf(" \n");
    printf("Ctrl-c - exit\n");
    printf("---\n");
}

static void stdin_process(char cmd){
    uint8_t status = ERROR_CODE_SUCCESS;
    operation_cmd = cmd;
    
    switch (cmd){
        case 'c':
            printf("Connecting...\n");
            bap_app_client_state = BAP_APP_CLIENT_STATE_W4_GAP_CONNECTED;
            status = gap_connect(bap_app_server_addr, 0);
            break;
        
        case 'X':
            printf("Disconnect...\n");
            bap_app_client_state = BAP_APP_CLIENT_STATE_W4_GAP_DISCONNECTED;
            status = gap_disconnect(bap_app_client_con_handle);
            break;

        case 'a':
            printf("MCS: connect 0x%02x\n", bap_app_client_con_handle);
            bap_app_client_state = BAP_APP_CLIENT_STATE_W4_MCS_CONNECTED;
            status = media_control_service_client_connect(
                bap_app_client_con_handle, 
                &mcs_connection, mcs_connection_characteristics, MCS_CLIENT_CHARACTERISTICS_MAX_NUM,
                &mcs_client_event_handler, &mcs_cid);
            break;

        case 'b':
            printf("MCS: get media player name\n");
            media_control_service_client_get_media_player_name(mcs_cid);
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

    le_device_db_init();
    
    sm_init();
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    sm_set_authentication_requirements(SM_AUTHREQ_SECURE_CONNECTION);
    sm_event_callback_registration.callback = &hci_event_handler;
    sm_add_event_handler(&sm_event_callback_registration);

    media_control_service_client_init();


    hci_event_callback_registration.callback = &hci_event_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // parse human readable Bluetooth address
    sscanf_bd_addr(bap_app_server_addr_string, bap_app_server_addr);
    btstack_stdin_setup(stdin_process);

    // turn on!
    hci_power_control(HCI_POWER_ON);

    return 0;
}
/* EXAMPLE_END */
