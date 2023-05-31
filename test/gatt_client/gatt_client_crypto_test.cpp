
// *****************************************************************************
//
// test rfcomm query tests
//
// *****************************************************************************


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "hci_cmd.h"

#include "btstack_memory.h"
#include "btstack_event.h"

#include "hci.h"
#include "hci_dump.h"
#include "ble/gatt_client.h"
#include "ble/att_db.h"
// #include "profile.h"
#include "le-audio/gatt-service/coordinated_set_identification_service_client.h"

static uint16_t gatt_client_handle = 0x40;

typedef enum {
    IDLE,
    SIRK_CONTAINS_RSI,
    SIRK_DOES_NOT_CONTAIN_RSI
} current_test_t;

current_test_t test = IDLE;

static uint8_t sirk[] = {
    0x83, 0x8E, 0x68, 0x05, 0x53, 0xF1, 0x41, 0x5A,
    0xA2, 0x65, 0xBB, 0xAF, 0xC6, 0xEA, 0x03, 0xB8   
};


void mock_simulate_discover_primary_services_response(void);
void mock_simulate_att_exchange_mtu_response(void);

static void csis_handle_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    
    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_GATTSERVICE_META) return;

    uint8_t status;
    bd_addr_t rsi;

    switch (hci_event_gattservice_meta_get_subevent_code(packet)){
        case GATTSERVICE_SUBEVENT_CSIS_RSI_MATCH:
            // printf("GATTSERVICE_SUBEVENT_CSIS_RSI_MATCH %u\n", gattservice_subevent_csis_rsi_match_get_match(packet));
            switch(test){
                case SIRK_CONTAINS_RSI:
                    CHECK_EQUAL(1, gattservice_subevent_csis_rsi_match_get_match(packet));
                    break;
                case SIRK_DOES_NOT_CONTAIN_RSI:
                    CHECK_EQUAL(0, gattservice_subevent_csis_rsi_match_get_match(packet));
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
     }
}

TEST_GROUP(GATTClientCrypto){
    void setup(void){
        test = IDLE;
        coordinated_set_identification_service_client_init(csis_handle_client_event);
    }

    void reset_query_state(void){
    }
};

TEST(GATTClientCrypto, sirk_contains_rsi_test){
    test = SIRK_CONTAINS_RSI;
    uint8_t rsi[] = {0x6F, 0x6C, 0x99, 0x62, 0xCC, 0x86};
    coordinated_set_identification_service_client_check_rsi(rsi, sirk);
}

TEST(GATTClientCrypto, sirk_does_not_contain_rsi_test){
    test = SIRK_DOES_NOT_CONTAIN_RSI;
    uint8_t rsi[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    coordinated_set_identification_service_client_check_rsi(rsi, sirk);
}

int main (int argc, const char * argv[]){
    gatt_client_init();
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
