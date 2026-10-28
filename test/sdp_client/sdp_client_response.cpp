// *****************************************************************************
//
// SDP client response validation tests
//
// *****************************************************************************

#include "btstack_config.h"

#include <stdint.h>
#include <string.h>

#include "bluetooth_sdp.h"
#include "btstack_event.h"
#include "classic/sdp_client.h"
#include "classic/sdp_util.h"
#include "mock.h"

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

static bd_addr_t remote_address;
static uint8_t received_attribute_value[64];
static uint16_t received_attribute_length;
static uint16_t received_attribute_events;

static const uint8_t sdp_poc_empty_nested_des[] = {
    0x35, 0x09, 0x35, 0x07, 0x09, 0x00, 0x04, 0x35, 0x02, 0x35, 0x00
};

static const uint8_t sdp_poc_truncated_length[] = {
    0x35, 0x11, 0x35, 0x0f, 0x09, 0x00, 0x04, 0x37, 0xff, 0xff, 0x00, 0x07,
    0x35, 0x30, 0x19, 0x00, 0x19, 0x00, 0x00
};

static const uint8_t sdp_poc_far_length[] = {
    0x35, 0x14, 0x35, 0x12, 0x09, 0x00, 0x04, 0x37, 0xff, 0xff, 0x00, 0x0a,
    0x37, 0x00, 0x00, 0xff, 0x00, 0x35, 0x03, 0x19, 0x00, 0x19
};

static void handle_sdp_client_event(uint8_t packet_type, uint16_t channel, uint8_t * packet, uint16_t size){
    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(size);

    if (hci_event_packet_get_type(packet) != SDP_EVENT_QUERY_ATTRIBUTE_VALUE) return;

    uint16_t data_offset = sdp_event_query_attribute_byte_get_data_offset(packet);
    received_attribute_value[data_offset] = sdp_event_query_attribute_byte_get_data(packet);
    received_attribute_length = sdp_event_query_attribute_byte_get_attribute_length(packet);
    received_attribute_events++;
}

static void start_query(void){
    sdp_client_query_uuid16(&handle_sdp_client_event, remote_address, 0x1101);
    mock_l2cap_emit_channel_opened(0x40);
    CHECK_EQUAL(1, mock_l2cap_get_send_prepared_count());
}

static void emit_service_search_attribute_response(const uint8_t * attribute_list, uint16_t attribute_list_len){
    uint8_t response[64];
    uint16_t transaction_id = mock_l2cap_get_last_transaction_id();

    response[0] = SDP_ServiceSearchAttributeResponse;
    big_endian_store_16(response, 1, transaction_id);
    big_endian_store_16(response, 3, attribute_list_len + 3u);
    big_endian_store_16(response, 5, attribute_list_len);
    (void)memcpy(&response[7], attribute_list, attribute_list_len);
    response[7 + attribute_list_len] = 0;
    mock_l2cap_emit_data(0x40, response, attribute_list_len + 8u);
}

TEST_GROUP(SDPClientResponse){
    void setup(void){
        sdp_client_reset();
        mock_l2cap_reset();
        (void)memset(received_attribute_value, 0, sizeof(received_attribute_value));
        received_attribute_length = 0;
        received_attribute_events = 0;
    }
};

TEST(SDPClientResponse, RejectsUnexpectedResponsePdu){
    start_query();

    uint16_t transaction_id = mock_l2cap_get_last_transaction_id();
    uint8_t response[] = { SDP_ServiceAttributeResponse, 0, 0 };
    big_endian_store_16(response, 1, transaction_id);
    mock_l2cap_emit_data(0x40, response, sizeof(response));

    CHECK_EQUAL(1, mock_l2cap_get_disconnect_count());
    CHECK_EQUAL(1, mock_l2cap_get_send_prepared_count());
}

TEST(SDPClientResponse, RejectsTruncatedContinuationState){
    start_query();

    uint16_t transaction_id = mock_l2cap_get_last_transaction_id();
    uint8_t first_response[] = {
        SDP_ServiceSearchAttributeResponse, 0, 0, 0, 4,
        0, 0, 1, 0xaa
    };
    big_endian_store_16(first_response, 1, transaction_id);
    mock_l2cap_emit_data(0x40, first_response, sizeof(first_response));
    CHECK_EQUAL(2, mock_l2cap_get_send_prepared_count());

    transaction_id = mock_l2cap_get_last_transaction_id();
    uint8_t truncated_response[] = {
        SDP_ServiceSearchAttributeResponse, 0, 0, 0, 3,
        0, 0, 1
    };
    big_endian_store_16(truncated_response, 1, transaction_id);
    mock_l2cap_emit_data(0x40, truncated_response, sizeof(truncated_response));

    CHECK_EQUAL(1, mock_l2cap_get_disconnect_count());
    CHECK_EQUAL(2, mock_l2cap_get_send_prepared_count());
}

TEST(SDPClientResponse, RejectsAttributeValueLongerThanUint16){
    start_query();

    uint16_t transaction_id = mock_l2cap_get_last_transaction_id();
    uint8_t response[] = {
        SDP_ServiceSearchAttributeResponse, 0, 0, 0, 15,
        0, 12,
        0x35, 0x0a, 0x35, 0x08, 0x09, 0x00, 0x04, 0x37, 0xff, 0xff, 0xff, 0xff,
        0
    };
    big_endian_store_16(response, 1, transaction_id);
    mock_l2cap_emit_data(0x40, response, sizeof(response));

    CHECK_EQUAL(1, mock_l2cap_get_disconnect_count());
    CHECK_EQUAL(1, mock_l2cap_get_send_prepared_count());
}

TEST(SDPClientResponse, AcceptsPocWithEmptyNestedDES){
    static const uint8_t expected_attribute_value[] = { 0x35, 0x02, 0x35, 0x00 };

    start_query();
    emit_service_search_attribute_response(sdp_poc_empty_nested_des, sizeof(sdp_poc_empty_nested_des));

    CHECK_EQUAL(sizeof(expected_attribute_value), received_attribute_length);
    CHECK_EQUAL(sizeof(expected_attribute_value), received_attribute_events);
    MEMCMP_EQUAL(expected_attribute_value, received_attribute_value, sizeof(expected_attribute_value));
}

TEST(SDPClientResponse, RejectsPocWithTruncatedLength){
    start_query();
    emit_service_search_attribute_response(sdp_poc_truncated_length, sizeof(sdp_poc_truncated_length));

    CHECK_EQUAL(1, mock_l2cap_get_disconnect_count());
    CHECK_EQUAL(1, mock_l2cap_get_send_prepared_count());
}

TEST(SDPClientResponse, RejectsPocWithFarLength){
    start_query();
    emit_service_search_attribute_response(sdp_poc_far_length, sizeof(sdp_poc_far_length));

    CHECK_EQUAL(1, mock_l2cap_get_disconnect_count());
    CHECK_EQUAL(1, mock_l2cap_get_send_prepared_count());
}

int main(int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
