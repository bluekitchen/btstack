// *****************************************************************************
//
// test ANCS Client
//
// *****************************************************************************


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"
#include "CppUTestExt/MockSupport.h"

#include "bluetooth.h"
#include "bluetooth_gatt.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_memory.h"
#include "btstack_util.h"
#include "hci.h"

#include "ble/gatt-service/ancs_client.h"
#include "mock_gatt_client.h"

extern "C" void ancs_client_set_invalid_parser_state(void);

// mock hci

static const uint8_t ancs_service_uuid[] =             {0x79,0x05,0xF4,0x31,0xB5,0xCE,0x4E,0x99,0xA4,0x0F,0x4B,0x1E,0x12,0x2D,0x00,0xD0};
static const uint8_t ancs_notification_source_uuid[] = {0x9F,0xBF,0x12,0x0D,0x63,0x01,0x42,0xD9,0x8C,0x58,0x25,0xE6,0x99,0xA2,0x1D,0xBD};
static const uint8_t ancs_control_point_uuid[] =       {0x69,0xD1,0xD8,0xF3,0x45,0xE1,0x49,0xA8,0x98,0x21,0x9B,0xBD,0xFD,0xAA,0xD9,0xD9};
static const uint8_t ancs_data_source_uuid[] =         {0x22,0xEA,0xC6,0xE9,0x24,0xD6,0x4B,0xB5,0xBE,0x44,0xB3,0x6A,0xCE,0x7C,0x7B,0xFB};

static const hci_con_handle_t ancs_con_handle = 0x001;
static bool ancs_connected;

// mock sm
void sm_request_pairing(hci_con_handle_t con_handle){
}

// temp btstack run loop mock

static btstack_timer_source_t * btstack_timer = NULL;

void btstack_run_lopo_deinit(void){
    btstack_timer = NULL;
}

void btstack_run_loop_add_timer(btstack_timer_source_t * timer){
    btstack_timer = timer;
}

int btstack_run_loop_remove_timer(btstack_timer_source_t * timer){
    btstack_timer = NULL;
    return 1;
}

void btstack_run_loop_set_timer(btstack_timer_source_t * timer, uint32_t timeout_in_ms){
}

void btstack_run_loop_set_timer_handler(btstack_timer_source_t * timer, void (*process)(btstack_timer_source_t * _timer)){
    timer->process = process;
}

void btstack_run_loop_set_timer_context(btstack_timer_source_t * timer, void * context){
    timer->context = context;
}

void * btstack_run_loop_get_timer_context(btstack_timer_source_t * timer){
    return timer->context;
}

void mock_btstack_run_loop_trigger_timer(void){
    btstack_assert(btstack_timer != NULL);
    (*btstack_timer->process)(btstack_timer);
}

static void ancs_client_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(size);

    switch (hci_event_gattservice_meta_get_subevent_code(packet)){
        case ANCS_SUBEVENT_CLIENT_CONNECTED:
            ancs_connected = true;
            break;
        default:
            break;
    }
}

TEST_GROUP(ANCS_CLIENT){
    mock_gatt_client_service_t * service;
    mock_gatt_client_characteristic_t * ancs_data_source_characteristic;
    mock_gatt_client_characteristic_t * ancs_notification_source_characteristic;
    mock_gatt_client_characteristic_t * ancs_control_point_characteristic;
    mock_gatt_client_characteristic_descriptor_t * descriptor;

    uint8_t  value_buffer[3];

    void setup(void){
        ancs_connected = false;
        mock_gatt_client_reset();
        ancs_client_init();
        ancs_client_register_callback(&ancs_client_event_handler);
    }

    void setup_service(bool add_characteristics, bool add_descriptors){
        // TODO: setup ANCS Service
        service = mock_gatt_client_add_primary_service_uuid128(ancs_service_uuid);

        if (!add_characteristics) return;
        ancs_control_point_characteristic = mock_gatt_client_add_characteristic_uuid128(ancs_control_point_uuid, ATT_PROPERTY_NOTIFY);
        if (add_descriptors) {
            descriptor = mock_gatt_client_add_characteristic_descriptor_uuid16(ORG_BLUETOOTH_DESCRIPTOR_GATT_CLIENT_CHARACTERISTIC_CONFIGURATION);
        }
        ancs_data_source_characteristic = mock_gatt_client_add_characteristic_uuid128(ancs_data_source_uuid, ATT_PROPERTY_NOTIFY);
        if (add_descriptors) {
            descriptor = mock_gatt_client_add_characteristic_descriptor_uuid16(ORG_BLUETOOTH_DESCRIPTOR_GATT_CLIENT_CHARACTERISTIC_CONFIGURATION);
        }
        ancs_notification_source_characteristic = mock_gatt_client_add_characteristic_uuid128(ancs_notification_source_uuid, ATT_PROPERTY_NOTIFY);
        if (add_descriptors) {
            descriptor = mock_gatt_client_add_characteristic_descriptor_uuid16(ORG_BLUETOOTH_DESCRIPTOR_GATT_CLIENT_CHARACTERISTIC_CONFIGURATION);
        }
        // mock_gatt_client_dump_services();

    }

    void connect(void){
        // simulated connected
        bd_addr_t some_address;
        mock_hci_emit_le_connection_complete(0, some_address, ancs_con_handle, ERROR_CODE_SUCCESS);
        mock_hci_emit_connection_encrypted(ancs_con_handle, 1);
    }

    void teardown(void){
    }
};


TEST(ANCS_CLIENT, resolve_ids){
    for (int i = 0; i<6; i++){
        const char * name = ancs_client_attribute_name_for_id(i);
        CHECK_TRUE(name != NULL);
    }
    CHECK_EQUAL(NULL, ancs_client_attribute_name_for_id(6));
}

