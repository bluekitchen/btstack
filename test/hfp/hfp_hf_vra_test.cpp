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
// HFG HF state machine tests
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
#include "classic/hfp_hf.h"

#include "mock.h"
#include "test_sequences.h"
#include "btstack.h"

const uint8_t    rfcomm_channel_nr = 1;

static bd_addr_t device_addr = {0xD8,0xBb,0x2C,0xDf,0xF1,0x08};

static uint8_t codecs[2] = {1,2};
static uint16_t indicators[1] = {0x01};

static uint8_t service_level_connection_established = 0;
static uint8_t audio_connection_established = 0;
static uint8_t start_ringing = 0;
static uint8_t stop_ringing = 0;
static uint8_t call_termiated = 0;

static uint8_t last_received_event = 0;
static uint8_t last_received_event_status = 0;

// static int supported_features_with_codec_negotiation = 438;
static int supported_features_with_codec_negotiation =
        (1<<HFP_HFSF_ESCO_S4)               |
        (1<<HFP_HFSF_CLI_PRESENTATION_CAPABILITY) |
        (1<<HFP_HFSF_HF_INDICATORS)         |
        (1<<HFP_HFSF_CODEC_NEGOTIATION)     |
        (1<<HFP_HFSF_ENHANCED_CALL_STATUS)  |
        (1<<HFP_HFSF_VOICE_RECOGNITION_FUNCTION)  |
        (1<<HFP_HFSF_ENHANCED_VOICE_RECOGNITION_STATUS) |
        (1<<HFP_HFSF_VOICE_RECOGNITION_TEXT) |
        (1<<HFP_HFSF_EC_NR_FUNCTION) |
        (1<<HFP_HFSF_REMOTE_VOLUME_CONTROL);

static uint16_t acl_handle = -1;

static char * get_next_hfp_hf_command(void){
    return get_next_hfp_command(0,2);
}

static int has_more_hfp_hf_commands(void){
    return has_more_hfp_commands(0,2);
}

static void print_hf_commands(void){
    while (has_more_hfp_hf_commands()){
        printf("HFP HF Command: %s\n", get_next_hfp_hf_command());
    }
}


static bool check_equal_hf_commands(const char * cmd, uint16_t value){
    char * actual_command = get_next_hfp_hf_command();
    char buffer[40];
    uint16_t len = btstack_snprintf_assert_complete(buffer, sizeof(buffer), "AT%s=%d", cmd, value);
    bool status_succeeded = memcmp(actual_command, buffer, len) == 0u;
    if (!status_succeeded){
        printf("ERROR: actual HFP HF Command: \'%s\', expected \'%s\' \n", actual_command, buffer);
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
        case HFP_VOICE_RECOGNITION_STATE_AG_READY_TO_ACCEPT_AUDIO_INPUT:
//            last_received_event_status = hfp_
            break;
        case HFP_VOICE_RECOGNITION_STATE_AG_IS_STARTING_SOUND:
            event[2] = HFP_SUBEVENT_ENHANCED_VOICE_RECOGNITION_AG_IS_STARTING_SOUND;
            break;
        case HFP_VOICE_RECOGNITION_STATE_AG_IS_PROCESSING_AUDIO_INPUT:
            break;
        default:
            printf("event not handled %u\n", event[2]);
            break;
    }
}

