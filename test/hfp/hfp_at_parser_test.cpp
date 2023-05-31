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


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "btstack_event.h"
#include "classic/hfp.h"
#include "classic/hfp_hf.h"
#include "classic/hfp_ag.h"

void hfp_parse(hfp_connection_t * context, uint8_t byte, int isHandsFree);

static  hfp_connection_t context;
static int hfp_ag_indicators_nr = 7;
static hfp_ag_indicator_t hfp_ag_indicators[] = {
    // index, name, min range, max range, status, mandatory, enabled, status changed
    {1, "service",   0, 1, 1, 0, 0, 0},
    {2, "call",      0, 1, 0, 1, 1, 0},
    {3, "callsetup", 0, 3, 0, 1, 1, 0},
    {4, "battchg",   0, 5, 3, 0, 0, 0},
    {5, "signal",    0, 5, 5, 0, 0, 0},
    {6, "roam",      0, 1, 0, 0, 0, 0},
    {7, "callheld",  0, 2, 0, 1, 1, 0}
};
static uint8_t call_status_index = 2;
static uint8_t callsetup_status_index = 3;
static uint8_t callheld_status_index = 7;

// #define LOG_LINE_BUFFER
static void hfp_at_parser_test_dump_line_buffer(void){
#ifdef LOG_LINE_BUFFER
    uint16_t line_len = strlen(reinterpret_cast<const char *>(context.line_buffer));
    printf("\nLine buffer: %s\n", context.line_buffer);
    printf_hexdump(context.line_buffer, line_len);
#endif
}

static void parse_ag(const char * packet){
    for (uint16_t pos = 0; pos < strlen(packet); pos++){
        hfp_parse(&context, packet[pos], 0);
    }
}

static void parse_hf(const char * packet){
    for (uint16_t pos = 0; pos < strlen(packet); pos++){
        hfp_parse(&context, packet[pos], 1);
    }
}

TEST_GROUP(HFPParser){
    char packet[200];
    int pos;
    int offset;

    void setup(void){
        hfp_init();
        memset(&context, 0, sizeof(hfp_connection_t));
        context.parser_state = HFP_PARSER_CMD_HEADER;
        context.parser_item_index = 0;
        context.line_size = 0;
        context.ag_indicators_nr = 0;
        context.remote_codecs_nr = 0;
        context.bnip_number[0] = 0;
        context.bnip_type = 0;
        memset(packet,0, sizeof(packet));
    }

    void teardown(void){
        hfp_deinit();
    }
};

TEST(HFPParser, HFP_HF_OK){
    snprintf(packet, sizeof(packet), "\r\n%s\r\n", HFP_OK);
    parse_hf(packet);
    CHECK_EQUAL(HFP_CMD_OK, context.command);
}

TEST(HFPParser, HFP_HF_SUPPORTED_FEATURES){
    snprintf(packet, sizeof(packet), "\r\n%s:1007\r\n\r\nOK\r\n", HFP_SUPPORTED_FEATURES);
    parse_hf(packet);
    CHECK_EQUAL(HFP_CMD_OK, context.command);
    CHECK_EQUAL(1007, context.remote_supported_features);
}

TEST(HFPParser, HFP_CMD_INDICATORS_QUERY){
    snprintf(packet, sizeof(packet), "\r\nAT%s?\r\n", HFP_INDICATOR);
    parse_ag(packet);
    CHECK_EQUAL(HFP_CMD_RETRIEVE_AG_INDICATORS_STATUS, context.command);
}

TEST(HFPParser, HFP_CMD_INDICATORS_RETRIEVE){
    snprintf(packet, sizeof(packet), "\r\nAT%s=?\r\n", HFP_INDICATOR);
    parse_ag(packet);
    CHECK_EQUAL(HFP_CMD_RETRIEVE_AG_INDICATORS, context.command);
}

