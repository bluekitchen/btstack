
// *****************************************************************************
//
// test rfcomm query tests
//
// *****************************************************************************

#include "btstack-config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <btstack/hci_cmds.h>
#include <btstack/run_loop.h>

#include "hci.h"
#include "btstack_memory.h"
#include "hci_dump.h"
#include "l2cap.h"
#include "sdp_parser.h"

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
        attribute_value = (uint8_t *) realloc(attribute_value, attribute_value_buffer_size);
    }
}

static void test_attribute_value_event(sdp_query_attribute_value_event_t* event){
    static int recordId = 0;
    static int attributeId = 0;
    static int attributeOffset = 0;
    static int attributeLength = 0;

    CHECK_EQUAL(event->type, 0x93);

    // record ids are sequential
   // printf("event->record_id %d",event->record_id);

    if (event->record_id != recordId){
        recordId++;
    }
    CHECK_EQUAL(event->record_id, recordId);
    
    // is attribute value complete
    if (event->attribute_id != attributeId ){
        if (attributeLength > 0){
            CHECK_EQUAL(attributeLength, attributeOffset+1);
        }
        attributeId = event->attribute_id;
        attributeOffset = 0;
    }

    // count attribute value bytes
    if (event->data_offset != attributeOffset){
        attributeOffset++;
    }
    attributeLength = event->attribute_length;

    CHECK_EQUAL(event->data_offset, attributeOffset);
}


static void handle_sdp_parser_event(sdp_query_event_t * event){

    sdp_query_attribute_value_event_t * ve;
    sdp_query_complete_event_t * ce;

    switch (event->type){
        case SDP_QUERY_ATTRIBUTE_VALUE:
            ve = (sdp_query_attribute_value_event_t*) event;
            
            test_attribute_value_event(ve);
            
            // handle new record
            if (ve->record_id != record_id){
                record_id = ve->record_id;
            }
            // buffer data
            assertBuffer(ve->attribute_length);
            attribute_value[ve->data_offset] = ve->data;
            
            break;
        case SDP_QUERY_COMPLETE:
            ce = (sdp_query_complete_event_t*) event;
            printf("General query done with status %d.\n", ce->status);
            break;
    }
}


TEST_GROUP(SDPClient){
    void setup(void){
        attribute_value_buffer_size = 1000;
        attribute_value = (uint8_t*) malloc(attribute_value_buffer_size);
        record_id = -1;
        sdp_parser_init_service_attribute_search();
        sdp_parser_register_callback(handle_sdp_parser_event);
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
