
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

#include <btstack/hci_cmds.h>

#include "btstack_memory.h"
#include "hci.h"
#include "hci_dump.h"
#include "gatt_client.h"
#include "att.h"
#include "profile.h"
#include "expected_results.h"

static uint16_t gatt_client_handle = 0x40;
static uint16_t gatt_client_id;
static int gatt_query_complete = 0;

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
	DISCOVER_CHARACTERISTICS_FOR_SERVICE_BY_UUID,

	READ_CHARACTERISTIC_VALUE,
	READ_LONG_CHARACTERISTIC_VALUE,
	WRITE_CHARACTERISTIC_VALUE,
	WRITE_LONG_CHARACTERISTIC_VALUE,

    DISCOVER_CHARACTERISTIC_DESCRIPTORS,
    READ_CHARACTERISTIC_DESCRIPTOR,
    WRITE_CHARACTERISTIC_DESCRIPTOR,
    WRITE_CLIENT_CHARACTERISTIC_CONFIGURATION,
    READ_LONG_CHARACTERISTIC_DESCRIPTOR,
    WRITE_LONG_CHARACTERISTIC_DESCRIPTOR,
    WRITE_RELIABLE_LONG_CHARACTERISTIC_VALUE,
    WRITE_CHARACTERISTIC_VALUE_WITHOUT_RESPONSE
} current_test_t;

current_test_t test = IDLE;

uint8_t  characteristic_uuid128[] = {0x00, 0x00, 0xf0, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb};
uint16_t characteristic_uuid16 = 0xF000;

static int result_index;
static uint8_t result_counter;

static le_service_t services[50];
static le_service_t included_services[50];

static le_characteristic_t characteristics[50];
static le_characteristic_descriptor_t descriptors[50];

void mock_simulate_discover_primary_services_response();
void mock_simulate_att_exchange_mtu_response();

void CHECK_EQUAL_ARRAY(const uint8_t * expected, uint8_t * actual, int size){
	for (int i=0; i<size; i++){
		BYTES_EQUAL(expected[i], actual[i]);
	}
}

void pUUID128(const uint8_t *uuid) {
    printf("%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
           uuid[0], uuid[1], uuid[2], uuid[3], uuid[4], uuid[5], uuid[6], uuid[7],
           uuid[8], uuid[9], uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]);
}

//static int counter = 0;
void CHECK_EQUAL_GATT_ATTRIBUTE(const uint8_t * exp_uuid, const uint8_t * exp_handles, uint8_t * uuid, uint16_t start_handle, uint16_t end_handle){
	CHECK_EQUAL_ARRAY(exp_uuid, uuid, 16);
	if (!exp_handles) return;
	CHECK_EQUAL(exp_handles[0], start_handle);
 	CHECK_EQUAL(exp_handles[1], end_handle);
}

// -----------------------------------------------------

static void verify_primary_services_with_uuid16(void){
	CHECK_EQUAL(1, result_index);
	CHECK_EQUAL_GATT_ATTRIBUTE(primary_service_uuid16,  primary_service_uuid16_handles, services[0].uuid128, services[0].start_group_handle, services[0].end_group_handle);	
}


static void verify_primary_services_with_uuid128(void){
	CHECK_EQUAL(1, result_index);
	CHECK_EQUAL_GATT_ATTRIBUTE(primary_service_uuid128, primary_service_uuid128_handles, services[0].uuid128, services[0].start_group_handle, services[0].end_group_handle);
}

static void verify_primary_services(void){
	CHECK_EQUAL(6, result_index);
	for (int i=0; i<result_index; i++){
		CHECK_EQUAL_GATT_ATTRIBUTE(primary_service_uuids[i], NULL, services[i].uuid128, services[i].start_group_handle, services[i].end_group_handle);
	}
}

static void verify_included_services_uuid16(void){
	CHECK_EQUAL(1, result_index);
	CHECK_EQUAL_GATT_ATTRIBUTE(included_services_uuid16, included_services_uuid16_handles, included_services[0].uuid128, included_services[0].start_group_handle, included_services[0].end_group_handle);	
}

static void verify_included_services_uuid128(void){
	CHECK_EQUAL(2, result_index);
	for (int i=0; i<result_index; i++){
		CHECK_EQUAL_GATT_ATTRIBUTE(included_services_uuid128[i], included_services_uuid128_handles[i], included_services[i].uuid128, included_services[i].start_group_handle, included_services[i].end_group_handle);	
	}
}

