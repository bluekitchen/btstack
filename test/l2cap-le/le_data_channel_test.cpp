
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
#define TEST_PSM 0x1001

static bool l2cap_channel_accept_incoming;
static uint16_t initial_credits = L2CAP_LE_AUTOMATIC_CREDITS;
static uint8_t data_channel_buffer[TEST_PACKET_SIZE];
static uint16_t l2cap_cid;
static bool l2cap_channel_opened;

const uint8_t le_data_channel_conn_request_1[] = {
        0x05, 0x20, 0x12, 0x00, 0x0e, 0x00, 0x05, 0x00, 0x14, 0x01, 0x0a, 0x00, 0x01, 0x10, 0x41, 0x00,
        0x64, 0x00, 0x30, 0x00, 0xff, 0xff
};

const uint8_t le_data_channel_invalid_pdu[] = {
        0x05, 0x20, 0x12, 0x00, 0x0e, 0x00, 0x05, 0x00, 0x11, 0x01, 0x0a, 0x00, 0x01, 0x10, 0x41, 0x00,
        0x64, 0x00, 0x30, 0x00, 0xff, 0xff
};

const uint8_t le_data_channel_conn_response_1[] = {
        0x05, 0x20, 0x12, 0x00, 0x0e, 0x00, 0x05, 0x00, 0x15, 0x01, 0x0a, 0x00, 0x41, 0x00, 0x64, 0x00,
        0x30, 0x00, 0xff, 0xff, 0x00, 0x00
};

const uint8_t le_data_channel_data_1[] = {
        0x05, 0x20, 0x04, 0x00, 0x00, 0x00, 0x41, 0x00
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

static void l2cap_channel_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    uint16_t psm;
    uint16_t cid;
    switch (packet_type) {
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case L2CAP_EVENT_LE_INCOMING_CONNECTION:
                    psm = l2cap_event_le_incoming_connection_get_psm(packet);
                    cid = l2cap_event_le_incoming_connection_get_local_cid(packet);
                    if (l2cap_channel_accept_incoming){
                        l2cap_le_accept_connection(cid, data_channel_buffer, sizeof(data_channel_buffer), initial_credits);
                    } else {
                        l2cap_le_decline_connection(cid);
                    }
                    break;
                case L2CAP_EVENT_LE_CHANNEL_OPENED:
                    l2cap_channel_opened = true;
                    break;
                default:
                    break;
            }
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
        l2cap_register_packet_handler(&l2cap_channel_packet_handler);
        l2cap_register_fixed_channel(&l2cap_channel_packet_handler, L2CAP_CID_ATTRIBUTE_PROTOCOL);
        hci_dump_init(hci_dump_posix_stdout_get_instance());
        l2cap_channel_opened = false;
    }
    void teardown(void){
        l2cap_deinit();
        hci_deinit();
        btstack_memory_deinit();
        btstack_run_loop_deinit();
    }
};

TEST(L2CAP_CHANNELS, fixed_channel){
    hci_setup_test_connections_fuzz();
    // channel does not exist
    l2cap_request_can_send_fix_channel_now_event(HCI_CON_HANDLE_TEST_LE, 0x003f);
    // att
    l2cap_request_can_send_fix_channel_now_event(HCI_CON_HANDLE_TEST_LE, L2CAP_CID_ATTRIBUTE_PROTOCOL);
    //
    (void) l2cap_can_send_fixed_channel_packet_now(HCI_CON_HANDLE_TEST_LE, L2CAP_CID_ATTRIBUTE_PROTOCOL);
    // packet buffer not reserved
    log_info("ignore error in next line (calling .. without reserving buffer first");
    l2cap_send_prepared_connectionless(HCI_CON_HANDLE_TEST_LE, L2CAP_CID_ATTRIBUTE_PROTOCOL, 5);
    // packet buffer reserved
    l2cap_reserve_packet_buffer();
    l2cap_send_prepared_connectionless(HCI_CON_HANDLE_TEST_LE, L2CAP_CID_ATTRIBUTE_PROTOCOL, 5);
    //
    l2cap_send_connectionless(HCI_CON_HANDLE_TEST_LE, L2CAP_CID_ATTRIBUTE_PROTOCOL, (uint8_t *) "hallo", 5);
}

TEST(L2CAP_CHANNELS, some_functions){
    hci_setup_test_connections_fuzz();
    l2cap_reserve_packet_buffer();
    (void) l2cap_get_outgoing_buffer();
    l2cap_release_packet_buffer();
    l2cap_set_max_le_mtu(30);
    l2cap_set_max_le_mtu(30);
    l2cap_le_unregister_service(TEST_PSM);
    l2cap_le_accept_connection(0X01, NULL, 0, 0);
    l2cap_le_decline_connection(0x01);
    l2cap_le_disconnect(0x01);
}

TEST(L2CAP_CHANNELS, outgoing_no_connection){
    l2cap_le_create_channel(&l2cap_channel_packet_handler, HCI_CON_HANDLE_TEST_LE, TEST_PSM, data_channel_buffer,
                            sizeof(data_channel_buffer), L2CAP_LE_AUTOMATIC_CREDITS, LEVEL_0, &l2cap_cid);
}

