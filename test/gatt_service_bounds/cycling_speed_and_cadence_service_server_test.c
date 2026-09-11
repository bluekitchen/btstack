#include "read_bounds.h"
#include "ble/gatt-service/cycling_speed_and_cadence_service_server.c"

int main(void){
    cycling_speed_and_cadence.measurement_client_configuration_descriptor_handle = 1;
    cycling_speed_and_cadence.measurement_client_configuration_descriptor_notify = 4660;
    cycling_speed_and_cadence.control_point_client_configuration_descriptor_handle = 2;
    cycling_speed_and_cadence.control_point_client_configuration_descriptor_indicate = 4660;
    cycling_speed_and_cadence.sensor_location_value_handle = 3;
    cycling_speed_and_cadence.sensor_location = 3;
    const uint8_t expected_1[] = {0x34, 0x12};
    check_read_bounds(cycling_speed_and_cadence_service_read_callback, 1, expected_1, sizeof(expected_1));
    const uint8_t expected_2[] = {0x34, 0x12};
    check_read_bounds(cycling_speed_and_cadence_service_read_callback, 2, expected_2, sizeof(expected_2));
    const uint8_t expected_3[] = {0x3};
    check_read_bounds(cycling_speed_and_cadence_service_read_callback, 3, expected_3, sizeof(expected_3));
    cycling_speed_and_cadence.feature_handle = 4;
    cycling_speed_and_cadence.wheel_revolution_data_supported = 1;
    const uint8_t feature[] = {1, 0};
    check_read_bounds(cycling_speed_and_cadence_service_read_callback, 4, feature, sizeof(feature));
    return 0;
}
