// *****************************************************************************
// test generic GATT Service Client descriptor discovery
// *****************************************************************************

#include <string.h>

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "bluetooth_gatt.h"
#include "btstack_event.h"
#include "ble/gatt_service_client.h"
#include "mock_gatt_client.h"

static const hci_con_handle_t con_handle = 0x0001;
static bool connected;
static bool gatt_service_client_initialized;

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(size);

    if (hci_event_packet_get_type(packet) != HCI_EVENT_GATTSERVICE_META) return;
    if (hci_event_gattservice_meta_get_subevent_code(packet) != GATTSERVICE_SUBEVENT_CLIENT_CONNECTED) return;
    connected = gattservice_subevent_client_connected_get_status(packet) == ERROR_CODE_SUCCESS;
}

TEST_GROUP(GATT_SERVICE_CLIENT){
    gatt_service_client_t client;
    gatt_service_client_connection_t connection;
    gatt_service_client_characteristic_t characteristics[1];
    const uint16_t characteristic_uuids[1] = { ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL };

    void setup(void){
        memset(&client, 0, sizeof(client));
        memset(&connection, 0, sizeof(connection));
        memset(characteristics, 0, sizeof(characteristics));
        connected = false;
        mock_gatt_client_reset();
        if (!gatt_service_client_initialized){
            gatt_service_client_init();
            gatt_service_client_initialized = true;
        }
        gatt_service_client_register_client_with_uuid16s(&client, packet_handler, characteristic_uuids, 1);
    }

    void teardown(void){
        if (connection.state == GATT_SERVICE_CLIENT_STATE_CONNECTED){
            CHECK_EQUAL(ERROR_CODE_SUCCESS, gatt_service_client_disconnect(&connection));
        }
        CHECK_EQUAL(ERROR_CODE_SUCCESS, gatt_service_client_unregister_client(&client));
    }

    void setup_notifying_battery_service(void){
        mock_gatt_client_add_primary_service_uuid16(ORG_BLUETOOTH_SERVICE_BATTERY_SERVICE);
        mock_gatt_client_add_characteristic_uuid16(ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL, ATT_PROPERTY_NOTIFY);
    }

    void connect(void){
        CHECK_EQUAL(ERROR_CODE_SUCCESS, gatt_service_client_connect_primary_service_with_uuid16(
            con_handle, &client, &connection, ORG_BLUETOOTH_SERVICE_BATTERY_SERVICE, characteristics, 1));
        mock_gatt_client_run();
    }
};

TEST(GATT_SERVICE_CLIENT, duplicate_ccc_descriptors_do_not_advance_characteristic_index){
    setup_notifying_battery_service();
    mock_gatt_client_characteristic_descriptor_t * first_ccc =
        mock_gatt_client_add_characteristic_descriptor_uuid16(ORG_BLUETOOTH_DESCRIPTOR_GATT_CLIENT_CHARACTERISTIC_CONFIGURATION);
    mock_gatt_client_add_characteristic_descriptor_uuid16(ORG_BLUETOOTH_DESCRIPTOR_GATT_CLIENT_CHARACTERISTIC_CONFIGURATION);

    connect();

    CHECK_TRUE(connected);
    CHECK_EQUAL(GATT_SERVICE_CLIENT_STATE_CONNECTED, connection.state);
    CHECK_EQUAL(first_ccc->handle, characteristics[0].client_configuration_handle);
}

TEST(GATT_SERVICE_CLIENT, missing_ccc_descriptor_advances_to_connected){
    setup_notifying_battery_service();

    connect();

    CHECK_TRUE(connected);
    CHECK_EQUAL(GATT_SERVICE_CLIENT_STATE_CONNECTED, connection.state);
    CHECK_EQUAL(0, characteristics[0].client_configuration_handle);
}

int main(int argc, char **argv){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
