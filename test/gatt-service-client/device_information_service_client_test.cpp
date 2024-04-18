
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

#include "ble/gatt-service/device_information_service_client.h"
#include "mock_gatt_client.h"

static const hci_con_handle_t con_handle = 0x01;
static bool connected;

// temp btstack run loop mock

static btstack_timer_source_t * btstack_timer = NULL;

static bool manufacturer_name = false;
static bool model_number = false;
static bool serial_number = false;
static bool hardware_revision = false;
static bool firmware_revision = false;
static bool software_revision = false;
static bool system_id = false;
static bool ieee_regulatory_certification = false;
static bool pnp_id = false;

static bool mock_wrong_hci_event_packet_type = false;

extern "C" void device_information_service_client_set_invalid_state(void);

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
    // uint8_t att_status;

    if (hci_event_packet_get_type(packet) != HCI_EVENT_GATTSERVICE_META){
        return;
    }
    
    switch (hci_event_gattservice_meta_get_subevent_code(packet)){
        case GATTSERVICE_SUBEVENT_DEVICE_INFORMATION_DONE:
            status = gattservice_subevent_device_information_done_get_att_status(packet);
            switch (status){
                case ATT_ERROR_SUCCESS:
                    connected = true;
                    break;
                default:
                    connected = false;
                    break;
            }
            break;
        case GATTSERVICE_SUBEVENT_DEVICE_INFORMATION_MANUFACTURER_NAME:
            manufacturer_name = true;
            break;
        case GATTSERVICE_SUBEVENT_DEVICE_INFORMATION_MODEL_NUMBER:
            model_number = true;
            break;
        case GATTSERVICE_SUBEVENT_DEVICE_INFORMATION_SERIAL_NUMBER:
            serial_number = true;
            break;
        case GATTSERVICE_SUBEVENT_DEVICE_INFORMATION_HARDWARE_REVISION:
            hardware_revision = true;
            break;
        case GATTSERVICE_SUBEVENT_DEVICE_INFORMATION_FIRMWARE_REVISION:
            firmware_revision = true;
            break;
        case GATTSERVICE_SUBEVENT_DEVICE_INFORMATION_SOFTWARE_REVISION:
            software_revision = true;
            break;
        case GATTSERVICE_SUBEVENT_DEVICE_INFORMATION_SYSTEM_ID:
            system_id = true;
            break;
        case GATTSERVICE_SUBEVENT_DEVICE_INFORMATION_IEEE_REGULATORY_CERTIFICATION:
            ieee_regulatory_certification = true;
            break;
        case GATTSERVICE_SUBEVENT_DEVICE_INFORMATION_PNP_ID:
            pnp_id = true;
            break;
        
        default:
            break;
    }
}

TEST_GROUP(DEVICE_INFORMATION_SERVICE_CLIENT){ 
    mock_gatt_client_service_t * service;
    mock_gatt_client_characteristic_t * characteristic;

    void setup(void){
        mock_wrong_hci_event_packet_type = false;
        manufacturer_name = false;
        model_number = false;
        serial_number = false;
        hardware_revision = false;
        firmware_revision = false;
        software_revision = false;
        system_id = false;
        ieee_regulatory_certification = false;
        pnp_id = false;

        device_information_service_client_init();
        mock_gatt_client_reset();
    }

    void setup_service(bool add_characteristic){
        uint8_t buffer[] = {0x01, 0x02};
        service = mock_gatt_client_add_primary_service_uuid16(ORG_BLUETOOTH_SERVICE_DEVICE_INFORMATION);
        
        if (!add_characteristic) return;
        characteristic = mock_gatt_client_add_characteristic_uuid16(ORG_BLUETOOTH_CHARACTERISTIC_MANUFACTURER_NAME_STRING, ATT_PROPERTY_READ);
        mock_gatt_client_set_characteristic_value(characteristic, buffer, sizeof(buffer));
    }
    
    void setup_full_service(void){
        static uint8_t buffer[] = {0x01, 0x02};
        static uint8_t system_id_buffer[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
        static uint8_t ieee_buffer[]  = {0x01, 0x02, 0x03, 0x04};
        static uint8_t pnp_id_buffer[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};

        service = mock_gatt_client_add_primary_service_uuid16(ORG_BLUETOOTH_SERVICE_DEVICE_INFORMATION);
        
        characteristic = mock_gatt_client_add_characteristic_uuid16(ORG_BLUETOOTH_CHARACTERISTIC_MANUFACTURER_NAME_STRING, ATT_PROPERTY_READ);
        mock_gatt_client_set_characteristic_value(characteristic, buffer, sizeof(buffer));
   
        characteristic = mock_gatt_client_add_characteristic_uuid16(ORG_BLUETOOTH_CHARACTERISTIC_MODEL_NUMBER_STRING     , ATT_PROPERTY_READ);
        mock_gatt_client_set_characteristic_value(characteristic, buffer, sizeof(buffer));

        characteristic = mock_gatt_client_add_characteristic_uuid16(ORG_BLUETOOTH_CHARACTERISTIC_SERIAL_NUMBER_STRING    , ATT_PROPERTY_READ);
        mock_gatt_client_set_characteristic_value(characteristic, buffer, sizeof(buffer));

        characteristic = mock_gatt_client_add_characteristic_uuid16(ORG_BLUETOOTH_CHARACTERISTIC_HARDWARE_REVISION_STRING, ATT_PROPERTY_READ);
        mock_gatt_client_set_characteristic_value(characteristic, buffer, sizeof(buffer));

        characteristic = mock_gatt_client_add_characteristic_uuid16(ORG_BLUETOOTH_CHARACTERISTIC_FIRMWARE_REVISION_STRING, ATT_PROPERTY_READ);
        mock_gatt_client_set_characteristic_value(characteristic, buffer, sizeof(buffer));

        characteristic = mock_gatt_client_add_characteristic_uuid16(ORG_BLUETOOTH_CHARACTERISTIC_SOFTWARE_REVISION_STRING, ATT_PROPERTY_READ);
        mock_gatt_client_set_characteristic_value(characteristic, buffer, sizeof(buffer));

        characteristic = mock_gatt_client_add_characteristic_uuid16(ORG_BLUETOOTH_CHARACTERISTIC_SYSTEM_ID, ATT_PROPERTY_READ);
        mock_gatt_client_set_characteristic_value(characteristic, system_id_buffer, sizeof(system_id_buffer));
    
        characteristic = mock_gatt_client_add_characteristic_uuid16(ORG_BLUETOOTH_CHARACTERISTIC_IEEE_11073_20601_REGULATORY_CERTIFICATION_DATA_LIST, ATT_PROPERTY_READ);
        mock_gatt_client_set_characteristic_value(characteristic, ieee_buffer, sizeof(ieee_buffer));
    
        characteristic = mock_gatt_client_add_characteristic_uuid16(ORG_BLUETOOTH_CHARACTERISTIC_PNP_ID, ATT_PROPERTY_READ);
        mock_gatt_client_set_characteristic_value(characteristic, pnp_id_buffer, sizeof(pnp_id_buffer));
    }

    void teardown(void){
        device_information_service_client_deinit();
    }
};


TEST(DEVICE_INFORMATION_SERVICE_CLIENT, double_query){
    uint8_t status;

    status = device_information_service_client_query(con_handle, &gatt_client_event_handler);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);

    status = device_information_service_client_query(con_handle, &gatt_client_event_handler);
    CHECK_EQUAL(ERROR_CODE_COMMAND_DISALLOWED, status);

    status = device_information_service_client_query(HCI_CON_HANDLE_INVALID, &gatt_client_event_handler);
    CHECK_EQUAL(ERROR_CODE_COMMAND_DISALLOWED, status);
}