static void verify_charasteristics(void){
	CHECK_EQUAL(15, result_index);
	for (int i=0; i<result_index; i++){
		CHECK_EQUAL_GATT_ATTRIBUTE(characteristic_uuids[i], characteristic_handles[i], characteristics[i].uuid128, characteristics[i].start_handle, characteristics[i].end_handle);	
    }
}

static void verify_blob(uint16_t value_length, uint16_t value_offset, uint8_t * value){
	uint8_t * expected_value = (uint8_t*)&long_value[value_offset];
    CHECK(value_length);
	CHECK_EQUAL_ARRAY(expected_value, value, value_length);
    if (value_offset + value_length != sizeof(long_value)) return;
    result_counter++;
}

static void handle_ble_client_event(le_event_t * event){
	
	switch(event->type){
		case GATT_SERVICE_QUERY_RESULT:
			services[result_index++] = ((le_service_event_t *) event)->service;
			result_counter++;
            break;

        case GATT_INCLUDED_SERVICE_QUERY_RESULT:
            included_services[result_index++] = ((le_service_event_t *) event)->service;
            result_counter++;
            break;
        
        case GATT_CHARACTERISTIC_QUERY_RESULT:{
        	characteristics[result_index++] = ((le_characteristic_event_t *) event)->characteristic;
        	result_counter++;
            break;
        }
        case GATT_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_RESULT:{
        	le_characteristic_descriptor_event *descriptor_event = (le_characteristic_descriptor_event_t *) event;
        	descriptors[result_index++] = descriptor_event->characteristic_descriptor;
        	result_counter++;
        	break;
        }
        case GATT_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT: {
        	le_characteristic_descriptor_event *descriptor_event = (le_characteristic_descriptor_event_t *) event;
        	CHECK_EQUAL(short_value_length, descriptor_event->value_length);
        	CHECK_EQUAL_ARRAY((uint8_t*)short_value, descriptor_event->value, short_value_length);
        	result_counter++;
        	break;
        }
        
        case GATT_LONG_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT:{
        	le_characteristic_descriptor_event_t *descriptor_event = (le_characteristic_descriptor_event_t *) event;
        	verify_blob(descriptor_event->value_length, descriptor_event->value_offset, descriptor_event->value);
        	result_counter++;
        	break;
        }

        case GATT_CHARACTERISTIC_VALUE_QUERY_RESULT:{
        	le_characteristic_value_event *cs = (le_characteristic_value_event *) event;
        	CHECK_EQUAL(short_value_length, cs->blob_length);
        	CHECK_EQUAL_ARRAY((uint8_t*)short_value, cs->blob, short_value_length);
        	result_counter++;
        	break;
        }
        case GATT_LONG_CHARACTERISTIC_VALUE_QUERY_RESULT:{
        	le_characteristic_value_event *cl = (le_characteristic_value_event *) event;
        	verify_blob(cl->blob_length, cl->value_offset, cl->blob);
        	result_counter++;
        	break;
		}
		case GATT_QUERY_COMPLETE:{
            gatt_complete_event_t *gce = (gatt_complete_event_t *)event;
            gatt_query_complete = 1;

            if (gce->status != 0){
                gatt_query_complete = 0;
                printf("GATT_QUERY_COMPLETE failed with status 0x%02X\n", gce->status);
            }
            break;
        }
		default:
			break;
	}
}

extern "C" int att_write_callback(uint16_t con_handle, uint16_t attribute_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
	switch(test){
		case WRITE_CHARACTERISTIC_DESCRIPTOR:
		case WRITE_CLIENT_CHARACTERISTIC_CONFIGURATION:
			CHECK_EQUAL(ATT_TRANSACTION_MODE_NONE, transaction_mode);
			CHECK_EQUAL(0, offset);
			CHECK_EQUAL_ARRAY(indication, buffer, 2);
			result_counter++;
			break;
		case WRITE_CHARACTERISTIC_VALUE:
			CHECK_EQUAL(ATT_TRANSACTION_MODE_NONE, transaction_mode);
			CHECK_EQUAL(0, offset);
			CHECK_EQUAL_ARRAY((uint8_t *)short_value, buffer, short_value_length);
    		result_counter++;
			break;
		case WRITE_LONG_CHARACTERISTIC_DESCRIPTOR:
		case WRITE_LONG_CHARACTERISTIC_VALUE:
		case WRITE_RELIABLE_LONG_CHARACTERISTIC_VALUE:
			if (transaction_mode == ATT_TRANSACTION_MODE_EXECUTE) break;
			CHECK_EQUAL(ATT_TRANSACTION_MODE_ACTIVE, transaction_mode);
			CHECK_EQUAL_ARRAY((uint8_t *)&long_value[offset], buffer, buffer_size);
			if (offset + buffer_size != sizeof(long_value)) break;
			result_counter++;
			break;
		default:
			break;
	}
	return 0;
}

