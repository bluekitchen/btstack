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
// HFG AG state machine tests
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
#include "hfp_ag.h"

#include "mock.h"
#include "test_sequences.h"

static bd_addr_t pts_addr = {0x00,0x15,0x83,0x5F,0x9D,0x46};

const uint8_t    rfcomm_channel_nr = 1;

static bd_addr_t device_addr = {0xD8,0xBb,0x2C,0xDf,0xF1,0x08};

static uint8_t codecs[2] = {1, 3};

static int ag_indicators_nr = 7;
static hfp_ag_indicator_t ag_indicators[] = {
    // index, name, min range, max range, status, mandatory, enabled, status changed
    {1, "service",   0, 1, 1, 0, 0, 0},
    {2, "call",      0, 1, 0, 1, 1, 0},
    {3, "callsetup", 0, 3, 0, 1, 1, 0},
    {4, "battchg",   0, 5, 3, 0, 0, 0},
    {5, "signal",    0, 5, 5, 0, 0, 0},
    {6, "roam",      0, 1, 0, 0, 0, 0},
    {7, "callheld",  0, 2, 0, 1, 1, 0}
};

hfp_ag_indicator_t ag_indicators_temp[] = {
    // index, name, min range, max range, status, mandatory, enabled, status changed
    {1, "service",   0, 1, 1, 0, 0, 0},
    {2, "call",      0, 1, 0, 1, 1, 0},
    {3, "callsetup", 0, 3, 0, 1, 1, 0},
    {4, "battchg",   0, 5, 3, 0, 0, 0},
    {5, "signal",    0, 5, 5, 0, 0, 0},
    {6, "roam",      0, 1, 0, 0, 0, 0},
    {7, "callheld",  0, 2, 0, 1, 1, 0}
};

static int supported_features_with_codec_negotiation = 4079;   // 0011 1110 1111

static int call_hold_services_nr = 5;
static const char* call_hold_services[] = {"1", "1x", "2", "2x", "3"};

static int hf_indicators_nr = 2;
static hfp_generic_status_indicator_t hf_indicators[] = {
    {1, 1},
    {2, 1},
};

static uint16_t handle = -1;
static int memory_1_enabled = 1;

int has_more_hfp_ag_commands(){
    return has_more_hfp_commands(2,2);
}

char * get_next_hfp_ag_command(){
   return get_next_hfp_command(2,2);
}

