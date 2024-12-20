
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
#include "profile.h"
#include "expected_results.h"

extern "C" void hci_setup_le_connection(uint16_t con_handle);
extern "C" void l2cap_set_can_send_fixed_channel_packet_now(bool value);

static uint16_t gatt_client_handle = 0x40;
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
    WRITE_NONEXISTING_CLIENT_CHARACTERISTIC_CONFIGURATION,
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

static gatt_client_service_t services[50];
static gatt_client_service_t included_services[50];

static gatt_client_characteristic_t characteristics[50];
static gatt_client_characteristic_descriptor_t descriptors[50];

void mock_simulate_discover_primary_services_response(void);
void mock_simulate_att_exchange_mtu_response(void);

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
	CHECK_EQUAL_ARRAY(expected_value, value, value_length);
    if (value_offset + value_length != sizeof(long_value)) return;
    result_counter++;
}

static void handle_ble_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
	if (packet_type != HCI_EVENT_PACKET) return;
	uint8_t status;
	gatt_client_service_t service;
	gatt_client_characteristic_t characteristic;
	gatt_client_characteristic_descriptor_t descriptor;
	switch (packet[0]){
		case GATT_EVENT_QUERY_COMPLETE:
			status = gatt_event_query_complete_get_att_status(packet);
            gatt_query_complete = 1;
            if (status){
                gatt_query_complete = 0;
            }
            break;
		case GATT_EVENT_SERVICE_QUERY_RESULT:
            gatt_event_service_query_result_get_service(packet, &service);
			services[result_index++] = service;
			result_counter++;
            break;
        case GATT_EVENT_INCLUDED_SERVICE_QUERY_RESULT:
            gatt_event_included_service_query_result_get_service(packet, &service);
            included_services[result_index++] = service;
            result_counter++;
            break;
        case GATT_EVENT_CHARACTERISTIC_QUERY_RESULT:
            gatt_event_characteristic_query_result_get_characteristic(packet, &characteristic);
        	characteristics[result_index++] = characteristic;
        	result_counter++;
            break;
        case GATT_EVENT_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_RESULT:
            gatt_event_all_characteristic_descriptors_query_result_get_characteristic_descriptor(packet, &descriptor);
        	descriptors[result_index++] = descriptor;
        	result_counter++;
        	break;
        case GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT:
        case GATT_EVENT_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT:
        	CHECK_EQUAL(short_value_length, gatt_event_characteristic_value_query_result_get_value_length(packet));
        	CHECK_EQUAL_ARRAY((uint8_t*)short_value, (uint8_t *)gatt_event_characteristic_value_query_result_get_value(packet), short_value_length);
        	result_counter++;
        	break;
        case GATT_EVENT_LONG_CHARACTERISTIC_VALUE_QUERY_RESULT:
        case GATT_EVENT_LONG_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT:
        	verify_blob(gatt_event_long_characteristic_value_query_result_get_value_length(packet),
                        gatt_event_long_characteristic_value_query_result_get_value_offset(packet),
                        (uint8_t *) gatt_event_long_characteristic_value_query_result_get_value(packet));
        	result_counter++;
        	break;
	}
}

extern "C" int att_write_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
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
			if (transaction_mode == ATT_TRANSACTION_MODE_VALIDATE) break;
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

// static const char * decode_status(uint8_t status){
// 	switch (status){
// 		case 0: return "0";
//     	case GATT_CLIENT_IN_WRONG_STATE: return "GATT_CLIENT_IN_WRONG_STATE";
//     	case GATT_CLIENT_DIFFERENT_CONTEXT_FOR_ADDRESS_ALREADY_EXISTS: return "GATT_CLIENT_DIFFERENT_CONTEXT_FOR_ADDRESS_ALREADY_EXISTS";
//     	case GATT_CLIENT_NOT_CONNECTED: return "GATT_CLIENT_NOT_CONNECTED";
//     	case GATT_CLIENT_VALUE_TOO_LONG: return "GATT_CLIENT_VALUE_TOO_LONG";
// 	    case GATT_CLIENT_BUSY: return "GATT_CLIENT_BUSY";
// 	    case GATT_CLIENT_CHARACTERISTIC_NOTIFICATION_NOT_SUPPORTED: return "GATT_CLIENT_CHARACTERISTIC_NOTIFICATION_NOT_SUPPORTED";
// 	    case GATT_CLIENTCHARACTERISTIC_INDICATION_NOT_SUPPORTED: return "GATT_CLIENTCHARACTERISTIC_INDICATION_NOT_SUPPORTED";
// 	}
// }

TEST_GROUP(GATTClient){
	int acl_buffer_size;
    uint8_t acl_buffer[27];
    uint8_t status;

	void setup(void){
		result_counter = 0;
		result_index = 0;
		test = IDLE;
		hci_setup_le_connection(gatt_client_handle);
        memset(characteristics, 0, sizeof(characteristics));
        memset(descriptors, 0, sizeof(descriptors));
        l2cap_set_can_send_fixed_channel_packet_now(true);
	}

	gatt_client_t * get_gatt_client(hci_con_handle_t con_handle){
        gatt_client_t * gatt_client;
		(void) gatt_client_get_client(gatt_client_handle, &gatt_client);
		return gatt_client;
	}

	void reset_query_state(void){
        gatt_client_t * gatt_client = get_gatt_client(gatt_client_handle);
		gatt_client->state = P_READY;
		
		gatt_client_set_required_security_level(LEVEL_0);
		gatt_client_mtu_enable_auto_negotiation(1);

		gatt_query_complete = 0;
		result_counter = 0;
		result_index = 0;
        l2cap_set_can_send_fixed_channel_packet_now(true);
	}

	void set_wrong_gatt_client_state(void){
		gatt_client_t * gatt_client = get_gatt_client(gatt_client_handle);
	    CHECK_TRUE(gatt_client != NULL);
	    gatt_client->state = P_W2_SEND_SERVICE_QUERY;
	}
};

TEST(GATTClient, gatt_client_setters){
	gatt_client_set_required_security_level(LEVEL_4);
	gatt_client_mtu_enable_auto_negotiation(0);
}

TEST(GATTClient, gatt_client_is_ready_mtu_exchange_disabled){
	gatt_client_mtu_enable_auto_negotiation(0);
	int status = gatt_client_is_ready(gatt_client_handle);
	CHECK_EQUAL(1, status);
}

TEST(GATTClient, TestDiscoverPrimaryServices){
	test = DISCOVER_PRIMARY_SERVICES;
	status = gatt_client_discover_primary_services(handle_ble_client_event, HCI_CON_HANDLE_INVALID);
	CHECK_EQUAL(ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, status);

	set_wrong_gatt_client_state();
	status = gatt_client_discover_primary_services(handle_ble_client_event, gatt_client_handle);
	CHECK_EQUAL(GATT_CLIENT_IN_WRONG_STATE, status);

	reset_query_state();
	status = gatt_client_discover_primary_services(handle_ble_client_event, gatt_client_handle);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	verify_primary_services();
	CHECK_EQUAL(1, gatt_query_complete);
}

TEST(GATTClient, TestDiscoverPrimaryServicesByUUID16){
	test = DISCOVER_PRIMARY_SERVICE_WITH_UUID16;
	reset_query_state();
	status = gatt_client_discover_primary_services_by_uuid16(handle_ble_client_event, gatt_client_handle, service_uuid16);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, result_counter);
	verify_primary_services_with_uuid16();
	CHECK_EQUAL(1, gatt_query_complete);
}

