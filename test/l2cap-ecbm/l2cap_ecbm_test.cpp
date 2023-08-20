
// hal_cpu
#include "hal_cpu.h"
void hal_cpu_disable_irqs(void){}
void hal_cpu_enable_irqs(void){}
void hal_cpu_enable_irqs_and_sleep(void){}

// mock_sm.c
#include "ble/sm.h"
void sm_add_event_handler(btstack_packet_callback_registration_t * callback_handler){}
void sm_request_pairing(hci_con_handle_t con_handle){}

// mock_hci_transport.h
#include "hci_transport.h"
void mock_hci_transport_receive_packet(uint8_t packet_type, uint8_t * packet, uint16_t size);
const hci_transport_t * mock_hci_transport_mock_get_instance(void);

// mock_hci_transport.c
#include <stddef.h>
static uint8_t  mock_hci_transport_outgoing_packet_buffer[HCI_ACL_PAYLOAD_SIZE];
static uint16_t mock_hci_transport_outgoing_packet_size;
static uint8_t  mock_hci_transport_outgoing_packet_type;

static void (*mock_hci_transport_packet_handler)(uint8_t packet_type, uint8_t * packet, uint16_t size);
static void mock_hci_transport_register_packet_handler(void (*packet_handler)(uint8_t packet_type, uint8_t * packet, uint16_t size)){
    mock_hci_transport_packet_handler = packet_handler;
}
static int mock_hci_transport_send_packet(uint8_t packet_type, uint8_t *packet, int size){
    mock_hci_transport_outgoing_packet_type = packet_type;
    mock_hci_transport_outgoing_packet_size = size;
    memcpy(mock_hci_transport_outgoing_packet_buffer, packet, size);
    return 0;
}
const hci_transport_t * mock_hci_transport_mock_get_instance(void){
    static hci_transport_t mock_hci_transport = {
        /*  .transport.name                          = */  "mock",
        /*  .transport.init                          = */  NULL,
        /*  .transport.open                          = */  NULL,
        /*  .transport.close                         = */  NULL,
        /*  .transport.register_packet_handler       = */  &mock_hci_transport_register_packet_handler,
        /*  .transport.can_send_packet_now           = */  NULL,
        /*  .transport.send_packet                   = */  &mock_hci_transport_send_packet,
        /*  .transport.set_baudrate                  = */  NULL,
    };
    return &mock_hci_transport;
}
void mock_hci_transport_receive_packet(uint8_t packet_type, const uint8_t * packet, uint16_t size){
    (*mock_hci_transport_packet_handler)(packet_type, (uint8_t *) packet, size);
}

// copy from hci.c
static void mock_hci_emit_disconnection_complete(hci_con_handle_t con_handle, uint8_t reason){
    uint8_t event[6];
    event[0] = HCI_EVENT_DISCONNECTION_COMPLETE;
    event[1] = sizeof(event) - 2u;
    event[2] = 0; // status = OK
    little_endian_store_16(event, 3, con_handle);
    event[5] = reason;
    (*mock_hci_transport_packet_handler)(HCI_EVENT_PACKET, event, sizeof(event));
}

//

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"
#include "CppUTestExt/MockSupport.h"

#include "hci_dump.h"
#include "btstack_debug.h"
#include "l2cap.h"
#include "btstack_memory.h"
#include "btstack_run_loop_embedded.h"
#include "hci_dump_posix_stdout.h"
#include "btstack_event.h"

#define TEST_PACKET_SIZE       100
#define HCI_CON_HANDLE_TEST_LE 0x0005
#define HCI_CON_HANDLE_TEST_CLASSIC 0x0003
#define TEST_PSM 0x1001

static bool l2cap_channel_accept_incoming;
static uint16_t initial_credits = L2CAP_LE_AUTOMATIC_CREDITS;
static uint8_t data_channel_buffer_1[TEST_PACKET_SIZE];
static uint8_t data_channel_buffer_2[TEST_PACKET_SIZE];
static uint16_t l2cap_cids[10];
static uint16_t num_l2cap_channel_opened;
static uint16_t num_l2cap_channel_closed;
static uint8_t * receive_buffers_2[] = { data_channel_buffer_1, data_channel_buffer_2 };
static uint8_t * reconfigure_buffers_2[] = {data_channel_buffer_1, data_channel_buffer_2 };
static uint8_t  received_packet[TEST_PACKET_SIZE];
static uint16_t reconfigure_result;
static btstack_packet_callback_registration_t l2cap_event_callback_registration;

// two channels with cids 0x041 and 0x042
const uint8_t l2cap_enhanced_data_channel_le_conn_request_1[] = {
        0x05, 0x20, 0x14, 0x00, 0x10, 0x00, 0x05, 0x00, 0x17, 0x01, 0x0c, 0x00, 0x01, 0x10, 0x64, 0x00,
        0x30, 0x00, 0xff, 0xff, 0x41, 0x00, 0x42, 0x00
};

