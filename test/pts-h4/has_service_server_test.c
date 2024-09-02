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

#define BTSTACK_FILE__ "has_service_server_test.c"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "has_service_server_test.h"
#include "btstack.h"

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static uint16_t att_read_callback(hci_con_handle_t con_handle, uint16_t att_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size);
static int att_write_callback(hci_con_handle_t con_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size);

static int  le_notification_enabled;
static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_packet_callback_registration_t sm_event_callback_registration;

typedef enum {
    HAS_APP_SERVER_STATE_IDLE = 0,
    HAS_APP_SERVER_STATE_CONNECTED
} has_app_server_state_t; 

static has_app_server_state_t  has_app_server_state      = HAS_APP_SERVER_STATE_IDLE;
static hci_con_handle_t        has_app_server_con_handle = HCI_CON_HANDLE_INVALID;
static hci_con_handle_t        has_app_server_cis_handle = HCI_CON_HANDLE_INVALID;
static bd_addr_t               has_client_addr;

#define APP_AD_FLAGS 0x06

static uint8_t adv_data[] = {
    // Flags general discoverable
    0x02, BLUETOOTH_DATA_TYPE_FLAGS, APP_AD_FLAGS,
    // RSI
    0x07, BLUETOOTH_DATA_TYPE_RESOLVABLE_SET_IDENTIFIER, 0x28, 0x31, 0xB6, 0x4C, 0x39, 0xCC,
    // HAS
    0x03, BLUETOOTH_DATA_TYPE_SERVICE_DATA_16_BIT_UUID, 0x54, 0x18,
    // Name
    0x04, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, 'I', 'U', 'T',
};

static const uint8_t adv_sid = 0;
static le_advertising_set_t le_advertising_set;
static uint8_t adv_handle = 0;

const uint8_t adv_data_len = sizeof(adv_data);

static const le_extended_advertising_parameters_t extended_params = {
        .advertising_event_properties = 1,  // connectable
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


static void setup_advertising(void) {
    gap_extended_advertising_setup(&le_advertising_set, &extended_params, &adv_handle);
    gap_extended_advertising_set_adv_data(adv_handle, sizeof(adv_data), adv_data);
    gap_extended_advertising_start(adv_handle, 0, 0);
}

// HAS
#define HAS_NUM_CLIENTS 1
#define HAS_NUM_PRESETS 15

static has_preset_record_t has_app_preset_records[HAS_NUM_PRESETS];
static has_server_connection_t has_app_clients[HAS_NUM_CLIENTS];
static uint8_t hearing_aid_features = 0x3F;

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

       case HCI_EVENT_LE_META:
            switch(hci_event_le_meta_get_subevent_code(packet)) {
                case HCI_SUBEVENT_LE_CIS_REQUEST:
                    has_app_server_cis_handle = hci_subevent_le_cis_request_get_cis_connection_handle(packet);
                    gap_cis_accept(has_app_server_cis_handle);
                    break;
                default:
                    break;
            }
            break;

       case HCI_EVENT_META_GAP:
            switch (hci_event_gap_meta_get_subevent_code(packet)){
                case GAP_SUBEVENT_LE_CONNECTION_COMPLETE:
                    has_app_server_con_handle = gap_subevent_le_connection_complete_get_connection_handle(packet);
                    has_app_server_state      = HAS_APP_SERVER_STATE_CONNECTED;
                    gap_subevent_le_connection_complete_get_peer_address(packet, has_client_addr);
                    printf("HAS Server: Connection to %s established\n", bd_addr_to_str(has_client_addr));
                    break;

                default:
                    return;
            }
            break;

       case HCI_EVENT_DISCONNECTION_COMPLETE:
            has_app_server_con_handle = HCI_CON_HANDLE_INVALID;
            has_app_server_state      = HAS_APP_SERVER_STATE_IDLE;

            le_notification_enabled = 0;

            printf("HAS Server: Disconnected from %s\n", bd_addr_to_str(has_client_addr));
            gap_extended_advertising_start(adv_handle, 0, 0);
            break;

       default:
            break;
    }
}


static void has_server_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_LEAUDIO_META) return;


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
    uint8_t   iut_address_type;
    bd_addr_t iut_address;
    gap_le_get_own_address(&iut_address_type, iut_address);
    printf("TSPX_bd_addr_iut: ");
    for (uint8_t i=0;i<6;i++) printf("%02x", iut_address[i]);
    printf("\n");

    printf("## HAS\n");
    
}

