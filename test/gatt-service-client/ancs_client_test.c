
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

#include "ble/gatt-service/ancs_client.h"
#include "mock_gatt_client.h"

// mock hci

static btstack_packet_callback_registration_t * ancs_callback_registration;
static const hci_con_handle_t ancs_con_handle = 0x001;

void hci_add_event_handler(btstack_packet_callback_registration_t * callback_handler){
    ancs_callback_registration = callback_handler;
}

static void hci_emit_event(uint8_t * event, uint16_t size, int dump){
    btstack_assert(ancs_callback_registration != NULL);
    (*ancs_callback_registration->callback)(HCI_EVENT_PACKET, 0, event, size);    
}

static void hci_emit_le_connection_complete(uint8_t address_type, const bd_addr_t address, hci_con_handle_t con_handle, uint8_t status){
    uint8_t event[21];
    event[0] = HCI_EVENT_LE_META;
    event[1] = sizeof(event) - 2u;
    event[2] = HCI_SUBEVENT_LE_CONNECTION_COMPLETE;
    event[3] = status;
    little_endian_store_16(event, 4, con_handle);
    event[6] = 0; // TODO: role
    event[7] = address_type;
    reverse_bd_addr(address, &event[8]);
    little_endian_store_16(event, 14, 0); // interval
    little_endian_store_16(event, 16, 0); // latency
    little_endian_store_16(event, 18, 0); // supervision timeout
    event[20] = 0; // master clock accuracy
    hci_emit_event(event, sizeof(event), 1);
}

static void hci_emit_connection_encrypted(hci_con_handle_t con_handle, uint8_t encrypted){
    uint8_t encryption_complete_event[6] = { HCI_EVENT_ENCRYPTION_CHANGE, 4, 0, 0, 0, 0};
    little_endian_store_16(encryption_complete_event, 3, con_handle);
    encryption_complete_event[5] = encrypted;
    hci_emit_event(encryption_complete_event, sizeof(encryption_complete_event), 0);
}

static void hci_emit_disconnection_complete(hci_con_handle_t con_handle, uint8_t reason){
    uint8_t event[6];
    event[0] = HCI_EVENT_DISCONNECTION_COMPLETE;
    event[1] = sizeof(event) - 2u;
    event[2] = 0; // status = OK
    little_endian_store_16(event, 3, con_handle);
    event[5] = reason;
    hci_emit_event(event, sizeof(event), 1);
}

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

    if (hci_event_packet_get_type(packet) != HCI_EVENT_GATTSERVICE_META){
        return;
    }
    
    switch (hci_event_gattservice_meta_get_subevent_code(packet)){
        default:
            break;
    }
}

TEST_GROUP(ANCS_CLIENT){ 
    mock_gatt_client_service_t * service;
    mock_gatt_client_characteristic_t * characteristic;
    mock_gatt_client_characteristic_descriptor_t * descriptor;

    uint8_t  value_buffer[3];

    void setup(void){
        mock_gatt_client_reset();
        ancs_client_init();
        ancs_client_register_callback(&ancs_client_event_handler);
    }

    void setup_service(bool add_characteristics, bool add_descriptors){
        // TODO: setup ANCS Service
#if 0
        service = mock_gatt_client_add_primary_service_uuid16(ORG_BLUETOOTH_SERVICE_BATTERY_SERVICE);
        if (!add_characteristics) return;

        characteristic = mock_gatt_client_add_characteristic_uuid16(ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL, ATT_PROPERTY_NOTIFY);
        
        if (!add_descriptors) return;
        descriptor = mock_gatt_client_add_characteristic_descriptor_uuid16(ORG_BLUETOOTH_DESCRIPTOR_GATT_CLIENT_CHARACTERISTIC_CONFIGURATION);
        mock_gatt_client_set_descriptor_characteristic_value(descriptor, value_buffer, sizeof(value_buffer));

        // mock_gatt_client_dump_services();
#endif
    }

    void connect(void){
        // simulated connected
        bd_addr_t some_address;
        hci_emit_le_connection_complete(0, some_address, ancs_con_handle, ERROR_CODE_SUCCESS);
        hci_emit_connection_encrypted(ancs_con_handle, 1);
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
    hci_emit_event(some_other_event, sizeof(some_other_event), 0);
    // default hci le subevent
    uint8_t some_le_event[] = { HCI_EVENT_LE_META, 1, 0};
    hci_emit_event(some_le_event, sizeof(some_le_event), 0);
    // encypted but different con handle
    hci_emit_connection_encrypted(ancs_con_handle+1, 1);
    // encypted but different state
    hci_emit_connection_encrypted(ancs_con_handle, 1);
    // non-encypted
    hci_emit_connection_encrypted(ancs_con_handle, 0);
    // disconnected different handle
    hci_emit_disconnection_complete(ancs_con_handle+1,0);
    // disconnected different handle
    hci_emit_disconnection_complete(ancs_con_handle,0);
}

TEST(ANCS_CLIENT, connect){
    connect();
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
