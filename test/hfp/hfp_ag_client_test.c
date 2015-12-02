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

static int supported_features_with_codec_negotiation = 4079;   // 0011 1110 1111
static int supported_features_without_codec_negotiation = 495; // 0001 1110 1111

static int call_hold_services_nr = 5;
static const char* call_hold_services[] = {"1", "1x", "2", "2x", "3"};

static int hf_indicators_nr = 2;
static hfp_generic_status_indicator_t hf_indicators[] = {
    {1, 1},
    {2, 1},
};


static uint8_t service_level_connection_established = 0;
static uint8_t codecs_connection_established = 0;
static uint8_t audio_connection_established = 0;
static uint8_t start_ringing = 0;
static uint8_t stop_ringing = 0;
static uint8_t call_termiated = 0;

int expected_rfcomm_command(const char * expected_cmd){
    char * ag_cmd = (char *)get_rfcomm_payload();
    int ag_len = get_rfcomm_payload_len();
    int expected_len = strlen(expected_cmd);
    for (int i = 0; i < ag_len; i++){
        if ( (ag_cmd+i)[0] == '\r' || (ag_cmd+i)[0] == '\n' ) {
            continue;
        }
        if (strncmp(ag_cmd + i, expected_cmd, expected_len) == 0) return 1;
    }
    return 0;
}


void simulate_test_sequence(hfp_test_item_t * test_item){
    char ** test_steps = test_item->test;
    printf("\nSimulate test sequence: \"%s\"\n", test_item->name);
    
    int i = 0;
    for (i=0; i < test_item->len; i++){
        char * cmd = test_steps[i];
        printf("\n---> NEXT STEP %s\n", cmd);
        if (strncmp(cmd, "AT", 2) == 0){
            inject_rfcomm_command_to_ag((uint8_t*)cmd, strlen(cmd));
        } else {
            int expected_cmd = expected_rfcomm_command(cmd);
            if (!expected_cmd){
                printf("\nError: Expected:'%s', but got:'%s'\n", cmd, (char *)get_rfcomm_payload());
                CHECK_EQUAL(expected_cmd,1);
                return;
            } 
            printf("AG response verified %s\n\n", cmd);
            inject_rfcomm_command_to_ag((uint8_t*)"NOP",3); 
        }
    }
       
}

void packet_handler(uint8_t * event, uint16_t event_size){
    if (event[0] != HCI_EVENT_HFP_META) return;
    if (event[3] && event[2] != HFP_SUBEVENT_EXTENDED_AUDIO_GATEWAY_ERROR){
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

        hfp_ag_init(rfcomm_channel_nr, supported_features_with_codec_negotiation, 
            codecs, sizeof(codecs), 
            ag_indicators, ag_indicators_nr, 
            hf_indicators, hf_indicators_nr, 
            call_hold_services, call_hold_services_nr);
    }

    void teardown(void){
        hfp_ag_release_audio_connection(device_addr);
        hfp_ag_release_service_level_connection(device_addr);
        
        service_level_connection_established = 0;
        codecs_connection_established = 0;
        audio_connection_established = 0;
    }

    void setup_hfp_service_level_connection(hfp_test_item_t * test_item){
        service_level_connection_established = 0;
        hfp_ag_establish_service_level_connection(device_addr);
        simulate_test_sequence(test_item);
    }

    void setup_hfp_codecs_connection(hfp_test_item_t * test_item){
        codecs_connection_established = 0;
        simulate_test_sequence(test_item);
    }
};

TEST(HFPClient, PTSSLCTests){
    for (int i = 0; i < hfp_pts_slc_tests_size(); i++){
        setup_hfp_service_level_connection(&hfp_pts_slc_tests()[i]);
        teardown();
    }
}


TEST(HFPClient, HFAudioConnectionEstablishedWithCodecNegotiation){
    setup_hfp_service_level_connection(default_hfp_slc_test());
    CHECK_EQUAL(service_level_connection_established, 1);
        
    setup_hfp_codecs_connection(default_hfp_cc_test());
    CHECK_EQUAL(codecs_connection_established, 1);

    hfp_ag_establish_audio_connection(device_addr);
    CHECK_EQUAL(audio_connection_established, 1);

    hfp_ag_release_audio_connection(device_addr);
    CHECK_EQUAL(audio_connection_established, 0);
}

TEST(HFPClient, HFAudioConnectionEstablishedWithoutCodecNegotiation){
    hfp_ag_init(rfcomm_channel_nr, supported_features_without_codec_negotiation, 
        codecs, sizeof(codecs), 
        ag_indicators, ag_indicators_nr, 
        hf_indicators, hf_indicators_nr, 
        call_hold_services, call_hold_services_nr);

    setup_hfp_service_level_connection(&hfp_slc_tests()[1]);
    CHECK_EQUAL(service_level_connection_established, 1);
        
    setup_hfp_codecs_connection(default_hfp_cc_test());
    CHECK_EQUAL(codecs_connection_established, 1);

    hfp_ag_establish_audio_connection(device_addr);
    CHECK_EQUAL(audio_connection_established, 1);

    hfp_ag_release_audio_connection(device_addr);
    CHECK_EQUAL(audio_connection_established, 0);
}

TEST(HFPClient, HFCodecsConnectionEstablished){
    for (int i = 0; i < hfp_cc_tests_size(); i++){
        setup_hfp_service_level_connection(default_hfp_slc_test());
        CHECK_EQUAL(service_level_connection_established, 1);
        
        setup_hfp_codecs_connection(&hfp_cc_tests()[i]);
        CHECK_EQUAL(codecs_connection_established, 1);
        teardown();
    }
}

TEST(HFPClient, HFServiceLevelConnectionCommands){
    setup_hfp_service_level_connection(default_hfp_slc_test());
    CHECK_EQUAL(service_level_connection_established, 1);
    for (int i = 0; i < hfp_slc_cmds_tests_size(); i++){
        simulate_test_sequence(&hfp_slc_cmds_tests()[i]);
    }
}

TEST(HFPClient, HFServiceLevelConnectionEstablishedWithoutCodecNegotiation){
    hfp_ag_init(rfcomm_channel_nr, supported_features_without_codec_negotiation, 
        codecs, sizeof(codecs), 
        ag_indicators, ag_indicators_nr, 
        hf_indicators, hf_indicators_nr, 
        call_hold_services, call_hold_services_nr);
    setup_hfp_service_level_connection(&hfp_slc_tests()[1]);
    CHECK_EQUAL(service_level_connection_established, 1);
}

TEST(HFPClient, HFServiceLevelConnectionEstablishedWithCodecNegotiation){
    setup_hfp_service_level_connection(default_hfp_slc_test());
    CHECK_EQUAL(service_level_connection_established, 1);
    teardown();
}

int main (int argc, const char * argv[]){
    hfp_ag_register_packet_handler(packet_handler);

    return CommandLineTestRunner::RunAllTests(argc, argv);
}