TEST(HFPParser, HFP_HF_INDICATORS){
    offset = 0;
    offset += snprintf(packet, sizeof(packet), "%s:", HFP_INDICATOR);
    for (pos = 0; pos < hfp_ag_indicators_nr; pos++){
    	if (pos != 0) {
			packet[offset++] = ',';
		}
    	offset += snprintf(packet+offset, sizeof(packet)-offset, "(\"%s\", (%d, %d)),", hfp_ag_indicators[pos].name, hfp_ag_indicators[pos].min_range, hfp_ag_indicators[pos].max_range);
    }
    offset += snprintf(packet+offset, sizeof(packet)-offset, "\r\n\r\nOK\r\n");
    context.state = HFP_W4_RETRIEVE_INDICATORS;

    parse_hf(packet);
    CHECK_EQUAL(HFP_CMD_OK, context.command);
    CHECK_EQUAL(hfp_ag_indicators_nr, context.ag_indicators_nr);
    for (pos = 0; pos < hfp_ag_indicators_nr; pos++){
        CHECK_EQUAL(hfp_ag_indicators[pos].index, context.ag_indicators[pos].index);
        STRCMP_EQUAL(hfp_ag_indicators[pos].name, context.ag_indicators[pos].name);
        CHECK_EQUAL(hfp_ag_indicators[pos].min_range, context.ag_indicators[pos].min_range);
        CHECK_EQUAL(hfp_ag_indicators[pos].max_range, context.ag_indicators[pos].max_range);
    }   
}

TEST(HFPParser, HFP_HF_INDICATORS_RANGE){
	offset = 0;
	offset += snprintf(packet, sizeof(packet), "%s:", HFP_INDICATOR);
	for (pos = 0; pos < hfp_ag_indicators_nr; pos++){
		if (pos != 0) {
			packet[offset++] = ',';
		}
		offset += snprintf(packet+offset, sizeof(packet)-offset, "(\"%s\", (%d-%d)),", hfp_ag_indicators[pos].name, hfp_ag_indicators[pos].min_range, hfp_ag_indicators[pos].max_range);
	}
	offset += snprintf(packet+offset, sizeof(packet)-offset, "\r\n\r\nOK\r\n");
	context.state = HFP_W4_RETRIEVE_INDICATORS;

	parse_hf(packet);
	CHECK_EQUAL(HFP_CMD_OK, context.command);
	CHECK_EQUAL(hfp_ag_indicators_nr, context.ag_indicators_nr);
	for (pos = 0; pos < hfp_ag_indicators_nr; pos++){
		CHECK_EQUAL(hfp_ag_indicators[pos].index, context.ag_indicators[pos].index);
		STRCMP_EQUAL(hfp_ag_indicators[pos].name, context.ag_indicators[pos].name);
		CHECK_EQUAL(hfp_ag_indicators[pos].min_range, context.ag_indicators[pos].min_range);
		CHECK_EQUAL(hfp_ag_indicators[pos].max_range, context.ag_indicators[pos].max_range);
	}
}

TEST(HFPParser, HFP_HF_INDICATOR_STATUS){
    // send status
    offset = 0;
    offset += snprintf(packet, sizeof(packet), "%s:", HFP_INDICATOR);
    for (pos = 0; pos < hfp_ag_indicators_nr - 1; pos++){
        offset += snprintf(packet+offset, sizeof(packet)-offset, "%d,", hfp_ag_indicators[pos].status);
    }
    offset += snprintf(packet+offset, sizeof(packet)-offset, "%d\r\n\r\nOK\r\n", hfp_ag_indicators[pos].status);
    
    //context.command = HFP_CMD_RETRIEVE_AG_INDICATORS_STATUS;
    context.state = HFP_W4_RETRIEVE_INDICATORS_STATUS;

    parse_hf(packet);
    CHECK_EQUAL(HFP_CMD_OK, context.command);
    for (pos = 0; pos < hfp_ag_indicators_nr; pos++){
        CHECK_EQUAL(hfp_ag_indicators[pos].status, context.ag_indicators[pos].status);
    } 
}

TEST(HFPParser, HFP_HF_SUPPORT_CALL_HOLD_AND_MULTIPARTY_SERVICES_TEST){
    snprintf(packet, sizeof(packet), "\r\nAT%s=?\r\n", HFP_SUPPORT_CALL_HOLD_AND_MULTIPARTY_SERVICES);
    parse_ag(packet);
    CHECK_EQUAL(HFP_CMD_SUPPORT_CALL_HOLD_AND_MULTIPARTY_SERVICES, context.command);
}

TEST(HFPParser, HFP_HF_SUPPORT_CALL_HOLD_AND_MULTIPARTY_SERVICES_SET){
    int action = 1;
    int call_index = 2;
    snprintf(packet, sizeof(packet), "\r\nAT%s=%u%u\r\n", HFP_SUPPORT_CALL_HOLD_AND_MULTIPARTY_SERVICES, action, call_index);
    parse_ag(packet);
    CHECK_EQUAL(HFP_CMD_CALL_HOLD, context.command);
    CHECK_EQUAL(action, context.ag_call_hold_action);
    CHECK_EQUAL(call_index, context.call_index);
}

