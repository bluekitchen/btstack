#include "read_bounds.h"
#include "ble/gatt-service/ublox_spp_service_server.c"

int main(void){
    ublox_spp.fifo_client_configuration_descriptor_handle = 1;
    ublox_spp.credits_client_configuration_descriptor_handle = 2;
    ublox_spp.fifo_client_configuration_descriptor_value = 0x1234;
    ublox_spp.credits_client_configuration_descriptor_value = 0x1234;
    const uint8_t expected[] = {0x34, 0x12};
    check_read_bounds(ublox_spp_service_read_callback, 1, expected, sizeof(expected));
    check_read_bounds(ublox_spp_service_read_callback, 2, expected, sizeof(expected));
    return 0;
}