int copy_bytes(uint8_t * value, uint16_t value_length, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
	int blob_length = value_length - offset;
	if (blob_length >= buffer_size) blob_length = buffer_size;
	
	memcpy(buffer, &value[offset], blob_length);
	return blob_length;
}

extern "C" uint16_t att_read_callback(uint16_t handle, uint16_t attribute_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
	//printf("gatt client test, att_read_callback_t handle 0x%04x, offset %u, buffer %p, buffer_size %u\n", handle, offset, buffer, buffer_size);
	switch(test){
		case READ_CHARACTERISTIC_DESCRIPTOR:
		case READ_CHARACTERISTIC_VALUE:
			result_counter++;
			if (buffer){
				return copy_bytes((uint8_t *)short_value, short_value_length, offset, buffer, buffer_size);
			}
			return short_value_length;
		case READ_LONG_CHARACTERISTIC_DESCRIPTOR:
		case READ_LONG_CHARACTERISTIC_VALUE:
			result_counter++;
			if (buffer) {
				return copy_bytes((uint8_t *)long_value, long_value_length, offset, buffer, buffer_size);
			}
			return long_value_length;
		default:
			break;
	}
	return 0;
}

// static const char * decode_status(le_command_status_t status){
// 	switch (status){
// 		case BLE_PERIPHERAL_OK: return "BLE_PERIPHERAL_OK";
//     	case BLE_PERIPHERAL_IN_WRONG_STATE: return "BLE_PERIPHERAL_IN_WRONG_STATE";
//     	case BLE_PERIPHERAL_DIFFERENT_CONTEXT_FOR_ADDRESS_ALREADY_EXISTS: return "BLE_PERIPHERAL_DIFFERENT_CONTEXT_FOR_ADDRESS_ALREADY_EXISTS";
//     	case BLE_PERIPHERAL_NOT_CONNECTED: return "BLE_PERIPHERAL_NOT_CONNECTED";
//     	case BLE_VALUE_TOO_LONG: return "BLE_VALUE_TOO_LONG";
// 	    case BLE_PERIPHERAL_BUSY: return "BLE_PERIPHERAL_BUSY";
// 	    case BLE_CHARACTERISTIC_NOTIFICATION_NOT_SUPPORTED: return "BLE_CHARACTERISTIC_NOTIFICATION_NOT_SUPPORTED";
// 	    case BLE_CHARACTERISTIC_INDICATION_NOT_SUPPORTED: return "BLE_CHARACTERISTIC_INDICATION_NOT_SUPPORTED";
// 	}
// }

TEST_GROUP(GATTClient){
	int acl_buffer_size;
    uint8_t acl_buffer[27];
    le_command_status_t status;

	void setup(void){
		result_counter = 0;
		result_index = 0;
		test = IDLE;
	}

	void reset_query_state(void){
		gatt_query_complete = 0;
		result_counter = 0;
		result_index = 0;
	}
};


TEST(GATTClient, TestDiscoverPrimaryServices){
	test = DISCOVER_PRIMARY_SERVICES;
	reset_query_state();
	status = gatt_client_discover_primary_services(gatt_client_id, gatt_client_handle);
	CHECK_EQUAL(status, BLE_PERIPHERAL_OK);
	CHECK_EQUAL(gatt_query_complete, 1);
	verify_primary_services();
	CHECK_EQUAL(gatt_query_complete, 1);
}

TEST(GATTClient, TestDiscoverPrimaryServicesByUUID16){
	test = DISCOVER_PRIMARY_SERVICE_WITH_UUID16;
	reset_query_state();
	status = gatt_client_discover_primary_services_by_uuid16(gatt_client_id, gatt_client_handle, service_uuid16);
	CHECK_EQUAL(status, BLE_PERIPHERAL_OK);
	CHECK_EQUAL(result_counter, 1);
	verify_primary_services_with_uuid16();
	CHECK_EQUAL(gatt_query_complete, 1);
}