const uint8_t l2cap_enhanced_data_channel_classic_conn_request_1[] = {
        0x03, 0x20, 0x14, 0x00, 0x10, 0x00, 0x01, 0x00, 0x17, 0x01, 0x0c, 0x00, 0x01, 0x10, 0x64, 0x00,
        0x30, 0x00, 0xff, 0xff, 0x41, 0x00, 0x42, 0x00
};

const uint8_t l2cap_enhanced_data_channel_le_conn_response_2_failed[] = {
        0x05, 0x20, 0x14, 0x00, 0x10, 0x00, 0x05, 0x00, 0x18, 0x01, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00
};

const uint8_t l2cap_enhanced_data_channel_le_conn_response_2_success[] = {
        0x05, 0x20, 0x14, 0x00, 0x10, 0x00, 0x05, 0x00, 0x18, 0x01, 0x0c, 0x00, 0x64, 0x00, 0x30, 0x00,
        0xff, 0xff, 0x00, 0x00, 0x41, 0x00, 0x42, 0x00
};

const uint8_t l2cap_enhanced_data_channel_classic_conn_response_2_success[] = {
        0x03, 0x20, 0x14, 0x00, 0x10, 0x00, 0x01, 0x00, 0x18, 0x01, 0x0c, 0x00, 0x64, 0x00, 0x30, 0x00,
        0xff, 0xff, 0x00, 0x00, 0x41, 0x00, 0x42, 0x00
};

const uint8_t l2cap_enhanced_data_channel_le_credits[] = {
        0x05, 0x20, 0x0c, 0x00, 0x08, 0x00, 0x05, 0x00,  0x16, 0x02, 0x04, 0x00,  0x41, 0x00, 0x05, 0x00,
};

const uint8_t l2cap_enhanced_data_channel_classic_credits[] = {
        0x03, 0x20, 0x0c, 0x00, 0x08, 0x00, 0x01, 0x00,  0x16, 0x02, 0x04, 0x00,  0x41, 0x00, 0x05, 0x00,
};

const uint8_t l2cap_enhanced_data_channel_le_single_packet[] = {
        0x05, 0x20, 0x0b, 0x00, 0x07, 0x00, 0x41, 0x00, 0x05, 0x00, 0x68, 0x65, 0x6c, 0x6c, 0x6f
};

const uint8_t l2cap_enhanced_data_channel_classic_single_packet[] = {
        0x03, 0x20, 0x0b, 0x00, 0x07, 0x00, 0x41, 0x00, 0x05, 0x00, 0x68, 0x65, 0x6c, 0x6c, 0x6f
};

const uint8_t l2cap_enhanced_data_channel_le_disconnect_request_1[] = {
        0x05, 0x20, 0x0c, 0x00, 0x08, 0x00, 0x05, 0x00, 0x06, 0x02, 0x04, 0x00, 0x41, 0x00, 0x41, 0x00
};

const uint8_t l2cap_enhanced_data_channel_le_disconnect_response_1[] = {
        0x05, 0x20, 0x0c, 0x00, 0x08, 0x00, 0x05, 0x00, 0x07, 0x02, 0x04, 0x00, 0x41, 0x00, 0x41, 0x00
};

const uint8_t l2cap_enhanced_data_channel_le_disconnect_request_2[] = {
        0x05, 0x20, 0x0c, 0x00, 0x08, 0x00, 0x05, 0x00, 0x06, 0x03, 0x04, 0x00, 0x42, 0x00, 0x42, 0x00
};

const uint8_t l2cap_enhanced_data_channel_le_disconnect_response_2[] = {
        0x05, 0x20, 0x0c, 0x00, 0x08, 0x00, 0x05, 0x00, 0x07, 0x03, 0x04, 0x00, 0x42, 0x00, 0x42, 0x00
};

const uint8_t l2cap_enhanced_data_channel_classic_disconnect_request_1[] = {
        0x03, 0x20, 0x0c, 0x00, 0x08, 0x00, 0x01, 0x00, 0x06, 0x02, 0x04, 0x00, 0x41, 0x00, 0x41, 0x00
};

const uint8_t l2cap_enhanced_data_channel_classic_disconnect_response_1[] = {
        0x03, 0x20, 0x0c, 0x00, 0x08, 0x00, 0x01, 0x00, 0x07, 0x02, 0x04, 0x00, 0x41, 0x00, 0x41, 0x00
};

const uint8_t l2cap_enhanced_data_channel_classic_disconnect_request_2[] = {
        0x03, 0x20, 0x0c, 0x00, 0x08, 0x00, 0x01, 0x00, 0x06, 0x03, 0x04, 0x00, 0x42, 0x00, 0x42, 0x00
};

