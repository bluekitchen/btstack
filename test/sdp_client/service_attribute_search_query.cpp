
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
0x35, 0x98, 0x09, 0x00, 0x01, 0x35, 0x03, 0x19, 0x11, 0x17, 0x09, 0x00, 0x04, 0x35, 0x1e, 0x35,
0x06, 0x19, 0x01, 0x00, 0x09, 0x00, 0x0f, 0x35, 0x14, 0x19, 0x00, 0x0f, 0x09, 0x01, 0x00, 0x35,
0x0c, 0x09, 0x08, 0x00, 0x09, 0x08, 0x06, 0x09, 0x86, 0xdd, 0x09, 0x88, 0x0b, 0x09, 0x00, 0x05,
0x35, 0x03, 0x19, 0x10, 0x02, 0x09, 0x00, 0x06, 0x35, 0x09, 0x09, 0x65, 0x6e, 0x09, 0x00, 0x6a,
0x09, 0x01, 0x00, 0x09, 0x00, 0x09, 0x35, 0x08, 0x35, 0x06, 0x19, 0x11, 0x17, 0x09, 0x01, 0x00,
0x09, 0x01, 0x00, 0x25, 0x1c, 0x47, 0x72, 0x6f, 0x75, 0x70, 0x20, 0x41, 0x64, 0x2d, 0x68, 0x6f,
0x63, 0x20, 0x4e, 0x65, 0x74, 0x77, 0x6f, 0x72, 0x6b, 0x20, 0x53, 0x65, 0x72, 0x76, 0x69, 0x63,
0x65, 0x09, 0x01, 0x01, 0x25, 0x18, 0x50, 0x41, 0x4e, 0x20, 0x47, 0x72, 0x6f, 0x75, 0x70, 0x20,
0x41, 0x64, 0x2d, 0x68, 0x6f, 0x63, 0x20, 0x4e, 0x65, 0x74, 0x77, 0x6f, 0x72, 0x6b, 0x09, 0x03,
0x0a, 0x09, 0x00, 0x01, 0x09, 0x03, 0x0b, 0x09, 0x00, 0x05
};


uint16_t attribute_id = -1;
uint16_t record_id = -1;

uint8_t * attribute_value = NULL;
int       attribute_value_buffer_size = 1000;

void assertBuffer(int size){
    if (size > attribute_value_buffer_size){
        attribute_value_buffer_size *= 2;
        uint8_t * new_attribute = (uint8_t *) realloc(attribute_value, attribute_value_buffer_size);
        if (!new_attribute) return;
        attribute_value = new_attribute;
    }
}

static void test_attribute_value_event(const uint8_t * event){
    static int recordId = 0;
    static int attributeId = 0;
    static int attributeOffset = 0;
    static int attributeLength = 0;

    CHECK_EQUAL(event[0], SDP_EVENT_QUERY_ATTRIBUTE_VALUE);

    // record ids are sequential
   // printf("sdp_event_query_attribute_byte_get_record_id(event) %d",sdp_event_query_attribute_byte_get_record_id(event));

    if (sdp_event_query_attribute_byte_get_record_id(event) != recordId){
        recordId++;
    }
    CHECK_EQUAL(sdp_event_query_attribute_byte_get_record_id(event), recordId);
    
    // is attribute value complete
    if (sdp_event_query_attribute_byte_get_attribute_id(event) != attributeId ){
        if (attributeLength > 0){
            CHECK_EQUAL(attributeLength, attributeOffset+1);
        }
        attributeId = sdp_event_query_attribute_byte_get_attribute_id(event);
        attributeOffset = 0;
    }

    // count attribute value bytes
    if (sdp_event_query_attribute_byte_get_data_offset(event) != attributeOffset){
        attributeOffset++;
    }
    attributeLength = sdp_event_query_attribute_byte_get_attribute_length(event);

    CHECK_EQUAL(sdp_event_query_attribute_byte_get_data_offset(event), attributeOffset);
}


static void handle_sdp_parser_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    switch (packet[0]){
        case SDP_EVENT_QUERY_ATTRIBUTE_VALUE:
            test_attribute_value_event(packet);
            
            // handle new record
            if (sdp_event_query_attribute_byte_get_record_id(packet) != record_id){
                record_id = sdp_event_query_attribute_byte_get_record_id(packet);
            }
            // buffer data
            assertBuffer(sdp_event_query_attribute_byte_get_attribute_length(packet));
            attribute_value[sdp_event_query_attribute_byte_get_data_offset(packet)] = sdp_event_query_attribute_byte_get_data(packet);
            
            break;
        case SDP_EVENT_QUERY_COMPLETE:
            printf("General query done with status %d.\n", sdp_event_query_complete_get_status(packet));
            break;
    }
}


TEST_GROUP(SDPClient){
    void setup(void){
        attribute_value_buffer_size = 1000;
        attribute_value = (uint8_t*) malloc(attribute_value_buffer_size);
        record_id = -1;
        sdp_parser_init(&handle_sdp_parser_event);
        sdp_parser_init_service_attribute_search();
    }
};


TEST(SDPClient, QueryRFCOMMWithMacOSXData){
    uint16_t expected_last_attribute_id = 0xffff;
    uint16_t expected_last_record_id = 0;
    uint8_t  expected_attribute_value[3] = {0x09, 0x00, 0x05};

    sdp_parser_handle_chunk(sdp_test_record_list, de_get_len(sdp_test_record_list));
    
    CHECK_EQUAL(expected_last_attribute_id, attribute_id);
    CHECK_EQUAL(expected_last_record_id, record_id);

    uint16_t i;
    for (i=0; i<sizeof(expected_attribute_value); i++){
       CHECK_EQUAL(expected_attribute_value[i], attribute_value[i]);
    }
}


int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