TEST(GATTClient, TestDiscoverPrimaryServicesByUUID128){
	test = DISCOVER_PRIMARY_SERVICE_WITH_UUID128;
	status = gatt_client_discover_primary_services_by_uuid128(gatt_client_id, gatt_client_handle, primary_service_uuid128);
	CHECK_EQUAL(status, BLE_PERIPHERAL_OK);
	CHECK_EQUAL(result_counter, 1);
	verify_primary_services_with_uuid128();
	CHECK_EQUAL(gatt_query_complete, 1);
}

TEST(GATTClient, TestFindIncludedServicesForServiceWithUUID16){
	test = DISCOVER_INCLUDED_SERVICE_FOR_SERVICE_WITH_UUID16;
	reset_query_state();
	status = gatt_client_discover_primary_services_by_uuid16(gatt_client_id, gatt_client_handle, service_uuid16);
	CHECK_EQUAL(status, BLE_PERIPHERAL_OK);
	CHECK_EQUAL(gatt_query_complete, 1);

	reset_query_state();
	status = gatt_client_find_included_services_for_service(gatt_client_id, gatt_client_handle, &services[0]);
	CHECK_EQUAL(status, BLE_PERIPHERAL_OK);
	CHECK_EQUAL(gatt_query_complete, 1);
	verify_included_services_uuid16();
}

TEST(GATTClient, TestFindIncludedServicesForServiceWithUUID128){
	test = DISCOVER_INCLUDED_SERVICE_FOR_SERVICE_WITH_UUID128;
	reset_query_state();
	status = gatt_client_discover_primary_services_by_uuid128(gatt_client_id, gatt_client_handle, primary_service_uuid128);
	CHECK_EQUAL(status, BLE_PERIPHERAL_OK);
	CHECK_EQUAL(gatt_query_complete, 1);

	reset_query_state();
	status = gatt_client_find_included_services_for_service(gatt_client_id, gatt_client_handle, &services[0]);
	CHECK_EQUAL(status, BLE_PERIPHERAL_OK);
	CHECK_EQUAL(gatt_query_complete, 1);
	verify_included_services_uuid128();
}

TEST(GATTClient, TestDiscoverCharacteristicsForService){
	test = DISCOVER_CHARACTERISTICS_FOR_SERVICE_WITH_UUID16;
	reset_query_state();
	status = gatt_client_discover_primary_services_by_uuid16(gatt_client_id, gatt_client_handle, service_uuid16);
	CHECK_EQUAL(status, BLE_PERIPHERAL_OK);
	CHECK_EQUAL(gatt_query_complete, 1);
	
	reset_query_state();
	status = gatt_client_discover_characteristics_for_service(gatt_client_id, gatt_client_handle, &services[0]);
	CHECK_EQUAL(status, BLE_PERIPHERAL_OK);
	CHECK_EQUAL(gatt_query_complete, 1);
	verify_charasteristics();
}

TEST(GATTClient, TestDiscoverCharacteristicsByUUID16){
	test = DISCOVER_CHARACTERISTICS_BY_UUID16;
	reset_query_state();
	status = gatt_client_discover_characteristics_for_handle_range_by_uuid16(gatt_client_id, gatt_client_handle, 0x30, 0x32, 0xF102);
	CHECK_EQUAL(status, BLE_PERIPHERAL_OK);
	CHECK_EQUAL(gatt_query_complete, 1);
	CHECK_EQUAL(result_counter, 1);
}

TEST(GATTClient, TestDiscoverCharacteristicsByUUID128){
	test = DISCOVER_CHARACTERISTICS_BY_UUID128;
	reset_query_state();
	status = gatt_client_discover_characteristics_for_handle_range_by_uuid128(gatt_client_id, gatt_client_handle, characteristic_handles[1][0], characteristic_handles[1][1], characteristic_uuids[1]);
	CHECK_EQUAL(status, BLE_PERIPHERAL_OK);
	CHECK_EQUAL(gatt_query_complete, 1);
	CHECK_EQUAL(result_counter, 1);
}