const uint8_t l2cap_enhanced_data_channel_classic_disconnect_response_2[] = {
        0x03, 0x20, 0x0c, 0x00, 0x08, 0x00, 0x01, 0x00, 0x07, 0x03, 0x04, 0x00, 0x42, 0x00, 0x42, 0x00
};

const uint8_t l2cap_enhanced_data_channel_le_renegotiate_request[] = {
        0x05, 0x20, 0x10, 0x00, 0x0c, 0x00, 0x05, 0x00, 0x19, 0x02, 0x08, 0x00, 0x64, 0x00, 0x30, 0x00,
        0x41, 0x00, 0x42, 0x00
};

const uint8_t l2cap_enhanced_data_channel_classic_renegotiate_request[] = {
        0x03, 0x20, 0x10, 0x00, 0x0c, 0x00, 0x01, 0x00, 0x19, 0x02, 0x08, 0x00, 0x64, 0x00, 0x30, 0x00,
        0x41, 0x00, 0x42, 0x00
};

const uint8_t l2cap_enhanced_data_channel_le_renegotiate_response[] = {
        0x05, 0x20, 0x0a, 0x00, 0x06, 0x00, 0x05, 0x00, 0x1a, 0x02, 0x02, 0x00, 0x00, 0x00
};

const uint8_t l2cap_enhanced_data_channel_classic_renegotiate_response[] = {
        0x03, 0x20, 0x0a, 0x00, 0x06, 0x00, 0x01, 0x00, 0x1a, 0x02, 0x02, 0x00, 0x00, 0x00
};

static void fix_boundary_flags(uint8_t * packet, uint16_t size){
    uint8_t acl_flags = packet[1] >> 4;
    if (acl_flags == 0){
        acl_flags = 2;  // first fragment
    }
    packet[1] = (packet[1] & 0x0f) | (acl_flags << 4);
}

static void print_acl(const char * name, const uint8_t * packet, uint16_t size){
    printf("const uint8_t %s[] = {", name);
    uint16_t i;
    for (i=0;i<size;i++){
        if (i != 0){
            printf(", ");
        }
        if ((i % 16) == 0){
            printf("\n    ");
        }
        printf("0x%02x", packet[i]);
    }
    printf("\n};\n");
}

// print packet for test inclusion
// fix_boundary_flags(mock_hci_transport_outgoing_packet_buffer, mock_hci_transport_outgoing_packet_size);
// print_acl("l2cap_enhanced_data_channel_le_single_packet", mock_hci_transport_outgoing_packet_buffer, mock_hci_transport_outgoing_packet_size);

static void l2cap_channel_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    uint16_t psm;
    uint16_t cid;
    uint16_t cids[5];
    switch (packet_type) {
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case L2CAP_EVENT_ECBM_INCOMING_CONNECTION:
                    printf("L2CAP_EVENT_DATA_CHANNEL_INCOMING\n");
                    cid = l2cap_event_ecbm_incoming_connection_get_local_cid(packet);
                    if (l2cap_channel_accept_incoming){
                        l2cap_ecbm_accept_channels(cid, 2, initial_credits, TEST_PACKET_SIZE, receive_buffers_2, cids);
                    } else {
                        // reject request by providing zero buffers
                        l2cap_ecbm_accept_channels(cid, 0, 0, 0,NULL, NULL);
                    }
                    break;
                case L2CAP_EVENT_ECBM_CHANNEL_OPENED:
                    l2cap_cids[num_l2cap_channel_opened] = l2cap_event_ecbm_channel_opened_get_local_cid(packet);
                    printf("L2CAP_EVENT_DATA_CHANNEL_OPENED - cid 0x%04x\n", l2cap_cids[num_l2cap_channel_opened] );
                    num_l2cap_channel_opened++;
                    break;
                case L2CAP_EVENT_ECBM_RECONFIGURATION_COMPLETE:
                    reconfigure_result = l2cap_event_ecbm_reconfiguration_complete_get_reconfigure_result(packet);
                    break;
                case L2CAP_EVENT_CHANNEL_CLOSED:
                    num_l2cap_channel_closed++;
                    break;
                default:
                    break;
            }
            break;
        case L2CAP_DATA_PACKET:
            printf("Pacekt received (%u bytes): ", size);
            printf_hexdump(packet, size);
            memcpy(received_packet, packet, size);
            break;
        default:
            break;
    }
}

TEST_GROUP(L2CAP_CHANNELS){
    const hci_transport_t * hci_transport;
    void setup(void){
        btstack_memory_init();
        btstack_run_loop_init(btstack_run_loop_embedded_get_instance());
        hci_transport = mock_hci_transport_mock_get_instance();
        hci_init(hci_transport, NULL);
        l2cap_init();
        l2cap_event_callback_registration.callback = &l2cap_channel_packet_handler;
        l2cap_add_event_handler(&l2cap_event_callback_registration);
        l2cap_register_fixed_channel(&l2cap_channel_packet_handler, L2CAP_CID_ATTRIBUTE_PROTOCOL);
        hci_dump_init(hci_dump_posix_stdout_get_instance());
        num_l2cap_channel_opened = 0;
        num_l2cap_channel_closed = 0;
        memset(received_packet, 0, sizeof(received_packet));
    }
    void teardown(void){
        l2cap_deinit();
        hci_deinit();
        btstack_memory_deinit();
        btstack_run_loop_deinit();
    }
};