TEST(HFPParser, HFP_HF_SUPPORT_CALL_HOLD_AND_MULTIPARTY_SERVICES){
    snprintf(packet, sizeof(packet), "\r\n%s:(1,1x,2,2x,3)\r\n\r\nOK\r\n", HFP_SUPPORT_CALL_HOLD_AND_MULTIPARTY_SERVICES);
    parse_hf(packet);
    CHECK_EQUAL(HFP_CMD_OK, context.command);
    CHECK_EQUAL(5, context.remote_call_services_index);

    STRCMP_EQUAL("1", (char*)context.remote_call_services[0].name);
    STRCMP_EQUAL("1x", (char*)context.remote_call_services[1].name);
    STRCMP_EQUAL("2", (char*)context.remote_call_services[2].name);
    STRCMP_EQUAL("2x", (char*)context.remote_call_services[3].name);
    STRCMP_EQUAL("3", (char*)context.remote_call_services[4].name);
} 

TEST(HFPParser, HFP_GENERIC_STATUS_INDICATOR_TEST){
    snprintf(packet, sizeof(packet), "\r\nAT%s=?\r\n", HFP_GENERIC_STATUS_INDICATOR);
    parse_ag(packet);
    CHECK_EQUAL(HFP_CMD_RETRIEVE_GENERIC_STATUS_INDICATORS, context.command);
}

TEST(HFPParser, HFP_GENERIC_STATUS_INDICATOR_SET){
    int param1 = 1;
    int param2 = 2;
    int param3 = 3;
    snprintf(packet, sizeof(packet), "\r\nAT%s= %u,%u,%u\r\n", HFP_GENERIC_STATUS_INDICATOR, param1, param2, param3);
    parse_ag(packet);
    CHECK_EQUAL(HFP_CMD_LIST_GENERIC_STATUS_INDICATORS, context.command);
}

TEST(HFPParser, HFP_GENERIC_STATUS_INDICATOR_READ){
    snprintf(packet, sizeof(packet), "\r\nAT%s?\r\n", HFP_GENERIC_STATUS_INDICATOR);
    parse_ag(packet);
    CHECK_EQUAL(HFP_CMD_RETRIEVE_GENERIC_STATUS_INDICATORS_STATE, context.command);
}

TEST(HFPParser, HFP_HF_GENERIC_STATUS_INDICATOR_STATE){
    snprintf(packet, sizeof(packet), "\r\n%s:0,1\r\n\r\nOK\r\n", HFP_GENERIC_STATUS_INDICATOR);
    // context.command = HFP_CMD_RETRIEVE_GENERIC_STATUS_INDICATORS_STATE;
    context.state = HFP_W4_RETRIEVE_INITITAL_STATE_GENERIC_STATUS_INDICATORS;

    parse_hf(packet);
    CHECK_EQUAL(HFP_CMD_OK, context.command);
    CHECK_EQUAL(1, context.generic_status_indicators[0].state);
}

TEST(HFPParser, HFP_HF_AG_INDICATOR_STATUS_UPDATE){
    context.ag_indicators_nr = hfp_ag_indicators_nr;
    memcpy(context.ag_indicators, hfp_ag_indicators, hfp_ag_indicators_nr * sizeof(hfp_ag_indicator_t));
    
    uint8_t index = 4;
    uint8_t status = 5;

    snprintf(packet, sizeof(packet), "\r\n%s:%d,%d\r\n\r\nOK\r\n", HFP_TRANSFER_AG_INDICATOR_STATUS, index, status);

    parse_hf(packet);
    CHECK_EQUAL(HFP_CMD_OK, context.command);
    CHECK_EQUAL(status, context.ag_indicators[index - 1].status);
}

TEST(HFPParser, HFP_HF_AG_QUERY_OPERATOR_SELECTION){
    snprintf(packet, sizeof(packet), "\r\n%s:1,0,\"sunrise\"\r\n\r\nOK\r\n", HFP_QUERY_OPERATOR_SELECTION);
    
    context.command = HFP_CMD_QUERY_OPERATOR_SELECTION_NAME;

    parse_hf(packet);
    CHECK_EQUAL(HFP_CMD_OK, context.command);
    CHECK_EQUAL(0, context.operator_name_changed);
    STRCMP_EQUAL( "sunrise", context.network_operator.name);
}

