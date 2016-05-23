
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

/*
 * hfp_hs_demo.c
 */

// *****************************************************************************
/* EXAMPLE_START(hfp_hs_demo): HFP Hands-Free (HF) Demo
 *
 * @text This  HFP Hands-Free example demonstrates how to receive 
 * an output from a remote HFP audio gateway (AG), and, 
 * if HAVE_POSIX_STDIN is defined, how to control the HFP AG. 
 */
// *****************************************************************************


#include "btstack_config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "btstack.h"
#include "sco_demo_util.h"
#ifdef HAVE_POSIX_STDIN
#include "stdin_support.h"
#endif


uint8_t hfp_service_buffer[150];
const uint8_t   rfcomm_channel_nr = 1;
const char hfp_hf_service_name[] = "BTstack HFP HF Demo";

#ifdef HAVE_POSIX_STDIN
static bd_addr_t device_addr = {0x80,0xbe,0x05,0xd5,0x28,0x48};
// 80:BE:05:D5:28:48
// prototypes
static void show_usage(void);
#endif
static uint16_t handle = -1;
static hci_con_handle_t sco_handle;
static uint8_t codecs[] = {HFP_CODEC_CVSD, HFP_CODEC_MSBC};
static uint16_t indicators[1] = {0x01};

char cmd;

#ifdef HAVE_POSIX_STDIN

// Testig User Interface 
static void show_usage(void){
    printf("\n--- Bluetooth HFP Hands-Free (HF) unit Test Console ---\n");
    printf("---\n");

    printf("a - establish SLC connection to device %s\n", bd_addr_to_str(device_addr));
    printf("A - release SLC connection to device\n");
    
    printf("b - establish Audio connection\n");
    printf("B - release Audio connection\n");
    
    printf("c - disable registration status update for all AG indicators\n");
    printf("C - enable registration status update for all AG indicators\n");
    
    printf("d - query network operator.\n");
    printf("D - set HFP AG registration status update for individual indicators (IIA)\n");

    printf("e - disable reporting of the extended AG error result code\n");
    printf("E - enable reporting of the extended AG error result code\n");
    
    printf("f - answer incoming call\n");
    printf("F - Hangup call\n");

    printf("g - query network operator name\n");
    printf("G - reject incoming call\n");

    printf("i - dial 1234567\n");
    printf("I - dial 7654321\n");
    
    printf("j - dial #1\n");
    printf("J - dial #99\n");
    
    printf("k - deactivate call waiting notification\n");
    printf("K - activate call waiting notification\n");
    
    printf("l - deactivate calling line notification\n");
    printf("L - activate calling line notification\n");
    
    printf("m - deactivate echo canceling and noise reduction\n");
    printf("M - activate echo canceling and noise reduction\n");
    
    printf("n - deactivate voice recognition\n");
    printf("N - activate voice recognition\n");
    
    printf("o - Set speaker volume to 0  (minimum)\n");
    printf("O - Set speaker volume to 9  (default)\n");
    printf("p - Set speaker volume to 12 (higher)\n");
    printf("P - Set speaker volume to 15 (maximum)\n");

    printf("q - Set microphone gain to 0  (minimum)\n");
    printf("Q - Set microphone gain to 9  (default)\n");
    printf("s - Set microphone gain to 12 (higher)\n");
    printf("S - Set microphone gain to 15 (maximum)\n");

    printf("t - terminate connection\n");

    printf("u - send 'user busy' (TWC 0)\n");
    printf("U - end active call and accept other call' (TWC 1)\n");
    printf("v - Swap active call and hold/waiting call (TWC 2)\n");
    printf("V - Join held call (TWC 3)\n");
    printf("w - Connect calls (TWC 4)\n");
    printf("W - redial\n");

    printf("0123456789#*-+ - send DTMF dial tones\n");

    printf("x - request phone number for voice tag\n");
    printf("X - current call status (ECS)\n");
    printf("y - release call with index 2 (ECC)\n");
    printf("Y - private consulation with call 2(ECC)\n");

    printf("[ - Query Response and Hold status (RHH ?)\n");
    printf("] - Place call in a response and held state(RHH 0)\n");
    printf("{ - Accept held call(RHH 1)\n");
    printf("} - Reject held call(RHH 2)\n");

    printf("? - Query Subscriber Number (NUM)\n");

    printf("! - Update HF indicator with assigned number 1 (HFI)\n");

    printf("---\n");
    printf("Ctrl-c - exit\n");
    printf("---\n");
}

