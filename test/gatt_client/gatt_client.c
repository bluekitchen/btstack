
//*****************************************************************************
//
// test rfcomm query tests
//
//*****************************************************************************


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include <btstack/hci_cmds.h>

#include "btstack_memory.h"
#include "hci.h"
#include "ble_client.h"
#include "att.h"
#include "profile.h"
#include "expected_results.h"

static bd_addr_t test_device_addr = {0x34, 0xb1, 0xf7, 0xd1, 0x77, 0x9b};
static le_peripheral_t test_device;

typedef enum {
	IDLE,
	DISCOVER_PRIMARY_SERVICES,
    DISCOVER_PRIMARY_SERVICE_WITH_UUID16,
    DISCOVER_PRIMARY_SERVICE_WITH_UUID128,

    DISCOVER_INCLUDED_SERVICE_FOR_SERVICE_WITH_UUID16,
    DISCOVER_INCLUDED_SERVICE_FOR_SERVICE_WITH_UUID128,

    DISCOVER_CHARACTERISTICS_FOR_SERVICE_WITH_UUID16,
    DISCOVER_CHARACTERISTICS_FOR_SERVICE_WITH_UUID128,
    DISCOVER_CHARACTERISTICS_BY_UUID16,
    DISCOVER_CHARACTERISTICS_BY_UUID128,

    DISCOVER_CHARACTERISTICS_FOR_SERVICE_BY_UUID
} current_test_t;

current_test_t test = IDLE;

uint8_t  characteristic_uuid128[] = {0x00, 0x00, 0xf0, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb};
uint16_t characteristic_uuid16 = 0xF000;

static int result_index;
static le_service_t services[50];
static le_service_t included_services[50];

static le_characteristic_t characteristics[50];

 
static uint8_t advertisement_received;
static uint8_t connected;
static uint8_t result_found;

void mock_simulate_hci_state_working();
void mock_simulate_command_complete(const hci_cmd_t *cmd);
void mock_simulate_scan_response();
void mock_simulate_connected();
void mock_simulate_exchange_mtu();
void mock_simulate_discover_primary_services_response();

static void printUUID(uint8_t * uuid128, uint16_t uuid16){
    printf(", uuid ");
    if (uuid16){
        printf(" 0x%02x",uuid16);
    } else {
        printUUID128(uuid128);
    }
    printf("\n");
}

void printUUID1(uint8_t *uuid) {
    printf("0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x,\n",
           uuid[0], uuid[1], uuid[2], uuid[3], uuid[4], uuid[5], uuid[6], uuid[7],
           uuid[8], uuid[9], uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]);
}

static void hexdump2(void const *data, int size){
    for (int i=0; i<size;i++){
        printf("%02X ", ((uint8_t *)data)[i]);
    }
    printf("\n");
}

static void dump_characteristic(le_characteristic_t * characteristic){
    //printf(" {0x%02x, 0x%02x}", characteristic->start_handle, characteristic->end_handle);
    //printUUID(characteristic->uuid128, characteristic->uuid16);
    printUUID1(characteristic->uuid128);
}

static void dump_ad_event(ad_event_t * e){
    printf("    *** adv. event *** evt-type %u, addr-type %u, addr %s, rssi %u, length adv %u, data: ", e->event_type, 
                            e->address_type, bd_addr_to_str(e->address), e->rssi, e->length); 
    hexdump2( e->data, e->length);                            
}

static void dump_service(le_service_t * service){
    printf("    *** service *** start group handle %02x, end group handle %02x", service->start_group_handle, service->end_group_handle);
    printUUID(service->uuid128, service->uuid16);
}

static void dump_descriptor(le_characteristic_descriptor_t * descriptor){
    printf("    *** descriptor *** handle 0x%02x, value ", descriptor->handle);
    hexdump2(descriptor->value, descriptor->value_length);
    printUUID1(descriptor->uuid128);
}

static void dump_characteristic_value(le_characteristic_value_event_t * event){
    printf("    *** characteristic value of length %d *** ", event->characteristic_value_blob_length);
    hexdump2(event->characteristic_value, event->characteristic_value_blob_length);
    printf("\n");
}



void CHECK_EQUAL_ARRAY(const uint8_t * expected, uint8_t * actual, int size){
	for (int i=0; i<size; i++){
		BYTES_EQUAL(expected[i], actual[i]);
	}
}

void CHECK_EQUAL_GATT_ATTRIBUTE(const uint8_t * exp_uuid, const uint8_t * exp_handles, uint8_t * uuid, uint16_t start_handle, uint16_t end_handle){
	CHECK_EQUAL_ARRAY(exp_uuid, uuid, 16);
	if (!exp_handles) return;
	CHECK_EQUAL(exp_handles[0], start_handle);
    CHECK_EQUAL(exp_handles[1], end_handle);
}

// -----------------------------------------------------