TEST(HFPParser, HFP_HF_ERROR){
    snprintf(packet, sizeof(packet), "\r\n%s\r\n", HFP_ERROR);

    parse_hf(packet);
    CHECK_EQUAL(HFP_CMD_ERROR, context.command);
}

TEST(HFPParser, HFP_HF_EXTENDED_AUDIO_GATEWAY_ERROR){
    snprintf(packet, sizeof(packet), "\r\n%s:%d\r\n", HFP_EXTENDED_AUDIO_GATEWAY_ERROR, HFP_CME_ERROR_NO_NETWORK_SERVICE);

    parse_hf(packet);
    CHECK_EQUAL(HFP_CMD_EXTENDED_AUDIO_GATEWAY_ERROR, context.command);
    CHECK_EQUAL(HFP_CME_ERROR_NO_NETWORK_SERVICE, context.extended_audio_gateway_error_value);
}

TEST(HFPParser, HFP_HF_AG_INDICATOR_CALLS_STATUS_UPDATE){
    context.ag_indicators_nr = hfp_ag_indicators_nr;
    memcpy(context.ag_indicators, hfp_ag_indicators, hfp_ag_indicators_nr * sizeof(hfp_ag_indicator_t));
    uint8_t status = 1;

    // call status
    uint8_t index = call_status_index;
    snprintf(packet, sizeof(packet), "\r\n%s:%d,%d\r\n\r\nOK\r\n", HFP_TRANSFER_AG_INDICATOR_STATUS, index, status);
    parse_hf(packet);
    CHECK_EQUAL(HFP_CMD_OK, context.command);
    CHECK_EQUAL(status, context.ag_indicators[index - 1].status);

    // callsetup status
    index = callsetup_status_index;
    snprintf(packet, sizeof(packet), "\r\n%s:%d,%d\r\n\r\nOK\r\n", HFP_TRANSFER_AG_INDICATOR_STATUS, index, status);
    parse_hf(packet);
    CHECK_EQUAL(HFP_CMD_OK, context.command);
    CHECK_EQUAL(status, context.ag_indicators[index - 1].status);

    // callheld status
    index = callheld_status_index;
    snprintf(packet, sizeof(packet), "\r\n%s:%d,%d\r\n\r\nOK\r\n", HFP_TRANSFER_AG_INDICATOR_STATUS, index, status);
    parse_hf(packet);
    CHECK_EQUAL(HFP_CMD_OK, context.command);
    CHECK_EQUAL(status, context.ag_indicators[index - 1].status);
}

TEST(HFPParser, HFP_LIST_CURRENT_CALLS_1){
    strcpy(packet, "\r\n+CLCC: 1,2,3,4,5,,129\r\n");
    parse_hf(packet);
    CHECK_EQUAL(HFP_CMD_LIST_CURRENT_CALLS, context.command);
    CHECK_EQUAL(1, context.clcc_idx);
    CHECK_EQUAL(2, context.clcc_dir);
    CHECK_EQUAL(3, context.clcc_status);
    CHECK_EQUAL(4, context.clcc_mode);
    CHECK_EQUAL(5, context.clcc_mpty);
    STRCMP_EQUAL("", context.bnip_number);
    CHECK_EQUAL(129, context.bnip_type);
}

TEST(HFPParser, HFP_LIST_CURRENT_CALLS_2){
    strcpy(packet, "\r\n+CLCC: 1,2,3,4,5,"",129\r\n");
    parse_hf(packet);
    CHECK_EQUAL(HFP_CMD_LIST_CURRENT_CALLS, context.command);
    CHECK_EQUAL(1, context.clcc_idx);
    CHECK_EQUAL(2, context.clcc_dir);
    CHECK_EQUAL(3, context.clcc_status);
    CHECK_EQUAL(4, context.clcc_mode);
    CHECK_EQUAL(5, context.clcc_mpty);
    STRCMP_EQUAL("", context.bnip_number);
    CHECK_EQUAL(129, context.bnip_type);
}

TEST(HFPParser, HFP_AG_SUPPORTED_FEATURES){
    snprintf(packet, sizeof(packet), "\r\nAT%s=159\r\n", HFP_SUPPORTED_FEATURES);
    //context.keep_separator = 0;
    parse_ag(packet);
    CHECK_EQUAL(HFP_CMD_SUPPORTED_FEATURES, context.command);
    CHECK_EQUAL(159, context.remote_supported_features);
}

