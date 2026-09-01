#include <stdint.h>

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"
#include "CppUTestExt/MockSupport.h"

#include "btstack_run_loop.h"
#include "btstack_run_loop_posix.h"
#include "hci.h"

static void (*packet_handler)(uint8_t packet_type, uint8_t *packet, uint16_t size);

static int hci_transport_test_send_packet(uint8_t packet_type, uint8_t *packet, int size){
    UNUSED(packet_type);
    UNUSED(packet);
    UNUSED(size);
    return 0;
}

static void hci_transport_test_init(const void *transport_config){
    UNUSED(transport_config);
}

static int hci_transport_test_open(void){
    return 0;
}

static int hci_transport_test_close(void){
    return 0;
}

static void hci_transport_test_register_packet_handler(void (*handler)(uint8_t packet_type, uint8_t *packet, uint16_t size)){
    packet_handler = handler;
}

static const hci_transport_t hci_transport_test = {
        "TEST",
        &hci_transport_test_init,
        &hci_transport_test_open,
        &hci_transport_test_close,
        &hci_transport_test_register_packet_handler,
        NULL,
        &hci_transport_test_send_packet,
        NULL,
        NULL,
        NULL,
};

TEST_GROUP(HCI_CLASSIC){
    hci_stack_t *hci_stack;

    void setup(void){
        hci_init(&hci_transport_test, NULL);
        hci_stack = hci_get_stack();
        hci_simulate_working_fuzz();
        hci_setup_test_connections_fuzz();
        mock().expectOneCall("hci_can_send_packet_now_using_packet_buffer").andReturnValue(1);
    }

    void teardown(void){
        mock().clear();
    }
};

TEST(HCI_CLASSIC, sco_implicit_flow_control_accumulates_received_payload_bytes){
    hci_connection_t *conn = hci_connection_for_handle(0x0004);
    CHECK(conn != NULL);

    hci_stack->synchronous_flow_control_enabled = 0;
    hci_stack->sco_packets_total_num = 10;
    hci_stack->sco_data_packet_length = 60;
    conn->sco_payload_length = 60;
    conn->sco_voice_setting = 0;
    conn->sco_tx_active = 1;
    conn->sco_tx_ready = 0;

    uint8_t packet[3 + 24] = { 0x04, 0x00, 24 };
    packet_handler(HCI_SCO_DATA_PACKET, packet, sizeof(packet));
    CHECK_EQUAL(24, conn->sco_tx_ready);
    CHECK_FALSE(hci_can_send_sco_packet_now_for_con_handle(conn->con_handle));

    packet_handler(HCI_SCO_DATA_PACKET, packet, sizeof(packet));
    CHECK_EQUAL(48, conn->sco_tx_ready);
    CHECK_FALSE(hci_can_send_sco_packet_now_for_con_handle(conn->con_handle));

    packet_handler(HCI_SCO_DATA_PACKET, packet, sizeof(packet));
    CHECK_EQUAL(72, conn->sco_tx_ready);
    CHECK_TRUE(hci_can_send_sco_packet_now_for_con_handle(conn->con_handle));
}

int main(int argc, const char *argv[]){
    btstack_run_loop_init(btstack_run_loop_posix_get_instance());
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
