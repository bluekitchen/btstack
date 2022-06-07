
// *****************************************************************************
//
// test battery service
//
// *****************************************************************************


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"
#include "CppUTestExt/MockSupport.h"

#include "hci.h"
#include "gap.h"
#include "btstack_util.h"
#include "bluetooth.h"
#include "bluetooth_gatt.h"
#include "ble/le_device_db.h"
#include "ble/gatt-service/bond_management_service_server.h"
#include "bond_management_service_server_test.h"
#include "mock_att_server.h"

static uint32_t bond_management_features = 0xFFFFFF;
static uint8_t  request[550];
    
// mocks
void gap_delete_bonding(bd_addr_type_t address_type, bd_addr_t address){
    // traack call if needed
}
void gap_drop_link_key_for_bd_addr(bd_addr_t addr){
    // traack call if needed
}
int gap_link_key_iterator_init(btstack_link_key_iterator_t * it){
    return 1;
}
int gap_link_key_iterator_get_next(btstack_link_key_iterator_t * it, bd_addr_t bd_addr, link_key_t link_key, link_key_type_t * type){
    // link keys!!
    return 0;  
}
void gap_link_key_iterator_done(btstack_link_key_iterator_t * it){
}
static hci_connection_t test_connection;
hci_connection_t * hci_connection_for_handle(hci_con_handle_t con_handle){
    memset(test_connection.address, 0x33, 6);
    test_connection.address_type = BD_ADDR_TYPE_ACL;
    return &test_connection;
}
int le_device_db_max_count(void){
    return 0;
}
void le_device_db_info(int index, int * addr_type, bd_addr_t addr, sm_key_t irk){
}
//

TEST_GROUP(BOND_MANAGEMENT_SERVICE_SERVER){ 
    att_service_handler_t * service; 
    uint16_t con_handle;
    uint16_t bm_features_value_handle;
    uint16_t bm_control_point_value_handle;

    void setup(void){
        // setup database
        att_set_db(profile_data);
        bm_features_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(0, 0xffff, ORG_BLUETOOTH_CHARACTERISTIC_BOND_MANAGEMENT_FEATURE);
        bm_control_point_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(0, 0xffff, ORG_BLUETOOTH_CHARACTERISTIC_BOND_MANAGEMENT_CONTROL_POINT);
        // setup tx power service
        con_handle = 0x00;
    }

    void validate_write_control_point(uint32_t local_flag, const char * local_auth_str, uint8_t remote_cmd, const char * remote_auth_str, uint16_t expected_outcome){
        bond_management_service_server_init(local_flag);
        service = mock_att_server_get_service();
        bond_management_service_server_set_authorisation_string(local_auth_str);

        request[0] = remote_cmd;
        uint16_t remote_auth_str_len = 0;
        if (remote_auth_str != NULL) {
            remote_auth_str_len = strlen(remote_auth_str);
        }
        memcpy(&request[1], remote_auth_str, remote_auth_str_len);
        uint16_t request_len = 1 + remote_auth_str_len;
        int response = mock_att_service_write_callback(con_handle, bm_control_point_value_handle, ATT_TRANSACTION_MODE_NONE, 0, request, request_len);
        CHECK_EQUAL(expected_outcome, response); 
    }

    void teardown(){
        mock_deinit();
    }
};

TEST(BOND_MANAGEMENT_SERVICE_SERVER, lookup_attribute_handles){
    bond_management_service_server_init(bond_management_features);
    service = mock_att_server_get_service();

    CHECK(bm_features_value_handle != 0);
    CHECK(bm_control_point_value_handle != 0);
}

TEST(BOND_MANAGEMENT_SERVICE_SERVER, read_bm_features_value0){
    uint8_t response[10];
    uint16_t response_len;

    bond_management_service_server_init(0x00);
    service = mock_att_server_get_service();

    // invalid attribute handle
    response_len = mock_att_service_read_callback(con_handle, 0xffff, 0xffff, response, sizeof(response));
    CHECK_EQUAL(0, response_len);

    response_len = mock_att_service_read_callback(con_handle, bm_features_value_handle, 0, response, sizeof(response));
    CHECK_EQUAL(3, response_len);
}


