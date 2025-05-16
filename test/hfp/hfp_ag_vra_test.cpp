/*
 * Copyright (C) 2025 BlueKitchen GmbH
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

#include "btstack_config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "hci_cmd.h"
#include "btstack_run_loop.h"
#include "classic/sdp_util.h"

#include "hci.h"
#include "l2cap.h"
#include "classic/rfcomm.h"
#include "classic/sdp_server.h"
#include "btstack_debug.h"
#include "classic/hfp.h"
#include "classic/hfp_ag.h"
#include "classic/hfp_gsm_model.h"

#include "mock.h"
#include "test_sequences.h"
#include "btstack.h"

const uint8_t    rfcomm_channel_nr = 1;

static bd_addr_t device_addr = {0xD8,0xBb,0x2C,0xDf,0xF1,0x08};

static uint8_t codecs[2] = {1,2};
static hfp_ag_indicator_t indicators[] = {
    // index, name, min range, max range, status, mandatory, enabled, status changed
    {1, "service",   0, 1, 1, 0, 0, 0},
    {2, "call",      0, 1, 0, 1, 1, 0},
    {3, "callsetup", 0, 3, 0, 1, 1, 0},
    {4, "battchg",   0, 5, 3, 0, 0, 0},
    {5, "signal",    0, 5, 5, 0, 1, 0},
    {6, "roam",      0, 1, 0, 0, 1, 0},
    {7, "callheld",  0, 2, 0, 1, 1, 0}
};


static uint8_t service_level_connection_established = 0;
static uint8_t audio_connection_established = 0;
static uint8_t start_ringing = 0;
static uint8_t stop_ringing = 0;
static uint8_t call_termiated = 0;

static uint8_t last_received_event = 0;
static uint8_t last_received_event_status = 0;

// static int supported_features_with_codec_negotiation = 438;
static int supported_features_with_codec_negotiation =
        (1<<HFP_AGSF_ESCO_S4)                     |
        (1<<HFP_AGSF_HF_INDICATORS)               |
        (1<<HFP_AGSF_CODEC_NEGOTIATION)           |
        (1<<HFP_AGSF_EXTENDED_ERROR_RESULT_CODES) |
        (1<<HFP_AGSF_ENHANCED_CALL_CONTROL)       |
        (1<<HFP_AGSF_ENHANCED_CALL_STATUS)        |
        (1<<HFP_AGSF_ABILITY_TO_REJECT_A_CALL)    |
        (1<<HFP_AGSF_IN_BAND_RING_TONE)           |
        (1<<HFP_AGSF_VOICE_RECOGNITION_FUNCTION)  |
        (1<<HFP_AGSF_ENHANCED_VOICE_RECOGNITION_STATUS) |
        (1<<HFP_AGSF_VOICE_RECOGNITION_TEXT) |
        (1<<HFP_AGSF_EC_NR_FUNCTION) |
        (1<<HFP_AGSF_THREE_WAY_CALLING);


static uint16_t acl_handle = -1;

static char * get_next_hfp_ag_command(void){
    return get_next_hfp_command(2,0);
}

static int has_more_hfp_ag_commands(void){
    return has_more_hfp_commands(2,0);
}

static void print_hf_ag_commands(void){
    while (has_more_hfp_ag_commands()){
        printf("HFP AG Command: %s\n", get_next_hfp_ag_command());
    }
}


static bool check_equal_ag_commands(const char * cmd, uint16_t value){
    char * actual_command = get_next_hfp_ag_command();
    char buffer[40];
    uint16_t len = btstack_snprintf_assert_complete(buffer, sizeof(buffer), "+BVRA: %d", value);
    if (len == 0){
        return false;
    }
    if (actual_command == NULL){
        return false;
    }
    bool status_succeeded = memcmp(actual_command, buffer, len) == 0u;
    if (!status_succeeded){
        printf("ERROR: actual HFP AG Command: \'%s\', expected \'%s\' \n", actual_command, buffer);
    }
    return status_succeeded;
}

static bool check_equal_ag_report(hfp_voice_recognition_state_t state){
    char * actual_command = get_next_hfp_ag_command();
    char buffer[40];
    uint16_t len = btstack_snprintf_assert_complete(buffer, sizeof(buffer), "%s: 1,%d", HFP_VOICE_RECOGNITION_STATUS, state);
    if (len == 0){
        return false;
    }
    if (actual_command == NULL){
        return false;
    }
    bool status_succeeded = memcmp(actual_command, buffer, len) == 0u;
    if (!status_succeeded){
        printf("ERROR: actual HFP AG Command: \'%s\', expected \'%s\' \n", actual_command, buffer);
    }
    return status_succeeded;
}

static bool check_equal_cmd_ok(void){
    char * actual_command = get_next_hfp_ag_command();
    char buffer[40];
    uint16_t len = btstack_snprintf_assert_complete(buffer, sizeof(buffer), "OK");
    if (len == 0){
        return false;
    }
    if (actual_command == NULL){
        return false;
    }
    bool status_succeeded = memcmp(actual_command, buffer, len) == 0u;
    if (!status_succeeded){
        printf("ERROR: actual HFP AG Command: \'%s\', expected \'%s\' \n", actual_command, buffer);
    }
    return status_succeeded;
}

static bool check_equal_cmd_error(void){
    char * actual_command = get_next_hfp_ag_command();
    char buffer[40];
    uint16_t len = btstack_snprintf_assert_complete(buffer, sizeof(buffer), "OK");
    if (len == 0){
        return false;
    }
    if (actual_command == NULL){
        return false;
    }
    bool status_succeeded = memcmp(actual_command, buffer, len) == 0u;
    if (!status_succeeded){
        printf("ERROR: actual HFP AG Command: \'%s\', expected \'%s\' \n", actual_command, buffer);
    }
    return status_succeeded;
}

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t * event, uint16_t event_size){
    if (event[0] != HCI_EVENT_HFP_META) return;
    last_received_event = event[2];

    switch (event[2]) {   
        case HFP_SUBEVENT_VOICE_RECOGNITION_DEACTIVATED:
            last_received_event_status = hfp_subevent_voice_recognition_activated_get_status(event);
            break;
        case HFP_SUBEVENT_VOICE_RECOGNITION_ACTIVATED:
            last_received_event_status = hfp_subevent_voice_recognition_activated_get_status(event);
            break;
        case HFP_SUBEVENT_ENHANCED_VOICE_RECOGNITION_ACTIVATED:
            last_received_event_status = hfp_subevent_enhanced_voice_recognition_activated_get_status(event);
            break;
        case HFP_SUBEVENT_ENHANCED_VOICE_RECOGNITION_AG_STATE:
            last_received_event_status = hfp_subevent_enhanced_voice_recognition_ag_state_get_status(event);
            break;
        case HFP_SUBEVENT_ENHANCED_VOICE_RECOGNITION_AG_MESSAGE_SENT:
            last_received_event_status = hfp_subevent_enhanced_voice_recognition_ag_message_sent_get_status(event);
            break;

        default:
            printf("event not handled %u\n", event[2]);
            break;
    }
}

TEST_GROUP(HFP_AG_VRA){

    hfp_connection_t * hfp_connection;

    void setup(void){
        last_received_event = 0;
        last_received_event_status = 0;

        service_level_connection_established = 0;
        audio_connection_established = 0;
        start_ringing = 0;
        stop_ringing = 0;
        call_termiated = 0;

        btstack_memory_init();
        hfp_ag_init(rfcomm_channel_nr);
        hfp_ag_init_supported_features(supported_features_with_codec_negotiation);
        hfp_ag_init_ag_indicators(7, indicators);
        hfp_ag_init_codecs(sizeof(codecs), codecs);
        hfp_ag_register_packet_handler(packet_handler);

        static bd_addr_t bd_addr = { 1,2,3,4,5,6 };
        hfp_connection = test_hfp_create_connection(bd_addr, HFP_ROLE_AG);
        btstack_assert(hfp_connection != NULL);
        hfp_connection->state = HFP_SERVICE_LEVEL_CONNECTION_ESTABLISHED;
        hfp_connection->acl_handle = 0x1234;
        hfp_connection->remote_supported_features |= (1<<HFP_AGSF_ENHANCED_VOICE_RECOGNITION_STATUS);
        hfp_connection->ok_pending = 0u;
        hfp_connection->enhanced_voice_recognition_enabled = true;
    }

    void test_sco_setup(void){
        hfp_connection->state = HFP_AUDIO_CONNECTION_ESTABLISHED;
        test_hfp_ag_vra_state_machine(hfp_connection, HFP_AG_VRA_EVENT_SCO_CONNECTED);
        CHECK_EQUAL(HFP_VRA_OFF, hfp_connection->vra_engine_ag_current_state);
        CHECK_EQUAL(false, hfp_connection->emit_vra_enabled_after_audio_established);
        CHECK_EQUAL(0, last_received_event);
    }

    void test_ag_activate(void){
        test_hfp_ag_vra_state_machine(hfp_connection, HFP_AG_VRA_EVENT_AG_ACTIVATE);
        CHECK_EQUAL(HFP_VRA_W4_ACTIVE, hfp_connection->vra_engine_ag_current_state);
        CHECK_EQUAL(false, hfp_connection->emit_vra_enabled_after_audio_established);
        test_hfp_ag_vra_state_machine(hfp_connection, HFP_AG_VRA_EVENT_CAN_SEND_NOW);
        CHECK_EQUAL(HFP_VRA_ACTIVE, hfp_connection->vra_engine_ag_current_state);
        if (hfp_connection->state == HFP_AUDIO_CONNECTION_ESTABLISHED){
            CHECK_EQUAL(false, hfp_connection->emit_vra_enabled_after_audio_established);
            check_equal_ag_commands(HFP_VOICE_RECOGNITION_STATUS, 1);
            CHECK_EQUAL(HFP_SUBEVENT_VOICE_RECOGNITION_ACTIVATED, last_received_event);
            last_received_event = 0;
        } else {
            CHECK_EQUAL(true, hfp_connection->emit_vra_enabled_after_audio_established);
            check_equal_ag_commands(HFP_VOICE_RECOGNITION_STATUS, 1);
            CHECK_EQUAL(0, last_received_event);
        }
    }
    
    void test_ag_deactivate(void){
        test_hfp_ag_vra_state_machine(hfp_connection, HFP_AG_VRA_EVENT_AG_DEACTIVATE);
        CHECK_EQUAL(HFP_VRA_W4_OFF, hfp_connection->vra_engine_ag_current_state);
        test_hfp_ag_vra_state_machine(hfp_connection, HFP_AG_VRA_EVENT_CAN_SEND_NOW);
        CHECK_EQUAL(HFP_VRA_OFF, hfp_connection->vra_engine_ag_current_state);
        check_equal_ag_commands(HFP_VOICE_RECOGNITION_STATUS, 0);
        CHECK_EQUAL(1, hfp_connection->release_audio_connection);
        test_hfp_ag_vra_state_machine(hfp_connection, HFP_AG_VRA_EVENT_SCO_DISCONNECTED);
        CHECK_EQUAL(HFP_SUBEVENT_VOICE_RECOGNITION_DEACTIVATED, last_received_event);
        last_received_event = 0;
    }

    void test_ag_starting_sound_report(void){
        hfp_connection->ag_vra_state = HFP_VOICE_RECOGNITION_STATE_AG_IS_STARTING_SOUND;
        test_hfp_ag_vra_state_machine(hfp_connection, HFP_AG_VRA_EVENT_AG_STATE);
        CHECK_EQUAL(HFP_VRA_W4_ENHANCED_ACTIVE_REPORT, hfp_connection->vra_engine_ag_current_state);
        test_hfp_ag_vra_state_machine(hfp_connection, HFP_AG_VRA_EVENT_CAN_SEND_NOW);
        check_equal_ag_report(hfp_connection->ag_vra_state);
        CHECK_EQUAL(HFP_VRA_ENHANCED_ACTIVE, hfp_connection->vra_engine_ag_current_state);
        CHECK_EQUAL(HFP_SUBEVENT_ENHANCED_VOICE_RECOGNITION_AG_MESSAGE_SENT, last_received_event);
        last_received_event = 0;
    }

    void test_ag_ready_for_audio_report(void){
        hfp_connection->ag_vra_state = HFP_VOICE_RECOGNITION_STATE_AG_READY_TO_ACCEPT_AUDIO_INPUT;
        test_hfp_ag_vra_state_machine(hfp_connection, HFP_AG_VRA_EVENT_AG_STATE);
        CHECK_EQUAL(HFP_VRA_W4_ENHANCED_ACTIVE_REPORT, hfp_connection->vra_engine_ag_current_state);
        test_hfp_ag_vra_state_machine(hfp_connection, HFP_AG_VRA_EVENT_CAN_SEND_NOW);
        check_equal_ag_report(hfp_connection->ag_vra_state);
        CHECK_EQUAL(HFP_VRA_ENHANCED_ACTIVE, hfp_connection->vra_engine_ag_current_state);
        CHECK_EQUAL(HFP_SUBEVENT_ENHANCED_VOICE_RECOGNITION_AG_MESSAGE_SENT, last_received_event);
        last_received_event = 0;
    }

    void test_ag_processing_audio_report(void){
        hfp_connection->ag_vra_state = HFP_VOICE_RECOGNITION_STATE_AG_IS_PROCESSING_AUDIO_INPUT;
        test_hfp_ag_vra_state_machine(hfp_connection, HFP_AG_VRA_EVENT_AG_STATE);
        CHECK_EQUAL(HFP_VRA_W4_ENHANCED_ACTIVE_REPORT, hfp_connection->vra_engine_ag_current_state);
        test_hfp_ag_vra_state_machine(hfp_connection, HFP_AG_VRA_EVENT_CAN_SEND_NOW);
        check_equal_ag_report(hfp_connection->ag_vra_state);
        CHECK_EQUAL(HFP_VRA_ENHANCED_ACTIVE, hfp_connection->vra_engine_ag_current_state);
        CHECK_EQUAL(HFP_SUBEVENT_ENHANCED_VOICE_RECOGNITION_AG_MESSAGE_SENT, last_received_event);
        last_received_event = 0;
    }

    void test_hf_activate_enhanced(void){
        test_hfp_ag_vra_state_machine(hfp_connection, HFP_AG_VRA_EVENT_HF_ACTIVATE_ENHANCED);
        CHECK_EQUAL(HFP_VRA_W4_ENHANCED_ACTIVE, hfp_connection->vra_engine_ag_current_state);
        test_hfp_ag_vra_state_machine(hfp_connection, HFP_AG_VRA_EVENT_CAN_SEND_NOW);
        CHECK_EQUAL(HFP_VRA_ENHANCED_ACTIVE, hfp_connection->vra_engine_ag_current_state);
        check_equal_cmd_ok();
        CHECK_EQUAL(HFP_SUBEVENT_ENHANCED_VOICE_RECOGNITION_ACTIVATED, last_received_event);
        last_received_event = 0;
    }

    void teardown(void){
        while (has_more_hfp_ag_commands()){
            get_next_hfp_ag_command();
        }
        service_level_connection_established = 0;
        audio_connection_established = 0;
        hfp_ag_deinit();
        btstack_memory_deinit();
    }
};

TEST(HFP_AG_VRA, SLC_established_HFP_VRA_OFF_test_agActivate){
    test_ag_activate();
}

TEST(HFP_AG_VRA, SCO_established_HFP_VRA_OFF_test_agActivate){
    test_sco_setup();
    test_ag_activate();
}

TEST(HFP_AG_VRA, SCO_established_HFP_VRA_OFF_test_agActivate_agDeactivate_sequence){
    test_sco_setup();
    test_ag_activate();
    test_ag_deactivate();
}

TEST(HFP_AG_VRA, SCO_established_HFP_VRA_OFF_test_agActivate_hfEnhanced_agDeactivate_sequence){
    test_sco_setup();
    test_ag_activate();
    test_hf_activate_enhanced();
    test_ag_deactivate();
}

TEST(HFP_AG_VRA, test_agActivate_hfEnhancedMsgs_agDeactivate_sequence){
    test_sco_setup();
    test_ag_activate();
    test_hf_activate_enhanced();
    test_ag_starting_sound_report();
    test_ag_ready_for_audio_report();
    test_hf_activate_enhanced();
    test_ag_deactivate();
    test_ag_activate();
    test_ag_deactivate();
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