TEST_GROUP(HFP_HF_VRA){

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
        hfp_hf_init(rfcomm_channel_nr);
        hfp_hf_init_supported_features(supported_features_with_codec_negotiation);
        hfp_hf_init_hf_indicators(sizeof(indicators)/sizeof(uint16_t), indicators);
        hfp_hf_init_codecs(sizeof(codecs), codecs);
        hfp_hf_register_packet_handler(packet_handler);

        static bd_addr_t bd_addr = { 1,2,3,4,5,6 };
        hfp_connection = test_hfp_create_connection(bd_addr, HFP_ROLE_HF);
        btstack_assert(hfp_connection != NULL);
        hfp_connection->state = HFP_SERVICE_LEVEL_CONNECTION_ESTABLISHED;
        hfp_connection->acl_handle = 0x1234;
        hfp_connection->remote_supported_features |= (1<<HFP_AGSF_ENHANCED_VOICE_RECOGNITION_STATUS);
        hfp_connection->ok_pending = 0u;
    }

    void setup_vra_w4_confirmation(hfp_vra_engine_state_t current_state){
        hfp_connection->vra_engine_current_state = current_state;
        hfp_connection->vra_engine_requested_state = HFP_VRA_NONE;
        hfp_connection->ok_pending = 1u;
    }

    void setup_vra_can_send_now(hfp_vra_engine_state_t current_state, hfp_vra_engine_state_t requested_state){
        hfp_connection->vra_engine_current_state = current_state;
        hfp_connection->vra_engine_requested_state = requested_state;
        hfp_connection->ok_pending = 0u;
    }

    void setup_vra_ag_report(hfp_vra_engine_state_t current_state, hfp_voice_recognition_status_t ag_vra_status){
        hfp_connection->vra_engine_current_state = current_state;
        switch (ag_vra_status) {
            case HFP_VOICE_RECOGNITION_STATUS_DISABLED:
                hfp_connection->vra_engine_requested_state = HFP_VRA_OFF;
                break;
            case HFP_VOICE_RECOGNITION_STATUS_ENABLED:
                hfp_connection->vra_engine_requested_state = HFP_VRA_ACTIVE;
                break;
            case HFP_VOICE_RECOGNITION_STATUS_READY_FOR_AUDIO:
                hfp_connection->vra_engine_requested_state = HFP_VRA_ENHANCED_ACTIVE;
                break;
            default:
                break;
        }
    }

    void teardown(void){
        service_level_connection_established = 0;
        audio_connection_established = 0;
        hfp_hf_deinit();
        btstack_memory_deinit();
    }
};

/*
 * AG initiated actions:
 * - ag_activate:   setup_vra_ag_report(current_state, HFP_VOICE_RECOGNITION_STATUS_ENABLED)
 * - ag_deactivate: setup_vra_ag_report(current_state, HFP_VOICE_RECOGNITION_STATUS_)
 * -
 */

TEST(HFP_HF_VRA, ag_activate_in_HFP_VRA_OFF_and_SCO_not_established){
    // SCO not established
    hfp_connection->state = HFP_SERVICE_LEVEL_CONNECTION_ESTABLISHED;

    // OFF --> ACTIVE: ag_activate
    // if (sco_established) {emit event ACTIVE} else {emit_activated_when_sco_established = 1}
    setup_vra_ag_report(HFP_VRA_OFF, HFP_VOICE_RECOGNITION_STATUS_ENABLED);
    test_hfp_hf_vra_state_machine(hfp_connection, HFP_HF_VRA_EVENT_AG_REPORT_ACTIVATED);
    CHECK_EQUAL(HFP_VRA_ACTIVE, hfp_connection->vra_engine_current_state);
    CHECK_EQUAL(true, hfp_connection->emit_vra_enabled_after_audio_established);
}

TEST(HFP_HF_VRA, ag_activate_in_HFP_VRA_OFF_and_SCO_established){
    // SCO not established
    hfp_connection->state = HFP_AUDIO_CONNECTION_ESTABLISHED;

    // OFF --> ACTIVE: ag_activate
    // if (sco_established) {emit event ACTIVE} else {emit_activated_when_sco_established = 1}
    setup_vra_ag_report(HFP_VRA_OFF, HFP_VOICE_RECOGNITION_STATUS_ENABLED);
    test_hfp_hf_vra_state_machine(hfp_connection, HFP_HF_VRA_EVENT_AG_REPORT_ACTIVATED);
    CHECK_EQUAL(HFP_VRA_ACTIVE, hfp_connection->vra_engine_current_state);
    CHECK_EQUAL(HFP_SUBEVENT_VOICE_RECOGNITION_ACTIVATED, last_received_event);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, last_received_event_status);
}

