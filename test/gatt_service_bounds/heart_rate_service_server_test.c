#include "read_bounds.h"
#include "ble/gatt-service/heart_rate_service_server.c"

int main(void){
    heart_rate.measurement_client_configuration_descriptor_handle = 1;
    heart_rate.measurement_client_configuration_descriptor_notify = 4660;
    heart_rate.sensor_location_value_handle = 2;
    heart_rate.sensor_location = 3;
    const uint8_t expected_1[] = {0x34, 0x12};
    check_read_bounds(heart_rate_service_read_callback, 1, expected_1, sizeof(expected_1));
    const uint8_t expected_2[] = {0x3};
    check_read_bounds(heart_rate_service_read_callback, 2, expected_2, sizeof(expected_2));
    return 0;
}