TEST(GATTClient, TestDiscoverPrimaryServicesByUUID128){
	test = DISCOVER_PRIMARY_SERVICE_WITH_UUID128;
	status = gatt_client_discover_primary_services_by_uuid128(handle_ble_client_event, gatt_client_handle, primary_service_uuid128);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, result_counter);
	verify_primary_services_with_uuid128();
	CHECK_EQUAL(1, gatt_query_complete);

	// invalid con handle
    status = gatt_client_discover_primary_services_by_uuid128(handle_ble_client_event, HCI_CON_HANDLE_INVALID, primary_service_uuid128);
    CHECK_EQUAL(ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, status);

    set_wrong_gatt_client_state();
    status = gatt_client_discover_primary_services_by_uuid128(handle_ble_client_event, gatt_client_handle, primary_service_uuid128);
    CHECK_EQUAL(GATT_CLIENT_IN_WRONG_STATE, status);
}

TEST(GATTClient, TestFindIncludedServicesForServiceWithUUID16){
	test = DISCOVER_INCLUDED_SERVICE_FOR_SERVICE_WITH_UUID16;
	reset_query_state();
	status = gatt_client_discover_primary_services_by_uuid16(handle_ble_client_event, gatt_client_handle, service_uuid16);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);

	reset_query_state();
	status = gatt_client_find_included_services_for_service(handle_ble_client_event, gatt_client_handle, &services[0]);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	verify_included_services_uuid16();
}

TEST(GATTClient, TestFindIncludedServicesForServiceWithUUID128){
	test = DISCOVER_INCLUDED_SERVICE_FOR_SERVICE_WITH_UUID128;
	reset_query_state();
	status = gatt_client_discover_primary_services_by_uuid128(handle_ble_client_event, gatt_client_handle, primary_service_uuid128);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);

	reset_query_state();
	status = gatt_client_find_included_services_for_service(handle_ble_client_event, gatt_client_handle, &services[0]);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	verify_included_services_uuid128();

	// invalid con handle
    status = gatt_client_find_included_services_for_service(handle_ble_client_event, HCI_CON_HANDLE_INVALID, &services[0]);
    CHECK_EQUAL(ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, status);

    set_wrong_gatt_client_state();
    status = gatt_client_find_included_services_for_service(handle_ble_client_event, gatt_client_handle, &services[0]);
    CHECK_EQUAL(GATT_CLIENT_IN_WRONG_STATE, status);
}

TEST(GATTClient, TestDiscoverCharacteristicsForService){
	test = DISCOVER_CHARACTERISTICS_FOR_SERVICE_WITH_UUID16;
	reset_query_state();
	status = gatt_client_discover_primary_services_by_uuid16(handle_ble_client_event, gatt_client_handle, service_uuid16);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	
	reset_query_state();
	status = gatt_client_discover_characteristics_for_service(handle_ble_client_event, gatt_client_handle, &services[0]);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	verify_charasteristics();

	// invalid con handle
    status = gatt_client_discover_characteristics_for_service(handle_ble_client_event, HCI_CON_HANDLE_INVALID, &services[0]);
    CHECK_EQUAL(ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, status);

    set_wrong_gatt_client_state();
    status = gatt_client_discover_characteristics_for_service(handle_ble_client_event, gatt_client_handle, &services[0]);
    CHECK_EQUAL(GATT_CLIENT_IN_WRONG_STATE, status);
}

TEST(GATTClient, TestDiscoverCharacteristicsByUUID16){
	test = DISCOVER_CHARACTERISTICS_BY_UUID16;
	reset_query_state();
	status = gatt_client_discover_characteristics_for_handle_range_by_uuid16(handle_ble_client_event, gatt_client_handle, 0x30, 0x32, 0xF102);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);

	// invalid con handle
    status = gatt_client_discover_characteristics_for_handle_range_by_uuid16(handle_ble_client_event, HCI_CON_HANDLE_INVALID, 0x30, 0x32, 0xF102);
    CHECK_EQUAL(ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, status);

    set_wrong_gatt_client_state();
    status = gatt_client_discover_characteristics_for_handle_range_by_uuid16(handle_ble_client_event, gatt_client_handle, 0x30, 0x32, 0xF102);
    CHECK_EQUAL(GATT_CLIENT_IN_WRONG_STATE, status);
}

TEST(GATTClient, TestDiscoverCharacteristicsByUUID128){
	test = DISCOVER_CHARACTERISTICS_BY_UUID128;
	reset_query_state();
	status = gatt_client_discover_characteristics_for_handle_range_by_uuid128(handle_ble_client_event, gatt_client_handle, characteristic_handles[1][0], characteristic_handles[1][1], characteristic_uuids[1]);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);

	// invalid con handle
    status = gatt_client_discover_characteristics_for_handle_range_by_uuid128(handle_ble_client_event, HCI_CON_HANDLE_INVALID, characteristic_handles[1][0], characteristic_handles[1][1], characteristic_uuids[1]);
    CHECK_EQUAL(ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, status);

    set_wrong_gatt_client_state();
    status = gatt_client_discover_characteristics_for_handle_range_by_uuid128(handle_ble_client_event, gatt_client_handle, characteristic_handles[1][0], characteristic_handles[1][1], characteristic_uuids[1]);
    CHECK_EQUAL(GATT_CLIENT_IN_WRONG_STATE, status);
}

TEST(GATTClient, TestDiscoverCharacteristics4ServiceByUUID128){
	test = DISCOVER_CHARACTERISTICS_FOR_SERVICE_BY_UUID;
	reset_query_state();
	status = gatt_client_discover_primary_services_by_uuid128(handle_ble_client_event, gatt_client_handle, primary_service_uuid128);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);

	reset_query_state();
	uint8_t characteristic_uuid[] = {0x00, 0x00, 0xF2, 0x01, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB};
	status = gatt_client_discover_characteristics_for_service_by_uuid128(handle_ble_client_event, gatt_client_handle, &services[0], characteristic_uuid);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);

	reset_query_state();
	status = gatt_client_discover_characteristics_for_service_by_uuid16(handle_ble_client_event, gatt_client_handle, &services[0], 0xF200);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);
}

TEST(GATTClient, TestDiscoverCharacteristics4ServiceByUUID16){
	test = DISCOVER_CHARACTERISTICS_FOR_SERVICE_BY_UUID;
	reset_query_state();
	status = gatt_client_discover_primary_services_by_uuid16(handle_ble_client_event, gatt_client_handle, service_uuid16);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);

	reset_query_state();
	uint8_t characteristic_uuid[]= { 0x00, 0x00, 0xF1, 0x05, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB};
	status = gatt_client_discover_characteristics_for_service_by_uuid128(handle_ble_client_event, gatt_client_handle, &services[0], characteristic_uuid);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);

	reset_query_state();
	status = gatt_client_discover_characteristics_for_service_by_uuid16(handle_ble_client_event, gatt_client_handle, &services[0], 0xF100);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);
}

TEST(GATTClient, TestDiscoverCharacteristicDescriptor){
	test = DISCOVER_CHARACTERISTIC_DESCRIPTORS;
	
	reset_query_state();
	// invalid con handle
	status = gatt_client_discover_primary_services_by_uuid16(handle_ble_client_event, HCI_CON_HANDLE_INVALID, service_uuid16);
	CHECK_EQUAL(ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, status);

	set_wrong_gatt_client_state();
	status = gatt_client_discover_primary_services_by_uuid16(handle_ble_client_event, gatt_client_handle, service_uuid16);
	CHECK_EQUAL(GATT_CLIENT_IN_WRONG_STATE, status);

	reset_query_state();
	status = gatt_client_discover_primary_services_by_uuid16(handle_ble_client_event, gatt_client_handle, service_uuid16);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);

	reset_query_state();
	status = gatt_client_discover_characteristics_for_service_by_uuid16(handle_ble_client_event, gatt_client_handle, &services[0], 0xF100);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);

	reset_query_state();
	status = gatt_client_discover_characteristic_descriptors(handle_ble_client_event, gatt_client_handle, &characteristics[0]);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK(result_counter);
	CHECK_EQUAL(3, result_index);
	CHECK_EQUAL(0x2902, descriptors[0].uuid16);
	CHECK_EQUAL(0x2900, descriptors[1].uuid16);
	CHECK_EQUAL(0x2901, descriptors[2].uuid16);
}

