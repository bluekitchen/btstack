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
        hfp_parse(&context, packet[pos], 1);
    }
    CHECK_EQUAL(HFP_CMD_OK, context.command);
}

TEST(HFPParser, HFP_HF_SUPPORTED_FEATURES){
    sprintf(packet, "\r\n%s:1007\r\n\r\nOK\r\n", HFP_SUPPORTED_FEATURES);
    for (pos = 0; pos < strlen(packet); pos++){
        hfp_parse(&context, packet[pos], 1);
    }
    CHECK_EQUAL(HFP_CMD_OK, context.command);
    CHECK_EQUAL(1007, context.remote_supported_features);
}


TEST(HFPParser, HFP_HF_INDICATORS){
    offset = 0;
    offset += snprintf(packet, sizeof(packet), "%s:", HFP_INDICATOR);
    for (pos = 0; pos < hfp_ag_indicators_nr - 1; pos++){
        offset += snprintf(packet+offset, sizeof(packet)-offset, "\"%s\", (%d, %d),", hfp_ag_indicators[pos].name, hfp_ag_indicators[pos].min_range, hfp_ag_indicators[pos].max_range);
    }
    offset += snprintf(packet+offset, sizeof(packet)-offset, "\"%s\", (%d, %d)\r\n\r\nOK\r\n", hfp_ag_indicators[pos].name, hfp_ag_indicators[pos].min_range, hfp_ag_indicators[pos].max_range);

    //context.command = HFP_CMD_RETRIEVE_AG_INDICATORS;
    context.state = HFP_W4_RETRIEVE_INDICATORS;

    for (pos = 0; pos < strlen(packet); pos++){
        hfp_parse(&context, packet[pos], 1);
    }
    CHECK_EQUAL(HFP_CMD_OK, context.command);
    CHECK_EQUAL(hfp_ag_indicators_nr, context.ag_indicators_nr);
    for (pos = 0; pos < hfp_ag_indicators_nr; pos++){
        CHECK_EQUAL(hfp_ag_indicators[pos].index, context.ag_indicators[pos].index);
        CHECK_EQUAL(0, strcmp(hfp_ag_indicators[pos].name, context.ag_indicators[pos].name));
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

    for (pos = 0; pos < strlen(packet); pos++){
        hfp_parse(&context, packet[pos], 1);
    }

    CHECK_EQUAL(HFP_CMD_OK, context.command);
    for (pos = 0; pos < hfp_ag_indicators_nr; pos++){
        CHECK_EQUAL(hfp_ag_indicators[pos].status, context.ag_indicators[pos].status);
    } 
}


TEST(HFPParser, HFP_HF_SUPPORT_CALL_HOLD_AND_MULTIPARTY_SERVICES){
    sprintf(packet, "\r\n%s:(1,1x,2,2x,3)\r\n\r\nOK\r\n", HFP_SUPPORT_CALL_HOLD_AND_MULTIPARTY_SERVICES);
    for (pos = 0; pos < strlen(packet); pos++){
        hfp_parse(&context, packet[pos], 1);
    }
    CHECK_EQUAL(HFP_CMD_OK, context.command);
    CHECK_EQUAL(5, context.remote_call_services_nr);
   
    CHECK_EQUAL(0, strcmp("1", (char*)context.remote_call_services[0].name));
    CHECK_EQUAL(0, strcmp("1x", (char*)context.remote_call_services[1].name));
    CHECK_EQUAL(0, strcmp("2", (char*)context.remote_call_services[2].name));
    CHECK_EQUAL(0, strcmp("2x", (char*)context.remote_call_services[3].name));
    CHECK_EQUAL(0, strcmp("3", (char*)context.remote_call_services[4].name));
} 

// TEST(HFPParser, HFP_HF_GENERIC_STATUS_INDICATOR){
//     sprintf(packet, "\r\n%s:0,1,2,3,4\r\n\r\nOK\r\n", HFP_GENERIC_STATUS_INDICATOR);
//     //context.command = HFP_CMD_RETRIEVE_GENERIC_STATUS_INDICATORS;
//     context.state = HFP_W4_RETRIEVE_GENERIC_STATUS_INDICATORS;
    
//     for (pos = 0; pos < strlen(packet); pos++){
//         hfp_parse(&context, packet[pos], 1);
//     }
    
//     CHECK_EQUAL(HFP_CMD_OK, context.command);
//     CHECK_EQUAL(5, context.generic_status_indicators_nr);
    
//     for (pos = 0; pos < context.generic_status_indicators_nr; pos++){
//         CHECK_EQUAL(pos, context.generic_status_indicators[pos].uuid);
//     } 
// }

TEST(HFPParser, HFP_HF_GENERIC_STATUS_INDICATOR_STATE){
    sprintf(packet, "\r\n%s:0,1\r\n\r\nOK\r\n", HFP_GENERIC_STATUS_INDICATOR);
    // context.command = HFP_CMD_RETRIEVE_GENERIC_STATUS_INDICATORS_STATE;
    context.state = HFP_W4_RETRIEVE_INITITAL_STATE_GENERIC_STATUS_INDICATORS;
                        
    for (pos = 0; pos < strlen(packet); pos++){
        hfp_parse(&context, packet[pos], 1);
    }
    
    CHECK_EQUAL(HFP_CMD_OK, context.command);
    CHECK_EQUAL(1, context.generic_status_indicators[0].state);
}

TEST(HFPParser, HFP_HF_AG_INDICATOR_STATUS_UPDATE){
    context.ag_indicators_nr = hfp_ag_indicators_nr;
    memcpy(context.ag_indicators, hfp_ag_indicators, hfp_ag_indicators_nr * sizeof(hfp_ag_indicator_t));
    
    uint8_t index = 4;
    uint8_t status = 5;

    sprintf(packet, "\r\n%s:%d,%d\r\n\r\nOK\r\n", HFP_TRANSFER_AG_INDICATOR_STATUS, index, status);
    for (pos = 0; pos < strlen(packet); pos++){
        hfp_parse(&context, packet[pos], 1);
    }

    CHECK_EQUAL(HFP_CMD_OK, context.command);
    CHECK_EQUAL(context.ag_indicators[index - 1].status, status);
}

TEST(HFPParser, HFP_HF_AG_QUERY_OPERATOR_SELECTION){
    sprintf(packet, "\r\n%s:1,0,\"sunrise\"\r\n\r\nOK\r\n", HFP_QUERY_OPERATOR_SELECTION);
    
    context.command = HFP_CMD_QUERY_OPERATOR_SELECTION_NAME;

    for (pos = 0; pos < strlen(packet); pos++){
        hfp_parse(&context, packet[pos], 1);
    }

    CHECK_EQUAL(context.command, HFP_CMD_OK);
    CHECK_EQUAL(context.operator_name_changed, 0); 
    CHECK_EQUAL( strcmp("sunrise", context.network_operator.name), 0);
}

TEST(HFPParser, HFP_HF_ERROR){
    sprintf(packet, "\r\n%s\r\n", HFP_ERROR);
    
    for (pos = 0; pos < strlen(packet); pos++){
        hfp_parse(&context, packet[pos], 1);
    }

    CHECK_EQUAL(context.command, HFP_CMD_ERROR);       
}

TEST(HFPParser, HFP_HF_EXTENDED_AUDIO_GATEWAY_ERROR){
    sprintf(packet, "\r\n%s:%d\r\n", HFP_EXTENDED_AUDIO_GATEWAY_ERROR, HFP_CME_ERROR_NO_NETWORK_SERVICE);
    
    for (pos = 0; pos < strlen(packet); pos++){
        hfp_parse(&context, packet[pos], 1);
    }

    CHECK_EQUAL(context.command, HFP_CMD_EXTENDED_AUDIO_GATEWAY_ERROR);
    CHECK_EQUAL(context.extended_audio_gateway_error, HFP_CME_ERROR_NO_NETWORK_SERVICE);       
}


TEST(HFPParser, HFP_HF_AG_INDICATOR_CALLS_STATUS_UPDATE){
    context.ag_indicators_nr = hfp_ag_indicators_nr;
    memcpy(context.ag_indicators, hfp_ag_indicators, hfp_ag_indicators_nr * sizeof(hfp_ag_indicator_t));
    uint8_t status = 1;

    // call status
    uint8_t index = call_status_index;
    sprintf(packet, "\r\n%s:%d,%d\r\n\r\nOK\r\n", HFP_TRANSFER_AG_INDICATOR_STATUS, index, status);
    for (pos = 0; pos < strlen(packet); pos++){
        hfp_parse(&context, packet[pos], 1);
    }
    CHECK_EQUAL(HFP_CMD_OK, context.command);
    CHECK_EQUAL(context.ag_indicators[index - 1].status, status);

    // callsetup status
    index = callsetup_status_index;
    sprintf(packet, "\r\n%s:%d,%d\r\n\r\nOK\r\n", HFP_TRANSFER_AG_INDICATOR_STATUS, index, status);
    for (pos = 0; pos < strlen(packet); pos++){
        hfp_parse(&context, packet[pos], 1);
    }
    CHECK_EQUAL(HFP_CMD_OK, context.command);
    CHECK_EQUAL(context.ag_indicators[index - 1].status, status);

    // callheld status
    index = callheld_status_index;
    sprintf(packet, "\r\n%s:%d,%d\r\n\r\nOK\r\n", HFP_TRANSFER_AG_INDICATOR_STATUS, index, status);
    for (pos = 0; pos < strlen(packet); pos++){
        hfp_parse(&context, packet[pos], 1);
    }
    CHECK_EQUAL(HFP_CMD_OK, context.command);
    CHECK_EQUAL(context.ag_indicators[index - 1].status, status);
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