TEST(ANCS_CLIENT, ignored_events){
    // default hci event
    uint8_t some_other_event[] = { 0, 0};
    mock_hci_emit_event(some_other_event, sizeof(some_other_event));
    
    // default hci le subevent
    uint8_t some_le_event[] = { HCI_EVENT_LE_META, 1, 0};
    mock_hci_emit_event(some_le_event, sizeof(some_le_event));
    // encypted but different con handle
    mock_hci_emit_connection_encrypted(ancs_con_handle+1, 1);
    // encypted but different state
    mock_hci_emit_connection_encrypted(ancs_con_handle, 1);
    // non-encypted
    mock_hci_emit_connection_encrypted(ancs_con_handle, 0);
    // disconnected different handle
    mock_hci_emit_disconnection_complete(ancs_con_handle+1,0);
    // disconnected different handle
    mock_hci_emit_disconnection_complete(ancs_con_handle,0);
    
    // some hci gap meta subevent
    uint8_t some_gap_meta_event[] = { HCI_EVENT_META_GAP, 1, HCI_SUBEVENT_LE_CONNECTION_COMPLETE + 1};
    mock_hci_emit_event(some_gap_meta_event, sizeof(some_gap_meta_event));
}

TEST(ANCS_CLIENT, connect_no_service){
    connect();
    mock_gatt_client_run();
}

TEST(ANCS_CLIENT, connect_unexpected_event_during_service_disc){
    connect();
    mock_gatt_client_emit_dummy_event();
}

TEST(ANCS_CLIENT, connect){
    setup_service(true, true);
    connect();
    mock_gatt_client_run();
    CHECK_TRUE(ancs_connected);
}

TEST(ANCS_CLIENT, connect_extra_characteristic){
    setup_service(true, true);
    mock_gatt_client_add_characteristic_uuid16(ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL, 0);
    connect();
    mock_gatt_client_run();
}

TEST(ANCS_CLIENT, disconnect){
    setup_service(true, true);
    connect();
    mock_gatt_client_run();
    mock_hci_emit_disconnection_complete(ancs_con_handle,0);
}

TEST(ANCS_CLIENT, notification_characteristic){
    setup_service(true, true);
    connect();
    mock_gatt_client_run();
    const uint8_t data[] = {};
    mock_gatt_client_send_notification(ancs_notification_source_characteristic, data, sizeof(data));
}

TEST(ANCS_CLIENT, data_source_characteristic){
    setup_service(true, true);
    connect();
    mock_gatt_client_run();
    // simulate notification
    const uint8_t notification_data[] = {0,0,0,0,0,0,0,0};
    mock_gatt_client_send_notification(ancs_notification_source_characteristic, notification_data, sizeof(notification_data));
    // command id = 0 / CommandIDGetNotificationAttributes
    // notification uid 0x1111
    // attribute id 1
    // len 1
    // attribute id 2
    // len 0
    const uint8_t data[] = { 0, 0,0,0,0, 1, 1, 0, 'a', 2, 0, 0};
    mock_gatt_client_send_notification(ancs_data_source_characteristic, data, sizeof(data));
}

TEST(ANCS_CLIENT, data_source_characteristic_no_handler){
    ancs_client_register_callback(NULL);
    setup_service(true, true);
    connect();
    mock_gatt_client_run();
    // simulate notification
    const uint8_t notification_data[] = {0,0,0,0,0,0,0,0};
    mock_gatt_client_send_notification(ancs_notification_source_characteristic, notification_data, sizeof(notification_data));
    // command id = 0 / CommandIDGetNotificationAttributes
    // notification uid 0x1111
    // attribute id 1
    // len 1
    // attribute id 2
    // len 0
    const uint8_t data[] = { 0, 0,0,0,0, 1, 1, 0, 'a', 2, 0, 0};
    mock_gatt_client_send_notification(ancs_data_source_characteristic, data, sizeof(data));
}

TEST(ANCS_CLIENT, notifications_other){
    setup_service(true, true);
    connect();
    mock_gatt_client_run();
    const uint8_t data[] = {};
    mock_gatt_client_send_notification_with_handle(ancs_notification_source_characteristic, 0x0001, data, sizeof(data));
}

TEST(ANCS_CLIENT, indications_other){
    setup_service(true, true);
    connect();
    mock_gatt_client_run();
    const uint8_t data[] = {};
    mock_gatt_client_send_indication_with_handle(ancs_notification_source_characteristic, 0x0001, data, sizeof(data));
}

TEST(ANCS_CLIENT, notifications_invalid_parser){
    setup_service(true, true);
    ancs_client_set_invalid_parser_state();
    connect();
    mock_gatt_client_run();
    const uint8_t data[] = {1,2,3,4,5,6};
    mock_gatt_client_send_notification(ancs_data_source_characteristic, data, sizeof(data));
}

TEST(ANCS_CLIENT, connect_unexpected_events){
    printf("connect_unexpected_events\n");
    setup_service(true, true);
    connect();
    mock_gatt_client_run_once();
    mock_gatt_client_emit_dummy_event();
    mock_gatt_client_run_once();
    mock_gatt_client_emit_dummy_event();
    mock_gatt_client_run_once();
    mock_gatt_client_emit_dummy_event();
    mock_gatt_client_run_once();
    mock_gatt_client_emit_dummy_event();
}

TEST(ANCS_CLIENT, connect_unexpected_events_in_idle_state){
    printf("connect_unexpected_events\n");
    connect();
    mock_gatt_client_run_once();
    mock_gatt_client_emit_dummy_event();
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