TEST(BOND_MANAGEMENT_SERVICE_SERVER, read_bm_features_value1){
    uint8_t  response[10];
    uint16_t response_len;

    bond_management_service_server_init(0xFF);
    service = mock_att_server_get_service();

    response_len = mock_att_service_read_callback(con_handle, bm_features_value_handle, 0, response, sizeof(response));
    CHECK_EQUAL(3, response_len);
}

TEST(BOND_MANAGEMENT_SERVICE_SERVER, read_bm_features_value2){
    uint8_t  response[10];
    uint16_t response_len;

    bond_management_service_server_init(0xFFFF);
    service = mock_att_server_get_service();

    response_len = mock_att_service_read_callback(con_handle, bm_features_value_handle, 0, response, sizeof(response));
    CHECK_EQUAL(3, response_len);
}

TEST(BOND_MANAGEMENT_SERVICE_SERVER, read_bm_features_value3){
    uint8_t  response[10];
    uint16_t response_len;

    // invalid attribute handle
    bond_management_service_server_init(0xFFFFFF);
    service = mock_att_server_get_service();

    response_len = mock_att_service_read_callback(con_handle, bm_features_value_handle, 0, response, sizeof(response));
    CHECK_EQUAL(3, response_len);
}


TEST(BOND_MANAGEMENT_SERVICE_SERVER, write_bm_control_point_0){
    uint8_t  request[520];
    uint16_t request_len = sizeof(request);
    int response;

    // invalid attribute handle
    bond_management_service_server_init(0xFFFFFF);
    service = mock_att_server_get_service();

    // wrong handle
    response = mock_att_service_write_callback(con_handle, 0xffff, ATT_TRANSACTION_MODE_NONE, 0, request, request_len);
    CHECK_EQUAL(response, 0);

    // buffer size 0
    response = mock_att_service_write_callback(con_handle, bm_control_point_value_handle, ATT_TRANSACTION_MODE_NONE, 0, NULL, 0);
    CHECK_EQUAL(response, BOND_MANAGEMENT_CONTROL_POINT_OPCODE_NOT_SUPPORTED);

    // wrong cmd
    request[0] = 20;
    response = mock_att_service_write_callback(con_handle, bm_control_point_value_handle, ATT_TRANSACTION_MODE_NONE, 0, request, request_len);
    CHECK_EQUAL(response, BOND_MANAGEMENT_CONTROL_POINT_OPCODE_NOT_SUPPORTED);
    
    // wrong authorisation code len: request_len > 512
    request[0] = BMF_DELETE_ACTIVE_BOND_CLASSIC_AND_LE;
    response = mock_att_service_write_callback(con_handle, bm_control_point_value_handle, ATT_TRANSACTION_MODE_NONE, 0, request, request_len);
    CHECK_EQUAL(response, BOND_MANAGEMENT_OPERATION_FAILED);
}


// locally no (string empty), remote no (string empty) ->  OK
TEST(BOND_MANAGEMENT_SERVICE_SERVER, write_control_point_ln_empty_rn_empty){
    validate_write_control_point(BMF_DELETE_ACTIVE_BOND_CLASSIC_AND_LE, NULL, 
        BOND_MANAGEMENT_CMD_DELETE_ACTIVE_BOND_CLASSIC_AND_LE, NULL, 0);
}

// locally no (string exists), remote no (string empty) ->  OK
TEST(BOND_MANAGEMENT_SERVICE_SERVER, write_control_point_ln_exists_rn_empty){
    validate_write_control_point(BMF_DELETE_ACTIVE_BOND_CLASSIC_AND_LE, "local_auth_str", 
        BOND_MANAGEMENT_CMD_DELETE_ACTIVE_BOND_CLASSIC_AND_LE, NULL, 0);
}

// locally no (string empty), remote yes (string exists) -> BOND_MANAGEMENT_CONTROL_POINT_OPCODE_NOT_SUPPORTED
TEST(BOND_MANAGEMENT_SERVICE_SERVER, write_control_point_ln_empty_ry_exists){
    validate_write_control_point(BMF_DELETE_ACTIVE_BOND_CLASSIC_AND_LE, "", 
        BOND_MANAGEMENT_CMD_DELETE_ACTIVE_BOND_CLASSIC_AND_LE, "remote", 
        BOND_MANAGEMENT_CONTROL_POINT_OPCODE_NOT_SUPPORTED);
}

