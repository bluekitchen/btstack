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
 
// *****************************************************************************
//
// HFG HF state machine tests
//
// *****************************************************************************

#include "btstack-config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include <btstack/hci_cmds.h>
#include <btstack/run_loop.h>
#include <btstack/sdp_util.h>

#include "hci.h"
#include "l2cap.h"
#include "rfcomm.h"
#include "sdp.h"
#include "sdp_parser.h"
#include "debug.h"
#include "hfp_hf.h"

#include "mock.h"
#include "test_sequences.h"

const uint8_t    rfcomm_channel_nr = 1;

static bd_addr_t device_addr = {0xD8,0xBb,0x2C,0xDf,0xF1,0x08};

static uint8_t codecs[2] = {1,2};
static uint16_t indicators[1] = {0x01};

static uint8_t service_level_connection_established = 0;
static uint8_t codecs_connection_established = 0;
static uint8_t audio_connection_established = 0;
static uint8_t start_ringing = 0;
static uint8_t stop_ringing = 0;
static uint8_t call_termiated = 0;

static int supported_features_with_codec_negotiation = 438;    

static uint16_t handle = -1;

char * get_next_hfp_hf_command(){
    return get_next_hfp_command(0,2);
}

int has_more_hfp_hf_commands(){
    return has_more_hfp_commands(0,2);
}