TEST(HFP_HF_VRA, ag_deactivate_in_HFP_VRA_ACTIVE) {
    // ACTIVE --> OFF : ag_deactivate  / emit event OFF
    setup_vra_ag_report(HFP_VRA_ACTIVE, HFP_VOICE_RECOGNITION_STATUS_DISABLED);
    test_hfp_hf_vra_state_machine(hfp_connection, HFP_HF_VRA_EVENT_AG_REPORT_DEACTIVATED);
    CHECK_EQUAL(HFP_VRA_OFF, hfp_connection->vra_engine_current_state);
    CHECK_EQUAL(HFP_SUBEVENT_VOICE_RECOGNITION_DEACTIVATED, last_received_event);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, last_received_event_status);
}

TEST(HFP_HF_VRA, ag_ready4audio_in_HFP_VRA_ACTIVE) {
    // ACTIVE --> READY_FOR_AUDIO : ag_ready4audio / emit event READY_FOR_AUDIO
    setup_vra_ag_report(HFP_VRA_ACTIVE, HFP_VOICE_RECOGNITION_STATUS_READY_FOR_AUDIO);
    test_hfp_hf_vra_state_machine(hfp_connection, HFP_HF_VRA_EVENT_AG_REPORT_READY_FOR_AUDIO);
    CHECK_EQUAL(HFP_VRA_ENHANCED_ACTIVE, hfp_connection->vra_engine_current_state);
    CHECK_EQUAL(HFP_SUBEVENT_VOICE_RECOGNITION_DEACTIVATED, last_received_event);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, last_received_event_status);
}

TEST(HFP_HF_VRA, sco_established_in_HFP_VRA_ACTIVE) {
    // ACTIVE --> ACTIVE : sco_established / [emit_activated_when_sco_established == 1] / emit event ACTIVE}, emit_activated_when_sco_established = 0
}

TEST(HFP_HF_VRA, ag_deactivate_in_READY_FOR_AUDIO) {
    // READY_FOR_AUDIO --> OFF: ag_deactivate  / emit event OFF
}

TEST(HFP_HF_VRA, ag_activate_in_READY_FOR_AUDIO) {
    // READY_FOR_AUDIO --> ACTIVE: ag_activate  / emit event ACTIVE
}

TEST(HFP_HF_VRA, hg_activate_with_ok_in_HFP_VRA_OFF_and_SCO_not_established){
    // SCO not established
    hfp_connection->state = HFP_SERVICE_LEVEL_CONNECTION_ESTABLISHED;

    // OFF --> W4_ACTIVE: hf_activate
    // requested_state = ACTIVE (wait for ready2send event to send activate command)
    setup_vra_can_send_now(HFP_VRA_OFF, HFP_VRA_ACTIVE);
    test_hfp_hf_vra_state_machine(hfp_connection, HFP_HF_VRA_EVENT_HF_REQUESTED_ACTIVATE);
    CHECK_EQUAL(HFP_VRA_W4_ACTIVE, hfp_connection->vra_engine_current_state);
    CHECK_EQUAL(0u, hfp_connection->ok_pending);

    // W4_ACTIVE --> W4_ACTIVE: ready2send [ok_pending == 0]
    // ok_pending = 1, send activate command, start timer
    test_hfp_hf_vra_state_machine(hfp_connection, HFP_HF_VRA_EVENT_CAN_SEND_NOW);
    check_equal_hf_commands(HFP_ACTIVATE_VOICE_RECOGNITION, 1);
    CHECK_EQUAL(HFP_VRA_W4_ACTIVE, hfp_connection->vra_engine_current_state);
    CHECK_EQUAL(1u, hfp_connection->ok_pending);

    // W4_ACTIVE --> ACTIVE: ok, assert (ok_pending == 1)
    // ok_pending = 0, if (sco_established) {emit event ACTIVE} else {emit_activated_when_sco_established = 1};
    // stop timer
    test_hfp_hf_vra_state_machine(hfp_connection, HFP_HF_VRA_EVENT_RECEIVED_OK);
    CHECK_EQUAL(HFP_VRA_ACTIVE, hfp_connection->vra_engine_current_state);
    CHECK_EQUAL(0u, hfp_connection->ok_pending);
    CHECK_EQUAL(true, hfp_connection->emit_vra_enabled_after_audio_established);
}