TEST(GATTClient, TestDiscoverCharacteristics4ServiceByUUID128){
	test = DISCOVER_CHARACTERISTICS_FOR_SERVICE_BY_UUID;
	reset_query_state();
	status = gatt_client_discover_primary_services_by_uuid128(gatt_client_id, gatt_client_handle, primary_service_uuid128);
	CHECK_EQUAL(status, BLE_PERIPHERAL_OK);
	CHECK_EQUAL(gatt_query_complete, 1);
	CHECK_EQUAL(result_counter, 1);

	reset_query_state();
	uint8_t characteristic_uuid[] = {0x00, 0x00, 0xF2, 0x01, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB};
	status = gatt_client_discover_characteristics_for_service_by_uuid128(gatt_client_id, gatt_client_handle, &services[0], characteristic_uuid);
	CHECK_EQUAL(status, BLE_PERIPHERAL_OK);
	CHECK_EQUAL(gatt_query_complete, 1);
	CHECK_EQUAL(result_counter, 1);

	reset_query_state();
	status = gatt_client_discover_characteristics_for_service_by_uuid16(gatt_client_id, gatt_client_handle, &services[0], 0xF200);
	CHECK_EQUAL(status, BLE_PERIPHERAL_OK);
	CHECK_EQUAL(gatt_query_complete, 1);
	CHECK_EQUAL(result_counter, 1);
}

TEST(GATTClient, TestDiscoverCharacteristics4ServiceByUUID16){
	test = DISCOVER_CHARACTERISTICS_FOR_SERVICE_BY_UUID;
	reset_query_state();
	status = gatt_client_discover_primary_services_by_uuid16(gatt_client_id, gatt_client_handle, service_uuid16);
	CHECK_EQUAL(status, BLE_PERIPHERAL_OK);
	CHECK_EQUAL(gatt_query_complete, 1);
	CHECK_EQUAL(result_counter, 1);

	reset_query_state();
	uint8_t characteristic_uuid[]= { 0x00, 0x00, 0xF1, 0x05, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB};
	status = gatt_client_discover_characteristics_for_service_by_uuid128(gatt_client_id, gatt_client_handle, &services[0], characteristic_uuid);
	CHECK_EQUAL(status, BLE_PERIPHERAL_OK);
	CHECK_EQUAL(gatt_query_complete, 1);
	CHECK_EQUAL(result_counter, 1);

	reset_query_state();
	status = gatt_client_discover_characteristics_for_service_by_uuid16(gatt_client_id, gatt_client_handle, &services[0], 0xF100);
	CHECK_EQUAL(status, BLE_PERIPHERAL_OK);
	CHECK_EQUAL(gatt_query_complete, 1);
	CHECK_EQUAL(result_counter, 1);
}

TEST(GATTClient, TestDiscoverCharacteristicDescriptor){
	test = DISCOVER_CHARACTERISTIC_DESCRIPTORS;
	reset_query_state();
	status = gatt_client_discover_primary_services_by_uuid16(gatt_client_id, gatt_client_handle, service_uuid16);
	CHECK_EQUAL(status, BLE_PERIPHERAL_OK);
	CHECK_EQUAL(gatt_query_complete, 1);
	CHECK_EQUAL(result_counter, 1);

	reset_query_state();
	status = gatt_client_discover_characteristics_for_service_by_uuid16(gatt_client_id, gatt_client_handle, &services[0], 0xF100);
	CHECK_EQUAL(status, BLE_PERIPHERAL_OK);
	CHECK_EQUAL(gatt_query_complete, 1);
	CHECK_EQUAL(result_counter, 1);

	reset_query_state();
	status = gatt_client_discover_characteristic_descriptors(gatt_client_id, gatt_client_handle, &characteristics[0]);
	CHECK_EQUAL(status, BLE_PERIPHERAL_OK);
	CHECK_EQUAL(gatt_query_complete, 1);
	CHECK(result_counter);
	CHECK_EQUAL(3, result_index);
	CHECK_EQUAL(0x2902, descriptors[0].uuid16);
	CHECK_EQUAL(0x2900, descriptors[1].uuid16);
	CHECK_EQUAL(0x2901, descriptors[2].uuid16);
}

