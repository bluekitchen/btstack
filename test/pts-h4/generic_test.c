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

static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_packet_callback_registration_t sm_event_callback_registration;
static void show_usage(void);

static const char * pts_address_string = "C007E8414591";

static bd_addr_t        pts_address;
static hci_con_handle_t pts_con_handle;

static void hci_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

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

        case HCI_EVENT_META_GAP:
            switch (hci_event_gap_meta_get_subevent_code(packet)){
                case GAP_SUBEVENT_LE_CONNECTION_COMPLETE:
                    pts_con_handle = gap_subevent_le_connection_complete_get_connection_handle(packet);
                    gap_subevent_le_connection_complete_get_peer_address(packet, pts_address);
                    printf("BAP  Client: connection to %s established, con_handle 0x%04x\n", bd_addr_to_str(pts_address), pts_con_handle);
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
                default:
                    break;
            }
            break;

        default:
            break;
    }
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
    printf("---\n");
    printf("c   - connect to %s\n", bd_addr_to_str(pts_address));
    printf("X   - disconnect\n");
    printf("---\n");
}

static void stdin_process(char cmd){
    uint8_t status = ERROR_CODE_SUCCESS;

    switch (cmd){
        case 'c':
            printf("Connecting...\n");
            status = gap_connect(pts_address, 0);
            break;
        
        case 'X':
            printf("Disconnect...\n");
            status = gap_disconnect(pts_con_handle);
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
    sm_set_authentication_requirements(SM_AUTHREQ_SECURE_CONNECTION);
    sm_event_callback_registration.callback = &hci_event_handler;
    sm_add_event_handler(&sm_event_callback_registration);

    hci_event_callback_registration.callback = &hci_event_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // parse human readable Bluetooth address
    sscanf_bd_addr(pts_address_string, pts_address);
    btstack_stdin_setup(stdin_process);

    // turn on!
    hci_power_control(HCI_POWER_ON);

    return 0;
}