static void verify_advertisement(ad_event_t * e){
    CHECK_EQUAL(0, e->event_type);
 	CHECK_EQUAL(0, e->address_type);
	CHECK_EQUAL(188, e->rssi);
	CHECK_EQUAL(3, e->length);
	CHECK_EQUAL_ARRAY((uint8_t *)test_device_addr, (uint8_t *)e->address, 6);
}

static void verify_primary_services_with_uuid16(){
	CHECK_EQUAL(1, result_index);
	CHECK_EQUAL_GATT_ATTRIBUTE(primary_service_uuid16,  primary_service_uuid16_handles, services[0].uuid128, services[0].start_group_handle, services[0].end_group_handle);	
}

static void verify_primary_services_with_uuid128(){
	CHECK_EQUAL(1, result_index);
	CHECK_EQUAL_GATT_ATTRIBUTE(primary_service_uuid128, primary_service_uuid128_handles, services[0].uuid128, services[0].start_group_handle, services[0].end_group_handle);
}

static void verify_primary_services(){
	CHECK_EQUAL(6, result_index);
	for (int i=0; i<result_index; i++){
		CHECK_EQUAL_GATT_ATTRIBUTE(primary_service_uuids[i], NULL, services[i].uuid128, services[i].start_group_handle, services[i].end_group_handle);
	}
}

static void verify_included_services_uuid16(){
	CHECK_EQUAL(1, result_index);
	CHECK_EQUAL_GATT_ATTRIBUTE(included_services_uuid16, included_services_uuid16_handles, included_services[0].uuid128, included_services[0].start_group_handle, included_services[0].end_group_handle);	
}

static void verify_included_services_uuid128(){
	CHECK_EQUAL(2, result_index);
	for (int i=0; i<result_index; i++){
		CHECK_EQUAL_GATT_ATTRIBUTE(included_services_uuid128[i], included_services_uuid128_handles[i], included_services[i].uuid128, included_services[i].start_group_handle, included_services[i].end_group_handle);	
	}
}

static void verify_charasteristics(){
	CHECK_EQUAL(14, result_index);
	for (int i=0; i<result_index; i++){
		CHECK_EQUAL_GATT_ATTRIBUTE(characteristic_uuids[i], characteristic_handles[i], characteristics[i].uuid128, characteristics[i].start_handle, characteristics[i].end_handle);	
    }
}


static void handle_le_central_event(le_central_event_t * event){
	switch(event->type){
		case GATT_ADVERTISEMENT:
			advertisement_received = 1;
			verify_advertisement((ad_event_t *) event);
			break;
		case GATT_CONNECTION_COMPLETE:
			connected = 1;
			break;
		case GATT_SERVICE_QUERY_RESULT:
            services[result_index++] = ((le_service_event_t *) event)->service;
            break;
        case GATT_SERVICE_QUERY_COMPLETE:
        	switch(test){
        		case DISCOVER_PRIMARY_SERVICES:
        			verify_primary_services();
        			break;
        		case DISCOVER_PRIMARY_SERVICE_WITH_UUID16:
        			verify_primary_services_with_uuid16();
        			break;
        		case DISCOVER_PRIMARY_SERVICE_WITH_UUID128:
        			verify_primary_services_with_uuid128();
        			break;
        		default:
        			break;
        	}
        	result_found = 1;
            break;
        case GATT_INCLUDED_SERVICE_QUERY_RESULT:
            included_services[result_index++] = ((le_service_event_t *) event)->service;
            break;
        case GATT_INCLUDED_SERVICE_QUERY_COMPLETE:
        	switch(test){
        		case DISCOVER_INCLUDED_SERVICE_FOR_SERVICE_WITH_UUID16:
        			verify_included_services_uuid16();
        			break;
        		case DISCOVER_INCLUDED_SERVICE_FOR_SERVICE_WITH_UUID128:
        			verify_included_services_uuid128();
        			break;
        		default:
        			break;
        	}
        	result_found = 1;
        	break;
        case GATT_CHARACTERISTIC_QUERY_RESULT:
        	characteristics[result_index++] = ((le_characteristic_event_t *) event)->characteristic;
            break;
        case GATT_CHARACTERISTIC_QUERY_COMPLETE:
        	switch(test){
        		case DISCOVER_CHARACTERISTICS_FOR_SERVICE_WITH_UUID16:
        			verify_charasteristics();
        			break;
        		case DISCOVER_CHARACTERISTICS_BY_UUID16:
        		case DISCOVER_CHARACTERISTICS_BY_UUID128:
        			CHECK_EQUAL(1, result_index);
        			break;
        		case DISCOVER_CHARACTERISTICS_FOR_SERVICE_BY_UUID:
        			CHECK_EQUAL(1, result_index);
        			break;
        		default:
        			break;
        	}
        	result_found = 1;
        	break;
		default:
			printf("handle_le_central_event");
			break;
	}
}

