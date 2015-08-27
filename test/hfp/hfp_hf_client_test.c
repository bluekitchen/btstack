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
// Minimal test for HSP Headset (!! UNDER DEVELOPMENT !!)
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

#define TEST_SEQUENCE(name) { (char**)name, sizeof(name) / sizeof(char *)}

typedef struct hfp_test_item{
    char ** test;
    int len;
} hfp_test_item_t;


const uint8_t    rfcomm_channel_nr = 1;

static bd_addr_t device_addr = {0xD8,0xBb,0x2C,0xDf,0xF1,0x08};

static uint8_t codecs[2] = {1,2};
static uint16_t indicators[1] = {0x01};

static uint8_t service_level_connection_established = 0;
static uint8_t codecs_connection_established = 0;
static uint8_t audio_connection_established = 0;
static uint8_t service_level_connection_released = 0;

// prototypes
uint8_t * get_rfcomm_payload();
uint16_t  get_rfcomm_payload_len();
void inject_rfcomm_command(uint8_t * payload, int len);

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

static int expected_rfcomm_command(const char * cmd){
    if (memcmp(cmd, "AT", 2) == 0){
        return memcmp((char *)cmd, (char *)get_rfcomm_payload(), get_rfcomm_payload_len()-2);
    }
    if (cmd[0] == '+'){
        return memcmp((char *)cmd, (char *)get_rfcomm_payload()+2, get_rfcomm_payload_len()-2);
    }
    if (memcmp(cmd, "OK", 2) == 0) return 0;
    if (memcmp(cmd, "ERROR", 5) == 0) return 0;
    return 1;
}
    
static void verify_expected_rfcomm_command(const char * cmd){
    if (expected_rfcomm_command(cmd)){
        printf("\nError: Expected:'%s', but got:'%s'", cmd, (char *)get_rfcomm_payload());
    }
    CHECK_EQUAL(expected_rfcomm_command(cmd),0);
}

/* Service Level Connection (slc) test sequences */

const char * hf_slc_test1[] = {
    "AT+BRSF=438",
    "+BRSF:1007", 
    HFP_OK,
    "AT+BAC=1,2", 
    HFP_OK,
    "AT+CIND=?",
    "+CIND:\"service\",(0,1),\"call\",(0,1),\"callsetup\",(0,3),\"battchg\",(0,5),\"signal\",(0,5),\"roam\",(0,1),\"callheld\",(0,2)",
    HFP_OK,
    "AT+CIND?",
    "+CIND:1,0,0,3,5,0,0",
    HFP_OK,
    "AT+CMER=3,0,0,1",
    HFP_OK,
    "AT+CHLD=?",
    "+CHLD:(1,1x,2,2x,3)",
    HFP_OK
};

hfp_test_item_t hfp_slc_tests[] = {
    TEST_SEQUENCE(hf_slc_test1)
};

/* Service Level Connection (slc) common commands */
const char * hf_slc_cmds_test1[] = {
    "AT+BAC=1,3", 
    HFP_OK
};

/* Codecs Connection (cc) test sequences */

const char * hf_cc_test1[] = {
    "AT+BCC", 
    HFP_OK,
    "+BCS:1",
    "AT+BCS=1",
    HFP_OK
};

const char * hf_cc_test2[] = {
    "AT+BCC", 
    HFP_OK,
    "+BCS:1",
    "AT+BAC=2,3", 
    HFP_OK,
    "+BCS:2",
    "AT+BCS=2",
    HFP_OK
};

hfp_test_item_t hfp_cc_tests[] = {
    TEST_SEQUENCE(hf_cc_test1),
    TEST_SEQUENCE(hf_cc_test2)  
};


TEST_GROUP(HandsfreeClient){
    int test_item_size;
    int cc_tests_size;
    int slc_tests_size;

    char ** default_slc_setup;
    int default_slc_setup_size;

    char ** default_cc_setup;
    int default_cc_setup_size;
    
    void setup(void){
        service_level_connection_established = 0;
        codecs_connection_established = 0;
        audio_connection_established = 0;
        service_level_connection_released = 0;
        
        test_item_size = sizeof(hfp_test_item_t);
        slc_tests_size = sizeof(hfp_slc_tests)/test_item_size;
        cc_tests_size  = sizeof(hfp_cc_tests) /test_item_size;

        default_slc_setup = (char **)hf_slc_test1;
        default_slc_setup_size = sizeof(hf_slc_test1)/sizeof(char*);

        default_cc_setup =  (char **)hf_cc_test1;
        default_cc_setup_size = sizeof(hf_cc_test1)/sizeof(char*);
    }

    void teardown(void){
        if (service_level_connection_established){
            hfp_hf_release_service_level_connection(device_addr);
            CHECK_EQUAL(service_level_connection_released, 1);
        }
    }

    void test(char ** test_steps, int nr_test_steps){
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
                    verify_expected_rfcomm_command(cmd);
                }
            } else {
                inject_rfcomm_command((uint8_t*)cmd, strlen(cmd));
            }
        }
    }

    void setup_hfp_service_level_connection(char ** test_steps, int nr_test_steps){
        service_level_connection_established = 0;
        hfp_hf_establish_service_level_connection(device_addr);
        test((char **) test_steps, nr_test_steps);
        CHECK_EQUAL(service_level_connection_established, 1);
        hfp_hf_set_codecs(codecs, 1);
        inject_rfcomm_command((uint8_t*)HFP_OK, strlen(HFP_OK));
    }

    void setup_hfp_codecs_connection_state_machine(char ** test_steps, int nr_test_steps){
        codecs_connection_established = 0;
        hfp_hf_negotiate_codecs(device_addr);
        test((char **) test_steps, nr_test_steps);
        CHECK_EQUAL(codecs_connection_established, 1);
    }

};


// TEST(HandsfreeClient, HFCodecsConnectionEstablished){
//     setup_hfp_service_level_connection(default_slc_setup, default_slc_setup_size);
//     for (int i = 0; i < cc_tests_size; i++){
//         setup_hfp_codecs_connection_state_machine(hfp_cc_tests[i].test, hfp_cc_tests[i].len);
//     }
// }

TEST(HandsfreeClient, HFCodecChange){
    setup_hfp_service_level_connection(default_slc_setup, default_slc_setup_size);
    test((char**)hf_slc_cmds_test1, sizeof(hf_slc_cmds_test1)/sizeof(char*));
    verify_expected_rfcomm_command(HFP_OK);
    CHECK_EQUAL(service_level_connection_established, 1);
}

TEST(HandsfreeClient, HFServiceLevelConnectionEstablished){
    for (int i = 0; i < slc_tests_size; i++){
        setup_hfp_service_level_connection(hfp_slc_tests[i].test, hfp_slc_tests[i].len);
    }
}


int main (int argc, const char * argv[]){
    hfp_hf_init(rfcomm_channel_nr, 438, indicators, sizeof(indicators)/sizeof(uint16_t), 1);
    hfp_hf_set_codecs(codecs, sizeof(codecs));
    hfp_hf_register_packet_handler(packet_handler);

    return CommandLineTestRunner::RunAllTests(argc, argv);
}
