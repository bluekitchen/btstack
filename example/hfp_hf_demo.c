
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

#define BTSTACK_FILE__ "hfp_hf_demo.c"

/*
 * hfp_hs_demo.c
 */

// *****************************************************************************
/* EXAMPLE_START(hfp_hs_demo): HFP Hands-Free (HF) Demo
 *
 * @text This  HFP Hands-Free example demonstrates how to receive 
 * an output from a remote HFP audio gateway (AG), and, 
 * if HAVE_BTSTACK_STDIN is defined, how to control the HFP AG. 
 */
// *****************************************************************************


#include "btstack_config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "btstack.h"

#include "sco_demo_util.h"

uint8_t hfp_service_buffer[150];
const uint8_t   rfcomm_channel_nr = 1;
const char hfp_hf_service_name[] = "HFP HF Demo";

#ifdef HAVE_BTSTACK_STDIN
// static const char * device_addr_string = "6C:72:E7:10:22:EE";
static const char * device_addr_string = "54:E4:3A:26:A2:39";
#endif

static bd_addr_t device_addr;

#ifdef HAVE_BTSTACK_STDIN
// 80:BE:05:D5:28:48
// prototypes
static void show_usage(void);
#endif
static hci_con_handle_t acl_handle = HCI_CON_HANDLE_INVALID;
static hci_con_handle_t sco_handle = HCI_CON_HANDLE_INVALID;
#ifdef ENABLE_HFP_WIDE_BAND_SPEECH
static uint8_t codecs[] = {HFP_CODEC_CVSD, HFP_CODEC_MSBC};
#else
static uint8_t codecs[] = {HFP_CODEC_CVSD};
#endif
static uint16_t indicators[1] = {0x01};
static uint8_t  negotiated_codec = HFP_CODEC_CVSD;
static btstack_packet_callback_registration_t hci_event_callback_registration;
char cmd;

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

    printf("\n--- Bluetooth HFP Hands-Free (HF) unit Test Console %s ---\n", bd_addr_to_str(iut_address));
    printf("\n");
    printf("a - establish SLC to %s     | ", bd_addr_to_str(device_addr));
    printf("A - release SLC connection to device\n");
    printf("b - establish Audio connection             | B - release Audio connection\n");
    printf("d - query network operator                 | D - Enable HFP AG registration status update via bitmap(IIA)\n");
    printf("f - answer incoming call                   | F - Hangup call\n");
    printf("g - query network operator name            | G - reject incoming call\n");
    printf("i - dial 1234567                           | I - dial 7654321\n");
    printf("j - dial #1                                | J - dial #99\n");
    printf("o - Set speaker volume to 0  (minimum)     | O - Set speaker volume to 9  (default)\n");
    printf("p - Set speaker volume to 12 (higher)      | P - Set speaker volume to 15 (maximum)\n");
    printf("q - Set microphone gain to 0  (minimum)    | Q - Set microphone gain to 9  (default)\n");
    printf("s - Set microphone gain to 12 (higher)     | S - Set microphone gain to 15 (maximum)\n");
    printf("t - terminate connection\n");
    printf("u - send 'user busy' (TWC 0)\n");
    printf("U - end active call and accept other call' (TWC 1)\n");
    printf("v - Swap active call call (TWC 2)          | V - Join held call (TWC 3)\n");
    printf("w - Connect calls (TWC 4)                  | W - redial\n");
    printf("c/C - disable/enable registration status update for all AG indicators\n");
    printf("e/E - disable/enable reporting of the extended AG error result code\n");
    printf("k/K - deactivate/activate call waiting notification\n");
    printf("l/L - deactivate/activate calling line notification\n");
    printf("m/M - deactivate/activate echo canceling and noise reduction\n");
    printf("n/N - deactivate/activate voice recognition\n");
    printf("0123456789#*-+ - send DTMF dial tones\n");
    printf("x - request phone number for voice tag     | X - current call status (ECS)\n");
    printf("y - release call with index 2 (ECC)        | Y - private consulation with call 2(ECC)\n");
    printf("[ - Query Response and Hold status (RHH ?) | ] - Place call in a response and held state(RHH 0)\n");
    printf("{ - Accept held call(RHH 1)                | } - Reject held call(RHH 2)\n");
    printf("? - Query Subscriber Number (NUM)\n");
    printf("! - Update HF indicator with assigned number 1 (HFI)\n");
    printf("\n");
}