static void user_command(char cmd){
    switch (cmd){
        case '#':
        case '-':
        case '+':
        case '*':
            printf("DTMF Code: %c\n", cmd);
            hfp_hf_send_dtmf_code(device_addr, cmd);
            break;
        case 'a':
            printf("Establish Service level connection to device with Bluetooth address %s...\n", bd_addr_to_str(device_addr));
            hfp_hf_establish_service_level_connection(device_addr);
            break;
        case 'A':
            printf("Release Service level connection.\n");
            hfp_hf_release_service_level_connection(device_addr);
            break;
        case 'b':
            printf("Establish Audio connection to device with Bluetooth address %s...\n", bd_addr_to_str(device_addr));
            hfp_hf_establish_audio_connection(device_addr);
            break;
        case 'B':
            printf("Release Audio service level connection.\n");
            hfp_hf_release_audio_connection(device_addr);
            break;
        case 'C':
            printf("Enable registration status update for all AG indicators.\n");
            hfp_hf_enable_status_update_for_all_ag_indicators(device_addr);
        case 'c':
            printf("Disable registration status update for all AG indicators.\n");
            hfp_hf_disable_status_update_for_all_ag_indicators(device_addr);
            break;
        case 'D':
            printf("Set HFP AG registration status update for individual indicators (0111111).\n");
            hfp_hf_set_status_update_for_individual_ag_indicators(device_addr, 63);
            break;
        case 'd':
            printf("Query network operator.\n");
            hfp_hf_query_operator_selection(device_addr);
            break;
        case 'E':
            printf("Enable reporting of the extended AG error result code.\n");
            hfp_hf_enable_report_extended_audio_gateway_error_result_code(device_addr);
            break;
        case 'e':
            printf("Disable reporting of the extended AG error result code.\n");
            hfp_hf_disable_report_extended_audio_gateway_error_result_code(device_addr);
            break;
        case 'f':
            printf("Answer incoming call.\n");
            hfp_hf_answer_incoming_call(device_addr);
            break;
        case 'F':
            printf("Hangup call.\n");
            hfp_hf_terminate_call(device_addr);
            break;
        case 'G':
            printf("Reject call.\n");
            hfp_hf_reject_call(device_addr);
            break;
        case 'g':
            printf("Query operator.\n");
            hfp_hf_query_operator_selection(device_addr);
            break;
        case 't':
            printf("Terminate HCI connection.\n");
            gap_disconnect(handle);
            break;
        case 'i':
            printf("Dial 1234567\n");
            hfp_hf_dial_number(device_addr, (char *)"1234567");
            break;
        case 'I':
            printf("Dial 7654321\n");
            hfp_hf_dial_number(device_addr, (char *)"7654321");
            break;
        case 'j':
            printf("Dial #1\n");
            hfp_hf_dial_memory(device_addr, (char *)"1");
            break;
        case 'J':
            printf("Dial #99\n");
            hfp_hf_dial_memory(device_addr, (char *)"99");
            break;
        case 'k':
            printf("Deactivate call waiting notification\n");
            hfp_hf_deactivate_call_waiting_notification(device_addr);
            break;
        case 'K':
            printf("Activate call waiting notification\n");
            hfp_hf_activate_call_waiting_notification(device_addr);
            break;
        case 'l':
            printf("Deactivate calling line notification\n");
            hfp_hf_deactivate_calling_line_notification(device_addr);
            break;
        case 'L':
            printf("Activate calling line notification\n");
            hfp_hf_activate_calling_line_notification(device_addr);
            break;
        case 'm':
            printf("Deactivate echo canceling and noise reduction\n");
            hfp_hf_deactivate_echo_canceling_and_noise_reduction(device_addr);
            break;
        case 'M':
            printf("Activate echo canceling and noise reduction\n");
            hfp_hf_activate_echo_canceling_and_noise_reduction(device_addr);
            break;
        case 'n':
            printf("Deactivate voice recognition\n");
            hfp_hf_deactivate_voice_recognition_notification(device_addr);
            break;
        case 'N':
            printf("Activate voice recognition\n");
            hfp_hf_activate_voice_recognition_notification(device_addr);
            break;
        case 'o':
            printf("Set speaker gain to 0 (minimum)\n");
            hfp_hf_set_speaker_gain(device_addr, 0);
            break;
        case 'O':
            printf("Set speaker gain to 9 (default)\n");
            hfp_hf_set_speaker_gain(device_addr, 9);
            break;
        case 'p':
            printf("Set speaker gain to 12 (higher)\n");
            hfp_hf_set_speaker_gain(device_addr, 12);
            break;
        case 'P':
            printf("Set speaker gain to 15 (maximum)\n");
            hfp_hf_set_speaker_gain(device_addr, 15);
            break;
        case 'q':
            printf("Set microphone gain to 0\n");
            hfp_hf_set_microphone_gain(device_addr, 0);
            break;
        case 'Q':
            printf("Set microphone gain to 9\n");
            hfp_hf_set_microphone_gain(device_addr, 9);
            break;
        case 's':
            printf("Set microphone gain to 12\n");
            hfp_hf_set_microphone_gain(device_addr, 12);
            break;
        case 'S':
            printf("Set microphone gain to 15\n");
            hfp_hf_set_microphone_gain(device_addr, 15);
            break;
        case 'u':
            printf("Send 'user busy' (Three-Way Call 0)\n");
            hfp_hf_user_busy(device_addr);
            break;
        case 'U':
            printf("End active call and accept waiting/held call (Three-Way Call 1)\n");
            hfp_hf_end_active_and_accept_other(device_addr);
            break;
        case 'v':
            printf("Swap active call and hold/waiting call (Three-Way Call 2)\n");
            hfp_hf_swap_calls(device_addr);
            break;
        case 'V':
            printf("Join hold call (Three-Way Call 3)\n");
            hfp_hf_join_held_call(device_addr);
            break;
        case 'w':
            printf("Connect calls (Three-Way Call 4)\n");
            hfp_hf_connect_calls(device_addr);
            break;
        case 'W':
            printf("Redial\n");
            hfp_hf_redial_last_number(device_addr);
            break;
        case 'x':
            printf("Request phone number for voice tag\n");
            hfp_hf_request_phone_number_for_voice_tag(device_addr);
            break;
        case 'X':
            printf("Query current call status\n");
            hfp_hf_query_current_call_status(device_addr);
            break;
        case 'y':
            printf("Release call with index 2\n");
            hfp_hf_release_call_with_index(device_addr, 2);
            break;
        case 'Y':
            printf("Private consulation with call 2\n");
            hfp_hf_private_consultation_with_call(device_addr, 2);
            break;
        case '[':
            printf("Query Response and Hold status (RHH ?)\n");
            hfp_hf_rrh_query_status(device_addr);
            break;
        case ']':
            printf("Place call in a response and held state (RHH 0)\n");
            hfp_hf_rrh_hold_call(device_addr);
           break;
        case '{':
            printf("Accept held call (RHH 1)\n");
            hfp_hf_rrh_accept_held_call(device_addr);
            break;
        case '}':
            printf("Reject held call (RHH 2)\n");
            hfp_hf_rrh_reject_held_call(device_addr);
            break;
        case '?':
            printf("Query Subscriber Number\n");
            hfp_hf_query_subscriber_number(device_addr);
            break;
        case '!':
            printf("Update HF indicator with assigned number 1 (HFI)\n");
            hfp_hf_set_hf_indicator(device_addr, 1, 1);
            break;
        default:
            printf("HF: undefined user command\n");
            break;
    }
}

