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

#include "hfp.h"

void hfp_parse(hfp_connection_t * context, uint8_t byte);

static  hfp_connection_t context;
static int ag_indicators_nr = 7;
static hfp_ag_indicator_t ag_indicators[] = {
    {1, "service", 0, 1, 1},
    {2, "call", 0, 1, 0},
    {3, "callsetup", 0, 3, 0},
    {4, "battchg", 0, 5, 3},
    {5, "signal", 0, 5, 5},
    {6, "roam", 0, 1, 0},
    {7, "callheld", 0, 2, 0},
};

TEST_GROUP(HFPParser){
    char packet[200];
    int pos;
    int offset;

    void setup(void){
        context.parser_state = HFP_PARSER_CMD_HEADER;
        context.parser_item_index = 0;
        context.line_size = 0;
        context.ag_indicators_nr = 0;
        context.remote_codecs_nr = 0;
        memset(packet,0, sizeof(packet)); 
    }
};

TEST(HFPParser, HFP_HF_OK){
    sprintf(packet, "\r\n%s\r\n", HFP_OK);
    for (pos = 0; pos < strlen(packet); pos++){
        hfp_parse(&context, packet[pos]);
    }
    CHECK_EQUAL(HFP_CMD_OK, context.command);
}

TEST(HFPParser, HFP_HF_SUPPORTED_FEATURES){
    sprintf(packet, "\r\n%s:0000001111101111\r\n", HFP_SUPPORTED_FEATURES);
    for (pos = 0; pos < strlen(packet); pos++){
        hfp_parse(&context, packet[pos]);
    }
    CHECK_EQUAL(HFP_CMD_SUPPORTED_FEATURES, context.command);
    CHECK_EQUAL(1007, context.remote_supported_features);
}

TEST(HFPParser, HFP_HF_INDICATORS){
    offset = 0;
    offset += snprintf(packet, sizeof(packet), "%s:", HFP_INDICATOR);
    for (pos = 0; pos < ag_indicators_nr - 1; pos++){
        offset += snprintf(packet+offset, sizeof(packet)-offset, "\"%s\", (%d, %d),", ag_indicators[pos].name, ag_indicators[pos].min_range, ag_indicators[pos].max_range);
    }
    offset += snprintf(packet+offset, sizeof(packet)-offset, "\"%s\", (%d, %d)\r\n", ag_indicators[pos].name, ag_indicators[pos].min_range, ag_indicators[pos].max_range);
    
    context.sent_command = HFP_CMD_INDICATOR;
    context.ag_indicators_nr = 0;

    for (pos = 0; pos < strlen(packet); pos++){
        hfp_parse(&context, packet[pos]);
    }
    CHECK_EQUAL(HFP_CMD_INDICATOR, context.command);
    CHECK_EQUAL(ag_indicators_nr, context.ag_indicators_nr);
    for (pos = 0; pos < ag_indicators_nr; pos++){
        CHECK_EQUAL(ag_indicators[pos].index, context.ag_indicators[pos].index);
        CHECK_EQUAL(0, strcmp(ag_indicators[pos].name, context.ag_indicators[pos].name));
        CHECK_EQUAL(ag_indicators[pos].min_range, context.ag_indicators[pos].min_range);
        CHECK_EQUAL(ag_indicators[pos].max_range, context.ag_indicators[pos].max_range);
    }   
}

TEST(HFPParser, HFP_HF_INDICATOR_STATUS){
    // send status
    offset = 0;
    offset += snprintf(packet, sizeof(packet), "%s:", HFP_INDICATOR);
    for (pos = 0; pos < ag_indicators_nr - 1; pos++){
        offset += snprintf(packet+offset, sizeof(packet)-offset, "%d,", ag_indicators[pos].status);
    }
    offset += snprintf(packet+offset, sizeof(packet)-offset, "%d\r\n", ag_indicators[pos].status);
    
    context.sent_command = HFP_CMD_INDICATOR_STATUS;
    
    for (pos = 0; pos < strlen(packet); pos++){
        hfp_parse(&context, packet[pos]);
    }

    CHECK_EQUAL(HFP_CMD_INDICATOR_STATUS, context.command);
    for (pos = 0; pos < ag_indicators_nr; pos++){
        CHECK_EQUAL(ag_indicators[pos].status, context.ag_indicators[pos].status);
    } 
}

TEST(HFPParser, HFP_HF_SUPPORT_CALL_HOLD_AND_MULTIPARTY_SERVICES){
    sprintf(packet, "\r\n%s:(1,1x,2,2x,3)\r\n", HFP_SUPPORT_CALL_HOLD_AND_MULTIPARTY_SERVICES);
    for (pos = 0; pos < strlen(packet); pos++){
        hfp_parse(&context, packet[pos]);
    }
    CHECK_EQUAL(HFP_CMD_SUPPORT_CALL_HOLD_AND_MULTIPARTY_SERVICES, context.command);
    CHECK_EQUAL(5, context.remote_call_services_nr);
   
    CHECK_EQUAL(0, strcmp("1", (char*)context.remote_call_services[0].name));
    CHECK_EQUAL(0, strcmp("1x", (char*)context.remote_call_services[1].name));
    CHECK_EQUAL(0, strcmp("2", (char*)context.remote_call_services[2].name));
    CHECK_EQUAL(0, strcmp("2x", (char*)context.remote_call_services[3].name));
    CHECK_EQUAL(0, strcmp("3", (char*)context.remote_call_services[4].name));
} 

TEST(HFPParser, HFP_HF_GENERIC_STATUS_INDICATOR){
    sprintf(packet, "\r\n%s:0,1\r\n", HFP_GENERIC_STATUS_INDICATOR);
    context.sent_command = HFP_CMD_GENERIC_STATUS_INDICATOR;
    
    for (pos = 0; pos < strlen(packet); pos++){
        hfp_parse(&context, packet[pos]);
    }
    
    CHECK_EQUAL(HFP_CMD_GENERIC_STATUS_INDICATOR, context.command);
    CHECK_EQUAL(2, context.generic_status_indicators_nr);
    
    for (pos = 0; pos < context.generic_status_indicators_nr; pos++){
        CHECK_EQUAL(pos, context.generic_status_indicators[pos].uuid);
    } 
}

TEST(HFPParser, HFP_HF_GENERIC_STATUS_INDICATOR_STATE){
    sprintf(packet, "\r\n%s:0,1\r\n", HFP_GENERIC_STATUS_INDICATOR);
    context.sent_command = HFP_CMD_GENERIC_STATUS_INDICATOR_STATE;
    
    for (pos = 0; pos < strlen(packet); pos++){
        hfp_parse(&context, packet[pos]);
    }
    
    CHECK_EQUAL(HFP_CMD_GENERIC_STATUS_INDICATOR_STATE, context.command);
    CHECK_EQUAL(1, context.generic_status_indicators[0].state);
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