TEST(HFPParser, HFP_AG_AVAILABLE_CODECS){
    snprintf(packet, sizeof(packet), "\r\nAT%s=0,1,2\r\n", HFP_AVAILABLE_CODECS);
    parse_ag(packet);
    CHECK_EQUAL(HFP_CMD_AVAILABLE_CODECS, context.command);
    CHECK_EQUAL(3, context.remote_codecs_nr);
    for (pos = 0; pos < 3; pos++){
        CHECK_EQUAL(pos, context.remote_codecs[pos]);
    }   
}

TEST(HFPParser, HFP_AG_GENERIC_STATUS_INDICATOR){
    snprintf(packet, sizeof(packet), "\r\nAT%s=0,1,2,3,4\r\n", HFP_GENERIC_STATUS_INDICATOR);
    parse_ag(packet);
    CHECK_EQUAL(context.command, HFP_CMD_LIST_GENERIC_STATUS_INDICATORS);
    CHECK_EQUAL(5, context.generic_status_indicators_nr);
    
    for (pos = 0; pos < context.generic_status_indicators_nr; pos++){
        CHECK_EQUAL(pos, context.generic_status_indicators[pos].uuid);
    } 
}

TEST(HFPParser, HFP_AG_ENABLE_INDICATOR_STATUS_UPDATE){
    snprintf(packet, sizeof(packet), "\r\nAT%s=3,0,0,1\r\n", HFP_ENABLE_STATUS_UPDATE_FOR_AG_INDICATORS);
    parse_ag(packet);
    CHECK_EQUAL(HFP_CMD_ENABLE_INDICATOR_STATUS_UPDATE, context.command);
    CHECK_EQUAL(1, context.enable_status_update_for_ag_indicators);
}

TEST(HFPParser, HFP_AG_ENABLE_INDIVIDUAL_INDICATOR_STATUS_UPDATE){
    hfp_ag_init_ag_indicators(hfp_ag_indicators_nr, (hfp_ag_indicator_t *)&hfp_ag_indicators);
    context.ag_indicators_nr = hfp_ag_indicators_nr;
    memcpy(context.ag_indicators, hfp_ag_indicators, hfp_ag_indicators_nr * sizeof(hfp_ag_indicator_t));
    
    for (pos = 0; pos < hfp_ag_indicators_nr; pos++){
        CHECK_EQUAL(hfp_ag_indicators[pos].index,   hfp_ag_get_ag_indicators(&context)[pos].index );
        CHECK_EQUAL(hfp_ag_indicators[pos].enabled, hfp_ag_get_ag_indicators(&context)[pos].enabled);
        CHECK_EQUAL(hfp_ag_indicators[pos].index,   context.ag_indicators[pos].index);
        CHECK_EQUAL(hfp_ag_indicators[pos].enabled, context.ag_indicators[pos].enabled);
    }
    snprintf(packet, sizeof(packet), "\r\nAT%s=0,0,0,0,0,0,0\r\n", 
        HFP_UPDATE_ENABLE_STATUS_FOR_INDIVIDUAL_AG_INDICATORS);
    parse_ag(packet);
    CHECK_EQUAL(HFP_CMD_ENABLE_INDIVIDUAL_AG_INDICATOR_STATUS_UPDATE, context.command);

    for (pos = 0; pos < hfp_ag_indicators_nr; pos++){
        if (hfp_ag_get_ag_indicators(&context)[pos].mandatory){
            CHECK_EQUAL(1, hfp_ag_get_ag_indicators(&context)[pos].enabled);
            CHECK_EQUAL(1, context.ag_indicators[pos].enabled);
        } else {
            CHECK_EQUAL(0, hfp_ag_get_ag_indicators(&context)[pos].enabled);
            CHECK_EQUAL(0, context.ag_indicators[pos].enabled);
        }
    }
}