TEST(DEVICE_INFORMATION_SERVICE_CLIENT, query_no_service){
    uint8_t status = device_information_service_client_query(con_handle, &gatt_client_event_handler);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);
    
    mock_gatt_client_run();
    CHECK_EQUAL(false, connected);
}

TEST(DEVICE_INFORMATION_SERVICE_CLIENT, query_service_no_caharacteristic){
    setup_service(false);
    
    uint8_t status = device_information_service_client_query(con_handle, &gatt_client_event_handler);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);
    
    mock_gatt_client_run();
    CHECK_EQUAL(false, connected);
}

TEST(DEVICE_INFORMATION_SERVICE_CLIENT, query_service_att_error){
    setup_service(false);
    mock_gatt_client_set_att_error_discover_primary_services();

    uint8_t status = device_information_service_client_query(con_handle, &gatt_client_event_handler);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);
    
    mock_gatt_client_run();
    CHECK_EQUAL(false, connected);
}

TEST(DEVICE_INFORMATION_SERVICE_CLIENT, query_service_one_chr){
    setup_service(true);

    uint8_t status = device_information_service_client_query(con_handle, &gatt_client_event_handler);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);
    
    mock_gatt_client_run();
    CHECK_EQUAL(true, connected);
}

TEST(DEVICE_INFORMATION_SERVICE_CLIENT, query_service_one_chr_att_error){
    setup_service(true);
    mock_gatt_client_set_att_error_read_value_characteristics();
    // mock_gatt_client_dump_services();

    uint8_t status = device_information_service_client_query(con_handle, &gatt_client_event_handler);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);
    
    mock_gatt_client_run();
    CHECK_EQUAL(true, connected);
}

TEST(DEVICE_INFORMATION_SERVICE_CLIENT, query_service_full_setup_no_error){
    setup_full_service();
    
    uint8_t status = device_information_service_client_query(con_handle, &gatt_client_event_handler);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);
    
    mock_gatt_client_run();
    CHECK_EQUAL(true, connected);

    CHECK_EQUAL(true, manufacturer_name);
    CHECK_EQUAL(true, model_number);
    CHECK_EQUAL(true, serial_number);
    CHECK_EQUAL(true, hardware_revision);
    CHECK_EQUAL(true, firmware_revision);
    CHECK_EQUAL(true, software_revision);

    CHECK_EQUAL(true, system_id);
    CHECK_EQUAL(true, ieee_regulatory_certification);
    CHECK_EQUAL(true, pnp_id);
}

TEST(DEVICE_INFORMATION_SERVICE_CLIENT, unhandled_gatt_event_when_connected){
    setup_service(true);
    uint8_t status = device_information_service_client_query(con_handle, &gatt_client_event_handler);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);
    mock_gatt_client_run();
    CHECK_EQUAL(true, connected);

    mock_gatt_client_emit_dummy_event();
    CHECK_EQUAL(true, connected);
}

TEST(DEVICE_INFORMATION_SERVICE_CLIENT, unhandled_gatt_event_when_not_connected){
    setup_service(true);
    uint8_t status = device_information_service_client_query(con_handle, &gatt_client_event_handler);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);
    
    CHECK_EQUAL(false, connected);

    mock_gatt_client_emit_dummy_event();
    CHECK_EQUAL(false, connected);

    mock_gatt_client_run();
    CHECK_EQUAL(true, connected);
}


TEST(DEVICE_INFORMATION_SERVICE_CLIENT, client_in_wrong_state){
    setup_service(true);
    uint8_t status = device_information_service_client_query(con_handle, &gatt_client_event_handler);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);
    
    device_information_service_client_set_invalid_state();
    mock_gatt_client_run();
    CHECK_EQUAL(false, connected);
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
