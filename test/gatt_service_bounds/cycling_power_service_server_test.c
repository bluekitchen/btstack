#include "read_bounds.h"
#include "ble/gatt-service/cycling_power_service_server.c"

int main(void){
    cycling_power.measurement_client_configuration_descriptor_handle = 1;
    cycling_power.measurement_client_configuration_descriptor_notify = 4660;
    cycling_power.measurement_server_configuration_descriptor_handle = 2;
    cycling_power.measurement_server_configuration_descriptor_broadcast = 4660;
    cycling_power.vector_client_configuration_descriptor_handle = 3;
    cycling_power.vector_client_configuration_descriptor_notify = 4660;
    cycling_power.control_point_client_configuration_descriptor_handle = 4;
    cycling_power.control_point_client_configuration_descriptor_indicate = 4660;
    cycling_power.feature_value_handle = 5;
    cycling_power.feature_flags = 305419896;
    cycling_power.sensor_location_value_handle = 6;
    cycling_power.sensor_location = 3;
    const uint8_t expected_1[] = {0x34, 0x12};
    check_read_bounds(cycling_power_service_read_callback, 1, expected_1, sizeof(expected_1));
    const uint8_t expected_2[] = {0x34, 0x12};
    check_read_bounds(cycling_power_service_read_callback, 2, expected_2, sizeof(expected_2));
    const uint8_t expected_3[] = {0x34, 0x12};
    check_read_bounds(cycling_power_service_read_callback, 3, expected_3, sizeof(expected_3));
    const uint8_t expected_4[] = {0x34, 0x12};
    check_read_bounds(cycling_power_service_read_callback, 4, expected_4, sizeof(expected_4));
    const uint8_t expected_5[] = {0x78, 0x56, 0x34, 0x12};
    check_read_bounds(cycling_power_service_read_callback, 5, expected_5, sizeof(expected_5));
    const uint8_t expected_6[] = {0x3};
    check_read_bounds(cycling_power_service_read_callback, 6, expected_6, sizeof(expected_6));
    // Exercise every caller capacity, including the old 12-byte threshold.
    cycling_power.masked_measurement_flags = CYCLING_POWER_MEASUREMENT_FLAGS_CLEARED;
    cycling_power.default_measurement_flags = 0x1fff;
    for (uint16_t capacity = 0; capacity <= 40; capacity++){
        uint8_t * advertisement = malloc(capacity + 1u);
        memset(advertisement, 0xa5, capacity + 1u);
        int length = cycling_power_get_measurement_adv(100, advertisement, capacity);
        assert(advertisement[capacity] == 0xa5);
        assert(length <= capacity);
        if (capacity < 15){
            assert(length == 0);
        } else {
            assert(length >= 15);
            assert(length <= 31);
        }
        free(advertisement);
    }
    return 0;
}