TEST(GATTClient, TestWriteClientCharacteristicConfiguration){
	test = WRITE_CLIENT_CHARACTERISTIC_CONFIGURATION;
	reset_query_state();
	status = gatt_client_discover_primary_services_by_uuid16(gatt_client_id, gatt_client_handle, service_uuid16);
	CHECK_EQUAL(status, BLE_PERIPHERAL_OK);
	CHECK_EQUAL(gatt_query_complete, 1);
	CHECK_EQUAL(result_counter, 1);

	reset_query_state();
	status = gatt_client_discover_characteristics_for_service_by_uuid16(gatt_client_id, gatt_client_handle, &services[0], 0xF100);
	CHECK_EQUAL(status, BLE_PERIPHERAL_OK);
	CHECK_EQUAL(gatt_query_complete, 1);
	CHECK_EQUAL(result_counter, 1);

	reset_query_state();
	status = gatt_client_write_client_characteristic_configuration(gatt_client_id, gatt_client_handle, &characteristics[0], GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION);
 	CHECK_EQUAL(status, BLE_PERIPHERAL_OK);
 	CHECK_EQUAL(gatt_query_complete, 1);
 	CHECK_EQUAL(result_counter, 1);
}

TEST(GATTClient, TestReadCharacteristicDescriptor){
	test = READ_CHARACTERISTIC_DESCRIPTOR;
	reset_query_state();
	status = gatt_client_discover_primary_services_by_uuid16(gatt_client_id, gatt_client_handle, service_uuid16);
	CHECK_EQUAL(status, BLE_PERIPHERAL_OK);
	CHECK_EQUAL(gatt_query_complete, 1);
	CHECK_EQUAL(result_counter, 1);

	reset_query_state();
	status = gatt_client_discover_characteristics_for_service_by_uuid16(gatt_client_id, gatt_client_handle, &services[0], 0xF100);
	CHECK_EQUAL(status, BLE_PERIPHERAL_OK);
	CHECK_EQUAL(gatt_query_complete, 1);
	CHECK_EQUAL(result_counter, 1);

	reset_query_state();
	status = gatt_client_discover_characteristic_descriptors(gatt_client_id, gatt_client_handle, &characteristics[0]);
	CHECK_EQUAL(status, BLE_PERIPHERAL_OK);
	CHECK_EQUAL(gatt_query_complete, 1);
	CHECK_EQUAL(result_counter, 3);

	reset_query_state();
	status = gatt_client_read_characteristic_descriptor(gatt_client_id, gatt_client_handle, &descriptors[0]);
	CHECK_EQUAL(status, BLE_PERIPHERAL_OK);
	CHECK_EQUAL(gatt_query_complete, 1);
	CHECK_EQUAL(result_counter, 3);
}

TEST(GATTClient, TestReadCharacteristicValue){
	test = READ_CHARACTERISTIC_VALUE;
	reset_query_state();
	status = gatt_client_discover_primary_services_by_uuid16(gatt_client_id, gatt_client_handle, service_uuid16);
	CHECK_EQUAL(status, BLE_PERIPHERAL_OK);
	CHECK_EQUAL(gatt_query_complete, 1);
	CHECK_EQUAL(result_counter, 1);

	reset_query_state();
	status = gatt_client_discover_characteristics_for_service_by_uuid16(gatt_client_id, gatt_client_handle, &services[0], 0xF100);
	CHECK_EQUAL(status, BLE_PERIPHERAL_OK);
	CHECK_EQUAL(gatt_query_complete, 1);
	CHECK_EQUAL(result_counter, 1);

	reset_query_state();
	status = gatt_client_read_value_of_characteristic(gatt_client_id, gatt_client_handle, &characteristics[0]);
	CHECK_EQUAL(status, BLE_PERIPHERAL_OK);
	CHECK_EQUAL(gatt_query_complete, 1);
	CHECK_EQUAL(result_counter, 3);
}

TEST(GATTClient, TestWriteCharacteristicValue){
    test = WRITE_CHARACTERISTIC_VALUE;
	reset_query_state();
	status = gatt_client_discover_primary_services_by_uuid16(gatt_client_id, gatt_client_handle, service_uuid16);
	CHECK_EQUAL(status, BLE_PERIPHERAL_OK);
	CHECK_EQUAL(gatt_query_complete, 1);
	CHECK_EQUAL(result_counter, 1);

	reset_query_state();
	status = gatt_client_discover_characteristics_for_service_by_uuid16(gatt_client_id, gatt_client_handle, &services[0], 0xF100);
	CHECK_EQUAL(status, BLE_PERIPHERAL_OK);
	CHECK_EQUAL(gatt_query_complete, 1);
	CHECK_EQUAL(result_counter, 1);

	reset_query_state();
	status = gatt_client_write_value_of_characteristic(gatt_client_id, gatt_client_handle, characteristics[0].value_handle, short_value_length, (uint8_t*)short_value);
	CHECK_EQUAL(status, BLE_PERIPHERAL_OK);
	CHECK_EQUAL(gatt_query_complete, 1);
}

