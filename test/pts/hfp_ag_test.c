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

#define BTSTACK_FILE__ "hfp_ag_test.c"
 
/*
 * hfp_ag_test.c
 */

// *****************************************************************************
/* EXAMPLE_START(hfp_ag_test): HFP AG - Audio Gateway
 *
 * @text This HFP Audio Gateway example demonstrates how to receive 
 * an output from a remote HFP Hands-Free (HF) unit, and, 
 * if HAVE_BTSTACK_STDIN is defined, how to control the HFP HF. 
 */
// *****************************************************************************


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack.h"
#include "sco_demo_util.h"
#ifdef HAVE_BTSTACK_STDIN
#include "btstack_stdin.h"
#endif

// uncomment to temp disable mSBC codec
// #undef ENABLE_HFP_WIDE_BAND_SPEECH

uint8_t hfp_service_buffer[150];
const uint8_t    rfcomm_channel_nr = 1;
const char hfp_ag_service_name[] = "HFP AG Demo";

static bd_addr_t device_addr;
static const char * device_addr_string = "00:1B:DC:08:E2:72";

// configuration
static const int wide_band_speech = 1;

static uint8_t codecs[] = {HFP_CODEC_CVSD, HFP_CODEC_MSBC};

static uint8_t negotiated_codec = HFP_CODEC_CVSD;

static hci_con_handle_t acl_handle = HCI_CON_HANDLE_INVALID;
static hci_con_handle_t sco_handle = HCI_CON_HANDLE_INVALID;
static int memory_1_enabled = 1;
static btstack_packet_callback_registration_t hci_event_callback_registration;

static int ag_indicators_nr = 7;
static hfp_ag_indicator_t ag_indicators[] = {
    // index, name, min range, max range, status, mandatory, enabled, status changed
    {1, "service",   0, 1, 1, 0, 1, 0},
    {2, "call",      0, 1, 0, 1, 1, 0},
    {3, "callsetup", 0, 3, 0, 1, 1, 0},
    {4, "battchg",   0, 5, 3, 0, 1, 0},
    {5, "signal",    0, 5, 5, 0, 1, 0},
    {6, "roam",      0, 1, 0, 0, 1, 0},
    {7, "callheld",  0, 2, 0, 1, 1, 0}
};

static int call_hold_services_nr = 5;
static const char* call_hold_services[] = {"1", "1x", "2", "2x", "3"};

static int hf_indicators_nr = 2;
static hfp_generic_status_indicator_t hf_indicators[] = {
    {1, 1},
    {2, 1},
};

#define INQUIRY_INTERVAL 5

enum STATE {INIT, W4_INQUIRY_MODE_COMPLETE, ACTIVE} ;
enum STATE state = INIT;

static void dump_supported_codecs(void){
    unsigned int i;
    int mSBC_skipped = 0;
    printf("Supported codecs: ");
    for (i = 0; i < sizeof(codecs); i++){
        switch(codecs[i]){
            case HFP_CODEC_CVSD:
                printf("CVSD");
                break;
            case HFP_CODEC_MSBC:
                if (hci_extended_sco_link_supported()){
                    printf(", mSBC");
                } else {
                    mSBC_skipped = 1;
                }
                break;
            default:
                btstack_assert(false);
                break;
        }
    }
    printf("\n");
    if (mSBC_skipped){
        printf("mSBC codec disabled because eSCO not supported by local controller.\n");
    }
}

