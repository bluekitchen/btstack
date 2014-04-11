
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

static bd_addr_t test_device_addr = {0x34, 0xb1, 0xf7, 0xd1, 0x77, 0x9b};
static le_peripheral_t test_device;
uint8_t  service_uuid128[] = {0x00, 0x00, 0xf0, 0x01, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb};
uint16_t service_uuid16 = 0xF000;

static int result_index;
static le_service_t services[50];
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
    int i;
    for (i=0; i<size;i++){
        printf("%02X ", ((uint8_t *)data)[i]);
    }
    printf("\n");
}

static void dump_characteristic(le_characteristic_t * characteristic){
    printf("    *** characteristic *** properties %x, start handle 0x%02x, value handle 0x%02x, end handle 0x%02x", 
                            characteristic->properties, characteristic->start_handle, characteristic->value_handle, characteristic->end_handle);
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



void CHECK_EQUAL_ARRAY(uint8_t * expected, uint8_t * actual, int size){
	int i;
	for (i=0; i<size; i++){
		BYTES_EQUAL(expected[i], actual[i]);
	}
}

static void verify_advertisement(ad_event_t * e){
    CHECK_EQUAL(0, e->event_type);
 	CHECK_EQUAL(0, e->address_type);
	CHECK_EQUAL(188, e->rssi);
	CHECK_EQUAL(3, e->length);
	CHECK_EQUAL_ARRAY((uint8_t *)test_device_addr, (uint8_t *)e->address, 6);
}

static void verify_primary_services(){
	if (result_index == 1){
    	if (services[0].uuid16){
    		CHECK_EQUAL(service_uuid16, services[0].uuid16);
    		CHECK_EQUAL(0x24, services[0].start_group_handle);
    		CHECK_EQUAL(0x55, services[0].end_group_handle);
    	} else {
			CHECK_EQUAL_ARRAY(service_uuid128, services[0].uuid128, 16);
			CHECK_EQUAL(0x56, services[0].start_group_handle);
    		CHECK_EQUAL(0x7A, services[0].end_group_handle);
		}
		return;
	}
	
	CHECK_EQUAL(6, result_index);
	uint8_t uuids[6][16] = {
		{0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb},
		{0x00, 0x00, 0x18, 0x01, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb},
		{0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb},
		{0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb},
		{0x00, 0x00, 0xf0, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb},
		{0x00, 0x00, 0xf0, 0x01, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb}
	};

	int i;
	for (i=0; i<result_index; i++){
		CHECK_EQUAL_ARRAY(uuids[i], services[i].uuid128, 16);
	}
}

static void verify_included_services(){
	uint8_t result_offset = services[1].uuid16 ? 0:1;

	uint8_t uuids[6][16] = {
		{0x00, 0x00, 0xff, 0xf4, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb},
		{0x00, 0x00, 0xff, 0x10, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb},
		{0x00, 0x00, 0xff, 0x11, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb},
	};

	uint16_t handles[3][2] = {
		{0x1F, 0x23},
		{0x0F, 0x11},
		{0x12, 0x14}
	};

	int i;
	for (i=1; i<result_index; i++){
		int expected_result_index = i-1+result_offset;
		CHECK_EQUAL_ARRAY(uuids[expected_result_index], services[i].uuid128, 16);
		CHECK_EQUAL(handles[expected_result_index][0], services[i].start_group_handle);
    	CHECK_EQUAL(handles[expected_result_index][1], services[i].end_group_handle);
	}
}

static void verify_charasteristics(){
	uint16_t handles[14][2]= {
		{0x26, 0x2a}, {0x2b, 0x2f}, {0x30, 0x32}, {0x33, 0x35}, {0x36, 0x38}, 
		{0x39, 0x3b}, {0x3c, 0x3e}, {0x3f, 0x41}, {0x42, 0x44}, {0x45, 0x47}, 
		{0x48, 0x49}, {0x4a, 0x4f}, {0x50, 0x51}, {0x52, 0x53}
	};

	uint8_t uuids[14][16] = {
		{0x00, 0x00, 0xf0, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb},
		{0x00, 0x00, 0xf1, 0x01, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb},
		{0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb},
		{0x00, 0x00, 0xf1, 0x03, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb},
		{0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb},
		{0x00, 0x00, 0xf1, 0x05, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb},
		{0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb},
		{0x00, 0x00, 0xf1, 0x07, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb},
		{0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb},
		{0x00, 0x00, 0xf1, 0x09, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb},
		{0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb},
		{0x00, 0x00, 0xf1, 0x0a, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb},
		{0x00, 0x00, 0xf1, 0x0b, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb},
		{0x00, 0x00, 0xf1, 0x0d, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb}
	};

	int i;
	for (i=0; i<result_index; i++){
		CHECK_EQUAL_ARRAY(uuids[i], characteristics[i].uuid128, 16);
		CHECK_EQUAL(handles[i][0], characteristics[i].start_handle);
    	CHECK_EQUAL(handles[i][1], characteristics[i].end_handle);
    }
}

static void handle_le_central_event(le_central_event_t * event){
	switch(event->type){
		case GATT_ADVERTISEMENT:
			advertisement_received = 1;
			verify_advertisement((ad_event_t *) event);
			break;
		case GATT_CONNECTION_COMPLETE:
			printf("GATT_CONNECTION_COMPLETE\n");
			connected = 1;
			break;
		case GATT_SERVICE_QUERY_RESULT:
            services[result_index++] = ((le_service_event_t *) event)->service;
            break;
        case GATT_SERVICE_QUERY_COMPLETE:
        	verify_primary_services();
        	result_found = 1;
            break;
        case GATT_INCLUDED_SERVICE_QUERY_RESULT:
            services[result_index++] = ((le_service_event_t *) event)->service;
            break;
        case GATT_INCLUDED_SERVICE_QUERY_COMPLETE:
        	verify_included_services();
        	result_found = 1;
        	break;
        case GATT_CHARACTERISTIC_QUERY_RESULT:
            characteristics[result_index++] = ((le_characteristic_event_t *) event)->characteristic;
            dump_characteristic(&characteristics[result_index]);
            break;
        case GATT_CHARACTERISTIC_QUERY_COMPLETE:
        	verify_charasteristics();
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

		le_central_init();
		le_central_register_handler(handle_le_central_event);
		mock_simulate_hci_state_working();
		att_set_db(profile_data);
		connect();
	}
};

// TEST(GATTClient, TestScanning){
// 	le_central_start_scan();
// 	mock_simulate_command_complete(&hci_le_set_scan_enable);
// 	mock_simulate_scan_response();
// 	CHECK(advertisement_received);
// }

// TEST(GATTClient, TestDiscoverPrimaryServices){
// 	le_central_discover_primary_services(&test_device);
// 	CHECK(result_found);
// }

// TEST(GATTClient, TestDiscoverPrimaryServicesByUUID16){
// 	le_central_discover_primary_services_by_uuid16(&test_device, service_uuid16);
// 	CHECK(result_found);
// }

// TEST(GATTClient, TestDiscoverPrimaryServicesByUUID128){
// 	le_central_discover_primary_services_by_uuid128(&test_device, service_uuid128);
// 	CHECK(result_found);
// }

// TEST(GATTClient, TestFindIncludedServicesForServiceWithUUID16){
// 	le_central_discover_primary_services_by_uuid16(&test_device, service_uuid16);
// 	CHECK(result_found);

// 	result_found = 0;
// 	le_central_find_included_services_for_service(&test_device, &services[0]);
// 	CHECK(result_found);
// }

// TEST(GATTClient, TestFindIncludedServicesForServiceWithUUID128){
// 	le_central_discover_primary_services_by_uuid128(&test_device, service_uuid128);
// 	CHECK(result_found);

// 	result_found = 0;
// 	le_central_find_included_services_for_service(&test_device, &services[0]);
// 	CHECK(result_found);
// }

TEST(GATTClient, TestDiscoverCharacteristicsForService){
	le_central_discover_primary_services_by_uuid16(&test_device, service_uuid16);
	CHECK(result_found);

	result_found = 0;
	result_index = 0;
	le_central_discover_characteristics_for_service(&test_device, &services[0]);
	CHECK(result_found);
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
