#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "btstack_util.h"
#include "btstack_debug.h"
#include "classic/obex.h"
#include "classic/obex_message_builder.h"

// mock hci_dump.c
extern "C" void hci_dump_log(int log_level, const char * format, ...){}

static const uint8_t  service_uuid[] = {0xbb, 0x58, 0x2b, 0x40, 0x42, 0xc, 0x11, 0xdb, 0xb0, 0xde, 0x8, 0x0, 0x20, 0xc, 0x9a, 0x66};
static const uint8_t  application_parameters[] = {0x29, 4, 0, 0, 0xFF, 0xFF};
static const char     path_element[] = "test";
static const uint8_t  flags = 1 << 1;
static const uint16_t maximum_obex_packet_length = 0xFFFF;
static const uint32_t connection_id = 10;
static const uint8_t  obex_version_number = OBEX_VERSION;
    

TEST_GROUP(OBEX_MESSAGE_BUILDER){
    uint8_t  actual_message[300];
    uint16_t actual_message_len;
    
    void setup(void){
        actual_message_len = sizeof(actual_message);
        memset(actual_message, 0, actual_message_len);
    }

    void CHECK_EQUAL_ARRAY(const uint8_t * expected, uint8_t * actual, int size){
        for (int i=0; i<size; i++){
            BYTES_EQUAL(expected[i], actual[i]);
        }
    }

    void validate(uint8_t * expected_message, uint16_t expected_len, uint8_t actual_status, uint8_t expected_status){
        CHECK_EQUAL(actual_status, expected_status);
        uint16_t actual_len = big_endian_read_16(actual_message, 1);
        CHECK_EQUAL(actual_len, expected_len);
        CHECK_EQUAL_ARRAY(actual_message, expected_message, expected_len);
    }

    void validate_invalid_parameter(uint8_t * expected_message, uint16_t expected_len, uint8_t actual_status){
        validate(expected_message, expected_len, actual_status, ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE);
    }

    void validate_success(uint8_t * expected_message, uint16_t expected_len, uint8_t actual_status){
        validate(expected_message, expected_len, actual_status, ERROR_CODE_SUCCESS);
    }
};

TEST(OBEX_MESSAGE_BUILDER, CreateConnect){
    uint8_t  expected_message[] = {OBEX_OPCODE_CONNECT, 0, 0, obex_version_number, flags, 0, 0};
    expected_message[2] = sizeof(expected_message);
    big_endian_store_16(expected_message, 5, maximum_obex_packet_length);
    
    uint8_t actual_status = obex_message_builder_request_create_connect(actual_message, actual_message_len, obex_version_number, flags, maximum_obex_packet_length);
    validate_success(expected_message, expected_message[2], actual_status);
}

TEST(OBEX_MESSAGE_BUILDER, CreateGet){
    uint8_t  expected_message[] = {OBEX_OPCODE_GET | OBEX_OPCODE_FINAL_BIT_MASK, 0, 0, OBEX_HEADER_CONNECTION_ID, 0, 0, 0, 0};
    expected_message[2] = sizeof(expected_message);
    big_endian_store_32(expected_message, 4, connection_id);
    
    uint8_t actual_status = obex_message_builder_request_create_get(actual_message, actual_message_len, connection_id);
    validate_success(expected_message, expected_message[2], actual_status);
}

TEST(OBEX_MESSAGE_BUILDER, CreateGetInvalidConnectionID){
    uint8_t  expected_message[] = {OBEX_OPCODE_GET | OBEX_OPCODE_FINAL_BIT_MASK, 0, 0};
    expected_message[2] = sizeof(expected_message);

    uint8_t actual_status = obex_message_builder_request_create_get(actual_message, actual_message_len, OBEX_CONNECTION_ID_INVALID);
    validate_invalid_parameter(expected_message, expected_message[2], actual_status);
}

TEST(OBEX_MESSAGE_BUILDER, CreatePut){
    uint8_t  expected_message[] = {OBEX_OPCODE_PUT | OBEX_OPCODE_FINAL_BIT_MASK, 0, 0, OBEX_HEADER_CONNECTION_ID, 0, 0, 0, 0};
    expected_message[2] = sizeof(expected_message);
    big_endian_store_32(expected_message, 4, connection_id);
    
    uint8_t actual_status = obex_message_builder_request_create_put(actual_message, actual_message_len, connection_id);
    validate_success(expected_message, expected_message[2], actual_status);
}

TEST(OBEX_MESSAGE_BUILDER, CreateSetPath){
    uint8_t  expected_message[] = {OBEX_OPCODE_SETPATH, 0, 0, flags, 0, OBEX_HEADER_CONNECTION_ID, 0, 0, 0, 0};
    expected_message[2] = sizeof(expected_message);
    big_endian_store_32(expected_message, 6, connection_id);
    
    uint8_t actual_status = obex_message_builder_request_create_set_path(actual_message, actual_message_len, flags, connection_id);
    validate_success(expected_message, expected_message[2], actual_status);
}

