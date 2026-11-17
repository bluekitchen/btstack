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

TEST(SDPServer, QueriesConstRecordAndFiltersConstAttributeList){
    static const uint8_t pattern[] = { 0x35, 0x03, 0x19, 0x11, 0x01 };
    static const uint8_t attributes[] = { 0x35, 0x05, 0x0a, 0x00, 0x00, 0xff, 0xff };
    uint8_t filtered[16];
    uint16_t used = 0;

    CHECK_EQUAL(0, sdp_register_service(service_record));
    POINTERS_EQUAL(service_record, sdp_get_record_for_handle(0x00010000));
    POINTERS_EQUAL(&service_record[5], sdp_get_attribute_value_for_attribute_id(service_record, 0));
    CHECK_TRUE(sdp_valid_service_search_pattern(pattern));
    CHECK_TRUE(sdp_record_matches_service_search_pattern(service_record, pattern));
    CHECK_TRUE(sdp_attribute_list_valid(attributes));
    CHECK_EQUAL(sizeof(filtered), sdp_get_filtered_size(service_record, attributes));
    CHECK_TRUE(sdp_filter_attributes_in_attributeIDList(service_record, attributes, 0, sizeof(filtered), &used, filtered));
    CHECK_EQUAL(sizeof(filtered), used);
    MEMCMP_EQUAL(&service_record[2], filtered, sizeof(filtered));
}

TEST(SDPServer, ServiceSearchRejectsTruncatedSearchPattern){
    uint8_t packet[] = {
        SDP_ServiceSearchRequest, 0, 0, 0, 3,
        0x35, 0x02, 0x19
    };

    CHECK_EQUAL(7, sdp_handle_service_search_request(packet, 48));
    check_error_response(SDP_ERROR_CODE_INVALID_PDU_SIZE);
}

TEST(SDPServer, ServiceAttributeRejectsTruncatedAttributeIdList){
    uint8_t packet[] = {
        SDP_ServiceAttributeRequest, 0, 0, 0, 9,
        0, 0, 0, 0, 0, 7,
        0x35, 0x02, 0x09
    };

    CHECK_EQUAL(7, sdp_handle_service_attribute_request(packet, 48));
    check_error_response(SDP_ERROR_CODE_INVALID_PDU_SIZE);
}

TEST(SDPServer, ServiceAttributeRejectsTruncatedAttributeIdListChild){
    uint8_t packet[] = {
        SDP_ServiceAttributeRequest, 0, 0, 0, 10,
        0, 0, 0, 0, 0, 7,
        0x35, 0x02, 0x09, 0x00
    };

    CHECK_EQUAL(7, sdp_handle_service_attribute_request(packet, 48));
    check_error_response(SDP_ERROR_CODE_INVALID_REQUEST_SYNTAX);
}

TEST(SDPServer, ServiceAttributeRejectsNilAttributeIdListElement){
    uint8_t packet[] = {
        SDP_ServiceAttributeRequest, 0, 1, 0, 9,
        0, 1, 0, 1,
        0, 0x40,
        0x35, 0x01, 0x02
    };

    CHECK_EQUAL(7, sdp_handle_service_attribute_request(packet, 48));
    check_error_response(SDP_ERROR_CODE_INVALID_REQUEST_SYNTAX);
}

TEST(SDPServer, ServiceSearchAttributeRejectsTruncatedAttributeIdList){
    uint8_t packet[] = {
        SDP_ServiceSearchAttributeRequest, 0, 0, 0, 10,
        0x35, 0x03, 0x19, 0x11, 0x01,
        0, 7,
        0x35, 0x02, 0x09
    };

    CHECK_EQUAL(7, sdp_handle_service_search_attribute_request(packet, 48));
    check_error_response(SDP_ERROR_CODE_INVALID_PDU_SIZE);
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
    check_error_response(SDP_ERROR_CODE_INVALID_REQUEST_SYNTAX);
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
    check_error_response(SDP_ERROR_CODE_INVALID_REQUEST_SYNTAX);
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