TEST(L2CAP_CHANNELS, outgoing_security_1){
    hci_setup_test_connections_fuzz();
    l2cap_le_create_channel(&l2cap_channel_packet_handler, HCI_CON_HANDLE_TEST_LE, TEST_PSM, data_channel_buffer,
                            sizeof(data_channel_buffer), L2CAP_LE_AUTOMATIC_CREDITS, LEVEL_2, NULL);
    l2cap_le_create_channel(&l2cap_channel_packet_handler, HCI_CON_HANDLE_TEST_LE, TEST_PSM, data_channel_buffer,
                            sizeof(data_channel_buffer), L2CAP_LE_AUTOMATIC_CREDITS, LEVEL_2, &l2cap_cid);
}

TEST(L2CAP_CHANNELS, outgoing_1){
    hci_setup_test_connections_fuzz();
    l2cap_le_create_channel(&l2cap_channel_packet_handler, HCI_CON_HANDLE_TEST_LE, TEST_PSM, data_channel_buffer,
                            sizeof(data_channel_buffer), L2CAP_LE_AUTOMATIC_CREDITS, LEVEL_0, &l2cap_cid);
    // fix_boundary_flags(mock_hci_transport_outgoing_packet_buffer, mock_hci_transport_outgoing_packet_size);
    // print_acl("le_data_channel_conn_request_1", mock_hci_transport_outgoing_packet_buffer, mock_hci_transport_outgoing_packet_size);
    // simulate conn response
    mock_hci_transport_receive_packet(HCI_ACL_DATA_PACKET, le_data_channel_conn_response_1, sizeof(le_data_channel_conn_response_1));
    CHECK(l2cap_channel_opened);
    CHECK(hci_number_free_acl_slots_for_handle(HCI_CON_HANDLE_TEST_LE) > 0);
    bool can_send_now = l2cap_le_can_send_now(l2cap_cid);
    CHECK(can_send_now);
    l2cap_le_request_can_send_now_event(l2cap_cid);
    CHECK(hci_number_free_acl_slots_for_handle(HCI_CON_HANDLE_TEST_LE) > 0);
    l2cap_le_send_data(l2cap_cid, (uint8_t *) "hallo", 5);
    // CHECK(hci_number_free_acl_slots_for_handle(HCI_CON_HANDLE_TEST) > 0);
    // fix_boundary_flags(mock_hci_transport_outgoing_packet_buffer, mock_hci_transport_outgoing_packet_size);
    // print_acl("le_data_channel_data_1", mock_hci_transport_outgoing_packet_buffer, mock_hci_transport_outgoing_packet_size);
    // simulate data
    mock_hci_transport_receive_packet(HCI_ACL_DATA_PACKET, le_data_channel_data_1, sizeof(le_data_channel_data_1));
    l2cap_le_disconnect(l2cap_cid);
}

TEST(L2CAP_CHANNELS, incoming_1){
    hci_setup_test_connections_fuzz();
    l2cap_le_register_service(&l2cap_channel_packet_handler, TEST_PSM, LEVEL_0);
    // simulate conn request
    l2cap_channel_accept_incoming = true;
    mock_hci_transport_receive_packet(HCI_ACL_DATA_PACKET, le_data_channel_conn_request_1, sizeof(le_data_channel_conn_request_1));
    // fix_boundary_flags(mock_hci_transport_outgoing_packet_buffer, mock_hci_transport_outgoing_packet_size);
    // print_acl("le_data_channel_conn_response_1", mock_hci_transport_outgoing_packet_buffer, mock_hci_transport_outgoing_packet_size);
    // TODO: verify data
    l2cap_le_unregister_service(TEST_PSM);
}

TEST(L2CAP_CHANNELS, incoming_2){
    hci_setup_test_connections_fuzz();
    l2cap_le_register_service(&l2cap_channel_packet_handler, TEST_PSM, LEVEL_2);
    // simulate conn request
    l2cap_channel_accept_incoming = true;
    mock_hci_transport_receive_packet(HCI_ACL_DATA_PACKET, le_data_channel_conn_request_1, sizeof(le_data_channel_conn_request_1));
}

TEST(L2CAP_CHANNELS, incoming_3){
    hci_setup_test_connections_fuzz();
    l2cap_le_register_service(&l2cap_channel_packet_handler, TEST_PSM, LEVEL_0);
    // simulate conn request
    l2cap_channel_accept_incoming = true;
    mock_hci_transport_receive_packet(HCI_ACL_DATA_PACKET, le_data_channel_invalid_pdu, sizeof(le_data_channel_invalid_pdu));
}

TEST(L2CAP_CHANNELS, incoming_decline){
    hci_setup_test_connections_fuzz();
    l2cap_le_register_service(&l2cap_channel_packet_handler, TEST_PSM, LEVEL_0);
    // simulate conn request
    l2cap_channel_accept_incoming = false;
    mock_hci_transport_receive_packet(HCI_ACL_DATA_PACKET, le_data_channel_conn_request_1, sizeof(le_data_channel_conn_request_1));
    // fix_boundary_flags(mock_hci_transport_outgoing_packet_buffer, mock_hci_transport_outgoing_packet_size);
    // print_acl("le_data_channel_conn_response_1", mock_hci_transport_outgoing_packet_buffer, mock_hci_transport_outgoing_packet_size);
    // TODO: verify data
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