#ifdef HAVE_BTSTACK_STDIN
// Testig User Interface 
static void show_usage(void){
    bd_addr_t iut_address;
    gap_local_bd_addr(iut_address);

    printf("\n--- Bluetooth HFP Audiogateway (AG) unit Test Console %s ---\n", bd_addr_to_str(iut_address));
    printf("\n");
    
    printf("a - establish HFP connection to %s\n", bd_addr_to_str(device_addr));
    // printf("A - release HFP connection\n");
    
    printf("b - establish AUDIO connection          | B - release AUDIO connection\n");
    printf("c - simulate incoming call from 1234567 | C - simulate call from 1234567 dropped\n");
    printf("d - report AG failure\n");
    printf("D - delete all link keys\n");
    printf("e - answer call on AG                   | E - reject call on AG\n");
    printf("r - disable in-band ring tone           | R - enable in-band ring tone\n");
    printf("f - Disable cellular network            | F - Enable cellular network\n");
    printf("g - Set signal strength to 0            | G - Set signal strength to 5\n");
    printf("h - Disable roaming                     | H - Enable roaming\n");
    printf("i - Set battery level to 3              | I - Set battery level to 5\n");
    printf("j - Answer call on remote side\n");
    printf("k - Clear memory #1                     | K - Set memory #1\n");
    printf("l - Clear last number                   | L - Set last number to 7654321\n");
    printf("m - simulate incoming call from 7654321\n");
    printf("M - simulate outgoing call to 1234567\n");
    printf("n - Disable Voice Regocnition           | N - Enable Voice Recognition\n");
    printf("o - Set speaker volume to 0  (minimum)  | O - Set speaker volume to 9  (default)\n");
    printf("p - Set speaker volume to 12 (higher)   | P - Set speaker volume to 15 (maximum)\n");
    printf("q - Set microphone gain to 0  (minimum) | Q - Set microphone gain to 9  (default)\n");
    printf("s - Set microphone gain to 12 (higher)  | S - Set microphone gain to 15 (maximum)\n");
    printf("t - terminate connection\n");
    printf("u - join held call\n");
    printf("v - discover nearby HF units\n");
    printf("w - put incoming call on hold (Response and Hold)\n");
    printf("x - accept held incoming call (Response and Hold)\n");
    printf("X - reject held incoming call (Response and Hold)\n");
    printf("---\n");
}

