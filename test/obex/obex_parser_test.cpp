#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"
#include "CppUTestExt/MockSupport.h"

#include "classic/obex.h"
#include "classic/obex_parser.h"
#include "classic/obex_message_builder.h"
#include "btstack_util.h"

static const uint8_t  flags = 1 << 1;
static const uint16_t maximum_obex_packet_length = 0xFFFF;
static const uint8_t  obex_version_number = OBEX_VERSION;
static const uint8_t  target[] = { 1, 2, 3, 4};

// from parser_callback
static uint8_t  test_header_id;
static uint8_t  test_header_buffer[100];
static uint16_t test_header_len;

// mock hci_dump.c
extern "C" void hci_dump_log(int log_level, const char * format, ...){}

static void parser_callback(void * user_data, uint8_t header_id, uint16_t total_len, uint16_t data_offset, const uint8_t * data_buffer, uint16_t data_len){
    if (obex_parser_header_store(test_header_buffer, sizeof(test_header_buffer), total_len, data_offset, data_buffer, data_len) == OBEX_PARSER_HEADER_COMPLETE){
        test_header_len = total_len;
        test_header_id  = header_id;
    }
}

TEST_GROUP(OBEX_PARSER){
    obex_parser_t parser;
    uint8_t  message[300];

    void setup(void){
        test_header_id = 0;
        test_header_len = 0;
    }
    void teardown(void){
    }
    void parse_request(void){
        obex_parser_init_for_request(&parser, &parser_callback, NULL);
        uint16_t message_len = big_endian_read_16(message, 1);
        for (uint16_t i = 0; i < message_len - 1;i++){
            obex_parser_object_state_t parser_state = obex_parser_process_data(&parser, &message[i], 1);
            CHECK_EQUAL(OBEX_PARSER_OBJECT_STATE_INCOMPLETE, parser_state);
        }
        obex_parser_object_state_t parser_state = obex_parser_process_data(&parser, &message[message_len-1], 1);
        CHECK_EQUAL(OBEX_PARSER_OBJECT_STATE_COMPLETE, parser_state);
    }
    void parse_response(uint8_t opcode){
        obex_parser_init_for_response(&parser, opcode, &parser_callback, NULL);
        uint16_t message_len = big_endian_read_16(message, 1);
        for (uint16_t i = 0; i < message_len - 1;i++){
            obex_parser_object_state_t parser_state = obex_parser_process_data(&parser, &message[i], 1);
            CHECK_EQUAL(OBEX_PARSER_OBJECT_STATE_INCOMPLETE, parser_state);
        }
        obex_parser_object_state_t parser_state = obex_parser_process_data(&parser, &message[message_len-1], 1);
        CHECK_EQUAL(OBEX_PARSER_OBJECT_STATE_COMPLETE, parser_state);
    }
};

TEST(OBEX_PARSER, RequestOverrun){
    (void) obex_message_builder_request_create_connect(message, sizeof(message), obex_version_number, flags, maximum_obex_packet_length);
    uint16_t message_len = big_endian_read_16(message, 1);
    for (uint16_t i = 0; i < message_len - 1;i++){
        obex_parser_object_state_t parser_state = obex_parser_process_data(&parser, &message[i], 1);
        CHECK_EQUAL(OBEX_PARSER_OBJECT_STATE_INCOMPLETE, parser_state);
    }
    obex_parser_object_state_t parser_state = obex_parser_process_data(&parser, &message[message_len-1], 1);
    CHECK_EQUAL(OBEX_PARSER_OBJECT_STATE_COMPLETE, parser_state);
    parser_state = obex_parser_process_data(&parser, &message[message_len], 1);
    CHECK_EQUAL(OBEX_PARSER_OBJECT_STATE_OVERRUN, parser_state);
}

TEST(OBEX_PARSER, RequestInvalid){
    (void) obex_message_builder_request_create_connect(message, sizeof(message), obex_version_number, flags, maximum_obex_packet_length);
    // decrease packet len
    uint16_t message_len = big_endian_read_16(message, 1) - 1;
    big_endian_store_16(message, 1, message_len);
    for (uint16_t i = 0; i < message_len;i++){
        obex_parser_object_state_t parser_state = obex_parser_process_data(&parser, &message[i], 1);
        if (i < 2){
            CHECK_EQUAL(OBEX_PARSER_OBJECT_STATE_INCOMPLETE, parser_state);
        } else {
            CHECK_EQUAL(OBEX_PARSER_OBJECT_STATE_INVALID, parser_state);
        }
    }
}

TEST(OBEX_PARSER, ConnectRequest){
    (void) obex_message_builder_request_create_connect(message, sizeof(message), obex_version_number, flags, maximum_obex_packet_length);
    parse_request();
    obex_parser_operation_info_t op_info;
    obex_parser_get_operation_info(&parser, &op_info);
    CHECK_EQUAL(OBEX_OPCODE_CONNECT, op_info.opcode);
    CHECK_EQUAL(obex_version_number, op_info.obex_version_number);
    CHECK_EQUAL(flags, op_info.flags);
    CHECK_EQUAL(maximum_obex_packet_length, op_info.max_packet_length);
}