TEST(GATTClient, TestWriteClientCharacteristicConfiguration){
	test = WRITE_CLIENT_CHARACTERISTIC_CONFIGURATION;
	reset_query_state();
	status = gatt_client_discover_primary_services_by_uuid16(handle_ble_client_event, gatt_client_handle, service_uuid16);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);

	reset_query_state();
	status = gatt_client_discover_characteristics_for_service_by_uuid16(handle_ble_client_event, gatt_client_handle, &services[0], 0xF100);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);

	// invalid con handle
    status = gatt_client_write_client_characteristic_configuration(handle_ble_client_event, HCI_CON_HANDLE_INVALID, &characteristics[0], GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION);
 	CHECK_EQUAL(ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, status);

    set_wrong_gatt_client_state();
    status = gatt_client_write_client_characteristic_configuration(handle_ble_client_event, gatt_client_handle, &characteristics[0], GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION);
 	CHECK_EQUAL(GATT_CLIENT_IN_WRONG_STATE, status);

	reset_query_state();
	status = gatt_client_write_client_characteristic_configuration(handle_ble_client_event, gatt_client_handle, &characteristics[0], GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION);
 	CHECK_EQUAL(0, status);
 	CHECK_EQUAL(1, gatt_query_complete);
 	CHECK_EQUAL(1, result_counter);

 	reset_query_state();
	characteristics->properties = 0;
	status = gatt_client_write_client_characteristic_configuration(handle_ble_client_event, gatt_client_handle, &characteristics[0], GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION);
 	CHECK_EQUAL(GATT_CLIENT_CHARACTERISTIC_NOTIFICATION_NOT_SUPPORTED, status);

 	reset_query_state();
	characteristics->properties = 0;
	status = gatt_client_write_client_characteristic_configuration(handle_ble_client_event, gatt_client_handle, &characteristics[0], GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_INDICATION);
 	CHECK_EQUAL(GATT_CLIENT_CHARACTERISTIC_INDICATION_NOT_SUPPORTED, status);

 	reset_query_state();
	status = gatt_client_write_client_characteristic_configuration(handle_ble_client_event, gatt_client_handle, &characteristics[0], 10);
 	CHECK_EQUAL(ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE, status);
}

TEST(GATTClient, TestWriteClientCharacteristicConfigurationNone){
 	reset_query_state();
	status = gatt_client_discover_primary_services_by_uuid16(handle_ble_client_event, gatt_client_handle, service_uuid16);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);

	reset_query_state();
	status = gatt_client_write_client_characteristic_configuration(handle_ble_client_event, gatt_client_handle, &characteristics[0], GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NONE);
 	CHECK_EQUAL(ERROR_CODE_SUCCESS, status);
 }

TEST(GATTClient, TestWriteNonexistingClientCharacteristicConfiguration){
	test = WRITE_NONEXISTING_CLIENT_CHARACTERISTIC_CONFIGURATION;
	reset_query_state();
	status = gatt_client_discover_primary_services_by_uuid16(handle_ble_client_event, gatt_client_handle, service_uuid16);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);

	reset_query_state();
	characteristics->properties = ATT_PROPERTY_INDICATE;
	status = gatt_client_write_client_characteristic_configuration(handle_ble_client_event, gatt_client_handle, &characteristics[0], GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_INDICATION);
 	CHECK_EQUAL(ERROR_CODE_SUCCESS, status);
}

TEST(GATTClient, TestReadCharacteristicDescriptor){
	test = READ_CHARACTERISTIC_DESCRIPTOR;
	reset_query_state();
	status = gatt_client_discover_primary_services_by_uuid16(handle_ble_client_event, gatt_client_handle, service_uuid16);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);

	reset_query_state();
	status = gatt_client_discover_characteristics_for_service_by_uuid16(handle_ble_client_event, gatt_client_handle, &services[0], 0xF100);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);

	reset_query_state();
	status = gatt_client_discover_characteristic_descriptors(handle_ble_client_event, gatt_client_handle, &characteristics[0]);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(3, result_counter);

	// invalid con handle
    status = gatt_client_discover_characteristic_descriptors(handle_ble_client_event, HCI_CON_HANDLE_INVALID,  &characteristics[0]);
    CHECK_EQUAL(ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, status);

    set_wrong_gatt_client_state();
    status = gatt_client_discover_characteristic_descriptors(handle_ble_client_event, gatt_client_handle,  &characteristics[0]);
    CHECK_EQUAL(GATT_CLIENT_IN_WRONG_STATE, status);

	reset_query_state();
	uint16_t end_handle = characteristics[0].end_handle;
	characteristics[0].end_handle = characteristics[0].value_handle;
	status = gatt_client_discover_characteristic_descriptors(handle_ble_client_event, gatt_client_handle, &characteristics[0]);
	CHECK_EQUAL(0, status);
	characteristics[0].end_handle = end_handle;

	reset_query_state();
	status = gatt_client_read_characteristic_descriptor(handle_ble_client_event, gatt_client_handle, &descriptors[0]);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(3, result_counter);

	// invalid con handle
	status = gatt_client_read_characteristic_descriptor(handle_ble_client_event, HCI_CON_HANDLE_INVALID, &descriptors[0]);
    CHECK_EQUAL(ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, status);

    set_wrong_gatt_client_state();
	status = gatt_client_read_characteristic_descriptor(handle_ble_client_event, gatt_client_handle, &descriptors[0]);
    CHECK_EQUAL(GATT_CLIENT_IN_WRONG_STATE, status);
}

TEST(GATTClient, gatt_client_read_value_of_characteristic_using_value_handle){
	test = WRITE_CLIENT_CHARACTERISTIC_CONFIGURATION;
	reset_query_state();
	status = gatt_client_discover_primary_services_by_uuid16(handle_ble_client_event, gatt_client_handle, service_uuid16);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);

	reset_query_state();
	status = gatt_client_discover_characteristics_for_service_by_uuid16(handle_ble_client_event, gatt_client_handle, &services[0], 0xF100);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);

	// invalid con handle
    status = gatt_client_read_value_of_characteristic_using_value_handle(handle_ble_client_event, HCI_CON_HANDLE_INVALID, characteristics[0].value_handle);
 	CHECK_EQUAL(ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, status);

    set_wrong_gatt_client_state();
    status = gatt_client_read_value_of_characteristic_using_value_handle(handle_ble_client_event, gatt_client_handle, characteristics[0].value_handle);
 	CHECK_EQUAL(GATT_CLIENT_IN_WRONG_STATE, status);
	reset_query_state();
}