TEST(HFPParser, HFP_AG_ENABLE_INDIVIDUAL_INDICATOR_STATUS_UPDATE_OPT_VALUES3){
    hfp_ag_init_ag_indicators(hfp_ag_indicators_nr, (hfp_ag_indicator_t *)&hfp_ag_indicators);
    context.ag_indicators_nr = hfp_ag_indicators_nr;
    memcpy(context.ag_indicators, hfp_ag_indicators, hfp_ag_indicators_nr * sizeof(hfp_ag_indicator_t));

    snprintf(packet, sizeof(packet), "\r\nAT%s=,1,,,,,1\r\n", 
        HFP_UPDATE_ENABLE_STATUS_FOR_INDIVIDUAL_AG_INDICATORS);
    parse_ag(packet);

    CHECK_EQUAL(HFP_CMD_ENABLE_INDIVIDUAL_AG_INDICATOR_STATUS_UPDATE, context.command);

    for (pos = 0; pos < hfp_ag_indicators_nr; pos++){
        CHECK_EQUAL(hfp_ag_indicators[pos].index, hfp_ag_get_ag_indicators(&context)[pos].index );
        CHECK_EQUAL(hfp_ag_indicators[pos].enabled, hfp_ag_get_ag_indicators(&context)[pos].enabled);
        CHECK_EQUAL(hfp_ag_indicators[pos].index, context.ag_indicators[pos].index );
        CHECK_EQUAL(hfp_ag_indicators[pos].enabled, context.ag_indicators[pos].enabled);
    }
}

TEST(HFPParser, HFP_AG_ENABLE_INDIVIDUAL_INDICATOR_STATUS_UPDATE_OPT_VALUES2){
    hfp_ag_init_ag_indicators(hfp_ag_indicators_nr, (hfp_ag_indicator_t *)&hfp_ag_indicators);
    context.ag_indicators_nr = hfp_ag_indicators_nr;
    memcpy(context.ag_indicators, hfp_ag_indicators, hfp_ag_indicators_nr * sizeof(hfp_ag_indicator_t));

    snprintf(packet, sizeof(packet), "\r\nAT%s=1,,,1,1,1,\r\n", 
        HFP_UPDATE_ENABLE_STATUS_FOR_INDIVIDUAL_AG_INDICATORS);
    parse_ag(packet);

    CHECK_EQUAL(HFP_CMD_ENABLE_INDIVIDUAL_AG_INDICATOR_STATUS_UPDATE, context.command);

    for (pos = 0; pos < hfp_ag_indicators_nr; pos++){
        CHECK_EQUAL(1,hfp_ag_get_ag_indicators(&context)[pos].enabled);
        CHECK_EQUAL(1, context.ag_indicators[pos].enabled);
    }
}

TEST(HFPParser, HFP_AG_ENABLE_INDIVIDUAL_INDICATOR_STATUS_UPDATE_OPT_VALUES1){
    hfp_ag_init_ag_indicators(hfp_ag_indicators_nr, (hfp_ag_indicator_t *)&hfp_ag_indicators);
    context.ag_indicators_nr = hfp_ag_indicators_nr;
    memcpy(context.ag_indicators, hfp_ag_indicators, hfp_ag_indicators_nr * sizeof(hfp_ag_indicator_t));

    snprintf(packet, sizeof(packet), "\r\nAT%s=1,,,1,1,1,\r\n", 
        HFP_UPDATE_ENABLE_STATUS_FOR_INDIVIDUAL_AG_INDICATORS);
    parse_ag(packet);

    CHECK_EQUAL(HFP_CMD_ENABLE_INDIVIDUAL_AG_INDICATOR_STATUS_UPDATE, context.command);

    for (pos = 0; pos < hfp_ag_indicators_nr; pos++){
        CHECK_EQUAL(1, hfp_ag_get_ag_indicators(&context)[pos].enabled);
        CHECK_EQUAL(1, context.ag_indicators[pos].enabled);
    }
}

TEST(HFPParser, HFP_AG_HF_QUERY_OPERATOR_SELECTION){
    context.network_operator.format = 0xff;
    snprintf(packet, sizeof(packet), "\r\nAT%s=3,0\r\n", HFP_QUERY_OPERATOR_SELECTION);

    parse_ag(packet);
    CHECK_EQUAL(0, context.operator_name_changed);
    CHECK_EQUAL(HFP_CMD_QUERY_OPERATOR_SELECTION_NAME_FORMAT, context.command);
    
    snprintf(packet, sizeof(packet), "\r\nAT%s?\r\n", HFP_QUERY_OPERATOR_SELECTION);

    parse_ag(packet);
    CHECK_EQUAL(HFP_CMD_QUERY_OPERATOR_SELECTION_NAME, context.command);
    CHECK_EQUAL(0, context.operator_name_changed);
}

TEST(HFPParser, HFP_AG_EXTENDED_AUDIO_GATEWAY_ERROR){
    snprintf(packet, sizeof(packet), "\r\nAT%s=1\r\n", HFP_ENABLE_EXTENDED_AUDIO_GATEWAY_ERROR);

    parse_ag(packet);
    CHECK_EQUAL(HFP_CMD_ENABLE_EXTENDED_AUDIO_GATEWAY_ERROR, context.command );
    CHECK_EQUAL(1, context.enable_extended_audio_gateway_error_report);
}

