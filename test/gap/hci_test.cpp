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

TEST_GROUP(HCI){
        void setup(void){
            transport_count_packets = 0;
            next_hci_packet = 0;
            hci_init(&hci_transport_test, NULL);
            hci_simulate_working_fuzz();
            hci_setup_test_connections_fuzz();
            // register for HCI events
            mock().expectOneCall("hci_can_send_packet_now_using_packet_buffer").andReturnValue(1);
        }
        void teardown(void){
            mock().clear();
        }
};

TEST(HCI, GetSetConnectionRange){
    le_connection_parameter_range_t range;
    gap_get_connection_parameter_range(&range);
    gap_set_connection_parameter_range(&range);
}

TEST(HCI, ConnectionRangeValid){
    le_connection_parameter_range_t range = {
            1, 10,
            1, 10,
            1,10
    };
    CHECK_EQUAL( 0, gap_connection_parameter_range_included(&range, 0, 0, 0, 0));
    CHECK_EQUAL( 0, gap_connection_parameter_range_included(&range, 2, 0, 0, 0));
    CHECK_EQUAL( 0, gap_connection_parameter_range_included(&range, 2, 9, 0, 0));
    CHECK_EQUAL( 0, gap_connection_parameter_range_included(&range, 2, 9, 10, 0));
    CHECK_EQUAL( 0, gap_connection_parameter_range_included(&range, 2, 9, 5, 0));
    CHECK_EQUAL( 0, gap_connection_parameter_range_included(&range, 2, 9, 5, 11));
    CHECK_EQUAL( 1, gap_connection_parameter_range_included(&range, 2, 9, 5, 5));
}

TEST(HCI, NumPeripherals){
    gap_set_max_number_peripheral_connections(1);
}

TEST(HCI, MaxAclLen){
    hci_max_acl_data_packet_length();
}

TEST(HCI, Flushable){
    hci_non_flushable_packet_boundary_flag_supported();
}

TEST(HCI, DoubleReserve){
    CHECK_TRUE(hci_reserve_packet_buffer());
    CHECK_FALSE(hci_reserve_packet_buffer());
}

TEST(HCI, RemovePacketHandler){
    hci_remove_event_handler(NULL);
}

static void dummy_fn(const void * config){};
TEST(HCI, SetChipset){
    hci_set_chipset(NULL);
    btstack_chipset_t chipset_driver = { 0 };
    hci_set_chipset(NULL);
    chipset_driver.init = dummy_fn;
}

TEST(HCI, SetControl){
    btstack_control_t hardware_control = { .init = &dummy_fn};
    hci_set_control(&hardware_control);
}

//TEST(HCI, Close){
//    hci_close();
//}

TEST(HCI, SetPublicAddress){
    bd_addr_t addr = { 0 };
    hci_set_bd_addr(addr);
}

TEST(HCI, DisconnectSecurityBlock){
    hci_disconnect_security_block(HCI_CON_HANDLE_INVALID);
    hci_disconnect_security_block(3);
}

TEST(HCI, SetDuplicateFilter){
    gap_set_scan_duplicate_filter(true);
}

TEST(HCI, ConnectCancel){
    gap_connect_cancel();
}

TEST(HCI, SetGapConnParams){
    gap_set_connection_parameters(0, 0, 0, 0, 0, 0, 0, 0);
}

TEST(HCI, UpdateConnParams){
    gap_update_connection_parameters(HCI_CON_HANDLE_INVALID, 0, 0, 0, 0);
    gap_update_connection_parameters(5, 0, 0, 0, 0);
}

TEST(HCI, RequestConnParamUpdate){
    gap_request_connection_parameter_update(HCI_CON_HANDLE_INVALID, 0, 0, 0, 0);
    gap_request_connection_parameter_update(5, 0, 0, 0, 0);
}

TEST(HCI, SetScanResponse){
    gap_scan_response_set_data(0, NULL);
}

TEST(HCI, SetAddrType){
    hci_le_set_own_address_type(0);
    hci_le_set_own_address_type(1);
}

TEST(HCI, AdvEnable){
    gap_advertisements_enable(0);
    gap_advertisements_enable(1);
}

TEST(HCI, SetRandomAddr){
    bd_addr_t addr = { 0 };
    hci_le_random_address_set(addr);
}

TEST(HCI, Disconnect){
    gap_disconnect(HCI_CON_HANDLE_INVALID);
    gap_disconnect(5);
}

TEST(HCI, GetRole){
    gap_get_role(HCI_CON_HANDLE_INVALID);
    gap_get_role(5);
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
