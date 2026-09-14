// *****************************************************************************
//
// SDP server tests
//
// *****************************************************************************

#include <stdint.h>

#include "classic/sdp_server.h"
#include "classic/sdp_util.h"
#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

TEST_GROUP(SDPServer){};

TEST(SDPServer, ServiceSearchRejectsTruncatedSearchPattern){
    uint8_t packet[] = {
        SDP_ServiceSearchRequest, 0, 0, 0, 3,
        0x35, 0x02, 0x19
    };

    CHECK_EQUAL(7, sdp_handle_service_search_request(packet, 48));
}

TEST(SDPServer, ServiceAttributeRejectsTruncatedAttributeIdList){
    uint8_t packet[] = {
        SDP_ServiceAttributeRequest, 0, 0, 0, 9,
        0, 0, 0, 0, 0, 1,
        0x35, 0x02, 0x09
    };

    CHECK_EQUAL(7, sdp_handle_service_attribute_request(packet, 48));
}

TEST(SDPServer, ServiceAttributeRejectsTruncatedAttributeIdListChild){
    uint8_t packet[] = {
        SDP_ServiceAttributeRequest, 0, 0, 0, 10,
        0, 0, 0, 0, 0, 1,
        0x35, 0x02, 0x09, 0x00
    };

    CHECK_EQUAL(7, sdp_handle_service_attribute_request(packet, 48));
}

TEST(SDPServer, ServiceSearchAttributeRejectsTruncatedAttributeIdList){
    uint8_t packet[] = {
        SDP_ServiceSearchAttributeRequest, 0, 0, 0, 10,
        0x35, 0x03, 0x19, 0x11, 0x01,
        0, 1,
        0x35, 0x02, 0x09
    };

    CHECK_EQUAL(7, sdp_handle_service_search_attribute_request(packet, 48));
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
