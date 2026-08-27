// *****************************************************************************
//
// SDP client response validation tests
//
// *****************************************************************************

#include "btstack_config.h"

#include <stdint.h>

#include "bluetooth_sdp.h"
#include "classic/sdp_client.h"
#include "classic/sdp_util.h"
#include "mock.h"

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

static bd_addr_t remote_address;

static void handle_sdp_client_event(uint8_t packet_type, uint16_t channel, uint8_t * packet, uint16_t size){
    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(packet);
    UNUSED(size);
}

static void start_query(void){
    sdp_client_query_uuid16(&handle_sdp_client_event, remote_address, 0x1101);
    mock_l2cap_emit_channel_opened(0x40);
    CHECK_EQUAL(1, mock_l2cap_get_send_prepared_count());
}

TEST_GROUP(SDPClientResponse){
    void setup(void){
        sdp_client_reset();
        mock_l2cap_reset();
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

int main(int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