TEST(OBEX_MESSAGE_BUILDER, CreateAbort){
    uint8_t  expected_message[] = {OBEX_OPCODE_ABORT, 0, 0, OBEX_HEADER_CONNECTION_ID, 0, 0, 0, 0};
    expected_message[2] = sizeof(expected_message);
    big_endian_store_32(expected_message, 4, connection_id);
    
    uint8_t actual_status = obex_message_builder_request_create_abort(actual_message, actual_message_len, connection_id);
    validate_success(expected_message, expected_message[2], actual_status);
}

TEST(OBEX_MESSAGE_BUILDER, CreateDisconnect){
    uint8_t  expected_message[] = {OBEX_OPCODE_DISCONNECT, 0, 0, OBEX_HEADER_CONNECTION_ID, 0, 0, 0, 0};
    expected_message[2] = sizeof(expected_message);
    big_endian_store_32(expected_message, 4, connection_id);
    
    uint8_t actual_status = obex_message_builder_request_create_disconnect(actual_message, actual_message_len, connection_id);
    validate_success(expected_message, expected_message[2], actual_status);
}

TEST(OBEX_MESSAGE_BUILDER, CreateGetAddHeader){
    uint8_t  expected_message[] = {OBEX_OPCODE_GET | OBEX_OPCODE_FINAL_BIT_MASK, 0, 0, OBEX_HEADER_CONNECTION_ID, 0, 0, 0, 0, OBEX_HEADER_SINGLE_RESPONSE_MODE, OBEX_SRM_ENABLE};
    expected_message[2] = 8; // only get request
    big_endian_store_32(expected_message, 4, connection_id);

    uint8_t actual_status = obex_message_builder_request_create_get(actual_message, actual_message_len, connection_id);
    validate_success(expected_message, expected_message[2], actual_status);
    
    actual_status = obex_message_builder_header_add_srm_enable(actual_message, actual_message_len);
    expected_message[2] += 2;
    validate_success(expected_message, expected_message[2], actual_status);
}

TEST(OBEX_MESSAGE_BUILDER, CreateConnectWithHeaderTarget){
    uint8_t  expected_message[] = {OBEX_OPCODE_CONNECT, 0, 0, obex_version_number, flags, 0, 0, 
        // service UUID
        OBEX_HEADER_TARGET, 0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    };
    int header_len = 3 + sizeof(service_uuid);
    expected_message[2] = 7;
    big_endian_store_16(expected_message, 5, maximum_obex_packet_length);
    big_endian_store_16(expected_message, 8, header_len);
    memcpy(expected_message + 10, service_uuid, sizeof(service_uuid));

    uint8_t actual_status = obex_message_builder_request_create_connect(actual_message, actual_message_len, obex_version_number, flags, maximum_obex_packet_length);
    validate_success(expected_message, expected_message[2], actual_status);

    actual_status = obex_message_builder_header_add_target(actual_message, actual_message_len, service_uuid, sizeof(service_uuid));
    expected_message[2] += header_len;
    validate_success(expected_message, expected_message[2], actual_status);
}

TEST(OBEX_MESSAGE_BUILDER, CreateConnectWithHeaderApplicationParameters){
    uint8_t  expected_message[] = {OBEX_OPCODE_CONNECT, 0, 0, obex_version_number, flags, 0, 0, 
        OBEX_HEADER_APPLICATION_PARAMETERS, 0,0, 0,0,0,0,0,0
    };
    int header_len = 3 + sizeof(application_parameters);
    expected_message[2] = 7;
    big_endian_store_16(expected_message, 5, maximum_obex_packet_length);
    big_endian_store_16(expected_message, 8, header_len);
    memcpy(expected_message + 10, application_parameters, sizeof(application_parameters));

    uint8_t actual_status = obex_message_builder_request_create_connect(actual_message, actual_message_len, obex_version_number, flags, maximum_obex_packet_length);
    validate_success(expected_message, expected_message[2], actual_status);

    actual_status = obex_message_builder_header_add_application_parameters(actual_message, actual_message_len, &application_parameters[0], sizeof(application_parameters));
    expected_message[2] += header_len;
    validate_success(expected_message, expected_message[2], actual_status);
}

TEST(OBEX_MESSAGE_BUILDER, CreateSetPathWithName){
    uint8_t  expected_message[] = {OBEX_OPCODE_SETPATH, 0, 0, flags, 0, OBEX_HEADER_CONNECTION_ID, 0, 0, 0, 0, OBEX_HEADER_NAME, 0x00, 0x0D, 0x00, 0x74, 0x00, 0x65, 0x00, 0x73, 0x00, 0x74, 0x00, 0x00};
    expected_message[2] = 10;
    big_endian_store_32(expected_message, 6, connection_id);
    
    uint8_t actual_status = obex_message_builder_request_create_set_path(actual_message, actual_message_len, flags, connection_id);
    validate_success(expected_message, expected_message[2], actual_status);

    expected_message[2] += 13;
    actual_status = obex_message_builder_header_add_name(actual_message, actual_message_len, (const char *) path_element); 
    validate_success(expected_message, expected_message[2], actual_status);
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