static void stdin_process(btstack_data_source_t *ds, btstack_data_source_callback_type_t callback_type){
    read(ds->fd, &cmd, 1);

    if (cmd >= '0' && cmd <= '9'){
        printf("DTMF Code: %c\n", cmd);
        hfp_hf_send_dtmf_code(device_addr, cmd);
        return;
    }

    switch (cmd){
        case '#':
        case '-':
        case '+':
        case '*':
            log_info("USER:\'%c\'", cmd);
            printf("DTMF Code: %c\n", cmd);
            hfp_hf_send_dtmf_code(device_addr, cmd);
            break;
        case 'a':
            log_info("USER:\'%c\'", cmd);
            printf("Establish Service level connection to device with Bluetooth address %s...\n", bd_addr_to_str(device_addr));
            hfp_hf_establish_service_level_connection(device_addr);
            break;
        case 'A':
            log_info("USER:\'%c\'", cmd);
            printf("Release Service level connection.\n");
            hfp_hf_release_service_level_connection(device_addr);
            break;
        case 'b':
            log_info("USER:\'%c\'", cmd);
            printf("Establish Audio connection to device with Bluetooth address %s...\n", bd_addr_to_str(device_addr));
            hfp_hf_establish_audio_connection(device_addr);
            break;
        case 'B':
            log_info("USER:\'%c\'", cmd);
            printf("Release Audio service level connection.\n");
            hfp_hf_release_audio_connection(device_addr);
            break;
        case 'C':
            log_info("USER:\'%c\'", cmd);
            printf("Enable registration status update for all AG indicators.\n");
            hfp_hf_enable_status_update_for_all_ag_indicators(device_addr);
        case 'c':
            log_info("USER:\'%c\'", cmd);
            printf("Disable registration status update for all AG indicators.\n");
            hfp_hf_disable_status_update_for_all_ag_indicators(device_addr);
            break;
        case 'D':
            log_info("USER:\'%c\'", cmd);
            printf("Set HFP AG registration status update for individual indicators (0111111).\n");
            hfp_hf_set_status_update_for_individual_ag_indicators(device_addr, 63);
            break;
        case 'd':
            log_info("USER:\'%c\'", cmd);
            printf("Query network operator.\n");
            hfp_hf_query_operator_selection(device_addr);
            break;
        case 'E':
            log_info("USER:\'%c\'", cmd);
            printf("Enable reporting of the extended AG error result code.\n");
            hfp_hf_enable_report_extended_audio_gateway_error_result_code(device_addr);
            break;
        case 'e':
            log_info("USER:\'%c\'", cmd);
            printf("Disable reporting of the extended AG error result code.\n");
            hfp_hf_disable_report_extended_audio_gateway_error_result_code(device_addr);
            break;
        case 'f':
            log_info("USER:\'%c\'", cmd);
            printf("Answer incoming call.\n");
            hfp_hf_answer_incoming_call(device_addr);
            break;
        case 'F':
            log_info("USER:\'%c\'", cmd);
            printf("Hangup call.\n");
            hfp_hf_terminate_call(device_addr);
            break;
        case 'G':
            log_info("USER:\'%c\'", cmd);
            printf("Reject incoming call.\n");
            hfp_hf_reject_incoming_call(device_addr);
            break;
        case 'g':
            log_info("USER:\'%c\'", cmd);
            printf("Query operator.\n");
            hfp_hf_query_operator_selection(device_addr);
            break;
        case 't':
            log_info("USER:\'%c\'", cmd);
            printf("Terminate HCI connection.\n");
            gap_disconnect(handle);
            break;
        case 'i':
            log_info("USER:\'%c\'", cmd);
            printf("Dial 1234567\n");
            hfp_hf_dial_number(device_addr, "1234567");
            break;
        case 'I':
            log_info("USER:\'%c\'", cmd);
            printf("Dial 7654321\n");
            hfp_hf_dial_number(device_addr, "7654321");
            break;
        case 'j':
            log_info("USER:\'%c\'", cmd);
            printf("Dial #1\n");
            hfp_hf_dial_memory(device_addr,1);
            break;
        case 'J':
            log_info("USER:\'%c\'", cmd);
            printf("Dial #99\n");
            hfp_hf_dial_memory(device_addr,99);
            break;
        case 'k':
            log_info("USER:\'%c\'", cmd);
            printf("Deactivate call waiting notification\n");
            hfp_hf_deactivate_call_waiting_notification(device_addr);
            break;
        case 'K':
            log_info("USER:\'%c\'", cmd);
            printf("Activate call waiting notification\n");
            hfp_hf_activate_call_waiting_notification(device_addr);
            break;
        case 'l':
            log_info("USER:\'%c\'", cmd);
            printf("Deactivate calling line notification\n");
            hfp_hf_deactivate_calling_line_notification(device_addr);
            break;
        case 'L':
            log_info("USER:\'%c\'", cmd);
            printf("Activate calling line notification\n");
            hfp_hf_activate_calling_line_notification(device_addr);
            break;
        case 'm':
            log_info("USER:\'%c\'", cmd);
            printf("Deactivate echo canceling and noise reduction\n");
            hfp_hf_deactivate_echo_canceling_and_noise_reduction(device_addr);
            break;
        case 'M':
            log_info("USER:\'%c\'", cmd);
            printf("Activate echo canceling and noise reduction\n");
            hfp_hf_activate_echo_canceling_and_noise_reduction(device_addr);
            break;
        case 'n':
            log_info("USER:\'%c\'", cmd);
            printf("Deactivate voice recognition\n");
            hfp_hf_deactivate_voice_recognition_notification(device_addr);
            break;
        case 'N':
            log_info("USER:\'%c\'", cmd);
            printf("Activate voice recognition\n");
            hfp_hf_activate_voice_recognition_notification(device_addr);
            break;
        case 'o':
            log_info("USER:\'%c\'", cmd);
            printf("Set speaker gain to 0 (minimum)\n");
            hfp_hf_set_speaker_gain(device_addr, 0);
            break;
        case 'O':
            log_info("USER:\'%c\'", cmd);
            printf("Set speaker gain to 9 (default)\n");
            hfp_hf_set_speaker_gain(device_addr, 9);
            break;
        case 'p':
            log_info("USER:\'%c\'", cmd);
            printf("Set speaker gain to 12 (higher)\n");
            hfp_hf_set_speaker_gain(device_addr, 12);
            break;
        case 'P':
            log_info("USER:\'%c\'", cmd);
            printf("Set speaker gain to 15 (maximum)\n");
            hfp_hf_set_speaker_gain(device_addr, 15);
            break;
        case 'q':
            log_info("USER:\'%c\'", cmd);
            printf("Set microphone gain to 0\n");
            hfp_hf_set_microphone_gain(device_addr, 0);
            break;
        case 'Q':
            log_info("USER:\'%c\'", cmd);
            printf("Set microphone gain to 9\n");
            hfp_hf_set_microphone_gain(device_addr, 9);
            break;
        case 's':
            log_info("USER:\'%c\'", cmd);
            printf("Set microphone gain to 12\n");
            hfp_hf_set_microphone_gain(device_addr, 12);
            break;
        case 'S':
            log_info("USER:\'%c\'", cmd);
            printf("Set microphone gain to 15\n");
            hfp_hf_set_microphone_gain(device_addr, 15);
            break;
        case 'u':
            log_info("USER:\'%c\'", cmd);
            printf("Send 'user busy' (Three-Way Call 0)\n");
            hfp_hf_user_busy(device_addr);
            break;
        case 'U':
            log_info("USER:\'%c\'", cmd);
            printf("End active call and accept waiting/held call (Three-Way Call 1)\n");
            hfp_hf_end_active_and_accept_other(device_addr);
            break;
        case 'v':
            log_info("USER:\'%c\'", cmd);
            printf("Swap active call and hold/waiting call (Three-Way Call 2)\n");
            hfp_hf_swap_calls(device_addr);
            break;
        case 'V':
            log_info("USER:\'%c\'", cmd);
            printf("Join hold call (Three-Way Call 3)\n");
            hfp_hf_join_held_call(device_addr);
            break;
        case 'w':
            log_info("USER:\'%c\'", cmd);
            printf("Connect calls (Three-Way Call 4)\n");
            hfp_hf_connect_calls(device_addr);
            break;
        case 'W':
            log_info("USER:\'%c\'", cmd);
            printf("Redial\n");
            hfp_hf_redial_last_number(device_addr);
            break;
        case 'x':
            log_info("USER:\'%c\'", cmd);
            printf("Request phone number for voice tag\n");
            hfp_hf_request_phone_number_for_voice_tag(device_addr);
            break;
        case 'X':
            log_info("USER:\'%c\'", cmd);
            printf("Query current call status\n");
            hfp_hf_query_current_call_status(device_addr);
            break;
        case 'y':
            log_info("USER:\'%c\'", cmd);
            printf("Release call with index 2\n");
            hfp_hf_release_call_with_index(device_addr, 2);
            break;
        case 'Y':
            log_info("USER:\'%c\'", cmd);
            printf("Private consulation with call 2\n");
            hfp_hf_private_consultation_with_call(device_addr, 2);
            break;
        case '[':
            log_info("USER:\'%c\'", cmd);
            printf("Query Response and Hold status (RHH ?)\n");
            hfp_hf_rrh_query_status(device_addr);
            break;
        case ']':
            log_info("USER:\'%c\'", cmd);
            printf("Place call in a response and held state (RHH 0)\n");
            hfp_hf_rrh_hold_call(device_addr);
           break;
        case '{':
            log_info("USER:\'%c\'", cmd);
            printf("Accept held call (RHH 1)\n");
            hfp_hf_rrh_accept_held_call(device_addr);
            break;
        case '}':
            log_info("USER:\'%c\'", cmd);
            printf("Reject held call (RHH 2)\n");
            hfp_hf_rrh_reject_held_call(device_addr);
            break;
        case '?':
            log_info("USER:\'%c\'", cmd);
            printf("Query Subscriber Number\n");
            hfp_hf_query_subscriber_number(device_addr);
            break;
        case '!':
            log_info("USER:\'%c\'", cmd);
            printf("Update HF indicator with assigned number 1 (HFI)\n");
            hfp_hf_set_hf_indicator(device_addr, 1, 1);
            break;

        default:
            show_usage();
            break;
    }
}
#endif

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t * event, uint16_t event_size){

    if (event[0] == HCI_EVENT_SCO_CAN_SEND_NOW){
        sco_demo_send(sco_handle);
        return;
    }

    if (event[0] != HCI_EVENT_HFP_META) return;

    switch (event[2]) {   
        case HFP_SUBEVENT_SERVICE_LEVEL_CONNECTION_ESTABLISHED:
            handle = hfp_subevent_service_level_connection_established_get_con_handle(event);
            printf("Service level connection established.\n\n");
            break;
        case HFP_SUBEVENT_SERVICE_LEVEL_CONNECTION_RELEASED:
            printf("Service level connection released.\n\n");
            break;
        case HFP_SUBEVENT_AUDIO_CONNECTION_ESTABLISHED:
            if (hfp_subevent_audio_connection_established_get_status(event)){
                sco_handle = 0;
                printf("Audio connection establishment failed with status %u\n", hfp_subevent_audio_connection_established_get_status(event));
            } else {
                sco_handle = hfp_subevent_audio_connection_established_get_handle(event);
                printf("Audio connection established with SCO handle 0x%04x.\n", sco_handle);
                hci_request_sco_can_send_now_event();
            }
            break;
        case HFP_SUBEVENT_AUDIO_CONNECTION_RELEASED:
            sco_handle = 0;
            printf("Audio connection released\n");
            break;
        case HFP_SUBEVENT_COMPLETE:
            switch (cmd){
                case 'd':
                    printf("HFP AG registration status update enabled.\n");
                    break;
                case 'e':
                    printf("HFP AG registration status update for individual indicators set.\n");
                default:
                    break;
            }
            break;
        case HFP_SUBEVENT_AG_INDICATOR_STATUS_CHANGED:
            printf("AG_INDICATOR_STATUS_CHANGED, AG indicator '%s' (index: %d) to: %d\n", (const char*) &event[5], event[3], event[4]);
            break;
        case HFP_SUBEVENT_NETWORK_OPERATOR_CHANGED:
            printf("NETWORK_OPERATOR_CHANGED, operator mode: %d, format: %d, name: %s\n", event[3], event[4], (char *) &event[5]);
            break;
        case HFP_SUBEVENT_EXTENDED_AUDIO_GATEWAY_ERROR:
            if (event[4])
            printf("EXTENDED_AUDIO_GATEWAY_ERROR_REPORT, status : %d\n", event[3]);
            break;
        case HFP_SUBEVENT_RING:
            printf("** Ring **\n");
            break;
        case HFP_SUBEVENT_NUMBER_FOR_VOICE_TAG:
            printf("Phone number for voice tag: %s\n", (const char *) &event[3]);
            break;
        case HFP_SUBEVENT_SPEAKER_VOLUME:
            printf("Speaker volume: %u\n", event[3]);
            break;
        case HFP_SUBEVENT_MICROPHONE_VOLUME:
            printf("Microphone volume: %u\n", event[3]);
            break;
        default:
            printf("event not handled %u\n", event[2]);
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

    sco_demo_init();

    gap_discoverable_control(1);

    // HFP AG address is hardcoded, please change it
    // init L2CAP
    l2cap_init();
    rfcomm_init();
    sdp_init();    

    hfp_hf_init(rfcomm_channel_nr);
    hfp_hf_init_supported_features(438 | (1<<HFP_HFSF_ESCO_S4) | (1<<HFP_HFSF_EC_NR_FUNCTION)); 
    hfp_hf_init_hf_indicators(sizeof(indicators)/sizeof(uint16_t), indicators);
    hfp_hf_init_codecs(sizeof(codecs), codecs);
    
    hfp_hf_register_packet_handler(packet_handler);
    hci_register_sco_packet_handler(&packet_handler);

    memset(hfp_service_buffer, 0, sizeof(hfp_service_buffer));
    hfp_hf_create_sdp_record(hfp_service_buffer, 0x10001, rfcomm_channel_nr, hfp_hf_service_name, 0);
    printf("SDP service record size: %u\n", de_get_len(hfp_service_buffer));
    sdp_register_service(hfp_service_buffer);

#ifdef HAVE_POSIX_STDIN
    btstack_stdin_setup(stdin_process);
#endif    
    // turn on!
    hci_power_control(HCI_POWER_ON);
    return 0;
}
/* LISTING_END */
/* EXAMPLE_END */