TEST(HFP_HF_VRA, hg_activate_with_ok_in_HFP_VRA_OFF_and_sco_established){
    // SCO established
    hfp_connection->state = HFP_AUDIO_CONNECTION_ESTABLISHED;

    // OFF --> W4_ACTIVE: hf_activate
    // requested_state = ACTIVE (wait for ready2send event to send activate command)
    setup_vra_can_send_now(HFP_VRA_OFF, HFP_VRA_ACTIVE);
    test_hfp_hf_vra_state_machine(hfp_connection, HFP_HF_VRA_EVENT_HF_REQUESTED_ACTIVATE);
    CHECK_EQUAL(HFP_VRA_W4_ACTIVE, hfp_connection->vra_engine_current_state);
    CHECK_EQUAL(0u, hfp_connection->ok_pending);

    // W4_ACTIVE --> W4_ACTIVE: ready2send [ok_pending == 0]
    // ok_pending = 1, send activate command, start timer
    test_hfp_hf_vra_state_machine(hfp_connection, HFP_HF_VRA_EVENT_CAN_SEND_NOW);
    check_equal_hf_commands(HFP_ACTIVATE_VOICE_RECOGNITION, 1);
    CHECK_EQUAL(HFP_VRA_W4_ACTIVE, hfp_connection->vra_engine_current_state);
    CHECK_EQUAL(1u, hfp_connection->ok_pending);

    // W4_ACTIVE --> ACTIVE: ok, assert (ok_pending == 1)
    // ok_pending = 0, if (sco_established) {emit event ACTIVE} else {emit_activated_when_sco_established = 1};
    // stop timer
    test_hfp_hf_vra_state_machine(hfp_connection, HFP_HF_VRA_EVENT_RECEIVED_OK);
    CHECK_EQUAL(HFP_VRA_ACTIVE, hfp_connection->vra_engine_current_state);
    CHECK_EQUAL(0u, hfp_connection->ok_pending);
    CHECK_EQUAL(HFP_SUBEVENT_VOICE_RECOGNITION_ACTIVATED, last_received_event);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, last_received_event_status);
}