TEST(HFPParser, HFP_AG_TRIGGER_CODEC_CONNECTION_SETUP){
    snprintf(packet, sizeof(packet), "\r\nAT%s\r\n", HFP_TRIGGER_CODEC_CONNECTION_SETUP);
    parse_ag(packet);
    CHECK_EQUAL(HFP_CMD_TRIGGER_CODEC_CONNECTION_SETUP, context.command);
}

TEST(HFPParser, HFP_AG_CONFIRM_COMMON_CODEC){
    int codec = 2;
    snprintf(packet, sizeof(packet), "\r\nAT%s=%d\r\n", HFP_CONFIRM_COMMON_CODEC, codec);

    parse_ag(packet);
    CHECK_EQUAL(HFP_CMD_HF_CONFIRMED_CODEC, context.command );
    CHECK_EQUAL(codec, context.codec_confirmed);
}

TEST(HFPParser, HFP_AG_DIAL){
    strcpy(packet, "\r\nATD00123456789;\r\n");

    parse_ag(packet);
    CHECK_EQUAL(HFP_CMD_CALL_PHONE_NUMBER, context.command);
    STRCMP_EQUAL("00123456789", (const char *) &context.line_buffer[3]);
}

TEST(HFPParser, HFP_ANSWER_CALL){
    snprintf(packet, sizeof(packet), "\r\n%s\r\n", HFP_ANSWER_CALL);
    parse_ag(packet);
    CHECK_EQUAL(HFP_CMD_CALL_ANSWERED, context.command);
}

TEST(HFPParser, HFP_CMD_RESPONSE_AND_HOLD_QUERY){
    snprintf(packet, sizeof(packet), "\r\nAT%s?\r\n", HFP_RESPONSE_AND_HOLD);
    parse_ag(packet);
    CHECK_EQUAL(HFP_CMD_RESPONSE_AND_HOLD_QUERY, context.command);
}

TEST(HFPParser, HFP_CMD_RESPONSE_AND_HOLD_COMMAND){
    int param = 1;
    snprintf(packet, sizeof(packet), "\r\nAT%s=%u\r\n", HFP_RESPONSE_AND_HOLD, param);
    parse_ag(packet);
    CHECK_EQUAL(HFP_CMD_RESPONSE_AND_HOLD_COMMAND, context.command);
    CHECK_EQUAL(param, context.ag_response_and_hold_action);
}

TEST(HFPParser, HFP_CMD_RESPONSE_AND_HOLD_STATUS){
    int status = 1;
    snprintf(packet, sizeof(packet), "\r\n%s:%u\r\n", HFP_RESPONSE_AND_HOLD, status);
    parse_hf(packet);
    CHECK_EQUAL(HFP_CMD_RESPONSE_AND_HOLD_STATUS, context.command);
}

TEST(HFPParser, HFP_CMD_ENABLE_CLIP){
    int param = 1;
    snprintf(packet, sizeof(packet), "\r\nAT%s=%u\r\n", HFP_ENABLE_CLIP, param);
    parse_ag(packet);
    CHECK_EQUAL(HFP_CMD_ENABLE_CLIP, context.command);
    CHECK_EQUAL(param, context.clip_enabled);
}

TEST(HFPParser, HFP_CMD_AG_SENT_CLIP_INFORMATION_a){
    // default/minimal
    parse_hf("\r\n+CLIP: \"+123456789\",145\r\n");
    CHECK_EQUAL(HFP_CMD_AG_SENT_CLIP_INFORMATION, context.command);
    STRCMP_EQUAL("+123456789", context.bnip_number);
    CHECK_EQUAL(145, context.bnip_type);
    CHECK_EQUAL(false, context.clip_have_alpha);
}

TEST(HFPParser, HFP_CMD_AG_SENT_CLIP_INFORMATION_b){
    // iOS
    parse_hf("\r\n+CLIP: \"+123456789\",145,,,\"BlueKitchen GmbH\"\r\n");
    CHECK_EQUAL(HFP_CMD_AG_SENT_CLIP_INFORMATION, context.command);
    STRCMP_EQUAL("+123456789", context.bnip_number);
    CHECK_EQUAL(145, context.bnip_type);
    CHECK_EQUAL(true, context.clip_have_alpha);
    STRCMP_EQUAL("BlueKitchen GmbH", (const char *)context.line_buffer);
}

