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

#define BTSTACK_FILE__ "gatt_profiles_bap.c"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "gatt_profiles_bap.h"
#include "btstack.h"

#define CSIS_COORDINATORS_MAX_NUM  3 

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static uint16_t att_read_callback(hci_con_handle_t con_handle, uint16_t att_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size);
static int att_write_callback(hci_con_handle_t con_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size);

static int  le_notification_enabled;
static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_packet_callback_registration_t sm_event_callback_registration;

typedef enum {
    BAP_APP_SERVER_STATE_IDLE = 0,
    BAP_APP_SERVER_STATE_CONNECTED
} bap_app_server_state_t; 

static bap_app_server_state_t  bap_app_server_state      = BAP_APP_SERVER_STATE_IDLE;
static hci_con_handle_t        bap_app_server_con_handle = HCI_CON_HANDLE_INVALID;
static bd_addr_t               bap_app_client_addr;

// MCS
static char * long_string1 = "abcdefghijkabcdefghijkabcdefghijkabcdefghijkabcdefghijk";
static char * long_string2 = "ghijkabcdefghijkabcdefghijkabcdefghijkabcdefghijkabcdefgfgfgf";

#define APP_AD_FLAGS 0x06

static uint8_t adv_data[] = {
    // Flags general discoverable
    0x02, BLUETOOTH_DATA_TYPE_FLAGS, APP_AD_FLAGS,
    // RSI
    0x07, BLUETOOTH_DATA_TYPE_RESOLVABLE_SET_IDENTIFIER, 0x28, 0x31, 0xB6, 0x4C, 0x39, 0xCC,
    // CSIS
    0x03, BLUETOOTH_DATA_TYPE_INCOMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS, 0x46, 0x18,
    // Name
    0x04, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, 'I', 'U', 'T', 
};

const uint8_t adv_data_len = sizeof(adv_data);

static ascs_codec_configuration_t ascs_codec_configuration = {
    0x01, 0x00, 0x00, 0x0FA0,
    0x000005, 0x00FFAA, 0x000000, 0x000000,
    HCI_AUDIO_CODING_FORMAT_LC3, 0x0000, 0x0000,
    {   0x1E,
        0x03, 0x01, 0x00000001, 0x0028, 0x00
    }
};

static csis_server_connection_t csis_coordiantors[CSIS_COORDINATORS_MAX_NUM];

static uint8_t ase_id = 0;

// ASCS
#define ASCS_NUM_STREAMENDPOINT_CHARACTERISTICS 5
#define ASCS_NUM_CLIENTS 3

static ascs_streamendpoint_characteristic_t ascs_streamendpoint_characteristics[ASCS_NUM_STREAMENDPOINT_CHARACTERISTICS];
static ascs_server_connection_t ascs_clients[ASCS_NUM_CLIENTS];

// MCS
static uint16_t media_player_id1 = 0;
static media_control_service_server_t media_player1;

static uint16_t media_player_id2 = 0;
static media_control_service_server_t media_player2;

static uint8_t sirk[] = {
    0x83, 0x8E, 0x68, 0x05, 0x53, 0xF1, 0x41, 0x5A,
    0xA2, 0x65, 0xBB, 0xAF, 0xC6, 0xEA, 0x03, 0xB8   
};

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;

    switch (hci_event_packet_get_type(packet)) {
        case ATT_EVENT_CAN_SEND_NOW:
            // att_server_notify(con_handle, ATT_CHARACTERISTIC_0000FF11_0000_1000_8000_00805F9B34FB_01_VALUE_HANDLE, (uint8_t*) counter_string, counter_string_len);
            break;
       case SM_EVENT_JUST_WORKS_REQUEST:
            printf("Accept Just Works\n");
            sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
            break;
        case SM_EVENT_NUMERIC_COMPARISON_REQUEST:
            printf("Confirming numeric comparison: %"PRIu32"\n", sm_event_numeric_comparison_request_get_passkey(packet));
            sm_numeric_comparison_confirm(sm_event_passkey_display_number_get_handle(packet));
            break;

        case HCI_EVENT_META_GAP:
            switch (hci_event_gap_meta_get_subevent_code(packet)){
                case GAP_SUBEVENT_LE_CONNECTION_COMPLETE:
                    bap_app_server_con_handle = gap_subevent_le_connection_complete_get_connection_handle(packet);
                    bap_app_server_state      = BAP_APP_SERVER_STATE_CONNECTED;
                    gap_subevent_le_connection_complete_get_peer_address(packet, bap_app_client_addr);
                    printf("BAP Server: Connection to %s established\n", bd_addr_to_str(bap_app_client_addr));
                    break;
                default:
                    return;
            }
            break;

        case HCI_EVENT_DISCONNECTION_COMPLETE:
            bap_app_server_con_handle = HCI_CON_HANDLE_INVALID;
            bap_app_server_state      = BAP_APP_SERVER_STATE_IDLE;

            le_notification_enabled = 0;

            printf("BAP Server: Disconnected from %s\n", bd_addr_to_str(bap_app_client_addr));
            break;

        default:
            break;
    }
}