TEST(L2CAP_CHANNELS, no_connection){
    uint16_t cids[2];
    uint8_t status = l2cap_ecbm_create_channels(&l2cap_channel_packet_handler, HCI_CON_HANDLE_TEST_LE, LEVEL_0, TEST_PSM,
                                                    2, initial_credits, TEST_PACKET_SIZE, receive_buffers_2, cids);
    CHECK_EQUAL(ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, status);
}

TEST(L2CAP_CHANNELS, outgoing_le_2_success){
    hci_setup_test_connections_fuzz();
    uint16_t cids[2];
    uint8_t status = l2cap_ecbm_create_channels(&l2cap_channel_packet_handler, HCI_CON_HANDLE_TEST_LE, LEVEL_0, TEST_PSM,
                                                    2, initial_credits, TEST_PACKET_SIZE, receive_buffers_2, cids);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);
    mock_hci_transport_receive_packet(HCI_ACL_DATA_PACKET, l2cap_enhanced_data_channel_le_conn_response_2_success, sizeof(l2cap_enhanced_data_channel_le_conn_response_2_success));
    CHECK_EQUAL(2, num_l2cap_channel_opened);
}

TEST(L2CAP_CHANNELS, outgoing_classic_2_success){
    hci_setup_test_connections_fuzz();
    uint16_t cids[2];
    uint8_t status = l2cap_ecbm_create_channels(&l2cap_channel_packet_handler, HCI_CON_HANDLE_TEST_CLASSIC, LEVEL_0, TEST_PSM,
                                                    2, initial_credits, TEST_PACKET_SIZE, receive_buffers_2, cids);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);
    mock_hci_transport_receive_packet(HCI_ACL_DATA_PACKET, l2cap_enhanced_data_channel_classic_conn_response_2_success, sizeof(l2cap_enhanced_data_channel_classic_conn_response_2_success));
    CHECK_EQUAL(2, num_l2cap_channel_opened);
}

TEST(L2CAP_CHANNELS, incoming_no_service){
    hci_setup_test_connections_fuzz();
    mock_hci_transport_receive_packet(HCI_ACL_DATA_PACKET, l2cap_enhanced_data_channel_le_conn_request_1, sizeof(l2cap_enhanced_data_channel_le_conn_request_1));
}

TEST(L2CAP_CHANNELS, incoming_classic_no_service){
    hci_setup_test_connections_fuzz();
    mock_hci_transport_receive_packet(HCI_ACL_DATA_PACKET, l2cap_enhanced_data_channel_classic_conn_request_1, sizeof(l2cap_enhanced_data_channel_classic_conn_request_1));
}

TEST(L2CAP_CHANNELS, incoming_1){
    hci_setup_test_connections_fuzz();
    l2cap_ecbm_register_service(&l2cap_channel_packet_handler, TEST_PSM, TEST_PACKET_SIZE, LEVEL_0, false);
    l2cap_channel_accept_incoming = false;
    mock_hci_transport_receive_packet(HCI_ACL_DATA_PACKET, l2cap_enhanced_data_channel_le_conn_request_1, sizeof(l2cap_enhanced_data_channel_le_conn_request_1));
    CHECK_EQUAL(0, num_l2cap_channel_opened);
}

TEST(L2CAP_CHANNELS, incoming_2){
    hci_setup_test_connections_fuzz();
    l2cap_ecbm_register_service(&l2cap_channel_packet_handler, TEST_PSM, TEST_PACKET_SIZE, LEVEL_0, false);
    l2cap_channel_accept_incoming = true;
    mock_hci_transport_receive_packet(HCI_ACL_DATA_PACKET, l2cap_enhanced_data_channel_le_conn_request_1, sizeof(l2cap_enhanced_data_channel_le_conn_request_1));
    CHECK_EQUAL(2, num_l2cap_channel_opened);
}

TEST(L2CAP_CHANNELS, outgoing_le_receive_credits){
    hci_setup_test_connections_fuzz();
    uint16_t cids[2];
    uint8_t status = l2cap_ecbm_create_channels(&l2cap_channel_packet_handler, HCI_CON_HANDLE_TEST_LE, LEVEL_0, TEST_PSM,
                                                    2, initial_credits, TEST_PACKET_SIZE, receive_buffers_2, cids);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);
    mock_hci_transport_receive_packet(HCI_ACL_DATA_PACKET, l2cap_enhanced_data_channel_le_conn_response_2_success, sizeof(l2cap_enhanced_data_channel_le_conn_response_2_success));
    CHECK_EQUAL(2, num_l2cap_channel_opened);
    mock_hci_transport_receive_packet(HCI_ACL_DATA_PACKET, l2cap_enhanced_data_channel_le_credits, sizeof(l2cap_enhanced_data_channel_le_credits));
}