TEST(OBEX_PARSER, ConnectRequestWithTarget){
    (void) obex_message_builder_request_create_connect(message, sizeof(message), obex_version_number, flags, maximum_obex_packet_length);
    (void) obex_message_builder_header_add_target(message, sizeof(message), target, sizeof(target));
    parse_request();
    obex_parser_operation_info_t op_info;
    obex_parser_get_operation_info(&parser, &op_info);
    CHECK_EQUAL(OBEX_HEADER_TARGET, test_header_id);
    CHECK_EQUAL(sizeof(target), test_header_len);
    MEMCMP_EQUAL(target, test_header_buffer, sizeof(target));
}

TEST(OBEX_PARSER, ConnectResponse){
    // no create response yet, fake it
    (void) obex_message_builder_request_create_connect(message, sizeof(message), obex_version_number, flags, maximum_obex_packet_length);
    message[0] = OBEX_RESP_SUCCESS;
    parse_response(OBEX_OPCODE_CONNECT);
    obex_parser_operation_info_t op_info;
    obex_parser_get_operation_info(&parser, &op_info);
    CHECK_EQUAL(OBEX_RESP_SUCCESS, op_info.response_code);
    CHECK_EQUAL(obex_version_number, op_info.obex_version_number);
    CHECK_EQUAL(flags, op_info.flags);
    CHECK_EQUAL(maximum_obex_packet_length, op_info.max_packet_length);
}

TEST(OBEX_PARSER, GetResponseWithSRM){
    // no get response yet, fake it
    (void) obex_message_builder_request_create_get(message, sizeof(message), 0x1234);
    obex_message_builder_header_add_srm_enable(message, sizeof(message));
    parse_request();
    obex_parser_operation_info_t op_info;
    obex_parser_get_operation_info(&parser, &op_info);
}

TEST(OBEX_PARSER, SetPathResponse){
    const uint8_t set_path_response_success[] = { 0xa0, 0x00, 0x03};
    memcpy(message, set_path_response_success, sizeof(set_path_response_success));
    parse_response(OBEX_OPCODE_SETPATH);
}

/** App Param Parser */

static uint8_t  test_tag_id;
static uint8_t  test_tag_buffer[100];
static uint16_t test_tag_len;

void app_param_parser_callback(void * user_data, uint8_t tag_id, uint8_t total_len, uint8_t data_offset, const uint8_t * data_buffer, uint8_t data_len){
    if (obex_app_param_parser_tag_store(test_header_buffer, sizeof(test_header_buffer), total_len, data_offset, data_buffer, data_len) == OBEX_APP_PARAM_PARSER_TAG_COMPLETE){
        test_tag_len = total_len;
        test_tag_id  = tag_id;
    }
}

TEST_GROUP(APP_PARAM_PARSER){
        obex_app_param_parser_t parser;
        void setup(void){
            test_tag_id = 0;
            test_tag_len = 0;
        }
        void teardown(void){
        }
        void parse_app_params(const uint8_t * app_params, uint8_t param_len){
            obex_app_param_parser_init(&parser, &app_param_parser_callback, param_len, NULL);
            for (int i = 0; i < param_len - 1;i++){
                obex_app_param_parser_params_state_t parser_state = obex_app_param_parser_process_data(&parser, &app_params[i], 1);
                CHECK_EQUAL(OBEX_APP_PARAM_PARSER_PARAMS_STATE_INCOMPLETE, parser_state);
            }
            if (param_len > 0){
                obex_app_param_parser_params_state_t parser_state = obex_app_param_parser_process_data(&parser, &app_params[param_len-1], 1);
                CHECK_EQUAL(OBEX_APP_PARAM_PARSER_PARAMS_STATE_COMPLETE, parser_state);
            }
        }
};
TEST(APP_PARAM_PARSER, EmptyParams){
    parse_app_params(NULL, 0);
    CHECK_EQUAL(0, test_tag_id);
    CHECK_EQUAL(0, test_tag_len);
}

TEST(APP_PARAM_PARSER, SingleParam){
    uint8_t message[] = { 0x01, 0x02, 0x03, 0x4};
    parse_app_params(message, sizeof(message));
    CHECK_EQUAL(1, test_tag_id);
    CHECK_EQUAL(2, test_tag_len);
}

TEST(APP_PARAM_PARSER, Overrun){
    uint8_t message[] = { 0x01, 0x02, 0x03, 0x4};
    parse_app_params(message, sizeof(message));
    obex_app_param_parser_params_state_t parser_state = obex_app_param_parser_process_data(&parser, &message[0], 1);
    CHECK_EQUAL(OBEX_APP_PARAM_PARSER_PARAMS_STATE_OVERRUN, parser_state);
    CHECK_EQUAL(1, test_tag_id);
    CHECK_EQUAL(2, test_tag_len);
}

TEST(APP_PARAM_PARSER, InvalidTagLen){
    uint8_t message[] = { 0x01, 0x04, 0x03, 0x4};
    obex_app_param_parser_t parser;
    obex_app_param_parser_init(&parser, &app_param_parser_callback, sizeof(message), NULL);
    obex_app_param_parser_params_state_t parser_state;
    parser_state = obex_app_param_parser_process_data(&parser, &message[0], 1);
    CHECK_EQUAL(OBEX_APP_PARAM_PARSER_PARAMS_STATE_INCOMPLETE, parser_state);
    parser_state = obex_app_param_parser_process_data(&parser, &message[1], 1);
    CHECK_EQUAL(OBEX_APP_PARAM_PARSER_PARAMS_STATE_INVALID, parser_state);
    CHECK_EQUAL(0, test_tag_id);
    CHECK_EQUAL(0, test_tag_len);
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
