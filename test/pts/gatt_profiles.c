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

#define BTSTACK_FILE__ "gatt_profiles.c"

// *****************************************************************************
/* EXAMPLE_START(le_counter): LE Peripheral - Heartbeat Counter over GATT
 *
 * @text All newer operating systems provide GATT Client functionality.
 * The LE Counter examples demonstrates how to specify a minimal GATT Database
 * with a custom GATT Service and a custom Characteristic that sends periodic
 * notifications. 
 */
 // *****************************************************************************

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "gatt_profiles.h"
#include "btstack.h"

#define HEARTBEAT_PERIOD_MS 1000

#define ATT_SERVICE_GATT_SERVICE_START_HANDLE 0x000c
#define ATT_SERVICE_GATT_SERVICE_END_HANDLE 0x000e

/* @section Main Application Setup
 *
 * @text Listing MainConfiguration shows main application code.
 * It initializes L2CAP, the Security Manager and configures the ATT Server with the pre-compiled
 * ATT Database generated from $le_counter.gatt$. 
 * Additionally, it enables the Battery Service Server with the current battery level.
 * Finally, it configures the advertisements 
 * and the heartbeat handler and boots the Bluetooth stack. 
 * In this example, the Advertisement contains the Flags attribute and the device name.
 * The flag 0x06 indicates: LE General Discoverable Mode and BR/EDR not supported.
 */
 
/* LISTING_START(MainConfiguration): Init L2CAP SM ATT Server and start heartbeat timer */
static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static uint16_t att_read_callback(hci_con_handle_t con_handle, uint16_t att_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size);
static int att_write_callback(hci_con_handle_t con_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size);

static int  le_notification_enabled;
static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_packet_callback_registration_t sm_event_callback_registration;

static uint8_t battery = 100;
static bool accept_next_pairing = true;

static gatt_microphone_control_mute_t mics_mute = GATT_MICROPHONE_CONTROL_MUTE_OFF;
static vcs_mute_t  vcs_volume_state_mute = VCS_MUTE_OFF;
static uint8_t vcs_volume_state_setting = 2;

static aics_info_t aics_info[] = {
    {
        {1, AICS_MUTE_MODE_NOT_MUTED, AICS_GAIN_MODE_AUTOMATIC},
        {1, -10, 10},
            AICS_AUDIO_INPUT_TYPE_MICROPHONE,
        "audio_input_description1",
        packet_handler
    },
    {
        {2, AICS_MUTE_MODE_NOT_MUTED, AICS_GAIN_MODE_AUTOMATIC},
        {1, -11, 11},
            AICS_AUDIO_INPUT_TYPE_MICROPHONE,
        "audio_input_description2",
        packet_handler
    },
    {
            {3, AICS_MUTE_MODE_NOT_MUTED, AICS_GAIN_MODE_MANUAL},
            {2, -12, 12},
            AICS_AUDIO_INPUT_TYPE_MICROPHONE,
            "audio_input_description3",
            packet_handler
    }

};
static uint8_t aics_info_num = 2;

static vocs_info_t vocs_info[] = {
        {
                10,VOCS_AUDIO_LOCATION_FRONT_RIGHT,
                "ao_description1",
                packet_handler
        },
        {
                20, VOCS_AUDIO_LOCATION_FRONT_LEFT,
                "ao_description2",
                packet_handler
        }
};
static uint8_t vocs_info_num = 2;


#ifdef ENABLE_GATT_OVER_CLASSIC
#include "classic/gatt_sdp.h"
#include "classic/sdp_util.h"
static uint8_t gatt_service_buffer[70];
#endif

const uint8_t adv_data[] = {
    // Flags general discoverable, BR/EDR not supported
    0x02, 0x01, 0x06, 
    // Name
    0x0b, 0x09, 'L', 'E', ' ', 'C', 'o', 'u', 'n', 't', 'e', 'r', 
};
const uint8_t adv_data_len = sizeof(adv_data);