static void stdin_process(char c){
    cmd = c;    // used in packet handler

    if (cmd >= '0' && cmd <= '9'){
        printf("DTMF Code: %c\n", cmd);
        hfp_hf_send_dtmf_code(acl_handle, cmd);
        return;
    }

    switch (cmd){
        case '#':
        case '-':
        case '+':
        case '*':
            log_info("USER:\'%c\'", cmd);
            printf("DTMF Code: %c\n", cmd);
            hfp_hf_send_dtmf_code(acl_handle, cmd);
            break;
        case 'a':
            log_info("USER:\'%c\'", cmd);
            printf("Establish Service level connection to device with Bluetooth address %s...\n", bd_addr_to_str(device_addr));
            hfp_hf_establish_service_level_connection(device_addr);
            break;
        case 'A':
            log_info("USER:\'%c\'", cmd);
            printf("Release Service level connection.\n");
            hfp_hf_release_service_level_connection(acl_handle);
            break;
        case 'b':
            log_info("USER:\'%c\'", cmd);
            printf("Establish Audio connection to device with Bluetooth address %s...\n", bd_addr_to_str(device_addr));
            hfp_hf_establish_audio_connection(acl_handle);
            break;
        case 'B':
            log_info("USER:\'%c\'", cmd);
            printf("Release Audio service level connection.\n");
            hfp_hf_release_audio_connection(acl_handle);
            break;
        case 'C':
            log_info("USER:\'%c\'", cmd);
            printf("Enable registration status update for all AG indicators.\n");
            hfp_hf_enable_status_update_for_all_ag_indicators(acl_handle);
            break;
        case 'c':
            log_info("USER:\'%c\'", cmd);
            printf("Disable registration status update for all AG indicators.\n");
            hfp_hf_disable_status_update_for_all_ag_indicators(acl_handle);
            break;
        case 'D':
            log_info("USER:\'%c\'", cmd);
            printf("Set HFP AG registration status update for individual indicators (0111111).\n");
            hfp_hf_set_status_update_for_individual_ag_indicators(acl_handle, 63);
            break;
        case 'd':
            log_info("USER:\'%c\'", cmd);
            printf("Query network operator.\n");
            hfp_hf_query_operator_selection(acl_handle);
            break;
        case 'E':
            log_info("USER:\'%c\'", cmd);
            printf("Enable reporting of the extended AG error result code.\n");
            hfp_hf_enable_report_extended_audio_gateway_error_result_code(acl_handle);
            break;
        case 'e':
            log_info("USER:\'%c\'", cmd);
            printf("Disable reporting of the extended AG error result code.\n");
            hfp_hf_disable_report_extended_audio_gateway_error_result_code(acl_handle);
            break;
        case 'f':
            log_info("USER:\'%c\'", cmd);
            printf("Answer incoming call.\n");
            hfp_hf_answer_incoming_call(acl_handle);
            break;
        case 'F':
            log_info("USER:\'%c\'", cmd);
            printf("Hangup call.\n");
            hfp_hf_terminate_call(acl_handle);
            break;
        case 'G':
            log_info("USER:\'%c\'", cmd);
            printf("Reject incoming call.\n");
            hfp_hf_reject_incoming_call(acl_handle);
            break;
        case 'g':
            log_info("USER:\'%c\'", cmd);
            printf("Query operator.\n");
            hfp_hf_query_operator_selection(acl_handle);
            break;
        case 't':
            log_info("USER:\'%c\'", cmd);
            printf("Terminate HCI connection.\n");
            gap_disconnect(acl_handle);
            break;
        case 'i':
            log_info("USER:\'%c\'", cmd);
            printf("Dial 1234567\n");
            hfp_hf_dial_number(acl_handle, "1234567");
            break;
        case 'I':
            log_info("USER:\'%c\'", cmd);
            printf("Dial 7654321\n");
            hfp_hf_dial_number(acl_handle, "7654321");
            break;
        case 'j':
            log_info("USER:\'%c\'", cmd);
            printf("Dial #1\n");
            hfp_hf_dial_memory(acl_handle,1);
            break;
        case 'J':
            log_info("USER:\'%c\'", cmd);
            printf("Dial #99\n");
            hfp_hf_dial_memory(acl_handle,99);
            break;
        case 'k':
            log_info("USER:\'%c\'", cmd);
            printf("Deactivate call waiting notification\n");
            hfp_hf_deactivate_call_waiting_notification(acl_handle);
            break;
        case 'K':
            log_info("USER:\'%c\'", cmd);
            printf("Activate call waiting notification\n");
            hfp_hf_activate_call_waiting_notification(acl_handle);
            break;
        case 'l':
            log_info("USER:\'%c\'", cmd);
            printf("Deactivate calling line notification\n");
            hfp_hf_deactivate_calling_line_notification(acl_handle);
            break;
        case 'L':
            log_info("USER:\'%c\'", cmd);
            printf("Activate calling line notification\n");
            hfp_hf_activate_calling_line_notification(acl_handle);
            break;
        case 'm':
            log_info("USER:\'%c\'", cmd);
            printf("Deactivate echo canceling and noise reduction\n");
            hfp_hf_deactivate_echo_canceling_and_noise_reduction(acl_handle);
            break;
        case 'M':
            log_info("USER:\'%c\'", cmd);
            printf("Activate echo canceling and noise reduction\n");
            hfp_hf_activate_echo_canceling_and_noise_reduction(acl_handle);
            break;
        case 'n':
            log_info("USER:\'%c\'", cmd);
            printf("Deactivate voice recognition\n");
            hfp_hf_deactivate_voice_recognition_notification(acl_handle);
            break;
        case 'N':
            log_info("USER:\'%c\'", cmd);
            printf("Activate voice recognition %s\n", bd_addr_to_str(device_addr));
            hfp_hf_activate_voice_recognition_notification(acl_handle);
            break;
        case 'o':
            log_info("USER:\'%c\'", cmd);
            printf("Set speaker gain to 0 (minimum)\n");
            hfp_hf_set_speaker_gain(acl_handle, 0);
            break;
        case 'O':
            log_info("USER:\'%c\'", cmd);
            printf("Set speaker gain to 9 (default)\n");
            hfp_hf_set_speaker_gain(acl_handle, 9);
            break;
        case 'p':
            log_info("USER:\'%c\'", cmd);
            printf("Set speaker gain to 12 (higher)\n");
            hfp_hf_set_speaker_gain(acl_handle, 12);
            break;
        case 'P':
            log_info("USER:\'%c\'", cmd);
            printf("Set speaker gain to 15 (maximum)\n");
            hfp_hf_set_speaker_gain(acl_handle, 15);
            break;
        case 'q':
            log_info("USER:\'%c\'", cmd);
            printf("Set microphone gain to 0\n");
            hfp_hf_set_microphone_gain(acl_handle, 0);
            break;
        case 'Q':
            log_info("USER:\'%c\'", cmd);
            printf("Set microphone gain to 9\n");
            hfp_hf_set_microphone_gain(acl_handle, 9);
            break;
        case 's':
            log_info("USER:\'%c\'", cmd);
            printf("Set microphone gain to 12\n");
            hfp_hf_set_microphone_gain(acl_handle, 12);
            break;
        case 'S':
            log_info("USER:\'%c\'", cmd);
            printf("Set microphone gain to 15\n");
            hfp_hf_set_microphone_gain(acl_handle, 15);
            break;
        case 'u':
            log_info("USER:\'%c\'", cmd);
            printf("Send 'user busy' (Three-Way Call 0)\n");
            hfp_hf_user_busy(acl_handle);
            break;
        case 'U':
            log_info("USER:\'%c\'", cmd);
            printf("End active call and accept waiting/held call (Three-Way Call 1)\n");
            hfp_hf_end_active_and_accept_other(acl_handle);
            break;
        case 'v':
            log_info("USER:\'%c\'", cmd);
            printf("Swap active call and hold/waiting call (Three-Way Call 2)\n");
            hfp_hf_swap_calls(acl_handle);
            break;
        case 'V':
            log_info("USER:\'%c\'", cmd);
            printf("Join hold call (Three-Way Call 3)\n");
            hfp_hf_join_held_call(acl_handle);
            break;
        case 'w':
            log_info("USER:\'%c\'", cmd);
            printf("Connect calls (Three-Way Call 4)\n");
            hfp_hf_connect_calls(acl_handle);
            break;
        case 'W':
            log_info("USER:\'%c\'", cmd);
            printf("Redial\n");
            hfp_hf_redial_last_number(acl_handle);
            break;
        case 'x':
            log_info("USER:\'%c\'", cmd);
            printf("Request phone number for voice tag\n");
            hfp_hf_request_phone_number_for_voice_tag(acl_handle);
            break;
        case 'X':
            log_info("USER:\'%c\'", cmd);
            printf("Query current call status\n");
            hfp_hf_query_current_call_status(acl_handle);
            break;
        case 'y':
            log_info("USER:\'%c\'", cmd);
            printf("Release call with index 2\n");
            hfp_hf_release_call_with_index(acl_handle, 2);
            break;
        case 'Y':
            log_info("USER:\'%c\'", cmd);
            printf("Private consulation with call 2\n");
            hfp_hf_private_consultation_with_call(acl_handle, 2);
            break;
        case '[':
            log_info("USER:\'%c\'", cmd);
            printf("Query Response and Hold status (RHH ?)\n");
            hfp_hf_rrh_query_status(acl_handle);
            break;
        case ']':
            log_info("USER:\'%c\'", cmd);
            printf("Place call in a response and held state (RHH 0)\n");
            hfp_hf_rrh_hold_call(acl_handle);
           break;
        case '{':
            log_info("USER:\'%c\'", cmd);
            printf("Accept held call (RHH 1)\n");
            hfp_hf_rrh_accept_held_call(acl_handle);
            break;
        case '}':
            log_info("USER:\'%c\'", cmd);
            printf("Reject held call (RHH 2)\n");
            hfp_hf_rrh_reject_held_call(acl_handle);
            break;
        case '?':
            log_info("USER:\'%c\'", cmd);
            printf("Query Subscriber Number\n");
            hfp_hf_query_subscriber_number(acl_handle);
            break;
        case '!':
            log_info("USER:\'%c\'", cmd);
            printf("Update HF indicator with assigned number 1 (HFI)\n");
            hfp_hf_set_hf_indicator(acl_handle, 1, 1);
            break;
        default:
            show_usage();
            break;
    }
}
#endif

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t * event, uint16_t event_size){
    UNUSED(channel);
    
    switch (packet_type){

        case HCI_SCO_DATA_PACKET:
            if (READ_SCO_CONNECTION_HANDLE(event) != sco_handle) break;
            sco_demo_receive(event, event_size);
            break;

        case HCI_EVENT_PACKET:
            switch (event[0]){
                case HCI_EVENT_SCO_CAN_SEND_NOW:
                    sco_demo_send(sco_handle);
                    break;

                case HCI_EVENT_COMMAND_COMPLETE:
                    if (HCI_EVENT_IS_COMMAND_COMPLETE(event, hci_read_local_supported_features)){
                        dump_supported_codecs();
                    }
                    break;

                case HCI_EVENT_HFP_META:
                    switch (event[2]) {   
                        case HFP_SUBEVENT_SERVICE_LEVEL_CONNECTION_ESTABLISHED:
                            acl_handle = hfp_subevent_service_level_connection_established_get_con_handle(event);
                            hfp_subevent_service_level_connection_established_get_bd_addr(event, device_addr);
                            printf("Service level connection established %s.\n\n", bd_addr_to_str(device_addr));
                            break;
                        case HFP_SUBEVENT_SERVICE_LEVEL_CONNECTION_RELEASED:
                            acl_handle = HCI_CON_HANDLE_INVALID;
                            printf("Service level connection released.\n\n");
                            break;
                        case HFP_SUBEVENT_AUDIO_CONNECTION_ESTABLISHED:
                            if (hfp_subevent_audio_connection_established_get_status(event)){
                                printf("Audio connection establishment failed with status %u\n", hfp_subevent_audio_connection_established_get_status(event));
                            } else {
                                sco_handle = hfp_subevent_audio_connection_established_get_handle(event);
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
                            sco_handle = HCI_CON_HANDLE_INVALID;
                            printf("Audio connection released\n");
                            sco_demo_close();
                            break;
                        case HFP_SUBEVENT_COMPLETE:
                            switch (cmd){
                                case 'd':
                                    printf("HFP AG registration status update enabled.\n");
                                    break;
                                case 'e':
                                    printf("HFP AG registration status update for individual indicators set.\n");
                                    break;
                                default:
                                    break;
                            }
                            break;
                        case HFP_SUBEVENT_AG_INDICATOR_STATUS_CHANGED:
                            printf("AG_INDICATOR_STATUS_CHANGED, AG indicator (index: %d) to: %d of range [%d, %d], name '%s'\n", 
                                hfp_subevent_ag_indicator_status_changed_get_indicator_index(event), 
                                hfp_subevent_ag_indicator_status_changed_get_indicator_status(event),
                                hfp_subevent_ag_indicator_status_changed_get_indicator_min_range(event),
                                hfp_subevent_ag_indicator_status_changed_get_indicator_max_range(event),
                                (const char*) hfp_subevent_ag_indicator_status_changed_get_indicator_name(event));
                            break;
                        case HFP_SUBEVENT_NETWORK_OPERATOR_CHANGED:
                            printf("NETWORK_OPERATOR_CHANGED, operator mode: %d, format: %d, name: %s\n", 
                                hfp_subevent_network_operator_changed_get_network_operator_mode(event), 
                                hfp_subevent_network_operator_changed_get_network_operator_format(event), 
                                (char *) hfp_subevent_network_operator_changed_get_network_operator_name(event));          
                            break;
                        case HFP_SUBEVENT_EXTENDED_AUDIO_GATEWAY_ERROR:
                            printf("EXTENDED_AUDIO_GATEWAY_ERROR_REPORT, status : %d\n", 
                                hfp_subevent_extended_audio_gateway_error_get_error(event));
                            break;
                        case HFP_SUBEVENT_RING:
                            printf("** Ring **\n");
                            break;
                        case HFP_SUBEVENT_NUMBER_FOR_VOICE_TAG:
                            printf("Phone number for voice tag: %s\n", 
                                (const char *) hfp_subevent_number_for_voice_tag_get_number(event));
                            break;
                        case HFP_SUBEVENT_SPEAKER_VOLUME:
                            printf("Speaker volume: status %u, gain %u\n", 
                                hfp_subevent_speaker_volume_get_status(event),
                                hfp_subevent_speaker_volume_get_gain(event));
                            break;
                        case HFP_SUBEVENT_MICROPHONE_VOLUME:
                            printf("Microphone volume: status %u, gain %u\n", 
                                hfp_subevent_microphone_volume_get_status(event),
                                hfp_subevent_microphone_volume_get_gain(event));
                            break;
                        case HFP_SUBEVENT_CALLING_LINE_IDENTIFICATION_NOTIFICATION:
                            printf("Caller ID, number %s\n", hfp_subevent_calling_line_identification_notification_get_number(event));
                            break;
                        case HFP_SUBEVENT_ENHANCED_CALL_STATUS:
                            printf("Enhanced call status:\n");
                            printf("  - call index: %d \n", hfp_subevent_enhanced_call_status_get_clcc_idx(event));
                            printf("  - direction : %s \n", hfp_enhanced_call_dir2str(hfp_subevent_enhanced_call_status_get_clcc_dir(event)));
                            printf("  - status    : %s \n", hfp_enhanced_call_status2str(hfp_subevent_enhanced_call_status_get_clcc_status(event)));
                            printf("  - mode      : %s \n", hfp_enhanced_call_mode2str(hfp_subevent_enhanced_call_status_get_clcc_mode(event)));
                            printf("  - multipart : %s \n", hfp_enhanced_call_mpty2str(hfp_subevent_enhanced_call_status_get_clcc_mpty(event)));
                            printf("  - type      : %d \n", hfp_subevent_enhanced_call_status_get_bnip_type(event));
                            printf("  - number    : %s \n", hfp_subevent_enhanced_call_status_get_bnip_number(event));
                            break;
                        default:
                            printf("event not handled %u\n", event[2]);
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

/* @section Main Application Setup
 *
 * @text Listing MainConfiguration shows main application code. 
 * To run a HFP HF service you need to initialize the SDP, and to create and register HFP HF record with it. 
 * The packet_handler is used for sending commands to the HFP AG. It also receives the HFP AG's answers.
 * The stdin_process callback allows for sending commands to the HFP AG. 
 * At the end the Bluetooth stack is started.
 */

/* LISTING_START(MainConfiguration): Setup HFP Hands-Free unit */
int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    (void)argc;
    (void)argv;

    sco_demo_init();

    gap_discoverable_control(1);
    gap_set_class_of_device(0x200408);   
    gap_set_local_name("HFP HF Demo 00:00:00:00:00:00");

    // init L2CAP
    l2cap_init();

    uint16_t hf_supported_features          =
        (1<<HFP_HFSF_ESCO_S4)               |
        (1<<HFP_HFSF_CLI_PRESENTATION_CAPABILITY) |
        (1<<HFP_HFSF_HF_INDICATORS)         |
        (1<<HFP_HFSF_CODEC_NEGOTIATION)     |
        (1<<HFP_HFSF_ENHANCED_CALL_STATUS)  |
        (1<<HFP_HFSF_REMOTE_VOLUME_CONTROL);
    int wide_band_speech = 1;

    rfcomm_init();
    hfp_hf_init(rfcomm_channel_nr);
    hfp_hf_init_supported_features(hf_supported_features);
    hfp_hf_init_hf_indicators(sizeof(indicators)/sizeof(uint16_t), indicators);
    hfp_hf_init_codecs(sizeof(codecs), codecs);
    
    sdp_init();    
    memset(hfp_service_buffer, 0, sizeof(hfp_service_buffer));
    hfp_hf_create_sdp_record(hfp_service_buffer, 0x10001, rfcomm_channel_nr, hfp_hf_service_name, hf_supported_features, wide_band_speech);
    printf("SDP service record size: %u\n", de_get_len(hfp_service_buffer));
    sdp_register_service(hfp_service_buffer);

    // register for HCI events and SCO packets
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);
    hci_register_sco_packet_handler(&packet_handler);
    hci_register_sco_packet_handler(&packet_handler);

    // register for HFP events
    hfp_hf_register_packet_handler(packet_handler);

#ifdef HAVE_BTSTACK_STDIN
    // parse human readable Bluetooth address
    sscanf_bd_addr(device_addr_string, device_addr);
    btstack_stdin_setup(stdin_process);
#endif
    // turn on!
    hci_power_control(HCI_POWER_ON);
    return 0;
}
/* LISTING_END */
/* EXAMPLE_END */