TEST(GATTClient, gatt_client_read_long_value_of_characteristic_using_value_handle_with_context){
	test = WRITE_CLIENT_CHARACTERISTIC_CONFIGURATION;
	reset_query_state();
	status = gatt_client_discover_primary_services_by_uuid16(handle_ble_client_event, gatt_client_handle, service_uuid16);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);

	reset_query_state();
	status = gatt_client_discover_characteristics_for_service_by_uuid16(handle_ble_client_event, gatt_client_handle, &services[0], 0xF100);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);

	// invalid con handle
    status = gatt_client_read_long_value_of_characteristic_using_value_handle_with_context(handle_ble_client_event, HCI_CON_HANDLE_INVALID, characteristics[0].value_handle, 0xF100, 0);
 	CHECK_EQUAL(ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, status);

    set_wrong_gatt_client_state();
    status = gatt_client_read_long_value_of_characteristic_using_value_handle_with_context(handle_ble_client_event, gatt_client_handle, characteristics[0].value_handle, 0xF100, 0);
 	CHECK_EQUAL(GATT_CLIENT_IN_WRONG_STATE, status);
	reset_query_state();
}

TEST(GATTClient, gatt_client_read_long_value_of_characteristic_using_value_handle_with_offset){
	test = WRITE_CLIENT_CHARACTERISTIC_CONFIGURATION;
	reset_query_state();
	status = gatt_client_discover_primary_services_by_uuid16(handle_ble_client_event, gatt_client_handle, service_uuid16);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);

	reset_query_state();
	status = gatt_client_discover_characteristics_for_service_by_uuid16(handle_ble_client_event, gatt_client_handle, &services[0], 0xF100);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);

	// invalid con handle
    status = gatt_client_read_long_value_of_characteristic_using_value_handle_with_offset(handle_ble_client_event, HCI_CON_HANDLE_INVALID, characteristics[0].value_handle, 0);
 	CHECK_EQUAL(ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, status);

    set_wrong_gatt_client_state();
    status = gatt_client_read_long_value_of_characteristic_using_value_handle_with_offset(handle_ble_client_event, gatt_client_handle, characteristics[0].value_handle, 0);
 	CHECK_EQUAL(GATT_CLIENT_IN_WRONG_STATE, status);
	reset_query_state();
}


TEST(GATTClient, gatt_client_read_value_of_characteristics_by_uuid16){
	test = WRITE_CLIENT_CHARACTERISTIC_CONFIGURATION;
	reset_query_state();
	status = gatt_client_discover_primary_services_by_uuid16(handle_ble_client_event, gatt_client_handle, service_uuid16);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);

	reset_query_state();
	status = gatt_client_discover_characteristics_for_service_by_uuid16(handle_ble_client_event, gatt_client_handle, &services[0], 0xF100);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);

	reset_query_state();
	status = gatt_client_read_value_of_characteristics_by_uuid16(handle_ble_client_event, gatt_client_handle, characteristics[0].start_handle, characteristics[0].end_handle, characteristics[0].uuid16);
 	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);

	// invalid con handle
    status = gatt_client_read_value_of_characteristics_by_uuid16(handle_ble_client_event, HCI_CON_HANDLE_INVALID, characteristics[0].start_handle, characteristics[0].end_handle, characteristics[0].uuid16);
 	CHECK_EQUAL(ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, status);

    set_wrong_gatt_client_state();
    status = gatt_client_read_value_of_characteristics_by_uuid16(handle_ble_client_event, gatt_client_handle, characteristics[0].start_handle, characteristics[0].end_handle, characteristics[0].uuid16);
 	CHECK_EQUAL(GATT_CLIENT_IN_WRONG_STATE, status);
	reset_query_state();
}

TEST(GATTClient, gatt_client_read_value_of_characteristics_by_uuid128){
	test = WRITE_CLIENT_CHARACTERISTIC_CONFIGURATION;
	reset_query_state();
	status = gatt_client_discover_primary_services_by_uuid16(handle_ble_client_event, gatt_client_handle, service_uuid16);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);

	reset_query_state();
	status = gatt_client_discover_characteristics_for_service_by_uuid16(handle_ble_client_event, gatt_client_handle, &services[0], 0xF100);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);

	reset_query_state();
	status = gatt_client_read_value_of_characteristics_by_uuid128(handle_ble_client_event, gatt_client_handle, characteristics[0].start_handle, characteristics[0].end_handle, characteristics[0].uuid128);
 	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);

	// invalid con handle
    status = gatt_client_read_value_of_characteristics_by_uuid128(handle_ble_client_event, HCI_CON_HANDLE_INVALID, characteristics[0].start_handle, characteristics[0].end_handle, characteristics[0].uuid128);
 	CHECK_EQUAL(ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, status);

    set_wrong_gatt_client_state();
    status = gatt_client_read_value_of_characteristics_by_uuid128(handle_ble_client_event, gatt_client_handle, characteristics[0].start_handle, characteristics[0].end_handle, characteristics[0].uuid128);
 	CHECK_EQUAL(GATT_CLIENT_IN_WRONG_STATE, status);
	reset_query_state();
}

TEST(GATTClient, gatt_client_write_characteristic_descriptor_using_descriptor_handle){
	test = WRITE_CLIENT_CHARACTERISTIC_CONFIGURATION;
	reset_query_state();
	status = gatt_client_discover_primary_services_by_uuid16(handle_ble_client_event, gatt_client_handle, service_uuid16);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);

	reset_query_state();
	status = gatt_client_discover_characteristics_for_service_by_uuid16(handle_ble_client_event, gatt_client_handle, &services[0], 0xF100);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);


	reset_query_state();
	status = gatt_client_discover_characteristic_descriptors(handle_ble_client_event, gatt_client_handle, &characteristics[0]);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(3, result_counter);

	reset_query_state();
	status = gatt_client_write_characteristic_descriptor_using_descriptor_handle(handle_ble_client_event, gatt_client_handle, descriptors[0].handle, characteristics[0].end_handle, characteristics[0].uuid128);
 	CHECK_EQUAL(0, status);

	// invalid con handle
    status = gatt_client_write_characteristic_descriptor_using_descriptor_handle(handle_ble_client_event, HCI_CON_HANDLE_INVALID, descriptors[0].handle, characteristics[0].end_handle, characteristics[0].uuid128);
 	CHECK_EQUAL(ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, status);

    set_wrong_gatt_client_state();
    status = gatt_client_write_characteristic_descriptor_using_descriptor_handle(handle_ble_client_event, gatt_client_handle, descriptors[0].handle, characteristics[0].end_handle, characteristics[0].uuid128);
 	CHECK_EQUAL(GATT_CLIENT_IN_WRONG_STATE, status);
	reset_query_state();
}

TEST(GATTClient, gatt_client_prepare_write){
	reset_query_state();
	status = gatt_client_discover_primary_services_by_uuid16(handle_ble_client_event, gatt_client_handle, service_uuid16);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);

	reset_query_state();
	status = gatt_client_discover_characteristics_for_service_by_uuid16(handle_ble_client_event, gatt_client_handle, &services[0], 0xF100);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);

	reset_query_state();
	status = gatt_client_prepare_write(handle_ble_client_event, gatt_client_handle, characteristics[0].value_handle, 0, short_value_length, (uint8_t*)short_value);
 	CHECK_EQUAL(0, status);

	// invalid con handle
    status = gatt_client_prepare_write(handle_ble_client_event, HCI_CON_HANDLE_INVALID, characteristics[0].value_handle, 0, short_value_length, (uint8_t*)short_value);
 	CHECK_EQUAL(ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, status);

    set_wrong_gatt_client_state();
    status = gatt_client_prepare_write(handle_ble_client_event, gatt_client_handle, characteristics[0].value_handle, 0, short_value_length, (uint8_t*)short_value);
 	CHECK_EQUAL(GATT_CLIENT_IN_WRONG_STATE, status);
	reset_query_state();
}