/* LISTING_START(packetHandler): Packet Handler */
static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    
    switch (hci_event_packet_get_type(packet)) {
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            le_notification_enabled = 0;
            break;
        case ATT_EVENT_CAN_SEND_NOW:
            // att_server_notify(con_handle, ATT_CHARACTERISTIC_0000FF11_0000_1000_8000_00805F9B34FB_01_VALUE_HANDLE, (uint8_t*) counter_string, counter_string_len);
            break;
           case SM_EVENT_JUST_WORKS_REQUEST:
                if (accept_next_pairing){
                    accept_next_pairing = false;
                    printf("Accept Just Works\n");
                    sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
                } else {
                    printf("Reject Just Works\n");
                    sm_bonding_decline(sm_event_just_works_request_get_handle(packet));
                }
                break;
            case SM_EVENT_NUMERIC_COMPARISON_REQUEST:
                printf("Confirming numeric comparison: %"PRIu32"\n", sm_event_numeric_comparison_request_get_passkey(packet));
                sm_numeric_comparison_confirm(sm_event_passkey_display_number_get_handle(packet));
                break;

        case HCI_EVENT_GATTSERVICE_META:
            switch (hci_event_gattservice_meta_get_subevent_code(packet)){
                
                case GATTSERVICE_SUBEVENT_LOCAL_MICS_MUTE: 
                    printf("MICS: mute value changed by remote to %d\n", gattservice_subevent_local_mics_mute_get_state(packet));
                    mics_mute = (gatt_microphone_control_mute_t)gattservice_subevent_local_mics_mute_get_state(packet);
                    break;

                case GATTSERVICE_SUBEVENT_AICS_MUTE_MODE:
                    switch ((aics_mute_mode_t)gattservice_subevent_aics_mute_mode_get_state(packet)){
                        case AICS_MUTE_MODE_NOT_MUTED:
                            printf("AICS[%d]: mute mode NOT_MUTED\n", gattservice_subevent_aics_mute_mode_get_index(packet));
                            break;
                        case AICS_MUTE_MODE_MUTED:
                            printf("AICS[%d]: mute mode MUTED\n", gattservice_subevent_aics_mute_mode_get_index(packet));
                            break;
                        case AICS_MUTE_MODE_DISABLED:
                            printf("AICS[%d]: mute mode DISABLED\n", gattservice_subevent_aics_mute_mode_get_index(packet));
                            break;
                        default:
                            break;
                    } 
                    break;

                case GATTSERVICE_SUBEVENT_AICS_GAIN_MODE:
                    switch ((aics_gain_mode_t)gattservice_subevent_aics_gain_mode_get_state(packet)) {
                        case AICS_GAIN_MODE_MANUAL:
                            printf("AICS[%d]: gain mode MANUAL\n", gattservice_subevent_aics_gain_mode_get_index(packet));
                            break;
                        case AICS_GAIN_MODE_AUTOMATIC:
                            printf("AICS[%d]: gain mode AUTOMATIC\n", gattservice_subevent_aics_gain_mode_get_index(packet));
                            break;
                        case AICS_GAIN_MODE_MANUAL_ONLY:
                            printf("AICS[%d]: gain mode MANUAL_ONLY\n", gattservice_subevent_aics_gain_mode_get_index(packet));
                            break;
                        case AICS_GAIN_MODE_AUTOMATIC_ONLY:
                            printf("AICS[%d]: gain mode AUTOMATIC_ONLY\n", gattservice_subevent_aics_gain_mode_get_index(packet));
                            break;
                        default:
                            break;
                    }
                    break;
                
                case GATTSERVICE_SUBEVENT_AICS_GAIN_CHANGED:
                    printf("AICS: gain for AICS %d changed to %ddB\n", gattservice_subevent_aics_gain_changed_get_index(packet), gattservice_subevent_aics_gain_changed_get_gain_db(packet));
                    break;

                default:
                    break;
            }
            break;

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

    // if (att_handle == ATT_CHARACTERISTIC_0000FF11_0000_1000_8000_00805F9B34FB_01_VALUE_HANDLE){
    //     return att_read_callback_handle_blob((const uint8_t *)counter_string, buffer_size, offset, buffer, buffer_size);
    // }
    return 0;
}
/* LISTING_END */


/*
 * @section ATT Write
 *
 * @text The only valid ATT write in this example is to the Client Characteristic Configuration, which configures notification
 * and indication. If the ATT handle matches the client configuration handle, the new configuration value is stored and used
 * in the heartbeat handler to decide if a new value should be sent. See Listing attWrite.
 */

/* LISTING_START(attWrite): ATT Write */
static int att_write_callback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
    UNUSED(connection_handle);
    UNUSED(att_handle);
    UNUSED(transaction_mode);
    UNUSED(offset);
    UNUSED(buffer);
    UNUSED(buffer_size);
    // if (att_handle != ATT_CHARACTERISTIC_0000FF11_0000_1000_8000_00805F9B34FB_01_CLIENT_CONFIGURATION_HANDLE) return 0;
    // le_notification_enabled = little_endian_read_16(buffer, 0) == GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION;
    // con_handle = connection_handle;
    return 0;
}
/* LISTING_END */

