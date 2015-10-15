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

#include "hci_cmds.h"
#include "run_loop.h"
#include "sdp_util.h"

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
static uint8_t service_level_connection_released = 0;

int expected_rfcomm_command(const char * cmd){
    char * ag_cmd = (char *)get_rfcomm_payload();
    int offset = 2;
    int cmd_size = strlen(cmd);

    int cmd_found = memcmp(ag_cmd+offset, cmd, cmd_size) == 0;
    while (!cmd_found && get_rfcomm_payload_len() - cmd_size >= offset){
        offset++;
        cmd_found = strncmp(ag_cmd+offset, cmd, cmd_size) == 0;
    }
    if (!cmd_found) return 0;
    
    // AG cmds that are not followed by OK
    if (memcmp(ag_cmd+offset, "+BCS", 4) == 0){
        return cmd_found;
    }

    offset += strlen(cmd)+4;
    // printf("cmd found, offset %d, cmd %s\n", offset, ag_cmd+offset);
    int ok_found = memcmp(ag_cmd+offset, "OK", 2) == 0;
    while (!ok_found && get_rfcomm_payload_len() - 2 >= offset){
        offset++;
        // printf("cmd found, offset %d, cmd %s\n", offset, ag_cmd+offset);
        ok_found = memcmp(ag_cmd+offset, "OK", 2) == 0;
    }
    // printf("cmd found, ok found %d\n", ok_found);
    return cmd_found && ok_found;
}

void hfp_hf_run_test_sequence(char ** test_steps, int nr_test_steps){
    int i = 0;
    for (i=0; i < nr_test_steps; i++){
        char * cmd = test_steps[i];
        // printf("---> next step %s\n", cmd);
        if (memcmp(cmd, "AT", 2) == 0){
            int parsed_codecs[2];
            uint8_t new_codecs[2];
            if (memcmp(cmd, "AT+BAC=", 7) == 0){
                sscanf(&cmd[7],"%d,%d", &parsed_codecs[0], &parsed_codecs[1]);
                new_codecs[0] = parsed_codecs[0];
                new_codecs[1] = parsed_codecs[1];
                hfp_hf_set_codecs((uint8_t*)new_codecs, 2);
            } else {
                int expected_cmd = expected_rfcomm_command(cmd);
                if (expected_cmd){
                    printf("\nError: Expected:'%s', but got:'%s'", cmd, (char *)get_rfcomm_payload());
                    return;
                }
            }
        } else {
            inject_rfcomm_command((uint8_t*)cmd, strlen(cmd));
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
            service_level_connection_established = 1;
            codecs_connection_established = 0;
            audio_connection_established = 0;
            break;
        case HFP_SUBEVENT_CODECS_CONNECTION_COMPLETE:
            codecs_connection_established = 1;
            audio_connection_established = 0;
            break;
        case HFP_SUBEVENT_AUDIO_CONNECTION_COMPLETE:
            audio_connection_established = 1;
            break;
        case HFP_SUBEVENT_SERVICE_LEVEL_CONNECTION_RELEASED:
            service_level_connection_released = 1;
            break;

        case HFP_SUBEVENT_COMPLETE:
            printf("HFP_SUBEVENT_COMPLETE.\n\n");
            break;
        case HFP_SUBEVENT_AG_INDICATOR_STATUS_CHANGED:
            printf("AG_INDICATOR_STATUS_CHANGED, AG indicator index: %d, status: %d\n", event[4], event[5]);
            break;
        case HFP_SUBEVENT_NETWORK_OPERATOR_CHANGED:
            printf("NETWORK_OPERATOR_CHANGED, operator mode: %d, format: %d, name: %s\n", event[4], event[5], (char *) &event[6]);
            break;
        case HFP_SUBEVENT_EXTENDED_AUDIO_GATEWAY_ERROR:
            if (event[4])
            printf("EXTENDED_AUDIO_GATEWAY_ERROR_REPORT, status : %d\n", event[3]);
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
        service_level_connection_released = 0;
    }

    void teardown(void){
        if (service_level_connection_established){
            hfp_hf_release_service_level_connection(device_addr);
            CHECK_EQUAL(service_level_connection_released, 1);
        }
    }

    void verify_hfp_service_level_connection_established(char ** test_steps, int nr_test_steps){
        service_level_connection_established = 0;
        hfp_hf_establish_service_level_connection(device_addr);
        hfp_hf_run_test_sequence((char **) test_steps, nr_test_steps);
        CHECK_EQUAL(service_level_connection_established, 1);
        hfp_hf_set_codecs(codecs, 1);
        inject_rfcomm_command((uint8_t*)HFP_OK, strlen(HFP_OK));
    }

    void verify_hfp_codecs_connection_established(char ** test_steps, int nr_test_steps){
        codecs_connection_established = 0;
        hfp_hf_negotiate_codecs(device_addr);
        hfp_hf_run_test_sequence((char **) test_steps, nr_test_steps);
        CHECK_EQUAL(codecs_connection_established, 1);
    }

};


TEST(HFPClient, HFCodecsConnectionEstablished){
    verify_hfp_service_level_connection_established(default_slc_setup(), default_slc_setup_size());
    for (int i = 0; i < cc_tests_size(); i++){
        verify_hfp_codecs_connection_established(hfp_cc_tests()[i].test, hfp_cc_tests()[i].len);
    }
}

TEST(HFPClient, HFServiceLevelConnectionCommands){
    verify_hfp_service_level_connection_established(default_slc_setup(), default_slc_setup_size());
    for (int i = 0; i < slc_cmds_tests_size(); i++){
        hfp_hf_run_test_sequence(hfp_slc_cmds_tests()[i].test, hfp_slc_cmds_tests()[i].len);
    }
    CHECK_EQUAL(service_level_connection_established, 1);
}

TEST(HFPClient, HFServiceLevelConnectionEstablished){
    for (int i = 0; i < slc_tests_size(); i++){
        verify_hfp_service_level_connection_established(hfp_slc_tests()[i].test, hfp_slc_tests()[i].len);
    }
}


int main (int argc, const char * argv[]){
    hfp_hf_init(rfcomm_channel_nr, 438, indicators, sizeof(indicators)/sizeof(uint16_t), 1);
    hfp_hf_set_codecs(codecs, sizeof(codecs));
    hfp_hf_register_packet_handler(packet_handler);

    return CommandLineTestRunner::RunAllTests(argc, argv);
}
