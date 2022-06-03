
// *****************************************************************************
//
// test Battery Service Client
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

#include "ble/gatt-service/battery_service_client.h"
#include "mock_gatt_client.h"

static const hci_con_handle_t con_handle = 0x01;
static bool connected;
static uint8_t num_instances = 0;
// temp btstack run loop mock

static btstack_timer_source_t * btstack_timer = NULL;
static uint8_t  battery_level[10];
static uint16_t battery_level_size;

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

void hci_add_event_handler(btstack_packet_callback_registration_t * callback_handler){
    (void)callback_handler;
}

// simulate  btstack_memory_battery_service_client_get

static bool mock_btstack_memory_battery_service_client_no_memory;
static battery_service_client_t * mock_btstack_memory_battery_service_client_last_alloc;

void btstack_memory_init(void){
    mock_btstack_memory_battery_service_client_no_memory = false;
    mock_btstack_memory_battery_service_client_last_alloc = NULL;
}

void mock_btstack_memory_battery_service_client_simulate_no_memory(void){
    mock_btstack_memory_battery_service_client_no_memory = true;
}

battery_service_client_t * mock_btstack_memory_battery_service_client_get_last_alloc(void){
    btstack_assert(mock_btstack_memory_battery_service_client_last_alloc != NULL);
    return mock_btstack_memory_battery_service_client_last_alloc;
}

battery_service_client_t * btstack_memory_battery_service_client_get(void){
    if (mock_btstack_memory_battery_service_client_no_memory){
        return NULL;
    }
    mock_btstack_memory_battery_service_client_last_alloc =  (battery_service_client_t *) malloc(sizeof(battery_service_client_t));
    memset(mock_btstack_memory_battery_service_client_last_alloc, 0, sizeof(battery_service_client_t));
    return mock_btstack_memory_battery_service_client_last_alloc;
}

void btstack_memory_battery_service_client_free(battery_service_client_t *battery_service_client){
    free(battery_service_client);
}

static void gatt_client_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(size);

    uint8_t status;
    uint8_t att_status;

    if (hci_event_packet_get_type(packet) != HCI_EVENT_GATTSERVICE_META){
        return;
    }
    
    switch (hci_event_gattservice_meta_get_subevent_code(packet)){
        case GATTSERVICE_SUBEVENT_BATTERY_SERVICE_CONNECTED:
            status = gattservice_subevent_battery_service_connected_get_status(packet);
            switch (status){
                case ERROR_CODE_SUCCESS:
                    num_instances = gattservice_subevent_battery_service_connected_get_num_instances(packet);
                    connected = true;
                    break;
                default:
                    connected = false;
                    break;
            }
            break;

        case GATTSERVICE_SUBEVENT_BATTERY_SERVICE_LEVEL:
            att_status = gattservice_subevent_battery_service_level_get_att_status(packet);
            if (att_status != ATT_ERROR_SUCCESS){
                // printf("Battery level read failed, ATT Error 0x%02x\n", att_status);
                break;
            } 
            
            // printf("Battery level 0x%02x\n", gattservice_subevent_battery_service_level_get_level(packet));
            CHECK_EQUAL(battery_level[0], gattservice_subevent_battery_service_level_get_level(packet));
            CHECK_EQUAL(0, gattservice_subevent_battery_service_level_get_sevice_index(packet));
            break;

        default:
            break;
    }
}