TEST(L2CAP_CHANNELS, outgoing_classic_receive_credits){
    hci_setup_test_connections_fuzz();
    uint16_t cids[2];
    uint8_t status = l2cap_ecbm_create_channels(&l2cap_channel_packet_handler, HCI_CON_HANDLE_TEST_CLASSIC, LEVEL_0, TEST_PSM,
                                                    2, initial_credits, TEST_PACKET_SIZE, receive_buffers_2, cids);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);
    mock_hci_transport_receive_packet(HCI_ACL_DATA_PACKET, l2cap_enhanced_data_channel_classic_conn_response_2_success, sizeof(l2cap_enhanced_data_channel_classic_conn_response_2_success));
    CHECK_EQUAL(2, num_l2cap_channel_opened);
    mock_hci_transport_receive_packet(HCI_ACL_DATA_PACKET, l2cap_enhanced_data_channel_classic_credits, sizeof(l2cap_enhanced_data_channel_classic_credits));
}

TEST(L2CAP_CHANNELS, outgoing_le_send_packet){
    hci_setup_test_connections_fuzz();
    uint16_t cids[2];
    uint8_t status = l2cap_ecbm_create_channels(&l2cap_channel_packet_handler, HCI_CON_HANDLE_TEST_LE, LEVEL_0, TEST_PSM,
                                                    2, initial_credits, TEST_PACKET_SIZE, receive_buffers_2, cids);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);
    mock_hci_transport_receive_packet(HCI_ACL_DATA_PACKET, l2cap_enhanced_data_channel_le_conn_response_2_success, sizeof(l2cap_enhanced_data_channel_le_conn_response_2_success));
    CHECK_EQUAL(2, num_l2cap_channel_opened);
    mock_hci_transport_receive_packet(HCI_ACL_DATA_PACKET, l2cap_enhanced_data_channel_le_credits, sizeof(l2cap_enhanced_data_channel_le_credits));
    status = l2cap_send(l2cap_cids[0], (const uint8_t *) "hello", 5);
}

TEST(L2CAP_CHANNELS, outgoing_classic_send_packet){
    hci_setup_test_connections_fuzz();
    uint16_t cids[2];
    uint8_t status = l2cap_ecbm_create_channels(&l2cap_channel_packet_handler, HCI_CON_HANDLE_TEST_CLASSIC, LEVEL_0, TEST_PSM,
                                                    2, initial_credits, TEST_PACKET_SIZE, receive_buffers_2, cids);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);
    mock_hci_transport_receive_packet(HCI_ACL_DATA_PACKET, l2cap_enhanced_data_channel_classic_conn_response_2_success, sizeof(l2cap_enhanced_data_channel_classic_conn_response_2_success));
    CHECK_EQUAL(2, num_l2cap_channel_opened);
    mock_hci_transport_receive_packet(HCI_ACL_DATA_PACKET, l2cap_enhanced_data_channel_classic_credits, sizeof(l2cap_enhanced_data_channel_classic_credits));
    status = l2cap_send(l2cap_cids[0], (const uint8_t *) "hello", 5);
}

TEST(L2CAP_CHANNELS, outgoing_le_receive_packet){
    hci_setup_test_connections_fuzz();
    uint16_t cids[2];
    uint8_t status = l2cap_ecbm_create_channels(&l2cap_channel_packet_handler, HCI_CON_HANDLE_TEST_LE, LEVEL_0, TEST_PSM,
                                                    2, initial_credits, TEST_PACKET_SIZE, receive_buffers_2, cids);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);
    mock_hci_transport_receive_packet(HCI_ACL_DATA_PACKET, l2cap_enhanced_data_channel_le_conn_response_2_success, sizeof(l2cap_enhanced_data_channel_le_conn_response_2_success));
    CHECK_EQUAL(2, num_l2cap_channel_opened);
    mock_hci_transport_receive_packet(HCI_ACL_DATA_PACKET, l2cap_enhanced_data_channel_le_single_packet, sizeof(l2cap_enhanced_data_channel_le_single_packet));
    MEMCMP_EQUAL("hello", received_packet, 5);
}