static void user_command(char cmd){
    switch (cmd){
        case 'a':
            memcpy(device_addr, pts_addr, 6);
            printf("Establish HFP service level connection to PTS module %s...\n", bd_addr_to_str(device_addr));
            hfp_ag_establish_service_level_connection(device_addr);
            break;
        case 'A':
            printf("Release HFP service level connection.\n");
            hfp_ag_release_service_level_connection(device_addr);
            break;
        case 'Z':
            printf("Release HFP service level connection to %s...\n", bd_addr_to_str(device_addr));
            hfp_ag_release_service_level_connection(device_addr);
            break;
        case 'b':
            printf("Establish Audio connection %s...\n", bd_addr_to_str(device_addr));
            hfp_ag_establish_audio_connection(device_addr);
            break;
        case 'B':
            printf("Release Audio connection.\n");
            hfp_ag_release_audio_connection(device_addr);
            break;
        case 'c':
            printf("Simulate incoming call from 1234567\n");
            hfp_ag_set_clip(129, "1234567");
            hfp_ag_incoming_call();
            break;
        case 'm':
            printf("Simulate incoming call from 7654321\n");
            hfp_ag_set_clip(129, "7654321");
            hfp_ag_incoming_call();
            break;
        case 'C':
            printf("Simulate terminate call\n");
            hfp_ag_call_dropped();
            break;
        case 'd':
            printf("Report AG failure\n");
            hfp_ag_report_extended_audio_gateway_error_result_code(device_addr, HFP_CME_ERROR_AG_FAILURE);
            break;
        case 'e':
            printf("Answer call on AG\n");
            hfp_ag_answer_incoming_call();
            break;
        case 'E':
            printf("Reject call on AG\n");
            hfp_ag_terminate_call();
            break;
        case 'f':
            printf("Disable cellular network\n");
            hfp_ag_set_registration_status(0);
            break;
        case 'F':
            printf("Enable cellular network\n");
            hfp_ag_set_registration_status(1);
            break;
        case 'g':
            printf("Set signal strength to 0\n");
            hfp_ag_set_signal_strength(0);
            break;
        case 'G':
            printf("Set signal strength to 5\n");
            hfp_ag_set_signal_strength(5);
            break;
        case 'h':
            printf("Disable roaming\n");
            hfp_ag_set_roaming_status(0);
            break;
        case 'H':
            printf("Enable roaming\n");
            hfp_ag_set_roaming_status(1);
            break;
        case 'i':
            printf("Set battery level to 3\n");
            hfp_ag_set_battery_level(3);
            break;
        case 'I':
            printf("Set battery level to 5\n");
            hfp_ag_set_battery_level(5);
            break;
        case 'j':
            printf("Answering call on remote side\n");
            hfp_ag_outgoing_call_established();
            break;
        case 'r':
            printf("Disable in-band ring tone\n");
            hfp_ag_set_use_in_band_ring_tone(0);
            break;
        case 'k':
            printf("Memory 1 cleared\n");
            memory_1_enabled = 0;
            break;
        case 'K':
            printf("Memory 1 set\n");
            memory_1_enabled = 1;
            break;
        case 'l':
            printf("Last dialed number cleared\n");
            hfp_ag_clear_last_dialed_number();
            break;
        case 'L':
            printf("Outgoing call connected, ringing\n");
            hfp_ag_outgoing_call_ringing();
            break;
        case 'n':
            printf("Disable Voice Recognition\n");
            hfp_ag_activate_voice_recognition(device_addr, 0);
            break;
        case 'N':
            printf("Enable Voice Recognition\n");
            hfp_ag_activate_voice_recognition(device_addr, 1);
            break;
        case 'o':
            printf("Set speaker gain to 0 (minimum)\n");
            hfp_ag_set_speaker_gain(device_addr, 0);
            break;
        case 'O':
            printf("Set speaker gain to 9 (default)\n");
            hfp_ag_set_speaker_gain(device_addr, 9);
            break;
        case 'p':
            printf("Set speaker gain to 12 (higher)\n");
            hfp_ag_set_speaker_gain(device_addr, 12);
            break;
        case 'P':
            printf("Set speaker gain to 15 (maximum)\n");
            hfp_ag_set_speaker_gain(device_addr, 15);
            break;
        case 'q':
            printf("Set microphone gain to 0\n");
            hfp_ag_set_microphone_gain(device_addr, 0);
            break;
        case 'Q':
            printf("Set microphone gain to 9\n");
            hfp_ag_set_microphone_gain(device_addr, 9);
            break;
        case 's':
            printf("Set microphone gain to 12\n");
            hfp_ag_set_microphone_gain(device_addr, 12);
            break;
        case 'S':
            printf("Set microphone gain to 15\n");
            hfp_ag_set_microphone_gain(device_addr, 15);
            break;
        case 'R':
            printf("Enable in-band ring tone\n");
            hfp_ag_set_use_in_band_ring_tone(1);
            break;
        case 't':
            printf("Terminate HCI connection.\n");
            gap_disconnect(handle);
            break;
        case 'u':
            printf("Join held call\n");
            hfp_ag_join_held_call();
            break;
        case 'v':
            printf("Starting inquiry scan..\n");
            // hci_send_cmd(&hci_inquiry, HCI_INQUIRY_LAP, INQUIRY_INTERVAL, 0);
            break;
        case 'w':
            printf("AG: Put incoming call on hold (Response and Hold)\n");
            hfp_ag_hold_incoming_call();
            break;
        case 'x':
            printf("AG: Accept held incoming call (Response and Hold)\n");
            hfp_ag_accept_held_incoming_call();
            break;
        case 'X':
            printf("AG: Reject held incoming call (Response and Hold)\n");
            hfp_ag_reject_held_incoming_call();
            break;
        default:
            break;
    }
}

static void simulate_test_sequence(hfp_test_item_t * test_item){
    char ** test_steps = test_item->test;
    printf("\nSimulate test sequence: \"%s\"\n", test_item->name);
    
    int i = 0;
    static char * previous_cmd = NULL;
    
    int previous_step = -1;
    while ( i < test_item->len){
        previous_step++;
        CHECK_EQUAL(i >= previous_step, 1);
        
        char * expected_cmd = test_steps[i];
        int expected_cmd_len = strlen(expected_cmd);
        printf("\nStep %d, %s \n", i, expected_cmd);

        if (strncmp(expected_cmd, "USER:", 5) == 0){
            printf("\n---> USER: ");
            user_command(expected_cmd[5]);
            i++;
        } else if (strncmp(expected_cmd, "AT", 2) == 0){
            // printf("\n---> NEXT STEP receive from HF: '%s'\n", expected_cmd);
            inject_hfp_command_to_ag((uint8_t*)expected_cmd, expected_cmd_len);
            i++;

        } else {
            printf("\n---> NEXT STEP expect from AG: %s\n", expected_cmd);

            while (has_more_hfp_ag_commands()){
                char * ag_cmd = get_next_hfp_ag_command();

                int equal_cmds = strncmp(ag_cmd, expected_cmd, expected_cmd_len) == 0;
                if (!equal_cmds){
                    printf("\nError: Expected:'%s', but got:'%s'\n", expected_cmd, ag_cmd);
                    CHECK_EQUAL(equal_cmds,1);
                    return;
                } 
                printf("Verified: '%s'\n", expected_cmd);
                previous_cmd = ag_cmd;

                i++;
                if (i < test_item->len){
                    expected_cmd = test_steps[i];
                    expected_cmd_len = strlen(expected_cmd);
                } 
            } 
            // printf("\n---> NEXT STEP trigger once more AG\n");
            inject_hfp_command_to_ag((uint8_t*)"NOP",3); 
        }
    }   
}

