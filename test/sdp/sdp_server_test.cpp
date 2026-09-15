// *****************************************************************************
//
// SDP server tests
//
// *****************************************************************************

#include <stdint.h>

#include "btstack_util.h"
#include "classic/sdp_server.h"
#include "classic/sdp_util.h"
#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

static const uint8_t service_record[] = {
    0x35, 0x10,
    0x09, 0x00, 0x00, 0x0a, 0x00, 0x01, 0x00, 0x00,
    0x09, 0x00, 0x01, 0x35, 0x03, 0x19, 0x11, 0x01
};

TEST_GROUP(SDPServer){
    void teardown(){
        sdp_unregister_service(0x00010000);
    }
};

static void check_error_response(uint16_t error_code){
    const uint8_t * response = sdp_server_get_response_buffer();
    CHECK_EQUAL(SDP_ErrorResponse, response[0]);
    CHECK_EQUAL(2, big_endian_read_16(response, 3));
    CHECK_EQUAL(error_code, big_endian_read_16(response, 5));
}

TEST(SDPServer, ServiceSearchRejectsTruncatedSearchPattern){
    uint8_t packet[] = {
        SDP_ServiceSearchRequest, 0, 0, 0, 3,
        0x35, 0x02, 0x19
    };

    CHECK_EQUAL(7, sdp_handle_service_search_request(packet, 48));
    check_error_response(0x0004);
}

TEST(SDPServer, ServiceAttributeRejectsTruncatedAttributeIdList){
    uint8_t packet[] = {
        SDP_ServiceAttributeRequest, 0, 0, 0, 9,
        0, 0, 0, 0, 0, 7,
        0x35, 0x02, 0x09
    };

    CHECK_EQUAL(7, sdp_handle_service_attribute_request(packet, 48));
    check_error_response(0x0004);
}

TEST(SDPServer, ServiceAttributeRejectsTruncatedAttributeIdListChild){
    uint8_t packet[] = {
        SDP_ServiceAttributeRequest, 0, 0, 0, 10,
        0, 0, 0, 0, 0, 7,
        0x35, 0x02, 0x09, 0x00
    };

    CHECK_EQUAL(7, sdp_handle_service_attribute_request(packet, 48));
    check_error_response(0x0003);
}

TEST(SDPServer, ServiceSearchAttributeRejectsTruncatedAttributeIdList){
    uint8_t packet[] = {
        SDP_ServiceSearchAttributeRequest, 0, 0, 0, 10,
        0x35, 0x03, 0x19, 0x11, 0x01,
        0, 7,
        0x35, 0x02, 0x09
    };

    CHECK_EQUAL(7, sdp_handle_service_search_attribute_request(packet, 48));
    check_error_response(0x0004);
}

TEST(SDPServer, ServiceAttributeRejectsZeroMaximumAttributeByteCount){
    uint8_t packet[] = {
        SDP_ServiceAttributeRequest, 0, 0, 0, 12,
        0, 1, 0, 0,
        0, 0,
        0x35, 0x03, 0x09, 0x00, 0x00,
        0
    };

    CHECK_EQUAL(0, sdp_register_service(service_record));
    CHECK_EQUAL(7, sdp_handle_service_attribute_request(packet, 48));
    check_error_response(0x0003);
}

TEST(SDPServer, ServiceSearchAttributeRejectsZeroMaximumAttributeByteCount){
    uint8_t packet[] = {
        SDP_ServiceSearchAttributeRequest, 0, 0, 0, 13,
        0x35, 0x03, 0x19, 0x11, 0x01,
        0, 0,
        0x35, 0x03, 0x09, 0x00, 0x00,
        0
    };

    CHECK_EQUAL(0, sdp_register_service(service_record));
    CHECK_EQUAL(7, sdp_handle_service_search_attribute_request(packet, 48));
    check_error_response(0x0003);
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