TEST_GROUP(BATTERY_SERVICE_CLIENT){ 
    uint16_t battery_service_cid;
    uint32_t poll_interval_ms;
    mock_gatt_client_service_t * service;
    mock_gatt_client_characteristic_t * characteristic;
    mock_gatt_client_characteristic_descriptor_t * descriptor;

    uint8_t  value_buffer[3];

    void setup(void){
        battery_service_cid = 1;
        connected = false;
        poll_interval_ms = 2000;
        
        uint16_t i;
        for (i = 0; i < sizeof(value_buffer); i++){
            value_buffer[i] = (i+1)*11;
        }

        btstack_memory_init();
        mock_gatt_client_reset();
        battery_service_client_init();
    }

    void set_battery_level_of_size(uint16_t value_length){
        battery_level_size = btstack_min(value_length, sizeof(battery_level));
        uint8_t i;
        for (i=0; i<battery_level_size; i++){
            battery_level[i] = i+1;
        }
        mock_gatt_client_set_characteristic_value(characteristic, battery_level, battery_level_size);
    }

    void setup_service(bool add_characteristics, bool add_descriptors){
        service = mock_gatt_client_add_primary_service_uuid16(ORG_BLUETOOTH_SERVICE_BATTERY_SERVICE);
        if (!add_characteristics) return;

        characteristic = mock_gatt_client_add_characteristic_uuid16(ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL, ATT_PROPERTY_NOTIFY);
        
        if (!add_descriptors) return;
        descriptor = mock_gatt_client_add_characteristic_descriptor_uuid16(ORG_BLUETOOTH_DESCRIPTOR_GATT_CLIENT_CHARACTERISTIC_CONFIGURATION);
        mock_gatt_client_set_descriptor_characteristic_value(descriptor, value_buffer, sizeof(value_buffer));

        // mock_gatt_client_dump_services();
    }

    void setup_service_without_notify_capabality(bool add_characteristics, bool add_descriptors){
        service = mock_gatt_client_add_primary_service_uuid16(ORG_BLUETOOTH_SERVICE_BATTERY_SERVICE);
        if (!add_characteristics) return;

        characteristic = mock_gatt_client_add_characteristic_uuid16(ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL, ATT_PROPERTY_READ);
        
        if (!add_descriptors) return;
        descriptor = mock_gatt_client_add_characteristic_descriptor_uuid16(ORG_BLUETOOTH_DESCRIPTOR_GATT_CLIENT_CHARACTERISTIC_CONFIGURATION);
        mock_gatt_client_set_descriptor_characteristic_value(descriptor, value_buffer, sizeof(value_buffer));

        // mock_gatt_client_dump_services();
    }

    void connect(void){
        uint8_t status = battery_service_client_connect(con_handle, &gatt_client_event_handler, poll_interval_ms, &battery_service_cid);
        CHECK_EQUAL(ERROR_CODE_SUCCESS, status);
        mock_gatt_client_run();
    }

    void teardown(void){
        battery_service_client_deinit();
    }
};

TEST(BATTERY_SERVICE_CLIENT, unhandled_gatt_event){
    mock_gatt_client_emit_dummy_event();
}

TEST(BATTERY_SERVICE_CLIENT, gatt_event_in_wrong_state){
    uint8_t status = battery_service_client_connect(con_handle, &gatt_client_event_handler, poll_interval_ms, NULL);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);
    // mock_gatt_client_run_once();
    // mock_gatt_client_emit_complete(ERROR_CODE_SUCCESS);
}

TEST(BATTERY_SERVICE_CLIENT, connect_no_memory){
    mock_btstack_memory_battery_service_client_simulate_no_memory();
    uint8_t status = battery_service_client_connect(con_handle, &gatt_client_event_handler, poll_interval_ms, NULL);
    CHECK_EQUAL(BTSTACK_MEMORY_ALLOC_FAILED, status);
}

TEST(BATTERY_SERVICE_CLIENT, connect_no_cid){
    uint8_t status = battery_service_client_connect(con_handle, &gatt_client_event_handler, poll_interval_ms, NULL);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);

    mock_gatt_client_run();
    CHECK_EQUAL(false, connected);
}

TEST(BATTERY_SERVICE_CLIENT, connect_no_service){
    uint8_t status = battery_service_client_connect(con_handle, &gatt_client_event_handler, poll_interval_ms, &battery_service_cid);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);

    mock_gatt_client_run();
    CHECK_EQUAL(false, connected);
}


TEST(BATTERY_SERVICE_CLIENT, connect_with_service_no_chr_no_desc){
    setup_service(false, false);
    connect();
    CHECK_EQUAL(false, connected);
}

TEST(BATTERY_SERVICE_CLIENT, connect_with_service_and_chr_no_desc){
    setup_service(true, false);
    connect();
    CHECK_EQUAL(true, connected);
}

TEST(BATTERY_SERVICE_CLIENT, connect_with_service_and_chr_and_desc){
    setup_service(true, true);
    connect();
    CHECK_EQUAL(true, connected);
}

TEST(BATTERY_SERVICE_CLIENT, connect_with_one_invalid_and_one_valid_service){
    setup_service(false, false);
    setup_service(true, true);
    connect();
    CHECK_EQUAL(true, connected);
}