void simulate_test_sequence(hfp_test_item_t * test_item){
    char ** test_steps = test_item->test;
    printf("\nSimulate test sequence: \"%s\" [%d steps]\n", test_item->name, test_item->len);

    int i = 0;
    int previous_step = -1;
    while ( i < test_item->len){
        previous_step++;
        CHECK_EQUAL(i >= previous_step, 1);
        
        char * expected_cmd = test_steps[i];
        int expected_cmd_len = strlen(expected_cmd);
        printf("\nStep %d, %s \n", i, expected_cmd);
        
        if (strncmp(expected_cmd, "USER:", 5) == 0){
            user_command(expected_cmd[5]);
            while (has_more_hfp_hf_commands()){
                // empty rfcomm payload buffer
                get_next_hfp_hf_command();
            }
            i++;
       
        } else if (strncmp(expected_cmd, "AT+BAC=", 7) == 0){
            int parsed_codecs[2];
            uint8_t new_codecs[2];
            sscanf(&expected_cmd[7],"%d,%d", &parsed_codecs[0], &parsed_codecs[1]);
            new_codecs[0] = parsed_codecs[0];
            new_codecs[1] = parsed_codecs[1];
            hfp_hf_set_codecs((uint8_t*)new_codecs, 2);
            while (has_more_hfp_hf_commands()){
                // empty rfcomm payload buffer
                get_next_hfp_hf_command();
            }
            i++;
        } else if (strncmp(expected_cmd, "AT+BRSF=", 8) == 0 ){
            int supported_features = 0;
            sscanf(&expected_cmd[8],"%d", &supported_features);
            printf("Call hfp_hf_init with SF %d\n", supported_features);
            hfp_hf_release_service_level_connection(device_addr);
            hfp_hf_init(rfcomm_channel_nr, supported_features, indicators, sizeof(indicators)/sizeof(uint16_t), 1);
            user_command('a');
            while (has_more_hfp_hf_commands()){
                // empty rfcomm payload buffer
                get_next_hfp_hf_command();
            }
            i++;
        }  else if (strncmp(expected_cmd, "AT+BCC", 6) == 0){
            user_command('b');
            while (has_more_hfp_hf_commands()){
                // empty rfcomm payload buffer
                get_next_hfp_hf_command();
            }
            i++;
         }  else if (strncmp(expected_cmd, "AT", 2) == 0){
            printf("\n---> NEXT STEP expect from HF: %s\n", expected_cmd);
            while (has_more_hfp_hf_commands()){
                char * ag_cmd = get_next_hfp_hf_command();
                printf("HF response verify %s == %s[%d]\n", expected_cmd, ag_cmd, expected_cmd_len);

                int equal_cmds = strncmp(ag_cmd, expected_cmd, expected_cmd_len) == 0;
                if (!equal_cmds){
                    printf("\nError: Expected:'%s', but got:'%s'\n", expected_cmd, ag_cmd);
                    CHECK_EQUAL(equal_cmds,1);
                    return;
                } 
                printf("Verified: '%s'\n", expected_cmd);
                i++;
                if (i < test_item->len){
                    expected_cmd = test_steps[i];
                    expected_cmd_len = strlen(expected_cmd);
                } 
            } 
        } else {
            //printf("\n---> NEXT STEP receive from AG: '%s'\n", expected_cmd);
            inject_hfp_command_to_hf((uint8_t*)expected_cmd, strlen(expected_cmd));
            i++;
        }
    }
}