TEST_GROUP(GATTClient){
	int acl_buffer_size;
    uint8_t acl_buffer[27];

	void connect(){
		le_central_connect(&test_device, 1, test_device_addr);
		mock_simulate_connected();
		mock_simulate_exchange_mtu();
		CHECK(connected);
	}

	void setup(){
		advertisement_received = 0;
		connected = 0;
		result_found = 0;
		result_index = 0;
		test = IDLE;

		le_central_init();
		le_central_register_handler(handle_le_central_event);
		mock_simulate_hci_state_working();
		att_set_db(profile_data);
		connect();
	}
};

TEST(GATTClient, TestScanning){
	le_central_start_scan();
	mock_simulate_command_complete(&hci_le_set_scan_enable);
	mock_simulate_scan_response();
	CHECK(advertisement_received);
}

TEST(GATTClient, TestDiscoverPrimaryServices){
	test = DISCOVER_PRIMARY_SERVICES;
	le_central_discover_primary_services(&test_device);
	CHECK(result_found);
}

TEST(GATTClient, TestDiscoverPrimaryServicesByUUID16){
	test = DISCOVER_PRIMARY_SERVICE_WITH_UUID16;
	le_central_discover_primary_services_by_uuid16(&test_device, service_uuid16);
	CHECK(result_found);
}

TEST(GATTClient, TestDiscoverPrimaryServicesByUUID128){
	test = DISCOVER_PRIMARY_SERVICE_WITH_UUID128;
	le_central_discover_primary_services_by_uuid128(&test_device, primary_service_uuid128);
	CHECK(result_found);
}

TEST(GATTClient, TestFindIncludedServicesForServiceWithUUID16){
	test = DISCOVER_INCLUDED_SERVICE_FOR_SERVICE_WITH_UUID16;
	le_central_discover_primary_services_by_uuid16(&test_device, service_uuid16);
	CHECK(result_found);

	result_index = 0;
	result_found = 0;
	le_central_find_included_services_for_service(&test_device, &services[0]);
	CHECK(result_found);
}

TEST(GATTClient, TestFindIncludedServicesForServiceWithUUID128){
	test = DISCOVER_INCLUDED_SERVICE_FOR_SERVICE_WITH_UUID128;
	le_central_discover_primary_services_by_uuid128(&test_device, primary_service_uuid128);
	CHECK(result_found);

	result_index = 0;
	result_found = 0;
	le_central_find_included_services_for_service(&test_device, &services[0]);
	CHECK(result_found);
}

TEST(GATTClient, TestDiscoverCharacteristicsForService){
	test = DISCOVER_CHARACTERISTICS_FOR_SERVICE_WITH_UUID16;
	le_central_discover_primary_services_by_uuid16(&test_device, service_uuid16);
	CHECK(result_found);

	result_found = 0;
	result_index = 0;
	le_central_discover_characteristics_for_service(&test_device, &services[0]);
	CHECK(result_found);
}

TEST(GATTClient, TestDiscoverCharacteristicsByUUID16){
	test = DISCOVER_CHARACTERISTICS_BY_UUID16;
	le_central_discover_characteristics_for_handle_range_by_uuid16(&test_device, 0x30, 0x32, 0xF102);
	CHECK(result_found);
}

TEST(GATTClient, TestDiscoverCharacteristicsByUUID128){
	test = DISCOVER_CHARACTERISTICS_BY_UUID128;
	le_central_discover_characteristics_for_handle_range_by_uuid128(&test_device, characteristic_handles[1][0], characteristic_handles[1][1], characteristic_uuids[1]);
	CHECK(result_found);
}

TEST(GATTClient, TestDiscoverCharacteristics4ServiceByUUID128){
	test = DISCOVER_CHARACTERISTICS_FOR_SERVICE_BY_UUID;
	le_central_discover_primary_services_by_uuid128
	(&test_device, primary_service_uuid128);
	CHECK_EQUAL(1, result_index);
	CHECK(result_found);

	result_found = 0;
	result_index = 0;
	uint8_t characteristic_uuid[] = {0x00, 0x00, 0xF2, 0x01, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB};
	le_central_discover_characteristics_for_service_by_uuid128(&test_device, &services[0], characteristic_uuid);
	CHECK(result_found);

	result_found = 0;
	result_index = 0;
	le_central_discover_characteristics_for_service_by_uuid16(&test_device, &services[0], 0xF200);
	CHECK(result_found);

}

TEST(GATTClient, TestDiscoverCharacteristics4ServiceByUUID16){
	test = DISCOVER_CHARACTERISTICS_FOR_SERVICE_BY_UUID;
	le_central_discover_primary_services_by_uuid16(&test_device, service_uuid16);
	CHECK_EQUAL(1, result_index);
	CHECK(result_found);

	result_found = 0;
	result_index = 0;
	uint8_t characteristic_uuid[]= { 0x00, 0x00, 0xF1, 0x05, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB};
	le_central_discover_characteristics_for_service_by_uuid128(&test_device, &services[0], characteristic_uuid);
	CHECK(result_found);

	result_found = 0;
	result_index = 0;
	le_central_discover_characteristics_for_service_by_uuid16(&test_device, &services[0], 0xF100);
	CHECK(result_found);
}



int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