static void show_usage(void){
    printf("## BAS\n");
    printf("a - send 50%% Battery level\n");
    printf("## TPS\n");
    printf("t - send 10 TX Power level\n");
    printf("\n## BMS\n");
    printf("b - enable classic and LE flags\n");
    printf("c - enable classic flag, no auth\n");
    printf("C - enable classic flag, with auth\n");
    printf("l - enable LE flag, no auth\n");
    printf("L - enable LE flag, with auth\n");
    
    printf("\n## MICS\n");
    printf("m - set MUTE_OFF\n");
    printf("M - set MUTE_ON\n");
    printf("P - disable mute\n");

    printf("\n## AICS\n");
    printf("q - disable mute of AICS[0]\n");
    printf("d - set MUTE of AICS[0]\n");
    printf("D - set UNMUTE of AICS[0]\n");
    printf("g - set AUTOMATIC gain mode of AICS[0]\n");
    printf("G - set MANUAL gain mode of AICS[0]\n");
    printf("f - set AUTOMATIC_ONLY gain mode of AICS[0]\n");
    printf("F - set MANUAL_ONLY gain mode of AICS[0]\n");
    printf("p - set audio input desc of AICS[0]\n");

    printf("\n##VOCS\n");
    printf("o - set audio location of VOCS[0]\n");
    printf("O - set audio output desc of VOCS[0]\n");

    printf("\n##VCS\n");
    // printf("o - set volume change step to 2\n");
    printf("v - toggle volume setting [2, 253], current %d\n", vcs_volume_state_setting);
    printf("V - toggle volume mute   [ON, OFF], current %s\n", vcs_volume_state_mute == VCS_MUTE_OFF ? "OFF" : "ON");
}

static void stdin_process(char c){

    switch (c){
        case 'a': // accept just works
            printf("BAS: Sending 50%% battery level\n");
            battery_service_server_set_battery_value(50);
            break;
        case 't':
            printf("TPS: Sending 10 TX Power level\n");
            tx_power_service_server_set_level(10);
            break;
        
        case 'c':
            printf("BMS: enable only classic flag, no auth\n");
            bond_management_service_server_init(1);
            break;
        case 'C':
            printf("BMS: enable only classic flag, with auth\n");
            bond_management_service_server_init(2);
            break;
        case 'l':
            printf("BMS: enable only LE flag, no auth\n");
            bond_management_service_server_init(0x03);
            break;
        case 'L':
            printf("BMS: enable only LE flag, with auth\n");
            bond_management_service_server_init(0x04);
            break;
        
        case 'b':
            printf("BMS: enable classic and LE flags\n");
            bond_management_service_server_init(0xFFFF);
            break;

        case 'm':
            printf("MICS: set MUTE_OFF\n");
            microphone_control_service_server_set_mute(GATT_MICROPHONE_CONTROL_MUTE_OFF);
            break;
        case 'M':
            printf("MICS: set MUTE_ON\n");
            microphone_control_service_server_set_mute(GATT_MICROPHONE_CONTROL_MUTE_ON);
            break;
        case 'P':
            printf("MICS: disable mute\n");
            mics_mute = GATT_MICROPHONE_CONTROL_MUTE_DISABLED;
            microphone_control_service_server_set_mute(mics_mute);
            break;

        case 'q':
            printf("AICS: disable mute of AICS[0]\n");
            aics_info[0].audio_input_state.mute_mode = AICS_MUTE_MODE_DISABLED;
            microphone_control_service_server_set_audio_input_state_for_aics(0, &aics_info[0].audio_input_state);
            break;

        case 'd':
            printf("AICS: set MUTED of AICS[0]\n");
            aics_info[0].audio_input_state.mute_mode = AICS_MUTE_MODE_MUTED;
            microphone_control_service_server_set_audio_input_state_for_aics(0, &aics_info[0].audio_input_state);
            break;
        case 'D':
            printf("AICS: set NOT_MUTED of AICS[0]\n");
            aics_info[0].audio_input_state.mute_mode = AICS_MUTE_MODE_NOT_MUTED;
            microphone_control_service_server_set_audio_input_state_for_aics(0, &aics_info[0].audio_input_state);
            break;
        case 'g':
            printf("AICS: set AUTOMATIC gain mode of AICS[0]\n");
            aics_info[0].audio_input_state.gain_mode = AICS_GAIN_MODE_AUTOMATIC;
            microphone_control_service_server_set_audio_input_state_for_aics(0, &aics_info[0].audio_input_state);
            break;
        case 'G':
            printf("AICS: set MANUAL gain mode of AICS[0]\n");
            aics_info[0].audio_input_state.gain_mode = AICS_GAIN_MODE_MANUAL;
            microphone_control_service_server_set_audio_input_state_for_aics(0, &aics_info[0].audio_input_state);
            break;
        case 'f':
            printf("AICS: set AUTOMATIC gain mode of AICS[0]\n");
            aics_info[0].audio_input_state.gain_mode = AICS_GAIN_MODE_AUTOMATIC_ONLY;
            microphone_control_service_server_set_audio_input_state_for_aics(0, &aics_info[0].audio_input_state);
            break;
        case 'F':
            printf("AICS: set MANUAL gain mode of AICS[0]\n");
            aics_info[0].audio_input_state.gain_mode = AICS_GAIN_MODE_MANUAL_ONLY;
            microphone_control_service_server_set_audio_input_state_for_aics(0, &aics_info[0].audio_input_state);
            break;
        case 'p':{
            printf("AICS: set audio input desc of AICS[0]\n");
            char str[20];
            static uint8_t str_update = 0;
            str_update++;
            sprintf(str, "Microphone %d", str_update);
            microphone_control_service_server_set_audio_input_description_for_aics(0, (const char*)str);
            break;
        }
        case 'o':
            printf("VOCS: set audio location of VOCS[0]\n");
            volume_control_service_server_set_audio_location_for_vocs(0, VOCS_AUDIO_LOCATION_FRONT_CENTER );
            break;
        case 'O':
            printf("VOCS: set audio output desc of VOCS[0]\n");
            volume_control_service_server_set_audio_output_description_for_vocs(0, "ao_desc3");
            break;

        case 'v':
            if (vcs_volume_state_setting == 2){
                vcs_volume_state_setting = 253;
            } else {
                vcs_volume_state_setting = 2;
            }
            printf("VCS: toggle Volume state setting to %d\n", vcs_volume_state_setting);
            volume_control_service_server_set_volume_state(vcs_volume_state_setting, vcs_volume_state_mute);
            break;
        case 'V':
            if (vcs_volume_state_mute == VCS_MUTE_OFF){
                vcs_volume_state_mute = VCS_MUTE_ON;
            } else {
                vcs_volume_state_mute = VCS_MUTE_OFF;
            }
            printf("VCS: toggle Mute state to %s\n", vcs_volume_state_mute == VCS_MUTE_OFF ? "OFF" : "ON");
            volume_control_service_server_set_volume_state(vcs_volume_state_setting, vcs_volume_state_mute);
            break;

        case '\n':
        case '\r':
            break;
        default:
            show_usage();
            break;
    }
}