void packet_handler(uint8_t * event, uint16_t event_size){
    if (event[0] != HCI_EVENT_HFP_META) return;
    if (event[3]
     && event[2] != HFP_SUBEVENT_EXTENDED_AUDIO_GATEWAY_ERROR
     && event[2] != HFP_SUBEVENT_NUMBER_FOR_VOICE_TAG
     && event[2] != HFP_SUBEVENT_SPEAKER_VOLUME
     && event[2] != HFP_SUBEVENT_MICROPHONE_VOLUME){
        printf("ERROR, status: %u\n", event[3]);
        return;
    }

    switch (event[2]) {   
        case HFP_SUBEVENT_SERVICE_LEVEL_CONNECTION_ESTABLISHED:
            printf("\n** SLC established **\n\n");
            service_level_connection_established = 1;
            codecs_connection_established = 0;
            audio_connection_established = 0;
            break;
        case HFP_SUBEVENT_CODECS_CONNECTION_COMPLETE:
            printf("\n** CC established **\n\n");
            codecs_connection_established = 1;
            audio_connection_established = 0;
            break;
        case HFP_SUBEVENT_SERVICE_LEVEL_CONNECTION_RELEASED:
            printf("\n** SLC released **\n\n");
            service_level_connection_established = 0;
            break;
        case HFP_SUBEVENT_AUDIO_CONNECTION_ESTABLISHED:
            printf("\n** AC established **\n\n");
            audio_connection_established = 1;
            break;
        case HFP_SUBEVENT_AUDIO_CONNECTION_RELEASED:
            printf("\n** AC released **\n\n");
            audio_connection_established = 0;
            break;
        case HFP_SUBEVENT_START_RINGINIG:
            printf("\n** Start ringing **\n\n"); 
            start_ringing = 1;
            break;
        case HFP_SUBEVENT_STOP_RINGINIG:
            printf("\n** Stop ringing **\n\n"); 
            stop_ringing = 1;
            start_ringing = 0;
            break;
        case HFP_SUBEVENT_CALL_TERMINATED:
            call_termiated = 1;
            break;
        case HFP_SUBEVENT_COMPLETE:
            printf("HFP AG HFP_SUBEVENT_COMPLETE.\n");
            break;
        case HFP_SUBEVENT_AG_INDICATOR_STATUS_CHANGED:
            printf("AG_INDICATOR_STATUS_CHANGED, AG indicator '%s' (index: %d) to: %d\n", (const char*) &event[6], event[4], event[5]);
            break;
        case HFP_SUBEVENT_NETWORK_OPERATOR_CHANGED:
            printf("NETWORK_OPERATOR_CHANGED, operator mode: %d, format: %d, name: %s\n", event[4], event[5], (char *) &event[6]);
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


TEST_GROUP(HFPClient){

    void setup(void){
        service_level_connection_established = 0;
        codecs_connection_established = 0;
        audio_connection_established = 0;
        start_ringing = 0;
        stop_ringing = 0;
        call_termiated = 0;

        hfp_hf_init(rfcomm_channel_nr, supported_features_with_codec_negotiation, indicators, sizeof(indicators)/sizeof(uint16_t), 1);
        hfp_hf_set_codecs(codecs, sizeof(codecs));
    }

    void teardown(void){
        hfp_hf_release_audio_connection(device_addr);
        hfp_hf_release_service_level_connection(device_addr);
        
        service_level_connection_established = 0;
        codecs_connection_established = 0;
        audio_connection_established = 0;
    }
};


TEST(HFPClient, PTSRHHTests){
    for (int i = 0; i < hfp_pts_hf_rhh_tests_size(); i++){
        setup();
        simulate_test_sequence(&hfp_pts_hf_rhh_tests()[i]);
        teardown();
    }
}

TEST(HFPClient, PTSECCTests){
    for (int i = 0; i < hfp_pts_hf_ecc_tests_size(); i++){
        setup();
        simulate_test_sequence(&hfp_pts_hf_ecc_tests()[i]);
        teardown();
    }
}

TEST(HFPClient, PTSECSTests){
    for (int i = 0; i < hfp_pts_hf_ecs_tests_size(); i++){
        setup();
        simulate_test_sequence(&hfp_pts_hf_ecs_tests()[i]);
        teardown();
    }
}

TEST(HFPClient, PTSTWCTests){
    for (int i = 0; i < hfp_pts_hf_twc_tests_size(); i++){
        setup();
        simulate_test_sequence(&hfp_pts_hf_twc_tests()[i]);
        teardown();
    }
}

TEST(HFPClient, PTSATATests){
    for (int i = 0; i < hfp_pts_hf_ata_tests_size(); i++){
        setup();
        simulate_test_sequence(&hfp_pts_hf_ata_tests()[i]);
        teardown();
    }
}

TEST(HFPClient, PTSSLCTests){
    for (int i = 0; i < hfp_pts_hf_slc_tests_size(); i++){
        setup();
        simulate_test_sequence(&hfp_pts_hf_slc_tests()[i]);
        teardown();
    }
}

int main (int argc, const char * argv[]){
    hfp_hf_register_packet_handler(packet_handler);
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