/*
TEST(HFP_HF_VRA, event_in_HFP_VRA_ACTIVE){
    // ACTIVE --> ACTIVE : ok | error | timeout / reset response_pending
    // 
    // simulate OK
    setup_vra_w4_confirmation(HFP_VRA_ACTIVE);
    test_hfp_hf_vra_state_machine(hfp_connection, HFP_HF_VRA_EVENT_RECEIVED_OK);
    CHECK_EQUAL(HFP_VRA_ACTIVE, hfp_connection->vra_engine_current_state);
    CHECK_EQUAL(0u, hfp_connection->ok_pending);

    // simulate ERROR
    setup_vra_w4_confirmation(HFP_VRA_ACTIVE);
    test_hfp_hf_vra_state_machine(hfp_connection, HFP_HF_VRA_EVENT_RECEIVED_ERROR);
    CHECK_EQUAL(HFP_VRA_ACTIVE, hfp_connection->vra_engine_current_state);
    CHECK_EQUAL(0u, hfp_connection->ok_pending);
    
    // simulate timeout
    setup_vra_w4_confirmation(HFP_VRA_ACTIVE);
    test_hfp_hf_vra_state_machine(hfp_connection, HFP_HF_VRA_EVENT_RECEIVED_TIMEOUT);
    CHECK_EQUAL(HFP_VRA_ACTIVE, hfp_connection->vra_engine_current_state);
    CHECK_EQUAL(0u, hfp_connection->ok_pending);

    // ACTIVE --> W4_READY_FOR_AUDIO: target = READY_FOR_AUDIO && ready2send / send message
    // simulate HFP_HF_VRA_EVENT_CAN_SEND_NOW
    setup_vra_can_send_now(HFP_VRA_ACTIVE, HFP_VRA_ENHANCED_ACTIVE);
    test_hfp_hf_vra_state_machine(hfp_connection, HFP_HF_VRA_EVENT_CAN_SEND_NOW);
    check_equal_hf_commands(HFP_ACTIVATE_VOICE_RECOGNITION, 2);
    CHECK_EQUAL(HFP_VRA_W4_ENHANCED_ACTIVE, hfp_connection->vra_engine_current_state);
    CHECK_EQUAL(1u, hfp_connection->ok_pending);
    
    // ACTIVE --> W4_OFF : target = OFF && ready2send / send deactivate command
    // simulate HFP_HF_VRA_EVENT_CAN_SEND_NOW
    setup_vra_can_send_now(HFP_VRA_ACTIVE, HFP_VRA_OFF);    
    test_hfp_hf_vra_state_machine(hfp_connection, HFP_HF_VRA_EVENT_CAN_SEND_NOW);
    check_equal_hf_commands(HFP_ACTIVATE_VOICE_RECOGNITION, 0);
    CHECK_EQUAL(HFP_VRA_W4_OFF, hfp_connection->vra_engine_current_state);
    CHECK_EQUAL(1u, hfp_connection->ok_pending);

    // ACTIVE --> READY_FOR_AUDIO: ag_ready4audio / if response_pending block HF cmds
    // simulate HFP_HF_VRA_EVENT_AG_REPORT_READY_FOR_AUDIO
    hfp_connection->ok_pending = 0u;
    setup_vra_ag_report(HFP_VRA_ACTIVE, HFP_VOICE_RECOGNITION_STATUS_READY_FOR_AUDIO);
    test_hfp_hf_vra_state_machine(hfp_connection, HFP_HF_VRA_EVENT_AG_REPORT_READY_FOR_AUDIO);
    CHECK_EQUAL(HFP_VRA_ENHANCED_ACTIVE, hfp_connection->vra_engine_current_state);

    // ACTIVE --> OFF: ag_deactivate | sco_disconnect
    // simulate HFP_HF_VRA_EVENT_AG_REPORT_DEACTIVATED
    hfp_connection->ok_pending = 0u;
    setup_vra_ag_report(HFP_VRA_ACTIVE, HFP_VOICE_RECOGNITION_STATUS_DISABLED);
    test_hfp_hf_vra_state_machine(hfp_connection, HFP_HF_VRA_EVENT_AG_REPORT_DEACTIVATED);
    CHECK_EQUAL(HFP_VRA_OFF, hfp_connection->vra_engine_current_state);

    // ACTIVE --> OFF: ag_deactivate | sco_disconnect
    // simulate HFP_HF_VRA_EVENT_SCO_DISCONNECTED
    hfp_connection->ok_pending = 0u;
    setup_vra_ag_report(HFP_VRA_ACTIVE, HFP_VOICE_RECOGNITION_STATUS_DISABLED);
    test_hfp_hf_vra_state_machine(hfp_connection, HFP_HF_VRA_EVENT_SCO_DISCONNECTED);
    CHECK_EQUAL(HFP_VRA_OFF, hfp_connection->vra_engine_current_state);
}

TEST(HFP_HF_VRA, event_in_HFP_eVRA_READY_FOR_AUDIO){
    // READY_FOR_AUDIO --> READY_FOR_AUDIO : ok | error | timeout / reset response_pending
    // 
    // simulate OK
    setup_vra_w4_confirmation(HFP_VRA_ENHANCED_ACTIVE);
    test_hfp_hf_vra_state_machine(hfp_connection, HFP_HF_VRA_EVENT_RECEIVED_OK);
    CHECK_EQUAL(HFP_VRA_ENHANCED_ACTIVE, hfp_connection->vra_engine_current_state);
    CHECK_EQUAL(0u, hfp_connection->ok_pending);

    // simulate ERROR
    setup_vra_w4_confirmation(HFP_VRA_ENHANCED_ACTIVE);
    test_hfp_hf_vra_state_machine(hfp_connection, HFP_HF_VRA_EVENT_RECEIVED_ERROR);
    CHECK_EQUAL(HFP_VRA_ENHANCED_ACTIVE, hfp_connection->vra_engine_current_state);
    CHECK_EQUAL(0u, hfp_connection->ok_pending);
    
    // simulate timeout
    setup_vra_w4_confirmation(HFP_VRA_ENHANCED_ACTIVE);
    test_hfp_hf_vra_state_machine(hfp_connection, HFP_HF_VRA_EVENT_RECEIVED_TIMEOUT);
    CHECK_EQUAL(HFP_VRA_ENHANCED_ACTIVE, hfp_connection->vra_engine_current_state);
    CHECK_EQUAL(0u, hfp_connection->ok_pending);

    // READY_FOR_AUDIO --> OFF: ag_deactivate | sco_disconnect
    // simulate HFP_HF_VRA_EVENT_AG_REPORT_DEACTIVATED
    hfp_connection->ok_pending = 0u;
    setup_vra_ag_report(HFP_VRA_ENHANCED_ACTIVE, HFP_VOICE_RECOGNITION_STATUS_DISABLED);
    test_hfp_hf_vra_state_machine(hfp_connection, HFP_HF_VRA_EVENT_AG_REPORT_DEACTIVATED);
    CHECK_EQUAL(HFP_VRA_OFF, hfp_connection->vra_engine_current_state);

    // READY_FOR_AUDIO --> OFF: ag_deactivate | sco_disconnect
    // simulate HFP_HF_VRA_EVENT_SCO_DISCONNECTED
    hfp_connection->ok_pending = 0u;
    setup_vra_ag_report(HFP_VRA_ENHANCED_ACTIVE, HFP_VOICE_RECOGNITION_STATUS_DISABLED);
    test_hfp_hf_vra_state_machine(hfp_connection, HFP_HF_VRA_EVENT_SCO_DISCONNECTED);
    CHECK_EQUAL(HFP_VRA_OFF, hfp_connection->vra_engine_current_state);

    // READY_FOR_AUDIO --> W4_OFF : target = OFF && ready2send / send deactivate command
    // simulate HFP_HF_VRA_EVENT_AG_REPORT_DEACTIVATED
    hfp_connection->ok_pending = 0u;
    setup_vra_ag_report(HFP_VRA_W4_ENHANCED_ACTIVE, HFP_VOICE_RECOGNITION_STATUS_DISABLED);
    test_hfp_hf_vra_state_machine(hfp_connection, HFP_HF_VRA_EVENT_AG_REPORT_DEACTIVATED);
    CHECK_EQUAL(HFP_VRA_OFF, hfp_connection->vra_engine_current_state);
}

TEST(HFP_HF_VRA, event_in_HFP_VRA_W4_ACTIVE){
    // W4_ACTIVE --> ACTIVE : ok 
    // simulate OK
    setup_vra_w4_confirmation(HFP_VRA_W4_ACTIVE);
    test_hfp_hf_vra_state_machine(hfp_connection, HFP_HF_VRA_EVENT_RECEIVED_OK);
    CHECK_EQUAL(HFP_VRA_ACTIVE, hfp_connection->vra_engine_current_state);
    CHECK_EQUAL(0u, hfp_connection->ok_pending);

    // W4_ACTIVE --> OFF : error
    // simulate ERROR
    setup_vra_w4_confirmation(HFP_VRA_W4_ACTIVE);
    test_hfp_hf_vra_state_machine(hfp_connection, HFP_HF_VRA_EVENT_RECEIVED_ERROR);
    CHECK_EQUAL(HFP_VRA_OFF, hfp_connection->vra_engine_current_state);
    CHECK_EQUAL(0u, hfp_connection->ok_pending);
}

TEST(HFP_HF_VRA, event_in_HFP_eVRA_W4_READY_FOR_AUDIO){
    // W4_READY_FOR_AUDIO --> READY_FOR_AUDIO : ok
    // simulate OK
    setup_vra_w4_confirmation(HFP_VRA_W4_ENHANCED_ACTIVE);
    test_hfp_hf_vra_state_machine(hfp_connection, HFP_HF_VRA_EVENT_RECEIVED_OK);
    CHECK_EQUAL(HFP_VRA_ENHANCED_ACTIVE, hfp_connection->vra_engine_current_state);
    CHECK_EQUAL(0u, hfp_connection->ok_pending);

    // W4_READY_FOR_AUDIO --> ACTIVE : error
    // simulate ERROR
    setup_vra_w4_confirmation(HFP_VRA_W4_ENHANCED_ACTIVE);
    test_hfp_hf_vra_state_machine(hfp_connection, HFP_HF_VRA_EVENT_RECEIVED_ERROR);
    CHECK_EQUAL(HFP_VRA_ACTIVE, hfp_connection->vra_engine_current_state);
    CHECK_EQUAL(0u, hfp_connection->ok_pending);
}

TEST(HFP_HF_VRA, event_in_HFP_VRA_W4_OFF){
    // W4_OFF --> OFF : ok
    // simulate OK
    setup_vra_w4_confirmation(HFP_VRA_W4_OFF);
    test_hfp_hf_vra_state_machine(hfp_connection, HFP_HF_VRA_EVENT_RECEIVED_OK);
    CHECK_EQUAL(HFP_VRA_OFF, hfp_connection->vra_engine_current_state);
    CHECK_EQUAL(0u, hfp_connection->ok_pending);

    // W4_OFF --> ACTIVE : error
    // simulate ERROR
    setup_vra_w4_confirmation(HFP_VRA_W4_OFF);
    test_hfp_hf_vra_state_machine(hfp_connection, HFP_HF_VRA_EVENT_RECEIVED_ERROR);
    CHECK_EQUAL(HFP_VRA_ACTIVE, hfp_connection->vra_engine_current_state);
    CHECK_EQUAL(0u, hfp_connection->ok_pending);
}

TEST(HFP_HF_VRA, event_in_HFP_VRA_W4_ACTIVE_overlap_with_AG_REPORT){
    // overlapping HF and AG
    // W4_ACTIVE --> OFF : ag_deactivate | sco_disconnect / set response_pending, block HF cmds
    // simulate ag_deactivate
    setup_vra_w4_confirmation(HFP_VRA_W4_ACTIVE);
    setup_vra_ag_report(HFP_VRA_W4_ACTIVE, HFP_VOICE_RECOGNITION_STATUS_DISABLED);
    test_hfp_hf_vra_state_machine(hfp_connection, HFP_HF_VRA_EVENT_AG_REPORT_DEACTIVATED);
    CHECK_EQUAL(HFP_VRA_OFF, hfp_connection->vra_engine_current_state);
    CHECK_EQUAL(1u, hfp_connection->ok_pending);
}

TEST(HFP_HF_VRA, event_in_HFP_eVRA_W4_READY_FOR_AUDIO_overlap_with_AG_REPORT){
    // overlapping HF and AG
    // W4_READY_FOR_AUDIO --> OFF : ag_deactivate | sco_disconnect / set response_pending, block HF cmds
}

TEST(HFP_HF_VRA, EnableVRA2){
    // request voice recognition
    hfp_hf_activate_voice_recognition(hfp_connection->acl_handle);
    // send next command
    test_hfp_hf_vra_state_machine(hfp_connection, HFP_HF_VRA_EVENT_CAN_SEND_NOW);
    print_hf_commands();
    // TODO: verify command instead of printing them
    // simulate OK
    test_hfp_hf_vra_state_machine(hfp_connection, HFP_HF_VRA_EVENT_RECEIVED_OK);
    test_hfp_hf_vra_state_machine(hfp_connection, HFP_HF_VRA_EVENT_AG_REPORT_ACTIVATED);
    // TODO: check if event was received
}
// disable
// extended vra
*/

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