TEST(GATTClient, TestWriteCharacteristicDescriptor){
	test = WRITE_CHARACTERISTIC_DESCRIPTOR;
	reset_query_state();
	status = gatt_client_discover_primary_services_by_uuid16(gatt_client_id, gatt_client_handle, service_uuid16);
	CHECK_EQUAL(status, BLE_PERIPHERAL_OK);
	CHECK_EQUAL(gatt_query_complete, 1);
	CHECK_EQUAL(result_counter, 1);

	reset_query_state();
	status = gatt_client_discover_characteristics_for_service_by_uuid16(gatt_client_id, gatt_client_handle, &services[0], 0xF100);
	CHECK_EQUAL(status, BLE_PERIPHERAL_OK);
	CHECK_EQUAL(gatt_query_complete, 1);
	CHECK_EQUAL(result_counter, 1);

	reset_query_state();
	status = gatt_client_discover_characteristic_descriptors(gatt_client_id, gatt_client_handle, &characteristics[0]);
	CHECK_EQUAL(status, BLE_PERIPHERAL_OK);
	CHECK_EQUAL(gatt_query_complete, 1);
	CHECK_EQUAL(result_counter, 3);
	
	reset_query_state();
	status = gatt_client_write_characteristic_descriptor(gatt_client_id, gatt_client_handle, &descriptors[0], sizeof(indication), indication);
	CHECK_EQUAL(status, BLE_PERIPHERAL_OK);
	CHECK_EQUAL(gatt_query_complete, 1);
}

TEST(GATTClient, TestReadLongCharacteristicValue){
	test = READ_LONG_CHARACTERISTIC_VALUE;
	reset_query_state();
	status = gatt_client_discover_primary_services_by_uuid16(gatt_client_id, gatt_client_handle, service_uuid16);
	CHECK_EQUAL(status, BLE_PERIPHERAL_OK);
	CHECK_EQUAL(gatt_query_complete, 1);
	CHECK_EQUAL(result_counter, 1);

	reset_query_state();
	status = gatt_client_discover_characteristics_for_service_by_uuid16(gatt_client_id, gatt_client_handle, &services[0], 0xF100);
	CHECK_EQUAL(status, BLE_PERIPHERAL_OK);
	CHECK_EQUAL(gatt_query_complete, 1);
	CHECK_EQUAL(result_counter, 1);

	reset_query_state();
	status = gatt_client_read_long_value_of_characteristic(gatt_client_id, gatt_client_handle, &characteristics[0]);
	CHECK_EQUAL(status, BLE_PERIPHERAL_OK);
	CHECK_EQUAL(gatt_query_complete, 1);
	CHECK_EQUAL(result_counter, 7);
}

TEST(GATTClient, TestReadLongCharacteristicDescriptor){
	test = READ_LONG_CHARACTERISTIC_DESCRIPTOR;
	reset_query_state();
	status = gatt_client_discover_primary_services_by_uuid128(gatt_client_id, gatt_client_handle, primary_service_uuid128);
	CHECK_EQUAL(status, BLE_PERIPHERAL_OK);
	CHECK_EQUAL(gatt_query_complete, 1);
	CHECK_EQUAL(result_counter, 1);

	reset_query_state();
	status = gatt_client_discover_characteristics_for_service_by_uuid16(gatt_client_id, gatt_client_handle, &services[0], 0xF200);
	CHECK_EQUAL(status, BLE_PERIPHERAL_OK);
	CHECK_EQUAL(gatt_query_complete, 1);
	CHECK_EQUAL(result_counter, 1);

	reset_query_state();
	status = gatt_client_discover_characteristic_descriptors(gatt_client_id, gatt_client_handle, &characteristics[0]);
	CHECK_EQUAL(status, BLE_PERIPHERAL_OK);
	CHECK_EQUAL(gatt_query_complete, 1);
	CHECK_EQUAL(result_counter, 3);

	reset_query_state();
	result_counter = 0;
	status = gatt_client_read_long_characteristic_descriptor(gatt_client_id, gatt_client_handle, &descriptors[0]);
	CHECK_EQUAL(status, BLE_PERIPHERAL_OK);
	CHECK_EQUAL(gatt_query_complete, 1);
	CHECK_EQUAL(result_counter, 7);
}


