/*
 * Copyright (C) 2026 BlueKitchen GmbH
 */

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "classic/avdtp_acceptor.h"
#include "classic/avdtp_util.h"
#include "btstack_event.h"

static avdtp_stream_endpoint_t test_stream_endpoint;
static btstack_linked_list_t test_stream_endpoints;
static uint8_t l2cap_sent_packet[16];
static uint16_t l2cap_sent_size;
static uint16_t l2cap_sent_cid;

extern "C" uint16_t l2cap_get_remote_mtu_for_local_cid(uint16_t local_cid){
    UNUSED(local_cid);
    return 1024;
}

extern "C" uint8_t l2cap_send(uint16_t local_cid, uint8_t *data, uint16_t len){
    l2cap_sent_cid = local_cid;
    l2cap_sent_size = len;
    memcpy(l2cap_sent_packet, data, len);
    return ERROR_CODE_SUCCESS;
}

extern "C" void l2cap_reserve_packet_buffer(void){
}

extern "C" uint8_t * l2cap_get_outgoing_buffer(void){
    static uint8_t buffer[128];
    return buffer;
}

extern "C" uint8_t l2cap_send_prepared(uint16_t local_cid, uint16_t len){
    UNUSED(local_cid);
    UNUSED(len);
    return ERROR_CODE_SUCCESS;
}

extern "C" uint8_t l2cap_request_can_send_now_event(uint16_t local_cid){
    UNUSED(local_cid);
    return ERROR_CODE_SUCCESS;
}

extern "C" void avdtp_emit_sink_and_source(uint8_t * packet, uint16_t size){
    UNUSED(packet);
    UNUSED(size);
}

extern "C" void avdtp_emit_source(uint8_t * packet, uint16_t size){
    UNUSED(packet);
    UNUSED(size);
}

extern "C" btstack_packet_handler_t avdtp_packet_handler_for_stream_endpoint(const avdtp_stream_endpoint_t * stream_endpoint){
    UNUSED(stream_endpoint);
    return NULL;
}

extern "C" btstack_linked_list_t * avdtp_get_stream_endpoints(void){
    return &test_stream_endpoints;
}

extern "C" avdtp_stream_endpoint_t * avdtp_get_stream_endpoint_for_seid(uint16_t seid){
    return test_stream_endpoint.sep.seid == seid ? &test_stream_endpoint : NULL;
}

extern "C" uint8_t avdtp_validate_media_configuration(const avdtp_stream_endpoint_t *stream_endpoint, uint16_t avdtp_cid,
                                                      uint8_t reconfigure, const adtvp_media_codec_capabilities_t *media_codec){
    UNUSED(stream_endpoint);
    UNUSED(avdtp_cid);
    UNUSED(reconfigure);
    UNUSED(media_codec);
    return ERROR_CODE_SUCCESS;
}

extern "C" uint8_t is_avdtp_remote_seid_registered(avdtp_stream_endpoint_t * stream_endpoint){
    UNUSED(stream_endpoint);
    return 0;
}

TEST_GROUP(AvdtpAcceptor){
    avdtp_connection_t connection;

    void setup(){
        memset(&connection, 0, sizeof(connection));
        memset(&test_stream_endpoint, 0, sizeof(test_stream_endpoint));
        memset(l2cap_sent_packet, 0, sizeof(l2cap_sent_packet));
        l2cap_sent_size = 0;
        l2cap_sent_cid = 0;
        test_stream_endpoints = NULL;
    }
};

TEST(AvdtpAcceptor, reject_start_response_contains_acp_seid_and_error_code){
    const uint8_t local_seid = 0x0b;
    const uint8_t transaction_label = 0x05;
    const uint16_t signaling_cid = 0x0040;

    connection.avdtp_cid = 0x1234;
    connection.l2cap_signaling_cid = signaling_cid;
    connection.acceptor_local_seid = local_seid;
    connection.acceptor_transaction_label = transaction_label;
    connection.acceptor_signaling_packet.signal_identifier = AVDTP_SI_START;

    test_stream_endpoint.connection = &connection;
    test_stream_endpoint.sep.seid = local_seid;
    test_stream_endpoint.state = AVDTP_STREAM_ENDPOINT_STREAMING;
    test_stream_endpoint.acceptor_config_state = AVDTP_ACCEPTOR_W2_REJECT_START_STREAM;

    avdtp_acceptor_stream_config_subsm_run(&connection);

    const uint8_t expected[] = {
            avdtp_header(transaction_label, AVDTP_SINGLE_PACKET, AVDTP_RESPONSE_REJECT_MSG),
            AVDTP_SI_START,
            (uint8_t)(local_seid << 2),
            AVDTP_ERROR_CODE_BAD_STATE
    };
    CHECK_EQUAL(signaling_cid, l2cap_sent_cid);
    CHECK_EQUAL(sizeof(expected), l2cap_sent_size);
    MEMCMP_EQUAL(expected, l2cap_sent_packet, sizeof(expected));
}

TEST(AvdtpAcceptor, reject_start_for_unknown_seid_contains_acp_seid_and_error_code){
    const uint8_t local_seid = 0x0c;
    const uint8_t transaction_label = 0x06;
    const uint16_t signaling_cid = 0x0041;

    connection.l2cap_signaling_cid = signaling_cid;
    connection.acceptor_local_seid = local_seid;
    connection.acceptor_transaction_label = transaction_label;
    connection.acceptor_connection_state = AVDTP_SIGNALING_CONNECTION_ACCEPTOR_W2_REJECT_ACP_SEID_WITH_ERROR_CODE;
    connection.reject_signal_identifier = AVDTP_SI_START;
    connection.error_code = AVDTP_ERROR_CODE_BAD_ACP_SEID;

    avdtp_acceptor_stream_config_subsm_run(&connection);

    const uint8_t expected[] = {
            avdtp_header(transaction_label, AVDTP_SINGLE_PACKET, AVDTP_RESPONSE_REJECT_MSG),
            AVDTP_SI_START,
            (uint8_t)(local_seid << 2),
            AVDTP_ERROR_CODE_BAD_ACP_SEID
    };
    CHECK_EQUAL(signaling_cid, l2cap_sent_cid);
    CHECK_EQUAL(sizeof(expected), l2cap_sent_size);
    MEMCMP_EQUAL(expected, l2cap_sent_packet, sizeof(expected));
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