TEST(GATTClient, gatt_client_execute_write){
	reset_query_state();
	status = gatt_client_execute_write(handle_ble_client_event, gatt_client_handle);
 	CHECK_EQUAL(0, status);

	// invalid con handle
    status = gatt_client_execute_write(handle_ble_client_event, HCI_CON_HANDLE_INVALID);
 	CHECK_EQUAL(ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, status);

    set_wrong_gatt_client_state();
    status = gatt_client_execute_write(handle_ble_client_event, gatt_client_handle);
 	CHECK_EQUAL(GATT_CLIENT_IN_WRONG_STATE, status);
	reset_query_state();
}

TEST(GATTClient, gatt_client_cancel_write){
	reset_query_state();
	status = gatt_client_cancel_write(handle_ble_client_event, gatt_client_handle);
 	CHECK_EQUAL(0, status);

	// invalid con handle
    status = gatt_client_cancel_write(handle_ble_client_event, HCI_CON_HANDLE_INVALID);
 	CHECK_EQUAL(ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, status);

    set_wrong_gatt_client_state();
    status = gatt_client_cancel_write(handle_ble_client_event, gatt_client_handle);
 	CHECK_EQUAL(GATT_CLIENT_IN_WRONG_STATE, status);
	reset_query_state();
}

TEST(GATTClient, TestReadCharacteristicValue){
	test = READ_CHARACTERISTIC_VALUE;
	reset_query_state();
	status = gatt_client_discover_primary_services_by_uuid16(handle_ble_client_event, gatt_client_handle, service_uuid16);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);

	reset_query_state();
	status = gatt_client_discover_characteristics_for_service_by_uuid16(handle_ble_client_event, gatt_client_handle, &services[0], 0xF100);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);

	reset_query_state();
	status = gatt_client_read_value_of_characteristic(handle_ble_client_event, gatt_client_handle, &characteristics[0]);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(3, result_counter);
}

TEST(GATTClient, TestWriteCharacteristicValue){
    test = WRITE_CHARACTERISTIC_VALUE;
	reset_query_state();
	status = gatt_client_discover_primary_services_by_uuid16(handle_ble_client_event, gatt_client_handle, service_uuid16);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);

	reset_query_state();
	status = gatt_client_discover_characteristics_for_service_by_uuid16(handle_ble_client_event, gatt_client_handle, &services[0], 0xF100);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);

// invalid con handle
    status = gatt_client_write_value_of_characteristic(handle_ble_client_event, HCI_CON_HANDLE_INVALID, characteristics[0].value_handle, short_value_length, (uint8_t*)short_value);
	CHECK_EQUAL(ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, status);

    set_wrong_gatt_client_state();
    status = gatt_client_write_value_of_characteristic(handle_ble_client_event, gatt_client_handle, characteristics[0].value_handle, short_value_length, (uint8_t*)short_value);
	CHECK_EQUAL(GATT_CLIENT_IN_WRONG_STATE, status);


	reset_query_state();
	status = gatt_client_write_value_of_characteristic(handle_ble_client_event, gatt_client_handle, characteristics[0].value_handle, short_value_length, (uint8_t*)short_value);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
}

TEST(GATTClient, TestWriteCharacteristicDescriptor){
	test = WRITE_CHARACTERISTIC_DESCRIPTOR;
	reset_query_state();
	status = gatt_client_discover_primary_services_by_uuid16(handle_ble_client_event, gatt_client_handle, service_uuid16);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);

	reset_query_state();
	status = gatt_client_discover_characteristics_for_service_by_uuid16(handle_ble_client_event, gatt_client_handle, &services[0], 0xF100);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);

	reset_query_state();
	status = gatt_client_discover_characteristic_descriptors(handle_ble_client_event, gatt_client_handle, &characteristics[0]);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(3, result_counter);
	
	reset_query_state();
	status = gatt_client_write_characteristic_descriptor(handle_ble_client_event, gatt_client_handle, &descriptors[0], sizeof(indication), indication);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
}

TEST(GATTClient, TestReadLongCharacteristicValue){
	test = READ_LONG_CHARACTERISTIC_VALUE;
	reset_query_state();
	status = gatt_client_discover_primary_services_by_uuid16(handle_ble_client_event, gatt_client_handle, service_uuid16);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);

	reset_query_state();
	status = gatt_client_discover_characteristics_for_service_by_uuid16(handle_ble_client_event, gatt_client_handle, &services[0], 0xF100);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);

	reset_query_state();
	status = gatt_client_read_long_value_of_characteristic(handle_ble_client_event, gatt_client_handle, &characteristics[0]);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(7, result_counter);
}

TEST(GATTClient, TestReadLongCharacteristicDescriptor){
	test = READ_LONG_CHARACTERISTIC_DESCRIPTOR;
	reset_query_state();
	status = gatt_client_discover_primary_services_by_uuid128(handle_ble_client_event, gatt_client_handle, primary_service_uuid128);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);

	reset_query_state();
	status = gatt_client_discover_characteristics_for_service_by_uuid16(handle_ble_client_event, gatt_client_handle, &services[0], 0xF200);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);

	reset_query_state();
	status = gatt_client_discover_characteristic_descriptors(handle_ble_client_event, gatt_client_handle, &characteristics[0]);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(3, result_counter);

	reset_query_state();
	result_counter = 0;
	status = gatt_client_read_long_characteristic_descriptor(handle_ble_client_event, gatt_client_handle, &descriptors[0]);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(7, result_counter);
}


TEST(GATTClient, TestWriteLongCharacteristicDescriptor){
	test = WRITE_LONG_CHARACTERISTIC_DESCRIPTOR;
	reset_query_state();
	status = gatt_client_discover_primary_services_by_uuid128(handle_ble_client_event, gatt_client_handle, primary_service_uuid128);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);

	reset_query_state();
	status = gatt_client_discover_characteristics_for_service_by_uuid16(handle_ble_client_event, gatt_client_handle, &services[0], 0xF200);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);

	reset_query_state();
	status = gatt_client_discover_characteristic_descriptors(handle_ble_client_event, gatt_client_handle, &characteristics[0]);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(3, result_counter);

	result_counter = 0;
	status = gatt_client_write_long_characteristic_descriptor(handle_ble_client_event, gatt_client_handle, &descriptors[0], sizeof(long_value), (uint8_t *)long_value);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);

	result_counter = 0;
	status = gatt_client_write_long_characteristic_descriptor_using_descriptor_handle_with_offset(handle_ble_client_event, HCI_CON_HANDLE_INVALID, descriptors[0].handle, 0, sizeof(long_value), (uint8_t *)long_value);
	CHECK_EQUAL(ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, status);
}

TEST(GATTClient, TestWriteLongCharacteristicValue){
	test = WRITE_LONG_CHARACTERISTIC_VALUE;
	reset_query_state();
	status = gatt_client_discover_primary_services_by_uuid16(handle_ble_client_event, gatt_client_handle, service_uuid16);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);

	reset_query_state();
	status = gatt_client_discover_characteristics_for_service_by_uuid16(handle_ble_client_event, gatt_client_handle, &services[0], 0xF100);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);


	reset_query_state();
	status = gatt_client_write_long_value_of_characteristic(handle_ble_client_event, gatt_client_handle, characteristics[0].value_handle, long_value_length, (uint8_t*)long_value);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
}