int btstack_main(void);
int btstack_main(void)
{
    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    l2cap_init();

    // setup le device db
    le_device_db_init();

    // setup SM: Display only
    sm_init();
    // register for SM events
    sm_event_callback_registration.callback = &packet_handler;
    sm_add_event_handler(&sm_event_callback_registration);
    sm_set_authentication_requirements(SM_AUTHREQ_BONDING);

    // setup ATT server
    att_server_init(profile_data, att_read_callback, att_write_callback);    
    att_server_register_packet_handler(packet_handler);

#ifdef ENABLE_GATT_OVER_CLASSIC
    // init SDP, create record for GATT and register with SDP
    sdp_init();
    memset(gatt_service_buffer, 0, sizeof(gatt_service_buffer));
    // TODO: ATT_SERVICE_GATT_SERVICE_START_HANDLE and ATT_SERVICE_GATT_SERVICE_END_HANDLE used from gatt_server_test and propably not correct
    gatt_create_sdp_record(gatt_service_buffer, 0x10001, ATT_SERVICE_GATT_SERVICE_START_HANDLE, ATT_SERVICE_GATT_SERVICE_END_HANDLE);
    sdp_register_service(gatt_service_buffer);
    printf("SDP service record size: %u\n", de_get_len(gatt_service_buffer));
#endif

    // setup battery service
    battery_service_server_init(battery);

    // setup device information service
    device_information_service_server_init();
    device_information_service_server_set_pnp_id(1, BLUETOOTH_COMPANY_ID_BLUEKITCHEN_GMBH, 0x0001, 0x001);
    
    tx_power_service_server_init(20);

    bond_management_service_server_init(0xFFFF);
    bond_management_service_server_set_authorisation_string("000000000000");

    mics_mute = GATT_MICROPHONE_CONTROL_MUTE_OFF;
    
    microphone_control_service_server_init(mics_mute, aics_info_num, aics_info);

    microphone_control_service_server_register_packet_handler(&packet_handler);

    volume_control_service_server_init(vcs_volume_state_setting , vcs_volume_state_mute, aics_info_num, aics_info, vocs_info_num, vocs_info);
    
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
/* EXAMPLE_END */
