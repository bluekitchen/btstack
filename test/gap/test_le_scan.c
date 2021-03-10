#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"
#include "CppUTestExt/MockSupport.h"

#include "hci_cmd.h"

#include "btstack_memory.h"
#include "hci.h"
#include "ble/gatt_client.h"
#include "btstack_event.h"
#include "hci_dump.h"
#include "hci_dump_posix_fs.h"
#include "btstack_debug.h"

typedef struct {
    uint8_t type;
    uint16_t size;
    uint8_t  buffer[258];
} hci_packet_t;

#define MAX_HCI_PACKETS 10
static uint16_t transport_count_packets;
static hci_packet_t transport_packets[MAX_HCI_PACKETS];

static  void (*packet_handler)(uint8_t packet_type, uint8_t *packet, uint16_t size);

static const uint8_t packet_sent_event[] = { HCI_EVENT_TRANSPORT_PACKET_SENT, 0};

static int hci_transport_test_set_baudrate(uint32_t baudrate){
    return 0;
}

static int hci_transport_test_can_send_now(uint8_t packet_type){
    return 1;
}

static int hci_transport_test_send_packet(uint8_t packet_type, uint8_t * packet, int size){
    btstack_assert(transport_count_packets < MAX_HCI_PACKETS);
    memcpy(transport_packets[transport_count_packets].buffer, packet, size);
    transport_packets[transport_count_packets].type = packet_type;
    transport_packets[transport_count_packets].size = size;
    transport_count_packets++;
    // notify upper stack that it can send again
    packet_handler(HCI_EVENT_PACKET, (uint8_t *) &packet_sent_event[0], sizeof(packet_sent_event));
    return 0;
}

static void hci_transport_test_init(const void * transport_config){
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
        /* const char * name; */                                        "TEST",
        /* void   (*init) (const void *transport_config); */            &hci_transport_test_init,
        /* int    (*open)(void); */                                     &hci_transport_test_open,
        /* int    (*close)(void); */                                    &hci_transport_test_close,
        /* void   (*register_packet_handler)(void (*handler)(...); */   &hci_transport_test_register_packet_handler,
        /* int    (*can_send_packet_now)(uint8_t packet_type); */       &hci_transport_test_can_send_now,
        /* int    (*send_packet)(...); */                               &hci_transport_test_send_packet,
        /* int    (*set_baudrate)(uint32_t baudrate); */                &hci_transport_test_set_baudrate,
        /* void   (*reset_link)(void); */                               NULL,
        /* void   (*set_sco_config)(uint16_t voice_setting, int num_connections); */ NULL,
};

static uint16_t next_hci_packet;

void CHECK_EQUAL_ARRAY(const uint8_t * expected, uint8_t * actual, int size){
    for (int i=0; i<size; i++){
        BYTES_EQUAL(expected[i], actual[i]);
    }
}

void CHECK_HCI_COMMAND(const hci_cmd_t * expected_hci_command){
    uint16_t actual_opcode = little_endian_read_16(transport_packets[next_hci_packet].buffer, 0);
    next_hci_packet++;
    CHECK_EQUAL(expected_hci_command->opcode, actual_opcode);
}

TEST_GROUP(GAP_LE){
        void setup(void){
            transport_count_packets = 0;
            next_hci_packet = 0;
            hci_init(&hci_transport_test, NULL);
            hci_simulate_working_fuzz();
            // register for HCI events
            mock().expectOneCall("hci_can_send_packet_now_using_packet_buffer").andReturnValue(1);
        }
        void teardown(void){
            mock().clear();
        }
};

TEST(GAP_LE, ScanStart){
    log_info("TEST(GAP_LE, ScanStart)");
    gap_start_scan();
    CHECK_EQUAL(1, transport_count_packets);
    CHECK_HCI_COMMAND(&hci_le_set_scan_enable);
}
TEST(GAP_LE, ScanStartStop){
    log_info("TEST(GAP_LE, ScanStartStop)");
    gap_start_scan();
    gap_stop_scan();
    CHECK_EQUAL(2, transport_count_packets);
    CHECK_HCI_COMMAND(&hci_le_set_scan_enable);
    CHECK_HCI_COMMAND(&hci_le_set_scan_enable);
}

TEST(GAP_LE, ScanStartParam){
    log_info("TEST(GAP_LE, ScanStartParam)");
    gap_start_scan();
    gap_set_scan_parameters(0, 10, 10);
    CHECK_EQUAL(4, transport_count_packets);
    CHECK_HCI_COMMAND(&hci_le_set_scan_enable);
    CHECK_HCI_COMMAND(&hci_le_set_scan_enable);
    CHECK_HCI_COMMAND(&hci_le_set_scan_parameters);
    CHECK_HCI_COMMAND(&hci_le_set_scan_enable);
}

int main (int argc, const char * argv[]){
    // log into file using HCI_DUMP_PACKETLOGGER format
    const char * pklg_path = "hci_dump.pklg";
    hci_dump_posix_fs_open(pklg_path, HCI_DUMP_PACKETLOGGER);
    hci_dump_init(hci_dump_posix_fs_get_instance());
    printf("Packet Log: %s\n", pklg_path);

    return CommandLineTestRunner::RunAllTests(argc, argv);
}