TEST(GATTClient, TestWriteReliableLongCharacteristicValue){
	test = WRITE_RELIABLE_LONG_CHARACTERISTIC_VALUE;
	reset_query_state();
	status = gatt_client_discover_primary_services_by_uuid16(handle_ble_client_event, gatt_client_handle, service_uuid16);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);

	reset_query_state();
	status = gatt_client_discover_characteristics_for_service_by_uuid16(handle_ble_client_event, gatt_client_handle, &services[0], 0xF100);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);

	// invalid con handle
	status = gatt_client_reliable_write_long_value_of_characteristic(handle_ble_client_event, HCI_CON_HANDLE_INVALID, characteristics[0].value_handle, long_value_length, (uint8_t*)long_value);
    CHECK_EQUAL(ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, status);

    set_wrong_gatt_client_state();
    status = gatt_client_reliable_write_long_value_of_characteristic(handle_ble_client_event, gatt_client_handle, characteristics[0].value_handle, long_value_length, (uint8_t*)long_value);
    CHECK_EQUAL(GATT_CLIENT_IN_WRONG_STATE, status);

    reset_query_state();
    status = gatt_client_reliable_write_long_value_of_characteristic(handle_ble_client_event, gatt_client_handle, characteristics[0].value_handle, long_value_length, (uint8_t*)long_value);
    CHECK_EQUAL(0, status);
    CHECK_EQUAL(1, gatt_query_complete);

    set_wrong_gatt_client_state();
	status = gatt_client_write_long_value_of_characteristic_with_context(handle_ble_client_event, gatt_client_handle, characteristics[0].value_handle, long_value_length, (uint8_t*)long_value, 0xF100, 0);
    CHECK_EQUAL(GATT_CLIENT_IN_WRONG_STATE, status);

	reset_query_state();
	status = gatt_client_write_long_value_of_characteristic_with_context(handle_ble_client_event, gatt_client_handle, characteristics[0].value_handle, long_value_length, (uint8_t*)long_value, 0xF100, 0);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
}

TEST(GATTClient, gatt_client_write_long_value_of_characteristic_with_offset){
	reset_query_state();
	status = gatt_client_discover_primary_services_by_uuid16(handle_ble_client_event, gatt_client_handle, service_uuid16);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);

	reset_query_state();
	status = gatt_client_discover_characteristics_for_service_by_uuid16(handle_ble_client_event, gatt_client_handle, &services[0], 0xF100);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);

	reset_query_state();
	status = gatt_client_write_long_value_of_characteristic_with_offset(handle_ble_client_event, gatt_client_handle, characteristics[0].value_handle, 0, long_value_length, (uint8_t*)long_value);
	CHECK_EQUAL(0, status);

	reset_query_state();
	// invalid con handle
	status = gatt_client_write_long_value_of_characteristic_with_offset(handle_ble_client_event, HCI_CON_HANDLE_INVALID, characteristics[0].value_handle, 0, long_value_length, (uint8_t*)long_value);
    CHECK_EQUAL(ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, status);

    reset_query_state();
    set_wrong_gatt_client_state();
	status = gatt_client_write_long_value_of_characteristic_with_offset(handle_ble_client_event, gatt_client_handle, characteristics[0].value_handle, 0, long_value_length, (uint8_t*)long_value);
    CHECK_EQUAL(GATT_CLIENT_IN_WRONG_STATE, status);

    	reset_query_state();
	// invalid con handle
	status = gatt_client_write_long_value_of_characteristic_with_context(handle_ble_client_event, HCI_CON_HANDLE_INVALID, characteristics[0].value_handle, long_value_length, (uint8_t*)long_value, 0, 0);
    CHECK_EQUAL(ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, status);

    reset_query_state();
    set_wrong_gatt_client_state();
	status = gatt_client_write_long_value_of_characteristic_with_context(handle_ble_client_event, gatt_client_handle, characteristics[0].value_handle, long_value_length, (uint8_t*)long_value, 0, 0);
    CHECK_EQUAL(GATT_CLIENT_IN_WRONG_STATE, status);
}

TEST(GATTClient, gatt_client_read_long_characteristic_descriptor_using_descriptor_handle_with_offset){
	reset_query_state();
	status = gatt_client_discover_primary_services_by_uuid16(handle_ble_client_event, gatt_client_handle, service_uuid16);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);

    reset_query_state();
    status = gatt_client_discover_characteristics_for_service(handle_ble_client_event, gatt_client_handle, &services[0]);
    CHECK_EQUAL(0, status);
    CHECK_EQUAL(1, gatt_query_complete);
    verify_charasteristics();

	reset_query_state();
	status = gatt_client_discover_characteristic_descriptors(handle_ble_client_event, gatt_client_handle, &characteristics[0]);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK(result_counter != 0);
	CHECK_EQUAL(3, result_index);
	CHECK_EQUAL(0x2902, descriptors[0].uuid16);
	CHECK_EQUAL(0x2900, descriptors[1].uuid16);
	CHECK_EQUAL(0x2901, descriptors[2].uuid16);

	reset_query_state();
	status = gatt_client_read_long_characteristic_descriptor_using_descriptor_handle_with_offset(handle_ble_client_event, gatt_client_handle, descriptors[0].handle, 0);
	CHECK_EQUAL(0, status);

	reset_query_state();
	// invalid con handle
	status = gatt_client_read_long_characteristic_descriptor_using_descriptor_handle_with_offset(handle_ble_client_event, HCI_CON_HANDLE_INVALID, descriptors[0].handle, 0);
    CHECK_EQUAL(ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, status);

    reset_query_state();
    set_wrong_gatt_client_state();
	status = gatt_client_read_long_characteristic_descriptor_using_descriptor_handle_with_offset(handle_ble_client_event, gatt_client_handle, descriptors[0].handle, 0);
    CHECK_EQUAL(GATT_CLIENT_IN_WRONG_STATE, status);
}

TEST(GATTClient, gatt_client_read_multiple_characteristic_values){
	reset_query_state();
	status = gatt_client_discover_primary_services_by_uuid16(handle_ble_client_event, gatt_client_handle, service_uuid16);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);

	reset_query_state();
	status = gatt_client_discover_characteristics_for_service_by_uuid16(handle_ble_client_event, gatt_client_handle, &services[0], 0xF100);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);

	uint16_t value_handles[] = {characteristics[0].value_handle};

	reset_query_state();
	status = gatt_client_read_multiple_characteristic_values(handle_ble_client_event, gatt_client_handle, 1, value_handles);
	CHECK_EQUAL(0, status);

	reset_query_state();
	// invalid con handle
	status = gatt_client_read_multiple_characteristic_values(handle_ble_client_event, HCI_CON_HANDLE_INVALID, 1, value_handles);
    CHECK_EQUAL(ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, status);
	
	reset_query_state();
    set_wrong_gatt_client_state();
	status = gatt_client_read_multiple_characteristic_values(handle_ble_client_event, gatt_client_handle, 1, value_handles);
    CHECK_EQUAL(GATT_CLIENT_IN_WRONG_STATE, status);
}

TEST(GATTClient, gatt_client_write_value_of_characteristic_without_response){
	reset_query_state();
	status = gatt_client_discover_primary_services_by_uuid16(handle_ble_client_event, gatt_client_handle, service_uuid16);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);

	reset_query_state();
	status = gatt_client_discover_characteristics_for_service_by_uuid16(handle_ble_client_event, gatt_client_handle, &services[0], 0xF100);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);

	reset_query_state();
	// invalid con handle
	status = gatt_client_write_value_of_characteristic_without_response(HCI_CON_HANDLE_INVALID, characteristics[0].value_handle, long_value_length, (uint8_t*)long_value);
    CHECK_EQUAL(ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, status);

	reset_query_state();
    set_wrong_gatt_client_state();
	status = gatt_client_write_value_of_characteristic_without_response(gatt_client_handle, characteristics[0].value_handle, long_value_length, (uint8_t*)long_value);
    CHECK_EQUAL(GATT_CLIENT_VALUE_TOO_LONG, status);

    l2cap_set_can_send_fixed_channel_packet_now(false);
    status = gatt_client_write_value_of_characteristic_without_response(gatt_client_handle, characteristics[0].value_handle, 19, (uint8_t*)long_value);
    CHECK_EQUAL(GATT_CLIENT_BUSY, status);

	reset_query_state();

	status = gatt_client_write_value_of_characteristic_without_response(gatt_client_handle, characteristics[0].value_handle, 19, (uint8_t*)long_value);
	CHECK_EQUAL(0, status);
}