TEST(HFPParser, HFP_CMD_AG_SENT_CLIP_INFORMATION_c){
    // older iOS with additional ','
    parse_hf("\r\n+CLIP: \"+123456789\",145,,,,\"BlueKitchen GmbH\"\r\n");
    CHECK_EQUAL(HFP_CMD_AG_SENT_CLIP_INFORMATION, context.command);
    STRCMP_EQUAL("+123456789", context.bnip_number);
    CHECK_EQUAL(145, context.bnip_type);
    CHECK_EQUAL(true, context.clip_have_alpha);
    STRCMP_EQUAL("BlueKitchen GmbH", (const char *)context.line_buffer);
}

TEST(HFPParser, HFP_CMD_AG_SENT_CLIP_INFORMATION_d){
    // BlackBerry, additional quotes
    parse_hf("\r\n+CLIP: \"+123456789\",145,\"\",,\"BlueKitchen GmbH\"\r\n");
    CHECK_EQUAL(HFP_CMD_AG_SENT_CLIP_INFORMATION, context.command);
    STRCMP_EQUAL("+123456789", context.bnip_number);
    CHECK_EQUAL(145, context.bnip_type);
    CHECK_EQUAL(true, context.clip_have_alpha);
    STRCMP_EQUAL("BlueKitchen GmbH", (const char *)context.line_buffer);
}

TEST(HFPParser, HFP_CMD_AG_SENT_CALL_WAITING_INFORMATION){
    parse_hf("\r\n+CCWA: \"+123456789\",145,\"\",1,\"BlueKitchen GmbH\"\r\n");
    CHECK_EQUAL(HFP_CMD_AG_SENT_CALL_WAITING_NOTIFICATION_UPDATE, context.command);
    STRCMP_EQUAL("+123456789", context.bnip_number);
    CHECK_EQUAL(145, context.bnip_type);
    CHECK_EQUAL(true, context.clip_have_alpha);
    STRCMP_EQUAL("BlueKitchen GmbH", (const char *)context.line_buffer);
}

TEST(HFPParser, custom_command_hf){
    hfp_custom_at_command_t custom_hf_command = {
            .command = "+FOO:",
            .command_id = 1
    };
    const char * custom_hf_command_string = "\r\n+FOO:1,2,3\r\n";
    hfp_register_custom_hf_command(&custom_hf_command);
    parse_hf(custom_hf_command_string);
    CHECK_EQUAL(1, context.custom_at_command_id);
    STRCMP_EQUAL("+FOO:1,2,3", (const char *)context.line_buffer);
    hfp_at_parser_test_dump_line_buffer();
}

TEST(HFPParser, custom_command_ag_with_colon){
    hfp_custom_at_command_t custom_ag_command = {
            .command = "AT+FOO:",
            .command_id = 2
    };
    const char * custom_hf_command_string = "\r\nAT+FOO:1,2,3\r\n";
    hfp_register_custom_ag_command(&custom_ag_command);
    parse_ag(custom_hf_command_string);
    CHECK_EQUAL(2, context.custom_at_command_id);
    STRCMP_EQUAL("AT+FOO:1,2,3", (const char *)context.line_buffer);
    hfp_at_parser_test_dump_line_buffer();
}

TEST(HFPParser, custom_command_ag_with_question){
    hfp_custom_at_command_t custom_ag_command = {
            .command = "AT+FOO?",
            .command_id = 3
    };
    const char * custom_hf_command_string = "\r\nAT+FOO?\r\n";
    hfp_register_custom_ag_command(&custom_ag_command);
    parse_ag(custom_hf_command_string);
    CHECK_EQUAL(3, context.custom_at_command_id);
    STRCMP_EQUAL("AT+FOO?", (const char *)context.line_buffer);
    hfp_at_parser_test_dump_line_buffer();
}

TEST(HFPParser, custom_command_hf_with_assignment){
    hfp_custom_at_command_t custom_ag_command = {
            .command = "AT+TEST=",
            .command_id = 3
    };
    const char * custom_hf_command_string = "\r\nAT+TEST=ABCDE\r\n";
    hfp_register_custom_ag_command(&custom_ag_command);
    parse_ag(custom_hf_command_string);
    CHECK_EQUAL(3, context.custom_at_command_id);
    STRCMP_EQUAL("AT+TEST=ABCDE", (const char *)context.line_buffer);
    hfp_at_parser_test_dump_line_buffer();
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