TEST(BATTERY_SERVICE_CLIENT, double_connect){
    setup_service(true, true);
    connect();
    CHECK_EQUAL(true, connected);
    uint8_t status = battery_service_client_connect(con_handle, &gatt_client_event_handler, poll_interval_ms, &battery_service_cid);
    CHECK_EQUAL(ERROR_CODE_COMMAND_DISALLOWED, status);
}

TEST(BATTERY_SERVICE_CLIENT, connect_discover_primary_service_error){
    mock_gatt_client_set_att_error_discover_primary_services();
    setup_service(true, true);
    connect();
    CHECK_EQUAL(false, connected);
}

TEST(BATTERY_SERVICE_CLIENT, connect_discover_characteristics_error){
    mock_gatt_client_set_att_error_discover_characteristics();
    setup_service(true, true);
    connect();
    CHECK_EQUAL(false, connected);
}

TEST(BATTERY_SERVICE_CLIENT, connect_discover_characteristic_descriptors_error){
    mock_gatt_client_set_att_error_discover_characteristic_descriptors();
    setup_service(true, true);
    connect();
    CHECK_EQUAL(true, connected);
}

TEST(BATTERY_SERVICE_CLIENT, connect_ignore_too_many_service){
    uint8_t i;
    for (i = 0; i < MAX_NUM_BATTERY_SERVICES + 2; i++){
        setup_service(true, true);
    }
    setup_service(true, true);
    connect();

    CHECK_EQUAL(num_instances, MAX_NUM_BATTERY_SERVICES);
    CHECK_EQUAL(true, connected);
}

TEST(BATTERY_SERVICE_CLIENT, disconnect_not_connected){
    uint8_t status;

    status = battery_service_client_disconnect(battery_service_cid);
    CHECK_EQUAL(ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, status);
}

TEST(BATTERY_SERVICE_CLIENT, double_disconnect){
    uint8_t status;

    setup_service(true, true);
    connect();
    CHECK_EQUAL(true, connected);

    status = battery_service_client_disconnect(battery_service_cid);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);
        
    status = battery_service_client_disconnect(battery_service_cid);
    CHECK_EQUAL(ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, status);
}


TEST(BATTERY_SERVICE_CLIENT, notify_battery_value_invalid_0){
    setup_service(true, true);
    connect();
    CHECK_EQUAL(true, connected);

    set_battery_level_of_size(0);
    mock_gatt_client_send_notification(characteristic, battery_level, battery_level_size);
    // TODO: check battery level was not received
}


TEST(BATTERY_SERVICE_CLIENT, notify_battery_value_invalid_5){
    setup_service(true, true);
    connect();
    CHECK_EQUAL(true, connected);

    set_battery_level_of_size(5);
    mock_gatt_client_send_notification(characteristic, battery_level, battery_level_size);
    // TODO: check battery level was not received
}

TEST(BATTERY_SERVICE_CLIENT, notify_battery_value_wrong_handle){
    setup_service(true, true);
    connect();
    CHECK_EQUAL(true, connected);

    set_battery_level_of_size(1);
    mock_gatt_client_send_notification_with_handle(characteristic, 0x1234, battery_level, battery_level_size);
    // TODO: check battery level was not received
}

TEST(BATTERY_SERVICE_CLIENT, notify_battery_value_wrong_handle_multiple){
    setup_service(true, true);
    setup_service(true, true);
    connect();
    CHECK_EQUAL(true, connected);

    set_battery_level_of_size(1);
    mock_gatt_client_send_notification_with_handle(characteristic, 0x1234, battery_level, battery_level_size);
    // TODO: check battery level was not received
}

TEST(BATTERY_SERVICE_CLIENT, notify_battery_value_valid){
    setup_service(true, true);
    connect();
    CHECK_EQUAL(true, connected);

    set_battery_level_of_size(1);
    mock_gatt_client_send_notification(characteristic, battery_level, battery_level_size);
    // TODO: check battery level was received
}

TEST(BATTERY_SERVICE_CLIENT, multiple_connection){
    battery_service_client_connect(con_handle, &gatt_client_event_handler, poll_interval_ms, &battery_service_cid);
    battery_service_client_connect(con_handle+1, &gatt_client_event_handler, poll_interval_ms, &battery_service_cid);
    battery_service_client_read_battery_level(10, 0);
}

TEST(BATTERY_SERVICE_CLIENT, read_battery_level_wrong_cid){
    uint8_t status = battery_service_client_read_battery_level(10, 0);
    CHECK_EQUAL(ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, status);
}