TEST(GATTClient, gatt_client_is_ready){
	int status = gatt_client_is_ready(HCI_CON_HANDLE_INVALID);
	CHECK_EQUAL(0, status);

	status = gatt_client_is_ready(gatt_client_handle);
	CHECK_EQUAL(1, status);
}


TEST(GATTClient, register_for_notification){
	reset_query_state();
	status = gatt_client_discover_primary_services_by_uuid16(handle_ble_client_event, gatt_client_handle, service_uuid16);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);

	reset_query_state();
	status = gatt_client_discover_characteristics_for_service_by_uuid16(handle_ble_client_event, gatt_client_handle, &services[0], 0xF100);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);

	gatt_client_notification_t notification;

	gatt_client_listen_for_characteristic_value_updates(&notification, handle_ble_client_event, gatt_client_handle, &characteristics[0]);
	gatt_client_stop_listening_for_characteristic_value_updates(&notification);

	gatt_client_listen_for_characteristic_value_updates(&notification, handle_ble_client_event, gatt_client_handle, NULL);
	gatt_client_stop_listening_for_characteristic_value_updates(&notification);

    gatt_client_service_notification_t service_notification;
    gatt_client_listen_for_service_characteristic_value_updates(&service_notification, handle_ble_client_event, gatt_client_handle, &services[0], 0xF100, 0);
    gatt_client_stop_listening_for_service_characteristic_value_updates(&service_notification);
}

TEST(GATTClient, gatt_client_signed_write_without_response){
	reset_query_state();
	status = gatt_client_discover_primary_services_by_uuid16(handle_ble_client_event, gatt_client_handle, service_uuid16);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);

	reset_query_state();
	status = gatt_client_discover_characteristics_for_service_by_uuid16(handle_ble_client_event, gatt_client_handle, &services[0], 0xF100);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(1, gatt_query_complete);
	CHECK_EQUAL(1, result_counter);

	reset_query_state();
	// invalid con handle
	status = gatt_client_signed_write_without_response(handle_ble_client_event, HCI_CON_HANDLE_INVALID, characteristics[0].value_handle, long_value_length, (uint8_t*)long_value);
    CHECK_EQUAL(ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, status);

	reset_query_state();
    set_wrong_gatt_client_state();
	status = gatt_client_signed_write_without_response(handle_ble_client_event, gatt_client_handle, characteristics[0].value_handle, long_value_length, (uint8_t*)long_value);
    CHECK_EQUAL(GATT_CLIENT_IN_WRONG_STATE, status);

	reset_query_state();

	status = gatt_client_signed_write_without_response(handle_ble_client_event, gatt_client_handle, characteristics[0].value_handle, long_value_length, (uint8_t*)long_value);
	CHECK_EQUAL(0, status);
}

TEST(GATTClient, gatt_client_discover_secondary_services){
	reset_query_state();
	// invalid con handle
	status = gatt_client_discover_secondary_services(handle_ble_client_event, HCI_CON_HANDLE_INVALID);
    CHECK_EQUAL(ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, status);

	reset_query_state();
    set_wrong_gatt_client_state();
	status = gatt_client_discover_secondary_services(handle_ble_client_event, gatt_client_handle);
    CHECK_EQUAL(GATT_CLIENT_IN_WRONG_STATE, status);

	reset_query_state();

	status = gatt_client_discover_secondary_services(handle_ble_client_event, gatt_client_handle);
	CHECK_EQUAL(0, status);
}

TEST(GATTClient, gatt_client_request_can_write_without_response_event){
	uint8_t status = gatt_client_request_can_write_without_response_event(handle_ble_client_event, HCI_CON_HANDLE_INVALID);
	CHECK_EQUAL(ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, status);

	gatt_client_t * gatt_client = get_gatt_client(gatt_client_handle);
	CHECK_TRUE(gatt_client != NULL);
	gatt_client->write_without_response_callback = handle_ble_client_event;
	status = gatt_client_request_can_write_without_response_event(handle_ble_client_event, gatt_client_handle);
	CHECK_EQUAL(GATT_CLIENT_IN_WRONG_STATE, status);

	gatt_client->write_without_response_callback = NULL;
	status = gatt_client_request_can_write_without_response_event(handle_ble_client_event, gatt_client_handle);
	CHECK_EQUAL(ERROR_CODE_SUCCESS, status);
}

TEST(GATTClient, gatt_client_request_to_write_without_response){
    btstack_context_callback_registration_t callback_registration = { 0 };
    uint8_t status = gatt_client_request_to_write_without_response(&callback_registration, HCI_CON_HANDLE_INVALID);
    CHECK_EQUAL(ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, status);

    status = gatt_client_request_can_write_without_response_event(handle_ble_client_event, gatt_client_handle);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);
}

static void dummy_callback(void * context){
    (void) context;
}
TEST(GATTClient, gatt_client_request_to_send_gatt_query){
    btstack_context_callback_registration_t callback_registration = { 0 };
    callback_registration.callback = &dummy_callback;

    uint8_t status = gatt_client_request_to_send_gatt_query(&callback_registration, HCI_CON_HANDLE_INVALID);
    CHECK_EQUAL(ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, status);

    status = gatt_client_request_to_send_gatt_query(&callback_registration, gatt_client_handle);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);
}

TEST(GATTClient, gatt_client_send_mtu_negotiation){
	gatt_client_send_mtu_negotiation(handle_ble_client_event, HCI_CON_HANDLE_INVALID);

	gatt_client_send_mtu_negotiation(handle_ble_client_event, gatt_client_handle);

	gatt_client_t * gatt_client = get_gatt_client(gatt_client_handle);
	CHECK_TRUE(gatt_client != NULL);
	gatt_client->mtu_state = MTU_AUTO_EXCHANGE_DISABLED;
	gatt_client_send_mtu_negotiation(handle_ble_client_event, gatt_client_handle);

	gatt_client->mtu_state = SEND_MTU_EXCHANGE;
}

TEST(GATTClient, gatt_client_get_mtu){
	reset_query_state();
	uint16_t mtu;
	int status = gatt_client_get_mtu(HCI_CON_HANDLE_INVALID, &mtu);
	CHECK_EQUAL(ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, status);

	status = gatt_client_get_mtu(gatt_client_handle, &mtu);
	CHECK_EQUAL(GATT_CLIENT_IN_WRONG_STATE, status);
	CHECK_EQUAL(ATT_DEFAULT_MTU, mtu);

	gatt_client_t * gatt_client = get_gatt_client(gatt_client_handle);
	CHECK_TRUE(gatt_client != NULL);
	gatt_client->mtu = 30;
	
	gatt_client->mtu_state = MTU_EXCHANGED;
	status = gatt_client_get_mtu(gatt_client_handle, &mtu);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(gatt_client->mtu, mtu);

	gatt_client->mtu_state = MTU_AUTO_EXCHANGE_DISABLED;
	status = gatt_client_get_mtu(gatt_client_handle, &mtu);
	CHECK_EQUAL(0, status);
	CHECK_EQUAL(gatt_client->mtu, mtu);

	gatt_client->mtu_state = SEND_MTU_EXCHANGE;
}