// locally no (string exists), remote yes (string exists) -> BOND_MANAGEMENT_CONTROL_POINT_OPCODE_NOT_SUPPORTED
TEST(BOND_MANAGEMENT_SERVICE_SERVER, write_control_point_ln_exists_ry_exists){
    validate_write_control_point(BMF_DELETE_ACTIVE_BOND_CLASSIC_AND_LE, "local_auth_str", 
        BOND_MANAGEMENT_CMD_DELETE_ACTIVE_BOND_CLASSIC_AND_LE, "remote_auth_str", 
        BOND_MANAGEMENT_CONTROL_POINT_OPCODE_NOT_SUPPORTED);
}

// locally no (string exists), remote yes, (string exact) -> BOND_MANAGEMENT_CONTROL_POINT_OPCODE_NOT_SUPPORTED
TEST(BOND_MANAGEMENT_SERVICE_SERVER, write_control_point_ln_exists_ry_exact){
    validate_write_control_point(BMF_DELETE_ACTIVE_BOND_CLASSIC_AND_LE, "auth_str", 
        BOND_MANAGEMENT_CMD_DELETE_ACTIVE_BOND_CLASSIC_AND_LE, "auth_str", 
        BOND_MANAGEMENT_CONTROL_POINT_OPCODE_NOT_SUPPORTED);
}

// locally yes (string empty), remote no (string empty) -> ATT_ERROR_INSUFFICIENT_AUTHORIZATION
TEST(BOND_MANAGEMENT_SERVICE_SERVER, write_control_point_ly_empty_ry_empty){
    validate_write_control_point(BMF_DELETE_ACTIVE_BOND_CLASSIC_AND_LE_WITH_AUTH, NULL, 
        BOND_MANAGEMENT_CMD_DELETE_ACTIVE_BOND_CLASSIC_AND_LE, NULL, 
        ATT_ERROR_INSUFFICIENT_AUTHORIZATION);
}

// locally yes (string exists), remote no (string empty) -> ATT_ERROR_INSUFFICIENT_AUTHORIZATION
TEST(BOND_MANAGEMENT_SERVICE_SERVER, write_control_point_ly_exists_rn_empty){
    validate_write_control_point(BMF_DELETE_ACTIVE_BOND_CLASSIC_AND_LE_WITH_AUTH, "local_auth_str", 
        BOND_MANAGEMENT_CMD_DELETE_ACTIVE_BOND_CLASSIC_AND_LE, NULL, 
        ATT_ERROR_INSUFFICIENT_AUTHORIZATION);
}

// locally yes (string exists), remote yes (string exists) -> ATT_ERROR_INSUFFICIENT_AUTHORIZATION
TEST(BOND_MANAGEMENT_SERVICE_SERVER, write_control_point_ly_exists_ry_exists){
    validate_write_control_point(BMF_DELETE_ACTIVE_BOND_CLASSIC_AND_LE_WITH_AUTH, "local_auth_str", 
        BOND_MANAGEMENT_CMD_DELETE_ACTIVE_BOND_CLASSIC_AND_LE, "remote_auth_str", 
        ATT_ERROR_INSUFFICIENT_AUTHORIZATION);
}

// locally yes (string exists), remote yes, (string exact) -> OK
TEST(BOND_MANAGEMENT_SERVICE_SERVER, write_control_point_ly_exists_ry_exact){
    validate_write_control_point(BMF_DELETE_ACTIVE_BOND_CLASSIC_AND_LE_WITH_AUTH, "auth_str", 
        BOND_MANAGEMENT_CMD_DELETE_ACTIVE_BOND_CLASSIC_AND_LE, "auth_str", 
        0);
}

// locally yes (string empty), remote yes (string exists) -> ATT_ERROR_INSUFFICIENT_AUTHORIZATION
TEST(BOND_MANAGEMENT_SERVICE_SERVER, write_control_point_ly_empty_ry_exists){
    validate_write_control_point(BMF_DELETE_ACTIVE_BOND_CLASSIC_AND_LE_WITH_AUTH, NULL, 
        BOND_MANAGEMENT_CMD_DELETE_ACTIVE_BOND_CLASSIC_AND_LE, "remote",  
        ATT_ERROR_INSUFFICIENT_AUTHORIZATION);
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