TEST(BATTERY_SERVICE_CLIENT, read_battery_level_wrong_state){
    // without calling mock_gatt_client_run(), state remains in BATTERY_SERVICE_CLIENT_STATE_W2_QUERY_SERVICE
    uint8_t status = battery_service_client_connect(con_handle, &gatt_client_event_handler, poll_interval_ms, &battery_service_cid);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);
    CHECK_EQUAL(false, connected);

    status = battery_service_client_read_battery_level(battery_service_cid, 0);
    CHECK_EQUAL(GATT_CLIENT_IN_WRONG_STATE, status);
}

TEST(BATTERY_SERVICE_CLIENT, read_battery_level_wrong_state_in_packet_handler){
    setup_service(true, true);
    connect();
    CHECK_EQUAL(true, connected);
    mock_btstack_memory_battery_service_client_get_last_alloc()->state = BATTERY_SERVICE_CLIENT_STATE_IDLE;
    mock_gatt_client_emit_complete(ERROR_CODE_SUCCESS);
}

TEST(BATTERY_SERVICE_CLIENT, read_battery_level_wrong_service_index){
    setup_service(true, true);
    connect();
    CHECK_EQUAL(true, connected);

    uint8_t status = battery_service_client_read_battery_level(battery_service_cid, 10);
    CHECK_EQUAL(ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE, status);
}

TEST(BATTERY_SERVICE_CLIENT, poll_battery_value_invalid){
    setup_service(true, true);
    mock_gatt_client_enable_notification(characteristic, false);
    set_battery_level_of_size(5);
    

    connect();
    CHECK_EQUAL(true, connected);
    
    uint8_t status = battery_service_client_read_battery_level(battery_service_cid, 0);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);
}

TEST(BATTERY_SERVICE_CLIENT, poll_battery_value){
    setup_service(true, true);
    set_battery_level_of_size(1);
    
    mock_gatt_client_enable_notification(characteristic, false);

    connect();
    CHECK_EQUAL(true, connected);
    
    uint8_t status = battery_service_client_read_battery_level(battery_service_cid, 0);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);
}

TEST(BATTERY_SERVICE_CLIENT, poll_battery_value_with_error){
    setup_service(true, true);
    set_battery_level_of_size(1);
    
    mock_gatt_client_enable_notification(characteristic, false);

    connect();
    CHECK_EQUAL(true, connected);
    
    uint8_t status = battery_service_client_read_battery_level(battery_service_cid, 0);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);
    mock_gatt_client_emit_complete(1);
}

TEST(BATTERY_SERVICE_CLIENT, poll_battery_value_zero_poll_interval){
    setup_service(true, true);
    set_battery_level_of_size(1);
    
    mock_gatt_client_enable_notification(characteristic, false);

    uint8_t status = battery_service_client_connect(con_handle, &gatt_client_event_handler, 0, &battery_service_cid);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);
    mock_gatt_client_run();
    CHECK_EQUAL(true, connected);
    
    status = battery_service_client_read_battery_level(battery_service_cid, 0);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);
}


TEST(BATTERY_SERVICE_CLIENT, poll_battery_value_trigger_timer){
    setup_service(true, true);
    set_battery_level_of_size(1);
    
    mock_gatt_client_enable_notification(characteristic, false);

    connect();
    CHECK_EQUAL(true, connected);
    mock_btstack_run_loop_trigger_timer();
    mock_gatt_client_run();
}

TEST(BATTERY_SERVICE_CLIENT, poll_battery_value_trigger_timer_unxpected_complete){
    setup_service(true, true);
    set_battery_level_of_size(1);
    
    mock_gatt_client_enable_notification(characteristic, false);

    connect();
    CHECK_EQUAL(true, connected);
    mock_btstack_run_loop_trigger_timer();
    mock_gatt_client_run();

    // polling done, emit another complete event
    mock_gatt_client_emit_complete(0);
}


TEST(BATTERY_SERVICE_CLIENT, mixed_poll_and_notify_battery_value){
    setup_service(true, true);
    mock_gatt_client_enable_notification(characteristic, true);
    
    setup_service(true, true);
    mock_gatt_client_enable_notification(characteristic, false);
    
    setup_service(true, true);
    mock_gatt_client_enable_notification(characteristic, true);
    
    connect();
    CHECK_EQUAL(true, connected);
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