TEST(GATTClient, gatt_client_deserialize_characteristic_descriptor){
	gatt_client_characteristic_descriptor_t descriptor;

	uint8_t packet[18];
	uint16_t expected_handle = 0x1234;
	uint16_t expected_uuid16 = 0xABCD;
	
	uint8_t uuid128[16];
	uuid_add_bluetooth_prefix(uuid128, expected_uuid16);

	little_endian_store_16(packet, 0, expected_handle);
	reverse_128(uuid128, &packet[2]);

	// uuid16 with Bluetooth prefix
	gatt_client_deserialize_characteristic_descriptor(packet, 0, &descriptor);
	CHECK_EQUAL(descriptor.handle, expected_handle);
	CHECK_EQUAL(descriptor.uuid16, expected_uuid16);
	MEMCMP_EQUAL(uuid128, descriptor.uuid128, sizeof(uuid128));

	// uuid128
	memset(&uuid128[4], 0xaa, 12);
	reverse_128(uuid128, &packet[2]);

	gatt_client_deserialize_characteristic_descriptor(packet, 0, &descriptor);
	CHECK_EQUAL(descriptor.handle, expected_handle);
	CHECK_EQUAL(descriptor.uuid16, 0);
	MEMCMP_EQUAL(uuid128, descriptor.uuid128, sizeof(uuid128));
}

TEST(GATTClient, gatt_client_deserialize_service){
	gatt_client_service_t service;

	uint8_t packet[20];
	uint16_t expected_start_group_handle = 0x1234;
	uint16_t expected_end_group_handle   = 0x5678;
	uint16_t expected_uuid16             = 0xABCD;
	
	uint8_t uuid128[16];
	uuid_add_bluetooth_prefix(uuid128, expected_uuid16);

	little_endian_store_16(packet, 0, expected_start_group_handle);
	little_endian_store_16(packet, 2, expected_end_group_handle);
	reverse_128(uuid128, &packet[4]);

	// uuid16 with Bluetooth prefix
	gatt_client_deserialize_service(packet, 0, &service);
	CHECK_EQUAL(service.start_group_handle, expected_start_group_handle);
	CHECK_EQUAL(service.end_group_handle  , expected_end_group_handle);
	CHECK_EQUAL(service.uuid16, expected_uuid16);
	MEMCMP_EQUAL(uuid128, service.uuid128, sizeof(uuid128));

	// uuid128
	memset(&uuid128[4], 0xaa, 12);
	reverse_128(uuid128, &packet[4]);

	gatt_client_deserialize_service(packet, 0, &service);
	CHECK_EQUAL(service.start_group_handle, expected_start_group_handle);
	CHECK_EQUAL(service.end_group_handle  , expected_end_group_handle);
	CHECK_EQUAL(service.uuid16, 0);
	MEMCMP_EQUAL(uuid128, service.uuid128, sizeof(uuid128));
}

TEST(GATTClient, gatt_client_deserialize_characteristic){
	gatt_client_characteristic_t characteristic;

	uint8_t packet[24];
	uint16_t expected_start_handle = 0x1234;
	uint16_t expected_value_handle = 0x3455;
	uint16_t expected_end_handle   = 0x5678;
	uint16_t expected_properties   = 0x6789;
	
	uint16_t expected_uuid16             = 0xABCD;
	
	uint8_t uuid128[16];
	uuid_add_bluetooth_prefix(uuid128, expected_uuid16);

	little_endian_store_16(packet, 0, expected_start_handle);
	little_endian_store_16(packet, 2, expected_value_handle);
	little_endian_store_16(packet, 4, expected_end_handle);
	little_endian_store_16(packet, 6, expected_properties);
	reverse_128(uuid128, &packet[8]);

	// uuid16 with Bluetooth prefix
	gatt_client_deserialize_characteristic(packet, 0, &characteristic);
	CHECK_EQUAL(characteristic.start_handle, expected_start_handle);
	CHECK_EQUAL(characteristic.value_handle, expected_value_handle);
	CHECK_EQUAL(characteristic.end_handle  , expected_end_handle);
	CHECK_EQUAL(characteristic.properties  , expected_properties);
	CHECK_EQUAL(characteristic.uuid16, expected_uuid16);
	MEMCMP_EQUAL(uuid128, characteristic.uuid128, sizeof(uuid128));

	// uuid128
	memset(&uuid128[4], 0xaa, 12);
	reverse_128(uuid128, &packet[8]);

	gatt_client_deserialize_characteristic(packet, 0, &characteristic);
	CHECK_EQUAL(characteristic.start_handle, expected_start_handle);
	CHECK_EQUAL(characteristic.value_handle, expected_value_handle);
	CHECK_EQUAL(characteristic.end_handle  , expected_end_handle);
	CHECK_EQUAL(characteristic.properties  , expected_properties);
	CHECK_EQUAL(characteristic.uuid16, 0);
	MEMCMP_EQUAL(uuid128, characteristic.uuid128, sizeof(uuid128));
}

TEST(GATTClient, gatt_client_remove_gatt_query) {
    btstack_context_callback_registration_t callback_registration = { 0 };

    uint8_t status = gatt_client_remove_gatt_query(&callback_registration, HCI_CON_HANDLE_INVALID);
    CHECK_EQUAL(ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, status);

    status = gatt_client_remove_gatt_query(&callback_registration, gatt_client_handle);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);
}

TEST(GATTClient, gatt_client_att_status_to_error_code) {
    uint8_t status = gatt_client_att_status_to_error_code(ATT_ERROR_SUCCESS);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);

    status = gatt_client_att_status_to_error_code(ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LENGTH);
    CHECK_EQUAL(ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE, status);

    status = gatt_client_att_status_to_error_code(ATT_ERROR_UNLIKELY_ERROR);
    CHECK_EQUAL(ERROR_CODE_UNSPECIFIED_ERROR, status);
}

TEST(GATTClient, gatt_client_att_packet_handler) {
    gatt_client_att_packet_handler_fuzz(0, 0, NULL, 0);

    gatt_client_att_packet_handler_fuzz(HCI_EVENT_PACKET, 0, NULL, 0);
    gatt_client_att_packet_handler_fuzz(ATT_DATA_PACKET, 0, NULL, 0);
    gatt_client_att_packet_handler_fuzz(L2CAP_DATA_PACKET, 0, NULL, 0);
    gatt_client_att_packet_handler_fuzz(10, 0, NULL, 0);

    uint8_t packet[10];
    little_endian_store_16(packet, 0, ATT_EVENT_MTU_EXCHANGE_COMPLETE);
    gatt_client_att_packet_handler_fuzz(HCI_EVENT_PACKET, HCI_CON_HANDLE_INVALID, packet, 2);
    gatt_client_att_packet_handler_fuzz(HCI_EVENT_PACKET, HCI_CON_HANDLE_INVALID, packet, 10);
    little_endian_store_16(packet, 0, ATT_EVENT_MTU_EXCHANGE_COMPLETE + 2);
    gatt_client_att_packet_handler_fuzz(HCI_EVENT_PACKET, HCI_CON_HANDLE_INVALID, packet, 10);

}

int main (int argc, const char * argv[]){
	att_set_db(profile_data);
	att_set_write_callback(&att_write_callback);
	att_set_read_callback(&att_read_callback);

	gatt_client_init();
	
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