static void stdin_process(char cmd){
    switch (cmd){
        case 'a':
            log_info("USER:\'%c\'", cmd);
            printf("Establish HFP service level connection to %s...\n", bd_addr_to_str(device_addr));
            hfp_ag_establish_service_level_connection(device_addr);
            break;
        case 'A':
            log_info("USER:\'%c\'", cmd);
            printf("Release HFP service level connection.\n");
            hfp_ag_release_service_level_connection(acl_handle);
            break;
        case 'Z':
            log_info("USER:\'%c\'", cmd);
            printf("Release HFP service level connection to %s...\n", bd_addr_to_str(device_addr));
            hfp_ag_release_service_level_connection(acl_handle);
            break;
        case 'b':
            log_info("USER:\'%c\'", cmd);
            printf("Establish Audio connection %s...\n", bd_addr_to_str(device_addr));
            hfp_ag_establish_audio_connection(acl_handle);
            break;
        case 'B':
            log_info("USER:\'%c\'", cmd);
            printf("Release Audio connection.\n");
            hfp_ag_release_audio_connection(acl_handle);
            break;
        case 'c':
            log_info("USER:\'%c\'", cmd);
            printf("Simulate incoming call from 1234567\n");
            hfp_ag_set_clip(129, "1234567");
            hfp_ag_incoming_call();
            break;
        case 'C':
            log_info("USER:\'%c\'", cmd);
            printf("Simulate terminate call\n");
            hfp_ag_call_dropped();
            break;
        case 'd':
            log_info("USER:\'%c\'", cmd);
            printf("Report AG failure\n");
            hfp_ag_report_extended_audio_gateway_error_result_code(acl_handle, HFP_CME_ERROR_AG_FAILURE);
            break;
        case 'D':
            printf("Deleting all link keys\n");
            gap_delete_all_link_keys();
            break;
        case 'e':
            log_info("USER:\'%c\'", cmd);
            printf("Answer call on AG\n");
            hfp_ag_answer_incoming_call();
            break;
        case 'E':
            log_info("USER:\'%c\'", cmd);
            printf("Reject call on AG\n");
            hfp_ag_terminate_call();
            break;
        case 'f':
            log_info("USER:\'%c\'", cmd);
            printf("Disable cellular network\n");
            hfp_ag_set_registration_status(0);
            break;
        case 'F':
            log_info("USER:\'%c\'", cmd);
            printf("Enable cellular network\n");
            hfp_ag_set_registration_status(1);
            break;
        case 'g':
            log_info("USER:\'%c\'", cmd);
            printf("Set signal strength to 0\n");
            hfp_ag_set_signal_strength(0);
            break;
        case 'G':
            log_info("USER:\'%c\'", cmd);
            printf("Set signal strength to 5\n");
            hfp_ag_set_signal_strength(5);
            break;
        case 'h':
            log_info("USER:\'%c\'", cmd);
            printf("Disable roaming\n");
            hfp_ag_set_roaming_status(0);
            break;
        case 'H':
            log_info("USER:\'%c\'", cmd);
            printf("Enable roaming\n");
            hfp_ag_set_roaming_status(1);
            break;
        case 'i':
            log_info("USER:\'%c\'", cmd);
            printf("Set battery level to 3\n");
            hfp_ag_set_battery_level(3);
            break;
        case 'I':
            log_info("USER:\'%c\'", cmd);
            printf("Set battery level to 5\n");
            hfp_ag_set_battery_level(5);
            break;
        case 'j':
            log_info("USER:\'%c\'", cmd);
            printf("Answering call on remote side\n");
            hfp_ag_outgoing_call_established();
            break;
        case 'r':
            log_info("USER:\'%c\'", cmd);
            printf("Disable in-band ring tone\n");
            hfp_ag_set_use_in_band_ring_tone(0);
            break;
        case 'k':
            log_info("USER:\'%c\'", cmd);
            printf("Memory 1 cleared\n");
            memory_1_enabled = 0;
            break;
        case 'K':
            log_info("USER:\'%c\'", cmd);
            printf("Memory 1 set\n");
            memory_1_enabled = 1;
            break;
        case 'l':
            log_info("USER:\'%c\'", cmd);
            printf("Last dialed number cleared\n");
            hfp_ag_clear_last_dialed_number();
            break;
        case 'L':
            log_info("USER:\'%c\'", cmd);
            printf("Set last dialed number to 7654321\n");
            hfp_ag_set_last_dialed_number("7654321");
            break;
        case 'm':
            log_info("USER:\'%c\'", cmd);
            printf("Simulate incoming call from 7654321\n");
            hfp_ag_set_clip(129, "7654321");
            hfp_ag_incoming_call();
            break;
        case 'M':
            log_info("USER:\'%c\'", cmd);
            printf("Outgoing call inited and connected, ringing\n");
            hfp_ag_outgoing_call_initiated("1234567");
            hfp_ag_outgoing_call_established();
            break;
        case 'n':
            log_info("USER:\'%c\'", cmd);
            printf("Disable Voice Recognition\n");
            hfp_ag_activate_voice_recognition(acl_handle, 0);
            break;
        case 'N':
            log_info("USER:\'%c\'", cmd);
            printf("Enable Voice Recognition\n");
            hfp_ag_activate_voice_recognition(acl_handle, 1);
            break;
        case 'o':
            log_info("USER:\'%c\'", cmd);
            printf("Set speaker gain to 0 (minimum)\n");
            hfp_ag_set_speaker_gain(acl_handle, 0);
            break;
        case 'O':
            log_info("USER:\'%c\'", cmd);
            printf("Set speaker gain to 9 (default)\n");
            hfp_ag_set_speaker_gain(acl_handle, 9);
            break;
        case 'p':
            log_info("USER:\'%c\'", cmd);
            printf("Set speaker gain to 12 (higher)\n");
            hfp_ag_set_speaker_gain(acl_handle, 12);
            break;
        case 'P':
            log_info("USER:\'%c\'", cmd);
            printf("Set speaker gain to 15 (maximum)\n");
            hfp_ag_set_speaker_gain(acl_handle, 15);
            break;
        case 'q':
            log_info("USER:\'%c\'", cmd);
            printf("Set microphone gain to 0\n");
            hfp_ag_set_microphone_gain(acl_handle, 0);
            break;
        case 'Q':
            log_info("USER:\'%c\'", cmd);
            printf("Set microphone gain to 9\n");
            hfp_ag_set_microphone_gain(acl_handle, 9);
            break;
        case 's':
            log_info("USER:\'%c\'", cmd);
            printf("Set microphone gain to 12\n");
            hfp_ag_set_microphone_gain(acl_handle, 12);
            break;
        case 'S':
            log_info("USER:\'%c\'", cmd);
            printf("Set microphone gain to 15\n");
            hfp_ag_set_microphone_gain(acl_handle, 15);
            break;
        case 'R':
            log_info("USER:\'%c\'", cmd);
            printf("Enable in-band ring tone\n");
            hfp_ag_set_use_in_band_ring_tone(1);
            break;
        case 't':
            log_info("USER:\'%c\'", cmd);
            printf("Terminate HCI connection. 0x%2x\n", acl_handle);
            gap_disconnect(acl_handle);
            break;
        case 'u':
            log_info("USER:\'%c\'", cmd);
            printf("Join held call\n");
            hfp_ag_join_held_call();
            break;
        case 'v':
            printf("Start scanning...\n");
            gap_inquiry_start(INQUIRY_INTERVAL);
            break;
        case 'w':
            log_info("USER:\'%c\'", cmd);
            printf("AG: Put incoming call on hold (Response and Hold)\n");
            hfp_ag_hold_incoming_call();
            break;
        case 'x':
            log_info("USER:\'%c\'", cmd);
            printf("AG: Accept held incoming call (Response and Hold)\n");
            hfp_ag_accept_held_incoming_call();
            break;
        case 'X':
            log_info("USER:\'%c\'", cmd);
            printf("AG: Reject held incoming call (Response and Hold)\n");
            hfp_ag_reject_held_incoming_call();
            break;
        default:
            show_usage();
            break;
    }
}
#endif

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t * event, uint16_t event_size){
    UNUSED(channel);
    bd_addr_t addr;
    uint8_t status;
    switch (packet_type){
        case HCI_EVENT_PACKET:
            switch(hci_event_packet_get_type(event)){
#ifndef HAVE_BTSTACK_STDIN
                case BTSTACK_EVENT_STATE:
                    if (btstack_event_state_get_state(event) != HCI_STATE_WORKING) break;
                    printf("Establish HFP AG service level connection to %s...\n", bd_addr_to_str(device_addr));
                    hfp_ag_establish_service_level_connection(device_addr);
                    break;
#endif
                case GAP_EVENT_INQUIRY_RESULT:
                    gap_event_inquiry_result_get_bd_addr(event, addr);
                    // print info
                    printf("Device found: %s ",  bd_addr_to_str(addr));
                    printf("with COD: 0x%06x, ", (unsigned int) gap_event_inquiry_result_get_class_of_device(event));
                    if (gap_event_inquiry_result_get_rssi_available(event)){
                        printf(", rssi %d dBm", (int8_t) gap_event_inquiry_result_get_rssi(event));
                    }
                    if (gap_event_inquiry_result_get_name_available(event)){
                        char name_buffer[240];
                        int name_len = gap_event_inquiry_result_get_name_len(event);
                        memcpy(name_buffer, gap_event_inquiry_result_get_name(event), name_len);
                        name_buffer[name_len] = 0;
                        printf(", name '%s'", name_buffer);
                    }
                    printf("\n");
                    break;
                case GAP_EVENT_INQUIRY_COMPLETE:
                    printf("Inquiry scan complete.\n");
                    break;
                case HCI_EVENT_SCO_CAN_SEND_NOW:
                    sco_demo_send(sco_handle); 
                    break; 
                case HCI_EVENT_COMMAND_COMPLETE:
                    if (HCI_EVENT_IS_COMMAND_COMPLETE(event, hci_read_local_supported_features)){
                        dump_supported_codecs();
                    }
                    break;
                default:
                    break;
            }

            if (hci_event_packet_get_type(event) != HCI_EVENT_HFP_META) return;

            switch (hci_event_hfp_meta_get_subevent_code(event)) {
                case HFP_SUBEVENT_SERVICE_LEVEL_CONNECTION_ESTABLISHED:
                    status = hfp_subevent_service_level_connection_established_get_status(event);
                    if (status != ERROR_CODE_SUCCESS){
                        printf("Connection failed, staus 0x%02x\n", status);
                        break;
                    }
                    acl_handle = hfp_subevent_service_level_connection_established_get_acl_handle(event);
                    hfp_subevent_service_level_connection_established_get_bd_addr(event, device_addr);
                    printf("Service level connection established to %s.\n", bd_addr_to_str(device_addr));
                    dump_supported_codecs();
#ifndef HAVE_BTSTACK_STDIN
                    log_info("Establish Audio connection %s", bd_addr_to_str(device_addr));
                    printf("Establish Audio connection %s...\n", bd_addr_to_str(device_addr));
                    hfp_ag_establish_audio_connection(acl_handle);
#endif
                    break;
                case HFP_SUBEVENT_SERVICE_LEVEL_CONNECTION_RELEASED:
                    printf("Service level connection released.\n");
                    acl_handle = HCI_CON_HANDLE_INVALID;
                    break;
                case HFP_SUBEVENT_AUDIO_CONNECTION_ESTABLISHED:
                    if (hfp_subevent_audio_connection_established_get_status(event) != ERROR_CODE_SUCCESS){
                        printf("Audio connection establishment failed with status %u\n", hfp_subevent_audio_connection_established_get_status(event));
                    } else {
                        sco_handle = hfp_subevent_audio_connection_established_get_sco_handle(event);
                        printf("Audio connection established with SCO handle 0x%04x.\n", sco_handle);
                        negotiated_codec = hfp_subevent_audio_connection_established_get_negotiated_codec(event);
                        switch (negotiated_codec){
                            case 0x01:
                                printf("Using CVSD codec.\n");
                                break;
                            case 0x02:
                                printf("Using mSBC codec.\n");
                                break;
                            default:
                                printf("Using unknown codec 0x%02x.\n", negotiated_codec);
                                break;
                        }
                        sco_demo_set_codec(negotiated_codec);
                        hci_request_sco_can_send_now_event();
                    }
                    break;
                case HFP_SUBEVENT_AUDIO_CONNECTION_RELEASED:
                    printf("Audio connection released\n");
                    sco_handle = HCI_CON_HANDLE_INVALID;
                    sco_demo_close();
                    break;
                case HFP_SUBEVENT_START_RINGINIG:
                    printf("Start Ringing\n");
                    break;        
                case HFP_SUBEVENT_STOP_RINGINIG:
                    printf("Stop Ringing\n");
                    break;
                case HFP_SUBEVENT_PLACE_CALL_WITH_NUMBER:
                    printf("Outgoing call '%s'\n", hfp_subevent_place_call_with_number_get_number(event));
                    // validate number
                    if ( strcmp("1234567", hfp_subevent_place_call_with_number_get_number(event)) == 0
                      || strcmp("7654321", hfp_subevent_place_call_with_number_get_number(event)) == 0
                      || (memory_1_enabled && strcmp(">1", hfp_subevent_place_call_with_number_get_number(event)) == 0)){
                        printf("Dialstring valid: accept call\n");
                        hfp_ag_outgoing_call_accepted();
                    } else {
                        printf("Dialstring invalid: reject call\n");
                        hfp_ag_outgoing_call_rejected();
                    }
                    break;
                
                case HFP_SUBEVENT_ATTACH_NUMBER_TO_VOICE_TAG:
                    printf("Attach number to voice tag. Sending 1234567\n");
                    hfp_ag_send_phone_number_for_voice_tag(acl_handle, "1234567");
                    break;
                case HFP_SUBEVENT_TRANSMIT_DTMF_CODES:
                    printf("Send DTMF Codes: '%s'\n", hfp_subevent_transmit_dtmf_codes_get_dtmf(event));
                    hfp_ag_send_dtmf_code_done(acl_handle);
                    break;
                case HFP_SUBEVENT_CALL_ANSWERED:
                    printf("Call answered by HF\n");
                    break;
                case HFP_SUBEVENT_SPEAKER_VOLUME:
                    printf("Set speaker volume to %d\n", hfp_subevent_speaker_volume_get_gain(event));
                    break;
                case HFP_SUBEVENT_MICROPHONE_VOLUME:
                    printf("Set microphone volume to %d\n", hfp_subevent_microphone_volume_get_gain(event));
                    break;
                default:
                    break;
            }
            break;
        case HCI_SCO_DATA_PACKET:
            if (READ_SCO_CONNECTION_HANDLE(event) != sco_handle) break;
            sco_demo_receive(event, event_size);
            break;
        default:
            break;
    }
}