static void pacs_server_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_LEAUDIO_META) return;

    switch (hci_event_gattservice_meta_get_subevent_code(packet)){
        case LEAUDIO_SUBEVENT_PACS_SERVER_AUDIO_LOCATIONS:
            printf("PACS: audio location received\n");
            break;
        
        default:
            break;
    }
}

static void csis_server_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_LEAUDIO_META) return;

    hci_con_handle_t con_handle;
    uint8_t status;
    uint8_t ris[6];

    switch (hci_event_gattservice_meta_get_subevent_code(packet)){
        
        case LEAUDIO_SUBEVENT_CSIS_SERVER_CONNECTED:
            con_handle = leaudio_subevent_csis_server_connected_get_con_handle(packet);
            status =     leaudio_subevent_csis_server_connected_get_status(packet);

            if (status != ERROR_CODE_SUCCESS){
                printf("CSIS Server: connection to coordinator failed, con_handle 0x%02x, status 0x%02x\n", con_handle, status);
                return;
            }
            printf("CSIS Server: connected, con_handle 0x%02x\n", con_handle);
            break;

        case LEAUDIO_SUBEVENT_CSIS_SERVER_RSI:
            printf("CSIS Server: RSI \n");
            leaudio_subevent_csis_server_rsi_get_rsi(packet, ris);
            reverse_48(ris, &adv_data[5]);
            printf_hexdump(ris, 6);
            //
            gap_advertisements_set_data(adv_data_len, (uint8_t*) adv_data);
            gap_advertisements_enable(1);
            break;

        case LEAUDIO_SUBEVENT_CSIS_SERVER_DISCONNECTED:
            con_handle = leaudio_subevent_csis_server_disconnected_get_con_handle(packet);
            printf("CSIS Server: RELEASED\n");
            break;

        default:
            break;
    }
}

static void mcs_server_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_GATTSERVICE_META) return;

    hci_con_handle_t con_handle;
    uint8_t status;
    uint8_t ris[6];

    switch (hci_event_gattservice_meta_get_subevent_code(packet)){
        default:
            break;
    }
}

// ATT Client Read Callback for Dynamic Data
// - if buffer == NULL, don't copy data, just return size of value
// - if buffer != NULL, copy data and return number bytes copied
// @param offset defines start of attribute value
static uint16_t att_read_callback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
    UNUSED(connection_handle);
    UNUSED(att_handle);
    UNUSED(offset);
    UNUSED(buffer);
    UNUSED(buffer_size);

    return 0;
}

static int att_write_callback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
    UNUSED(connection_handle);
    UNUSED(att_handle);
    UNUSED(transaction_mode);
    UNUSED(offset);
    UNUSED(buffer);
    UNUSED(buffer_size);
    return 0;
}

static void show_usage(void){
    uint8_t iut_address_type;
    bd_addr_t      iut_address;
    gap_le_get_own_address(&iut_address_type, iut_address);

    printf("IUT: addr type %u, addr %s", iut_address_type, bd_addr_to_str(iut_address));

    printf("\n## CSIS\n");
    printf("g - set OOB Sirk only\n");
    printf("G - set Encrypted Sirk\n");
    printf("h - Simulate server locked by another remote coordinator\n");
    printf("H - Simulate server unlock by another remote coordinator\n");
    printf("i - generate RIS\n");
    printf("I - set SIRK\n");

    printf("\n## MCS\n");
    printf("j - set long media player name\n");

}