TEST(GATTClient, TestWriteLongCharacteristicDescriptor){
	test = WRITE_LONG_CHARACTERISTIC_DESCRIPTOR;
	reset_query_state();
	status = gatt_client_discover_primary_services_by_uuid128(gatt_client_id, gatt_client_handle, primary_service_uuid128);
	CHECK_EQUAL(status, BLE_PERIPHERAL_OK);
	CHECK_EQUAL(gatt_query_complete, 1);
	CHECK_EQUAL(result_counter, 1);

	reset_query_state();
	status = gatt_client_discover_characteristics_for_service_by_uuid16(gatt_client_id, gatt_client_handle, &services[0], 0xF200);
	CHECK_EQUAL(status, BLE_PERIPHERAL_OK);
	CHECK_EQUAL(gatt_query_complete, 1);
	CHECK_EQUAL(result_counter, 1);

	reset_query_state();
	status = gatt_client_discover_characteristic_descriptors(gatt_client_id, gatt_client_handle, &characteristics[0]);
	CHECK_EQUAL(status, BLE_PERIPHERAL_OK);
	CHECK_EQUAL(gatt_query_complete, 1);
	CHECK_EQUAL(result_counter, 3);

	result_counter = 0;
	status = gatt_client_write_long_characteristic_descriptor(gatt_client_id, gatt_client_handle, &descriptors[0], sizeof(long_value), (uint8_t *)long_value);
	CHECK_EQUAL(status, BLE_PERIPHERAL_OK);
	CHECK_EQUAL(gatt_query_complete, 1);
	CHECK_EQUAL(result_counter, 1);
}

TEST(GATTClient, TestWriteLongCharacteristicValue){
	test = WRITE_LONG_CHARACTERISTIC_VALUE;
	reset_query_state();
	status = gatt_client_discover_primary_services_by_uuid16(gatt_client_id, gatt_client_handle, service_uuid16);
	CHECK_EQUAL(status, BLE_PERIPHERAL_OK);
	CHECK_EQUAL(gatt_query_complete, 1);
	CHECK_EQUAL(result_counter, 1);

	reset_query_state();
	status = gatt_client_discover_characteristics_for_service_by_uuid16(gatt_client_id, gatt_client_handle, &services[0], 0xF100);
	CHECK_EQUAL(status, BLE_PERIPHERAL_OK);
	CHECK_EQUAL(gatt_query_complete, 1);
	CHECK_EQUAL(result_counter, 1);


	reset_query_state();
	status = gatt_client_write_long_value_of_characteristic(gatt_client_id, gatt_client_handle, characteristics[0].value_handle, long_value_length, (uint8_t*)long_value);
	CHECK_EQUAL(status, BLE_PERIPHERAL_OK);
	CHECK_EQUAL(gatt_query_complete, 1);
}

TEST(GATTClient, TestWriteReliableLongCharacteristicValue){
	test = WRITE_RELIABLE_LONG_CHARACTERISTIC_VALUE;
	reset_query_state();
	status = gatt_client_discover_primary_services_by_uuid16(gatt_client_id, gatt_client_handle, service_uuid16);
	CHECK_EQUAL(status, BLE_PERIPHERAL_OK);
	CHECK_EQUAL(gatt_query_complete, 1);
	CHECK_EQUAL(result_counter, 1);

	reset_query_state();
	status = gatt_client_discover_characteristics_for_service_by_uuid16(gatt_client_id, gatt_client_handle, &services[0], 0xF100);
	CHECK_EQUAL(status, BLE_PERIPHERAL_OK);
	CHECK_EQUAL(gatt_query_complete, 1);
	CHECK_EQUAL(result_counter, 1);

	reset_query_state();
	status = gatt_client_reliable_write_long_value_of_characteristic(gatt_client_id, gatt_client_handle, characteristics[0].value_handle, long_value_length, (uint8_t*)long_value);
	CHECK_EQUAL(status, BLE_PERIPHERAL_OK);
	CHECK_EQUAL(gatt_query_complete, 1);
}


int main (int argc, const char * argv[]){
	att_set_db(profile_data);
	att_set_write_callback(&att_write_callback);
	att_set_read_callback(&att_read_callback);

	gatt_client_init();
	gatt_client_id = gatt_client_register_packet_handler(handle_ble_client_event);
	
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