TEST(L2CAP_CHANNELS, outgoing_classic_receive_packet){
    hci_setup_test_connections_fuzz();
    uint16_t cids[2];
    uint8_t status = l2cap_ecbm_create_channels(&l2cap_channel_packet_handler, HCI_CON_HANDLE_TEST_CLASSIC, LEVEL_0, TEST_PSM,
                                                    2, initial_credits, TEST_PACKET_SIZE, receive_buffers_2, cids);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);
    mock_hci_transport_receive_packet(HCI_ACL_DATA_PACKET, l2cap_enhanced_data_channel_classic_conn_response_2_success, sizeof(l2cap_enhanced_data_channel_classic_conn_response_2_success));
    CHECK_EQUAL(2, num_l2cap_channel_opened);
    mock_hci_transport_receive_packet(HCI_ACL_DATA_PACKET, l2cap_enhanced_data_channel_classic_single_packet, sizeof(l2cap_enhanced_data_channel_classic_single_packet));
    MEMCMP_EQUAL("hello", received_packet, 5);
}

TEST(L2CAP_CHANNELS, outgoing_le_provide_credits){
    hci_setup_test_connections_fuzz();
    uint16_t cids[2];
    uint8_t status = l2cap_ecbm_create_channels(&l2cap_channel_packet_handler, HCI_CON_HANDLE_TEST_LE, LEVEL_0, TEST_PSM,
                                                    2, 0, TEST_PACKET_SIZE, receive_buffers_2, cids);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);
    mock_hci_transport_receive_packet(HCI_ACL_DATA_PACKET, l2cap_enhanced_data_channel_le_conn_response_2_success, sizeof(l2cap_enhanced_data_channel_le_conn_response_2_success));
    CHECK_EQUAL(2, num_l2cap_channel_opened);
    status = l2cap_ecbm_provide_credits(l2cap_cids[0], 5);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);
}

TEST(L2CAP_CHANNELS, outgoing_classic_provide_credits){
    hci_setup_test_connections_fuzz();
    uint16_t cids[2];
    uint8_t status = l2cap_ecbm_create_channels(&l2cap_channel_packet_handler, HCI_CON_HANDLE_TEST_CLASSIC, LEVEL_0, TEST_PSM,
                                                    2, 0, TEST_PACKET_SIZE, receive_buffers_2, cids);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);
    mock_hci_transport_receive_packet(HCI_ACL_DATA_PACKET, l2cap_enhanced_data_channel_classic_conn_response_2_success, sizeof(l2cap_enhanced_data_channel_classic_conn_response_2_success));
    CHECK_EQUAL(2, num_l2cap_channel_opened);
    status = l2cap_ecbm_provide_credits(l2cap_cids[0], 5);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);
}

TEST(L2CAP_CHANNELS, outgoing_le_renegotiate_initiator){
    hci_setup_test_connections_fuzz();
    reconfigure_result = 0xffff;
    uint16_t cids[2];
    uint8_t status = l2cap_ecbm_create_channels(&l2cap_channel_packet_handler, HCI_CON_HANDLE_TEST_LE, LEVEL_0, TEST_PSM,
                                                    2, 0, TEST_PACKET_SIZE, receive_buffers_2, cids);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);
    mock_hci_transport_receive_packet(HCI_ACL_DATA_PACKET, l2cap_enhanced_data_channel_le_conn_response_2_success, sizeof(l2cap_enhanced_data_channel_le_conn_response_2_success));
    CHECK_EQUAL(2, num_l2cap_channel_opened);
    status = l2cap_ecbm_reconfigure_channels(2, l2cap_cids, TEST_PACKET_SIZE/2, reconfigure_buffers_2);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);
    mock_hci_transport_receive_packet(HCI_ACL_DATA_PACKET, l2cap_enhanced_data_channel_le_renegotiate_response, sizeof(l2cap_enhanced_data_channel_le_renegotiate_response));
    CHECK_EQUAL(0, reconfigure_result);
}

TEST(L2CAP_CHANNELS, outgoing_classic_renegotiate_initiator){
    hci_setup_test_connections_fuzz();
    reconfigure_result = 0xffff;
    uint16_t cids[2];
    uint8_t status = l2cap_ecbm_create_channels(&l2cap_channel_packet_handler, HCI_CON_HANDLE_TEST_CLASSIC, LEVEL_0, TEST_PSM,
                                                    2, 0, TEST_PACKET_SIZE, receive_buffers_2, cids);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);
    mock_hci_transport_receive_packet(HCI_ACL_DATA_PACKET, l2cap_enhanced_data_channel_classic_conn_response_2_success, sizeof(l2cap_enhanced_data_channel_classic_conn_response_2_success));
    CHECK_EQUAL(2, num_l2cap_channel_opened);
    status = l2cap_ecbm_reconfigure_channels(2, l2cap_cids, TEST_PACKET_SIZE/2, reconfigure_buffers_2);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);
    mock_hci_transport_receive_packet(HCI_ACL_DATA_PACKET, l2cap_enhanced_data_channel_classic_renegotiate_response, sizeof(l2cap_enhanced_data_channel_classic_renegotiate_response));
    CHECK_EQUAL(0, reconfigure_result);
}