static hfp_phone_number_t subscriber_number = {
    129, "225577"
};

/* @section Main Application Setup
 *
 * @text Listing MainConfiguration shows main application code. 
 * To run a HFP AG service you need to initialize the SDP, and to create and register HFP AG record with it. 
 * The packet_handler is used for sending commands to the HFP HF. It also receives the HFP HF's answers.
 * The stdin_process callback allows for sending commands to the HFP HF. 
 * At the end the Bluetooth stack is started.
 */

/* LISTING_START(MainConfiguration): Setup HFP Audio Gateway */

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    (void)argc;
    (void)argv;

    sco_demo_init();

    gap_set_local_name("HFP AG Demo 00:00:00:00:00:00");
    gap_discoverable_control(1);

    // L2CAP
    l2cap_init();

    uint16_t supported_features                   =
        (1<<HFP_AGSF_EC_NR_FUNCTION)              |
        (1<<HFP_AGSF_ESCO_S4)                     |
        (1<<HFP_AGSF_HF_INDICATORS)               |
        (1<<HFP_AGSF_CODEC_NEGOTIATION)           |
        (1<<HFP_AGSF_EXTENDED_ERROR_RESULT_CODES) |
        (1<<HFP_AGSF_ENHANCED_CALL_CONTROL)       |
        (1<<HFP_AGSF_ENHANCED_CALL_STATUS)        |
        (1<<HFP_AGSF_ABILITY_TO_REJECT_A_CALL)    |
        (1<<HFP_AGSF_IN_BAND_RING_TONE)           |
        (1<<HFP_AGSF_VOICE_RECOGNITION_FUNCTION)  |
        (1<<HFP_AGSF_THREE_WAY_CALLING)           |
        (1<<HFP_AGSF_ATTACH_A_NUMBER_TO_A_VOICE_TAG)    |
        (1<<HFP_HFSF_ENHANCED_VOICE_RECOGNITION_STATUS) |
        (1<<HFP_HFSF_VOICE_RECOGNITION_TEXT);

    // HFP
    rfcomm_init();
    hfp_ag_init(rfcomm_channel_nr);
    hfp_ag_init_supported_features(supported_features);
    hfp_ag_init_codecs(sizeof(codecs), codecs);
    hfp_ag_init_ag_indicators(ag_indicators_nr, ag_indicators);
    hfp_ag_init_hf_indicators(hf_indicators_nr, hf_indicators); 
    hfp_ag_init_call_hold_services(call_hold_services_nr, call_hold_services);
    hfp_ag_set_subcriber_number_information(&subscriber_number, 1);

    // SDP Server
    sdp_init();
    memset(hfp_service_buffer, 0, sizeof(hfp_service_buffer));
    hfp_ag_create_sdp_record( hfp_service_buffer, 0x10001, rfcomm_channel_nr, hfp_ag_service_name, 0, supported_features, wide_band_speech);
    printf("SDP service record size: %u\n", de_get_len( hfp_service_buffer));
    sdp_register_service(hfp_service_buffer);
    
    // register for HCI events and SCO packets
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);
    hci_register_sco_packet_handler(&packet_handler);

    // register for HFP events
    hfp_ag_register_packet_handler(&packet_handler);

    // parse humand readable Bluetooth address
    sscanf_bd_addr(device_addr_string, device_addr);

#ifdef HAVE_BTSTACK_STDIN
    btstack_stdin_setup(stdin_process);
#endif  
    // turn on!
    hci_power_control(HCI_POWER_ON);
    return 0;
}
/* LISTING_END */
/* EXAMPLE_END */
