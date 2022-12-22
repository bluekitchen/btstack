
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
#include "btstack_util.h"
#include "bluetooth.h"
#include "bluetooth_gatt.h"

#include "le-audio/gatt-service/microphone_control_service_server.h"
#include "microphone_control_service_server_test.h"
#include "mock_att_server.h"

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static aics_info_t aics_info[] = {
    {
        {1, AICS_MUTE_MODE_NOT_MUTED, AICS_GAIN_MODE_AUTOMATIC},
        {1, -10, 10},
            AICS_AUDIO_INPUT_TYPE_MICROPHONE,
        "audio_input_description1",
        packet_handler
    },
    {
        {2, AICS_MUTE_MODE_NOT_MUTED, AICS_GAIN_MODE_AUTOMATIC},
        {1, -11, 11},
            AICS_AUDIO_INPUT_TYPE_MICROPHONE,
        "audio_input_description2",
        packet_handler
    },
    {
            {3, AICS_MUTE_MODE_NOT_MUTED, AICS_GAIN_MODE_MANUAL},
            {2, -12, 12},
            AICS_AUDIO_INPUT_TYPE_MICROPHONE,
            "audio_input_description3",
            packet_handler
    }

};
static uint8_t aics_info_num = 3;

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    UNUSED(packet);

    if (packet_type != HCI_EVENT_PACKET) return;
}


TEST_GROUP(MICROPHONE_CONTROL_SERVICE_SERVER){ 
    att_service_handler_t * service; 
    uint16_t con_handle;
    uint16_t mute_value_handle;
    uint16_t mute_value_handle_client_configuration;

    void setup(void){
        // setup database
        att_set_db(profile_data);
        mute_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(0, 0xffff, ORG_BLUETOOTH_CHARACTERISTIC_MUTE);
        mute_value_handle_client_configuration = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(0, 0xffff, ORG_BLUETOOTH_CHARACTERISTIC_MUTE);

        // setup battery service
        microphone_control_service_server_init(GATT_MICROPHONE_CONTROL_MUTE_OFF, aics_info_num, aics_info);

        service = mock_att_server_get_service();
        con_handle = 0x00;
    }

    void teardown(){
        mock_deinit();
    }
};

TEST(MICROPHONE_CONTROL_SERVICE_SERVER, set_mute_value_trigger_can_send_now){
    // enable notifications
    const uint8_t enable_notify[]= { 0x1, 0x0 };
    mock_att_service_write_callback(con_handle, mute_value_handle_client_configuration, ATT_TRANSACTION_MODE_NONE, 0, enable_notify, sizeof(enable_notify));

    // set battery to trigger notification
    mock().expectOneCall("att_server_register_can_send_now_callback");
    microphone_control_service_server_set_mute(GATT_MICROPHONE_CONTROL_MUTE_ON);
    mock().checkExpectations();
    mock().expectOneCall("att_server_notify");
    mock_att_service_trigger_can_send_now();
    mock().checkExpectations();
}

TEST(MICROPHONE_CONTROL_SERVICE_SERVER, lookup_attribute_handles){
    CHECK(mute_value_handle != 0);
    CHECK(mute_value_handle_client_configuration != 0);
}

TEST(MICROPHONE_CONTROL_SERVICE_SERVER, set_mute_value){
    // mute_value_handle_client_configuration not set
    mock().expectNCalls(con_handle, "att_server_register_can_send_now_callback");
    microphone_control_service_server_set_mute(GATT_MICROPHONE_CONTROL_MUTE_ON);
    mock().checkExpectations();

    // mute_value_handle_client_configuration set
    mock().expectOneCall("att_server_register_can_send_now_callback");
    const uint8_t enable_notify[]= { 0x1, 0x0 };
    mock_att_service_write_callback(con_handle, mute_value_handle_client_configuration, ATT_TRANSACTION_MODE_NONE, 0, enable_notify, sizeof(enable_notify));
    microphone_control_service_server_set_mute(GATT_MICROPHONE_CONTROL_MUTE_OFF);
    mock().checkExpectations();
}

TEST(MICROPHONE_CONTROL_SERVICE_SERVER, set_wrong_handle_mute_value){
    // mute_value_handle_client_configuration not set
    mock().expectNCalls(con_handle, "att_server_register_can_send_now_callback");
    microphone_control_service_server_set_mute(GATT_MICROPHONE_CONTROL_MUTE_ON);
    mock().checkExpectations();

    // // mute_value_handle_client_configuration set
    mock().expectNoCall("att_server_register_can_send_now_callback");
    const uint8_t enable_notify[]= { 0x1, 0x0 };
    mock_att_service_write_callback(con_handle, mute_value_handle_client_configuration + 10, ATT_TRANSACTION_MODE_NONE, 0, enable_notify, sizeof(enable_notify));
    microphone_control_service_server_set_mute(GATT_MICROPHONE_CONTROL_MUTE_OFF);
    mock().checkExpectations();
}


TEST(MICROPHONE_CONTROL_SERVICE_SERVER, read_mute_value){
    uint8_t response[2];
    uint16_t response_len;

    // invalid attribute handle
    response_len = mock_att_service_read_callback(con_handle, 0xffff, 0xffff, response, sizeof(response));
    CHECK_EQUAL(0, response_len);

    response_len = mock_att_service_read_callback(con_handle, mute_value_handle, 0, response, sizeof(response));
    CHECK_EQUAL(1, response_len);

    response_len = mock_att_service_read_callback(con_handle, mute_value_handle_client_configuration, 0, response, sizeof(response));
    CHECK_EQUAL(2, response_len);
}


int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