void packet_handler(uint8_t * event, uint16_t event_size){
    if (event[0] == RFCOMM_EVENT_OPEN_CHANNEL_COMPLETE){
        handle = READ_BT_16(event, 9);
        printf("RFCOMM_EVENT_OPEN_CHANNEL_COMPLETE received for handle 0x%04x\n", handle);
        return;
    }


    if (event[0] != HCI_EVENT_HFP_META) return;

    if (event[3]
        && event[2] != HFP_SUBEVENT_PLACE_CALL_WITH_NUMBER
        && event[2] != HFP_SUBEVENT_ATTACH_NUMBER_TO_VOICE_TAG 
        && event[2] != HFP_SUBEVENT_TRANSMIT_DTMF_CODES
        && event[2] != HFP_SUBEVENT_TRANSMIT_STATUS_OF_CURRENT_CALL){
        printf("ERROR, status: %u\n", event[3]);
        return;
    }

    switch (event[2]) {   
        case HFP_SUBEVENT_SERVICE_LEVEL_CONNECTION_ESTABLISHED:
            printf("Service level connection established.\n");
            break;
        case HFP_SUBEVENT_SERVICE_LEVEL_CONNECTION_RELEASED:
            printf("Service level connection released.\n");
            break;
        case HFP_SUBEVENT_AUDIO_CONNECTION_ESTABLISHED:
            printf("\n** Audio connection established **\n");
            break;
        case HFP_SUBEVENT_AUDIO_CONNECTION_RELEASED:
            printf("\n** Audio connection released **\n");
            break;
        case HFP_SUBEVENT_START_RINGINIG:
            printf("\n** Start Ringing **\n");
            break;        
        case HFP_SUBEVENT_STOP_RINGINIG:
            printf("\n** Stop Ringing **\n");
            break;
        case HFP_SUBEVENT_PLACE_CALL_WITH_NUMBER:
            printf("\n** Outgoing call '%s' **\n", &event[3]);
            // validate number
            if ( strcmp("1234567", (char*) &event[3]) == 0
              || strcmp("7654321", (char*) &event[3]) == 0
              || (memory_1_enabled && strcmp(">1",      (char*) &event[3]) == 0)){
                printf("Dialstring valid: accept call\n");
                hfp_ag_outgoing_call_accepted();
            } else {
                printf("Dialstring invalid: reject call\n");
                hfp_ag_outgoing_call_rejected();
            }
            break;
        case HFP_SUBEVENT_ATTACH_NUMBER_TO_VOICE_TAG:
            printf("\n** Attach number to voice tag. Sending '1234567\n");
            hfp_ag_send_phone_number_for_voice_tag(device_addr, "1234567");
            break;
        case HFP_SUBEVENT_TRANSMIT_DTMF_CODES:
            printf("\n** Send DTMF Codes: '%s'\n", &event[3]);
            hfp_ag_send_dtmf_code_done(device_addr);
            break;
        default:
            printf("Event not handled %u\n", event[2]);
            break;
    }

}


TEST_GROUP(HFPClient){
    void setup(void){
        hfp_ag_init(rfcomm_channel_nr, supported_features_with_codec_negotiation, 
            codecs, sizeof(codecs), 
            ag_indicators, ag_indicators_nr, 
            hf_indicators, hf_indicators_nr, 
            call_hold_services, call_hold_services_nr);
    }

    void teardown(void){
        hfp_ag_release_audio_connection(device_addr);
        hfp_ag_release_service_level_connection(device_addr);
    }
};


TEST(HFPClient, PTSRHHTests){
    for (int i = 0; i < hfp_pts_ag_rhh_tests_size(); i++){
        setup();
        simulate_test_sequence(&hfp_pts_ag_rhh_tests()[i]);
        teardown();
    }
}

TEST(HFPClient, PTSECCTests){
    for (int i = 0; i < hfp_pts_ag_ecc_tests_size(); i++){
        setup();
        simulate_test_sequence(&hfp_pts_ag_ecc_tests()[i]);
        teardown();
    }
}

TEST(HFPClient, PTSECSTests){
    for (int i = 0; i < hfp_pts_ag_ecs_tests_size(); i++){
        setup();
        simulate_test_sequence(&hfp_pts_ag_ecs_tests()[i]);
        teardown();
    }
}

TEST(HFPClient, PTSTWCTests){
    for (int i = 0; i < hfp_pts_ag_twc_tests_size(); i++){
        setup();
        simulate_test_sequence(&hfp_pts_ag_twc_tests()[i]);
        teardown();
    }
}

TEST(HFPClient, PTSATATests){
    for (int i = 0; i < hfp_pts_ag_ata_tests_size(); i++){
        setup();
        simulate_test_sequence(&hfp_pts_ag_ata_tests()[i]);
        teardown();
    }
}

TEST(HFPClient, PTSSLCTests){
    for (int i = 0; i < hfp_pts_ag_slc_tests_size(); i++){
        setup();
        simulate_test_sequence(&hfp_pts_ag_slc_tests()[i]);
        teardown();
    }
}


int main (int argc, const char * argv[]){
    hfp_ag_register_packet_handler(packet_handler);

    return CommandLineTestRunner::RunAllTests(argc, argv);
}
