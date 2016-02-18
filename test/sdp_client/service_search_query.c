
// *****************************************************************************
//
// test rfcomm query tests
//
// *****************************************************************************

#include "btstack_config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack_event.h"
#include "btstack_memory.h"
#include "btstack_run_loop.h"
#include "hci.h"
#include "hci_cmd.h"
#include "hci_dump.h"
#include "l2cap.h"
#include "mock.h"
#include "classic/sdp_util.h"

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"


static uint8_t  sdp_test_record_list[] = { 
0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x01, 
0x00, 0x00, 0x00, 0x02, 
0x00, 0x00, 0x00, 0x03, 
0x00, 0x00, 0x00, 0x04, 
0x00, 0x00, 0x00, 0x05, 
0x00, 0x00, 0x00, 0x06, 
0x00, 0x00, 0x00, 0x07, 
0x00, 0x00, 0x00, 0x08, 
0x00, 0x00, 0x00, 0x09, 
0x00, 0x00, 0x00, 0x0A
};

static void handle_sdp_parser_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    static uint32_t record_handle = sdp_test_record_list[0];
    switch (packet[0]){
        case SDP_EVENT_QUERY_SERVICE_RECORD_HANDLE:
            CHECK_EQUAL(sdp_event_query_service_record_handle_get_record_handle(packet), record_handle);
            record_handle++;
            break;
        case SDP_EVENT_QUERY_COMPLETE:
            printf("General query done with status %d.\n", sdp_event_query_complete_get_status(packet));
            break;
    }
}


TEST_GROUP(SDPClient){
    void setup(void){
        sdp_parser_init(&handle_sdp_parser_event);
        sdp_parser_init_service_search();
    }
};


TEST(SDPClient, QueryData){
    uint16_t test_size = sizeof(sdp_test_record_list)/4;
    sdp_parser_handle_service_search(sdp_test_record_list, test_size, test_size);
}


int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