TEST(L2CAP_CHANNELS, outgoing_le_renegotiate_responder){
    hci_setup_test_connections_fuzz();
    uint16_t cids[2];
    uint8_t status = l2cap_ecbm_create_channels(&l2cap_channel_packet_handler, HCI_CON_HANDLE_TEST_LE, LEVEL_0, TEST_PSM,
                                                    2, 0, TEST_PACKET_SIZE, receive_buffers_2, cids);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);
    mock_hci_transport_receive_packet(HCI_ACL_DATA_PACKET, l2cap_enhanced_data_channel_le_conn_response_2_success, sizeof(l2cap_enhanced_data_channel_le_conn_response_2_success));
    CHECK_EQUAL(2, num_l2cap_channel_opened);
    mock_hci_transport_receive_packet(HCI_ACL_DATA_PACKET, l2cap_enhanced_data_channel_le_renegotiate_request, sizeof(l2cap_enhanced_data_channel_le_renegotiate_request));
    fix_boundary_flags(mock_hci_transport_outgoing_packet_buffer, mock_hci_transport_outgoing_packet_size);
    print_acl("l2cap_enhanced_data_channel_le_renegotiate_response", mock_hci_transport_outgoing_packet_buffer, mock_hci_transport_outgoing_packet_size);
}

TEST(L2CAP_CHANNELS, outgoing_classic_renegotiate_responder){
    hci_setup_test_connections_fuzz();
    uint16_t cids[2];
    uint8_t status = l2cap_ecbm_create_channels(&l2cap_channel_packet_handler, HCI_CON_HANDLE_TEST_CLASSIC, LEVEL_0, TEST_PSM,
                                                    2, 0, TEST_PACKET_SIZE, receive_buffers_2, cids);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);
    mock_hci_transport_receive_packet(HCI_ACL_DATA_PACKET, l2cap_enhanced_data_channel_classic_conn_response_2_success, sizeof(l2cap_enhanced_data_channel_classic_conn_response_2_success));
    CHECK_EQUAL(2, num_l2cap_channel_opened);
    mock_hci_transport_receive_packet(HCI_ACL_DATA_PACKET, l2cap_enhanced_data_channel_classic_renegotiate_request, sizeof(l2cap_enhanced_data_channel_classic_renegotiate_request));
    fix_boundary_flags(mock_hci_transport_outgoing_packet_buffer, mock_hci_transport_outgoing_packet_size);
    print_acl("l2cap_enhanced_data_channel_classic_renegotiate_response", mock_hci_transport_outgoing_packet_buffer, mock_hci_transport_outgoing_packet_size);
}

TEST(L2CAP_CHANNELS, outgoing_le_disconnect_initiator){
    hci_setup_test_connections_fuzz();
    uint16_t cids[2];
    uint8_t status = l2cap_ecbm_create_channels(&l2cap_channel_packet_handler, HCI_CON_HANDLE_TEST_LE, LEVEL_0, TEST_PSM,
                                                    2, 0, TEST_PACKET_SIZE, receive_buffers_2, cids);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);
    mock_hci_transport_receive_packet(HCI_ACL_DATA_PACKET, l2cap_enhanced_data_channel_le_conn_response_2_success, sizeof(l2cap_enhanced_data_channel_le_conn_response_2_success));
    CHECK_EQUAL(2, num_l2cap_channel_opened);
    l2cap_disconnect(l2cap_cids[0]);
    l2cap_disconnect(l2cap_cids[1]);
    mock_hci_transport_receive_packet(HCI_ACL_DATA_PACKET, l2cap_enhanced_data_channel_le_disconnect_response_1, sizeof(l2cap_enhanced_data_channel_le_disconnect_response_1));
    mock_hci_transport_receive_packet(HCI_ACL_DATA_PACKET, l2cap_enhanced_data_channel_le_disconnect_response_2, sizeof(l2cap_enhanced_data_channel_le_disconnect_response_2));
    CHECK_EQUAL(2, num_l2cap_channel_closed);
}

TEST(L2CAP_CHANNELS, outgoing_le_disconnect_responder){
    hci_setup_test_connections_fuzz();
    uint16_t cids[2];
    uint8_t status = l2cap_ecbm_create_channels(&l2cap_channel_packet_handler, HCI_CON_HANDLE_TEST_LE, LEVEL_0, TEST_PSM,
                                                    2, 0, TEST_PACKET_SIZE, receive_buffers_2, cids);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);
    mock_hci_transport_receive_packet(HCI_ACL_DATA_PACKET, l2cap_enhanced_data_channel_le_conn_response_2_success, sizeof(l2cap_enhanced_data_channel_le_conn_response_2_success));
    CHECK_EQUAL(2, num_l2cap_channel_opened);
    mock_hci_transport_receive_packet(HCI_ACL_DATA_PACKET, l2cap_enhanced_data_channel_le_disconnect_request_1, sizeof(l2cap_enhanced_data_channel_le_disconnect_request_1));
    mock_hci_transport_receive_packet(HCI_ACL_DATA_PACKET, l2cap_enhanced_data_channel_le_disconnect_request_2, sizeof(l2cap_enhanced_data_channel_le_disconnect_request_2));
    CHECK_EQUAL(2, num_l2cap_channel_closed);
}