static void stdin_process(char cmd){
    printf("%c\n", cmd);
    uint8_t status = ERROR_CODE_SUCCESS;

    switch (cmd){

        case 'g':
            printf("Set OOB Sirk only\n");
            coordinated_set_identification_service_server_set_sirk(CSIS_SIRK_TYPE_PUBLIC, &sirk[0], true);
            break;
        case 'G':
            printf("Set Encrypted Sirk\n");
            coordinated_set_identification_service_server_set_sirk(CSIS_SIRK_TYPE_ENCRYPTED, &sirk[0], false);
            break;
        case 'h':{
            printf("Simulate server locked by another remote member\n");
            hci_con_handle_t con_handle = 0x00AA;
            coordinated_set_identification_service_server_simulate_member_connected(con_handle);
            status = coordinated_set_identification_service_server_simulate_set_lock(con_handle, CSIS_MEMBER_LOCKED);
            if (status == ERROR_CODE_SUCCESS){
                printf("Server locked by member with con_handle 0x%02x\n", con_handle);
                break;
            }        
        
            printf("Server lock failed, status 0x%02x\n", status);
            
        }   
        case 'H':{
            printf("Simulate server unlock by another remote member\n");
            hci_con_handle_t con_handle = 0x00AA;
            status = coordinated_set_identification_service_server_simulate_set_lock(con_handle, CSIS_MEMBER_UNLOCKED);
            if (status == ERROR_CODE_SUCCESS){
                printf("Server unlocked by member with con_handle 0x%02x\n", con_handle);
                break;
            }        
        
            printf("Server unlock failed, status 0x%02x\n", status);
            break;
        }
        
        case 'i':
            coordinated_set_identification_service_server_generate_rsi();
            break;

        case 'I':{
            uint8_t test_sirk[] = {0x83, 0x8E, 0x68, 0x05, 0x53, 0xF1, 0x41, 0x5A, 0xA2, 0x65, 0xBB, 0xAF, 0xC6, 0xEA, 0x03, 0xB8}; 
            coordinated_set_identification_service_server_set_sirk(CSIS_SIRK_TYPE_ENCRYPTED, &test_sirk[0], false);
            // coordinated_set_identification_service_server_calculate_encrypted_sirk();
            break;
        }

        case '\n':
        case '\r':
            break;
        default:
            show_usage();
            break;
    }

    if (status != ERROR_CODE_SUCCESS){
        printf("Command '%c' could not be performed, status 0x%02x\n", cmd, status);
    }
}

int btstack_main(void);
int btstack_main(void)
{
    
    l2cap_init();

    // setup le device db
    le_device_db_init();

    // setup SM: Display only
    sm_init();
    // register for SM events
    sm_event_callback_registration.callback = &packet_handler;
    sm_add_event_handler(&sm_event_callback_registration);
    sm_set_authentication_requirements(SM_AUTHREQ_BONDING);
    sm_allow_ltk_reconstruction_without_le_device_db_entry(0);
    // setup ATT server
    att_server_init(profile_data, att_read_callback, att_write_callback);    
    // att_server_register_packet_handler(packet_handler);

    // setup MCS
    media_control_service_server_init();

    ots_object_id_t icon_object_id = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    char * icon_url = "https://www.bluetooth.com/";

    media_control_service_server_register_player(&media_player1,
        &mcs_server_packet_handler, 0xFFFF, 
        &media_player_id1);
    media_control_service_server_set_media_player_name(media_player_id1, "BK Player1");
    media_control_service_server_set_icon_object_id(media_player_id1, &icon_object_id);
    media_control_service_server_set_icon_url(media_player_id1, icon_url);
    media_control_service_server_set_track_title(media_player_id1, "Track Title 1");
    media_control_service_server_set_playing_orders_supported(media_player_id1, 0x3FF);
    media_control_service_server_set_playing_order(media_player_id1, PLAYING_ORDER_IN_ORDER_ONCE);

    coordinated_set_identification_service_server_init(CSIS_COORDINATORS_MAX_NUM, &csis_coordiantors[0], CSIS_COORDINATORS_MAX_NUM, 1);
    coordinated_set_identification_service_server_register_packet_handler(&csis_server_packet_handler);
    coordinated_set_identification_service_server_set_sirk(CSIS_SIRK_TYPE_PUBLIC, &sirk[0], false);
    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // setup advertisements
    uint16_t adv_int_min = 0x0030;
    uint16_t adv_int_max = 0x0030;
    uint8_t adv_type = 0;
    bd_addr_t null_addr;
    memset(null_addr, 0, 6);
    gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);
    gap_advertisements_set_data(adv_data_len, (uint8_t*) adv_data);
    gap_advertisements_enable(1);

    btstack_stdin_setup(stdin_process);

    // turn on!
	hci_power_control(HCI_POWER_ON);
	    
    return 0;
}