static void stdin_process(char cmd){
    printf("%c\n", cmd);
    uint8_t status = ERROR_CODE_SUCCESS;

    switch (cmd){
        case '0':
            printf("Set active index to 0\n");
            status = hearing_access_service_server_preset_record_set_active(0);
            break;

        case '1':
            printf("Set active index to 1\n");
            status = hearing_access_service_server_preset_record_set_active(1);
            break;
        case '2':
            printf("Set active index to 2\n");
            status = hearing_access_service_server_preset_record_set_active(2);
            break;
        case '3':
            printf("Set active index to 3\n");
            status = hearing_access_service_server_preset_record_set_active(3);
            break;
        case '4':
            printf("Set active index to 4\n");
            status = hearing_access_service_server_preset_record_set_active(4);
            break;
        case '5':
            printf("Set active index to 5\n");
            status = hearing_access_service_server_preset_record_set_active(5);
            break;
        case '6':
            printf("Set active index to 6\n");
            status = hearing_access_service_server_preset_record_set_active(6);
            break;
        case '7':
            printf("Set active index to 7\n");
            status = hearing_access_service_server_preset_record_set_active(7);
            break;
        case '8':
            printf("Set active index to 8\n");
            status = hearing_access_service_server_preset_record_set_active(8);
            break;

        case 'a':
            printf("Add preset index 8\n");
            status = hearing_access_service_server_add_preset(0x03, "Preset 8");
            break;

        case 'A':
            printf("Add preset index 9\n");
            status = hearing_access_service_server_add_preset(0x03, "Preset 9");
            break;

        case 'b':
            printf("Update name ar index 1\n");
            status = hearing_access_service_server_preset_record_set_name(1, "Preset 11");
            break;

        case 'c':
            printf("Set available index 4\n");
            status = hearing_access_service_server_preset_record_set_available(4);
            break;
        
        case 'C':
            printf("Set unavailable index 4\n");
            status = hearing_access_service_server_preset_record_set_unavailable(4);
            break;
        
        case 'd':
            printf("Set available index 1\n");
            status = hearing_access_service_server_preset_record_set_available(1);
            break;
        
        case 'D':
            printf("Set unavailable index 1\n");
            status = hearing_access_service_server_preset_record_set_unavailable(1);
            break;

        case 'e':
            printf("Remove index 1\n");
            status = hearing_access_service_server_delete_preset(1);
            break;
        
        case 'E':
            printf("Remove index 4\n");
            status = hearing_access_service_server_delete_preset(4);
            break;
        
        case 'f':
            printf("Change name of index 2\n");
            status = hearing_access_service_server_preset_record_set_name(2, "New Name item");
            break;

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
    att_server_init(profile_data, att_read_callback, att_write_callback);

    // setup HAS server
    hearing_access_service_server_init(hearing_aid_features, HAS_NUM_PRESETS, has_app_preset_records, HAS_NUM_CLIENTS, has_app_clients);
    hearing_access_service_server_register_packet_handler(&has_server_packet_handler);

    hearing_access_service_server_add_preset(HEARING_AID_PRESET_PROPERTIES_MASK_WRITABLE | HEARING_AID_PRESET_PROPERTIES_MASK_AVAILABLE, "Preset 1");
    hearing_access_service_server_add_preset(HEARING_AID_PRESET_PROPERTIES_MASK_WRITABLE | HEARING_AID_PRESET_PROPERTIES_MASK_AVAILABLE, "Preset 2");
    hearing_access_service_server_add_preset(                                               HEARING_AID_PRESET_PROPERTIES_MASK_AVAILABLE, "Preset 3");
    hearing_access_service_server_add_preset(HEARING_AID_PRESET_PROPERTIES_MASK_WRITABLE , "Preset 4");
    hearing_access_service_server_add_preset(HEARING_AID_PRESET_PROPERTIES_MASK_WRITABLE | HEARING_AID_PRESET_PROPERTIES_MASK_AVAILABLE, "Preset 5");
    hearing_access_service_server_add_preset(HEARING_AID_PRESET_PROPERTIES_MASK_WRITABLE | HEARING_AID_PRESET_PROPERTIES_MASK_AVAILABLE, "Preset 6");
    hearing_access_service_server_add_preset(HEARING_AID_PRESET_PROPERTIES_MASK_WRITABLE   , "Preset 7");


    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // setup advertisements
    setup_advertising();

    btstack_stdin_setup(stdin_process);

    // turn on!
	hci_power_control(HCI_POWER_ON);
	    
    return 0;
}