TEST(L2CAP_CHANNELS, outgoing_classic_disconnect_initiator){
    hci_setup_test_connections_fuzz();
    uint16_t cids[2];
    uint8_t status = l2cap_ecbm_create_channels(&l2cap_channel_packet_handler, HCI_CON_HANDLE_TEST_CLASSIC, LEVEL_0, TEST_PSM,
                                                    2, 0, TEST_PACKET_SIZE, receive_buffers_2, cids);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);
    mock_hci_transport_receive_packet(HCI_ACL_DATA_PACKET, l2cap_enhanced_data_channel_classic_conn_response_2_success, sizeof(l2cap_enhanced_data_channel_classic_conn_response_2_success));
    CHECK_EQUAL(2, num_l2cap_channel_opened);
    l2cap_disconnect(l2cap_cids[0]);
    l2cap_disconnect(l2cap_cids[1]);
    mock_hci_transport_receive_packet(HCI_ACL_DATA_PACKET, l2cap_enhanced_data_channel_classic_disconnect_response_1, sizeof(l2cap_enhanced_data_channel_classic_disconnect_response_1));
    mock_hci_transport_receive_packet(HCI_ACL_DATA_PACKET, l2cap_enhanced_data_channel_classic_disconnect_response_2, sizeof(l2cap_enhanced_data_channel_classic_disconnect_response_2));
    CHECK_EQUAL(2, num_l2cap_channel_closed);
}

TEST(L2CAP_CHANNELS, outgoing_classic_disconnect_responder){
    hci_setup_test_connections_fuzz();
    uint16_t cids[2];
    uint8_t status = l2cap_ecbm_create_channels(&l2cap_channel_packet_handler, HCI_CON_HANDLE_TEST_CLASSIC, LEVEL_0, TEST_PSM,
                                                    2, 0, TEST_PACKET_SIZE, receive_buffers_2, cids);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);
    mock_hci_transport_receive_packet(HCI_ACL_DATA_PACKET, l2cap_enhanced_data_channel_classic_conn_response_2_success, sizeof(l2cap_enhanced_data_channel_classic_conn_response_2_success));
    CHECK_EQUAL(2, num_l2cap_channel_opened);
    mock_hci_transport_receive_packet(HCI_ACL_DATA_PACKET, l2cap_enhanced_data_channel_classic_disconnect_request_1, sizeof(l2cap_enhanced_data_channel_classic_disconnect_request_1));
    mock_hci_transport_receive_packet(HCI_ACL_DATA_PACKET, l2cap_enhanced_data_channel_classic_disconnect_request_2, sizeof(l2cap_enhanced_data_channel_classic_disconnect_request_2));
    CHECK_EQUAL(2, num_l2cap_channel_closed);
}

TEST(L2CAP_CHANNELS, outgoing_le_hci_disconnect){
    hci_setup_test_connections_fuzz();
    uint16_t cids[2];
    uint8_t status = l2cap_ecbm_create_channels(&l2cap_channel_packet_handler, HCI_CON_HANDLE_TEST_LE, LEVEL_0, TEST_PSM,
                                                    2, 0, TEST_PACKET_SIZE, receive_buffers_2, cids);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);
    mock_hci_transport_receive_packet(HCI_ACL_DATA_PACKET, l2cap_enhanced_data_channel_le_conn_response_2_success, sizeof(l2cap_enhanced_data_channel_le_conn_response_2_success));
    CHECK_EQUAL(2, num_l2cap_channel_opened);
    mock_hci_emit_disconnection_complete(HCI_CON_HANDLE_TEST_LE, 0x13);
    CHECK_EQUAL(2, num_l2cap_channel_closed);
}

TEST(L2CAP_CHANNELS, outgoing_classic_hci_disconnect){
    hci_setup_test_connections_fuzz();
    uint16_t cids[2];
    uint8_t status = l2cap_ecbm_create_channels(&l2cap_channel_packet_handler, HCI_CON_HANDLE_TEST_CLASSIC, LEVEL_0, TEST_PSM,
                                                    2, 0, TEST_PACKET_SIZE, receive_buffers_2, cids);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);
    mock_hci_transport_receive_packet(HCI_ACL_DATA_PACKET, l2cap_enhanced_data_channel_classic_conn_response_2_success, sizeof(l2cap_enhanced_data_channel_classic_conn_response_2_success));
    CHECK_EQUAL(2, num_l2cap_channel_opened);
    mock_hci_emit_disconnection_complete(HCI_CON_HANDLE_TEST_CLASSIC, 0x13);
    CHECK_EQUAL(2, num_l2cap_channel_closed);
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
